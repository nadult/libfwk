// This plugin checks if functions which may call EXCEPT are also marked as EXCEPT or NOEXCEPT.
//
// TODO: it slows down compilation by about 20%; FIX IT!

#include "clang/AST/AST.h"
#include "clang/AST/ASTConsumer.h"
#include "clang/AST/RecursiveASTVisitor.h"
#include "clang/Frontend/CompilerInstance.h"
#include "clang/Frontend/FrontendPluginRegistry.h"
#include "clang/Sema/Sema.h"
#include "llvm/Support/raw_ostream.h"
#include <iostream>

using namespace clang;
using std::string;

//#define DBG_PRINT(...) printf(__VA_ARGS__)
#define DBG_PRINT(...)

namespace {

enum class Annotation { none, except, not_except };

Annotation getAnnotation(const Decl *decl) {
	// TODO: check for multiple conflicting annotations
	for(auto *attr : decl->attrs()) {
		if(attr->getKind() == clang::attr::Kind::Annotate) {
			AnnotateAttr *aattr = cast<AnnotateAttr>(attr);
			if(aattr->getAnnotation() == "except")
				return Annotation::except;
			if(aattr->getAnnotation() == "not_except")
				return Annotation::not_except;
		}
	}
	return Annotation::none;
}

bool isExpectedType(ASTContext &ctx, QualType qtype) {
	qtype = qtype.getSingleStepDesugaredType(ctx);
	auto *type = qtype.getTypePtrOrNull();

	if(type && type->getTypeClass() == Type::TypeClass::TemplateSpecialization) {
		const auto *spec = cast<TemplateSpecializationType>(type);
		auto tname = spec->getTemplateName();
		auto *tdecl = tname.getAsTemplateDecl();
		if(tdecl)
			return tdecl->getName() == "Expected" && tdecl->getQualifiedNameAsString() == "fwk::Expected";
	}

	return false;
}

bool hasAnnotation(const Decl *decl, Annotation anno) {
	auto cur = getAnnotation(decl);
	if(cur != Annotation::none)
		return cur == anno;

	using DK = Decl::Kind;
	auto dk = decl->getKind();

	if(dk == DK::CXXMethod || dk == DK::CXXConstructor || dk == DK::CXXDestructor) {
		auto *spec = cast<CXXMethodDecl>(decl);
		auto class_anno = getAnnotation(spec->getParent());
		if(class_anno != Annotation::none)
			return class_anno == anno;
	}
	return anno == Annotation::none;
}

bool hasExceptAnnotation(const Expr *expr) {
	// TODO: there is no need for recursion here, we're doing it already for Stmts
	if(expr->getStmtClass() == Stmt::StmtClass::DeclRefExprClass) {
		const auto *spec = cast<DeclRefExpr>(expr);
		return hasAnnotation(spec->getDecl(), Annotation::except);
	} else if(expr->getStmtClass() == Stmt::StmtClass::ImplicitCastExprClass) {
		const auto *spec = cast<ImplicitCastExpr>(expr);
		return hasExceptAnnotation(spec->getSubExpr());
	} else if(expr->getStmtClass() == Stmt::StmtClass::UnresolvedLookupExprClass) {
		const auto *spec = cast<UnresolvedLookupExpr>(expr);
		return false;
	}

	return false;
}

bool hasExceptAnnotation(const Stmt *stmt) {
	if(!stmt)
		return false;

	using SC = Stmt::StmtClass;
	auto clazz = stmt->getStmtClass();
	if(clazz == SC::CallExprClass || clazz == SC::CXXMemberCallExprClass) {
		const auto *ce = cast<CallExpr>(stmt);
		if(hasExceptAnnotation(ce->getCallee()))
			return true;
	}
	if(clazz == SC::MemberExprClass) {
		const auto *ce = cast<MemberExpr>(stmt);
		if(hasAnnotation(ce->getMemberDecl(), Annotation::except))
			return true;
	}
	if(clazz == SC::CXXConstructExprClass) {
		const auto *expr = cast<CXXConstructExpr>(stmt);
		CXXConstructorDecl *constr = expr->getConstructor();
		if(hasAnnotation(constr, Annotation::except))
			return true;
	}

	for(auto child : stmt->children())
		if(hasExceptAnnotation(child))
			return true;
	return false;
}

bool hasMissingExceptAnnotation(const FunctionDecl *decl) {
	if(!decl->hasBody())
		return false;

	using TK = FunctionDecl::TemplatedKind;
	auto tk = decl->getTemplatedKind();
	if(tk == TK::TK_FunctionTemplate) // TODO: how to handle this?
		return false;
	auto not_annotated = hasAnnotation(decl, Annotation::none);
	bool body_except = hasExceptAnnotation(decl->getBody());

	return not_annotated && body_except;
}

string functionName(const FunctionDecl *decl) {
	string name = decl->getQualifiedNameAsString();
	name += "(";
	for(auto param : decl->parameters()) {
		auto pname = param->getQualifiedNameAsString();
		auto ptype = param->getType().getAsString();
		name += ptype + " " + pname + ",";
	}
	if(name.back() == ',')
		name.pop_back();
	name += ")";
	return name;
}

const FunctionDecl *firstDeclaration(const FunctionDecl *decl) {
	auto pdecl = decl;
	do {
		decl = pdecl;
		pdecl = decl->getPreviousDecl();
	} while(pdecl);
	return decl;
}

template <class T> void makeUnique(std::vector<T> &vec) {
	std::sort(begin(vec), end(vec));
	auto it = std::unique(begin(vec), end(vec));
	vec.resize(it - vec.begin());
}

class CheckErrorAttribsConsumer : public ASTConsumer {
	CompilerInstance &ci;

