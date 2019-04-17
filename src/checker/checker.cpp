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
#include <unordered_map>

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
			return tdecl->getName() == "Expected" &&
				   tdecl->getQualifiedNameAsString() == "fwk::Expected";
	}

	return false;
}

// TODO: fix this function...
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

void getExcepts(const Expr *expr, std::vector<SourceLocation> &out) {
	// TODO: there is no need for recursion here, we're doing it already for Stmts
	if(expr->getStmtClass() == Stmt::StmtClass::DeclRefExprClass) {
		const auto *spec = cast<DeclRefExpr>(expr);
		if(hasAnnotation(spec->getDecl(), Annotation::except)) {
			out.emplace_back(spec->getSourceRange().getBegin());
			out.emplace_back(spec->getDecl()->getSourceRange().getBegin());
		}
	} else if(expr->getStmtClass() == Stmt::StmtClass::ImplicitCastExprClass) {
		const auto *spec = cast<ImplicitCastExpr>(expr);
		getExcepts(spec->getSubExpr(), out);
	}
}

void getExcepts(const Stmt *stmt, std::vector<SourceLocation> &out) {
	if(!stmt)
		return;

	using SC = Stmt::StmtClass;
	auto clazz = stmt->getStmtClass();
	if(clazz == SC::CallExprClass || clazz == SC::CXXMemberCallExprClass) {
		const auto *ce = cast<CallExpr>(stmt);
		getExcepts(ce->getCallee(), out);
	}
	if(clazz == SC::MemberExprClass) {
		const auto *ce = cast<MemberExpr>(stmt);
		if(hasAnnotation(ce->getMemberDecl(), Annotation::except)) {
			out.emplace_back(ce->getSourceRange().getBegin());
			out.emplace_back(ce->getMemberDecl()->getSourceRange().getBegin());
		}
	}
	if(clazz == SC::CXXConstructExprClass) {
		const auto *expr = cast<CXXConstructExpr>(stmt);
		CXXConstructorDecl *constr = expr->getConstructor();
		if(hasAnnotation(constr, Annotation::except)) {
			out.emplace_back(expr->getSourceRange().getBegin());
			out.emplace_back(constr->getSourceRange().getBegin());
		}
	}

	for(auto child : stmt->children())
		getExcepts(child, out);
}

std::vector<SourceLocation> getFunctionExcepts(const FunctionDecl *decl) {
	if(!decl->hasBody())
		return {};
	using TK = FunctionDecl::TemplatedKind;
	auto tk = decl->getTemplatedKind();
	if(tk == TK::TK_FunctionTemplate) // TODO: how to handle this?
		return {};

	std::vector<SourceLocation> out;
	getExcepts(decl->getBody(), out);
	return out;
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
	CheckErrorAttribsConsumer(CompilerInstance &ci) : ci(ci) {}

	void reportError(const FunctionDecl *decl) {}

	struct Visitor : public RecursiveASTVisitor<Visitor> {
		Visitor(ASTContext &ctx) : ctx(ctx) {}

		bool shouldVisitTemplateInstantiations() const { return true; }

		bool VisitFunctionDecl(FunctionDecl *decl) {
			auto excepts = getFunctionExcepts(decl);
			auto is_annotated = !hasAnnotation(decl, Annotation::none);
			bool return_expected = isExpectedType(ctx, decl->getReturnType());

			if(!excepts.empty() || is_annotated) {
				DBG_PRINT("Function: %s%s\n", functionName(decl).c_str(),
						  return_expected ? " Returns expected" : "");
			}

			if(!excepts.empty() && !is_annotated && !return_expected) {
				auto &elem = errors[decl];
				elem.insert(elem.end(), excepts.begin(), excepts.end());
			}
			return true;
		}

		std::unordered_map<const FunctionDecl *, std::vector<SourceLocation>> errors;
		ASTContext &ctx;
	};

	void HandleTranslationUnit(ASTContext &ctx) override {
		Visitor v(ctx);
		v.TraverseDecl(ctx.getTranslationUnitDecl());

		auto &diags = ci.getDiagnostics();
		auto err_id = diags.getCustomDiagID(DiagnosticsEngine::Error, "Missing EXCEPT attribute");
		auto note_id = diags.getCustomDiagID(DiagnosticsEngine::Note, "Caused by this statement");
		auto ref_id =
			diags.getCustomDiagID(DiagnosticsEngine::Note, "Referencing following function");

		for(auto [decl, locs] : v.errors) {
			diags.Report(decl->getSourceRange().getBegin(), err_id);
			for(uint n = 0; n < locs.size(); n += 2) {
				diags.Report(locs[n + 0], note_id);
				diags.Report(locs[n + 1], ref_id);
			}
		}
	}
};

class CheckErrorAttribsAction : public PluginASTAction {
  protected:
	std::unique_ptr<ASTConsumer> CreateASTConsumer(CompilerInstance &CI, llvm::StringRef) override {
		return llvm::make_unique<CheckErrorAttribsConsumer>(CI);
	}

	bool ParseArgs(const CompilerInstance &CI, const std::vector<std::string> &args) override {
		return true;
	}
};

}

static FrontendPluginRegistry::Add<CheckErrorAttribsAction>
	X("check-error-attribs", "Checks if error attribs are in place");