  public:
	CheckErrorAttribsConsumer(CompilerInstance &ci)
		: ci(ci) {}

	void reportError(const FunctionDecl *decl) {
		auto &diags = ci.getDiagnostics();
		auto diag_id =
			diags.getCustomDiagID(DiagnosticsEngine::Error,
								  "Missing EXCEPT attribute (function may generate exceptions)");

		// TODO: inform about locations which are causing this error
		diags.Report(decl->getSourceRange().getBegin(), diag_id);
	}

	struct Visitor : public RecursiveASTVisitor<Visitor> {
		Visitor(ASTContext &ctx) : ctx(ctx) {}

		bool shouldVisitTemplateInstantiations() const { return true; }

		bool VisitFunctionDecl(FunctionDecl *decl) {
			bool missing = hasMissingExceptAnnotation(decl);
			bool return_expected = missing && isExpectedType(ctx, decl->getReturnType());

			if(missing) {
				DBG_PRINT("Function: %s%s%s\n", functionName(decl).c_str(),
						  missing ? " Missing attribute" : "",
						  return_expected ? " Returns expected" : "");
			}
			if(missing && !return_expected)
				error_decls.emplace_back(firstDeclaration(decl));
			return true;
		}

		std::vector<const FunctionDecl *> error_decls;
		ASTContext &ctx;
	};

	void HandleTranslationUnit(ASTContext &ctx) override {
		Visitor v(ctx);
		v.TraverseDecl(ctx.getTranslationUnitDecl());

		auto &edecls = v.error_decls;
		makeUnique(edecls);
		for(auto *decl : edecls)
			reportError(decl);

		clang::Sema &sema = ci.getSema();
	}
};

class CheckErrorAttribsAction : public PluginASTAction {
  protected:
	std::unique_ptr<ASTConsumer> CreateASTConsumer(CompilerInstance &CI, llvm::StringRef) override {
		return llvm::make_unique<CheckErrorAttribsConsumer>(CI );
	}

	bool ParseArgs(const CompilerInstance &CI, const std::vector<std::string> &args) override {
		return true;
	}

};

}

static FrontendPluginRegistry::Add<CheckErrorAttribsAction>
	X("check-error-attribs", "Checks if error attribs are in place");
