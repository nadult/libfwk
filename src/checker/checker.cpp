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

#include <unordered_map>
#include <unordered_set>

using namespace clang;
using std::string;

//#define DBG_PRINT(...) printf(__VA_ARGS__)
#define DBG_PRINT(...)

namespace {

// TODO: rename to something else ?
enum class Annotation { none, except, inst_except, not_except };

const char *toString(Annotation a) {
	static const char *strings[] = {"none", "except", "inst_except", "not_except"};
	return strings[int(a)];
}

template <class T, class... Args> constexpr bool anyOf(const T &value, const Args &... args) {
	return ((value == args) || ...);
}

template <class T, class... Args> constexpr bool allOf(const T &value, const Args &... args) {
	return ((value == args) && ...);
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

bool isTemplateSpec(const FunctionDecl *decl) {
	return decl->getTemplatedKind() != FunctionDecl::TemplatedKind::TK_NonTemplate;
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

struct AnnoCtx {
	AnnoCtx(ASTContext &ast_ctx) : ast_ctx(ast_ctx) {}

	struct Info {
		Annotation decl_local, decl, body;
		const CXXRecordDecl *parent = nullptr;
		bool returns_expected = false;

		string describe(const FunctionDecl *decl) {
			string out = "Function " + functionName(decl) + ": ";
			out += string("decl_local:") + toString(decl_local) + " ";
			out += string("decl:") + toString(this->decl) + " ";
			out += string("body:") + toString(body) + " ";
			if(returns_expected)
				out += "ret_expected";
			return out;
		}

		bool canRaise() const {
			if(decl == Annotation::not_except)
				return false;
			if(decl == Annotation::except)
				return true;
			if(returns_expected)
				return false;
			return body == Annotation::except;
		}

		bool missingAnnotation() const {
			return decl == Annotation::none && body == Annotation::except && !returns_expected;
		}

		bool invalidInstExcept(const FunctionDecl *fdecl) const {
			return decl == Annotation::inst_except && !isTemplateSpec(fdecl);
		}
	};

	Info &access(const Decl *decl) {
		auto it = decls.find(decl);
		if(it != decls.end())
			return it->second;

		auto &ref = decls[decl];
		ref.decl = ref.decl_local = localAnnotation(decl);
		ref.parent = annotableParent(decl);
		if(ref.parent && ref.decl_local == Annotation::none) {
			auto &pinfo = access(ref.parent);
			ref.decl = pinfo.decl;
		}

		// TODO: what should we do in case of recursion?
		ref.body = ref.decl;
		if(isa<FunctionDecl>(decl)) {
			auto *spec = cast<FunctionDecl>(decl);
			ref.returns_expected = isExpectedType(spec->getReturnType());
			if(spec->hasBody()) {
				if(getExcepts(spec->getBody(), nullptr))
					ref.body = Annotation::except;
			}
		}

		return ref;
	}

	bool isExpectedType(QualType qtype) {
		qtype = qtype.getSingleStepDesugaredType(ast_ctx);
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

	bool getExcepts(const Expr *expr, std::vector<SourceLocation> *out) {
		// TODO: there is no need for recursion here, we're doing it already for Stmts
		if(expr->getStmtClass() == Stmt::StmtClass::DeclRefExprClass) {
			const auto *spec = cast<DeclRefExpr>(expr);
			if(access(spec->getDecl()).canRaise()) {
				if(out) {
					out->emplace_back(spec->getSourceRange().getBegin());
					out->emplace_back(spec->getDecl()->getSourceRange().getBegin());
				}
				return true;
			}
		} else if(expr->getStmtClass() == Stmt::StmtClass::ImplicitCastExprClass) {
			const auto *spec = cast<ImplicitCastExpr>(expr);
			return getExcepts(spec->getSubExpr(), out);
		}
		return false;
	}

	bool getExcepts(const Stmt *stmt, std::vector<SourceLocation> *out) {
		if(!stmt)
			return false;
		bool ret = false;

		using SC = Stmt::StmtClass;
		auto clazz = stmt->getStmtClass();
		if(clazz == SC::CallExprClass || clazz == SC::CXXMemberCallExprClass) {
			const auto *ce = cast<CallExpr>(stmt);
			ret |= getExcepts(ce->getCallee(), out);
		} else if(clazz == SC::MemberExprClass) {
			const auto *ce = cast<MemberExpr>(stmt);
			if(access(ce->getMemberDecl()).canRaise()) {
				if(out) {
					out->emplace_back(ce->getSourceRange().getBegin());
					out->emplace_back(ce->getMemberDecl()->getSourceRange().getBegin());
				}
				ret = true;
			}
		} else if(clazz == SC::CXXConstructExprClass) {
			const auto *expr = cast<CXXConstructExpr>(stmt);
			CXXConstructorDecl *constr = expr->getConstructor();
			if(access(constr).canRaise()) {
				if(out) {
					out->emplace_back(expr->getSourceRange().getBegin());
					out->emplace_back(constr->getSourceRange().getBegin());
				}
				ret = true;
			}
		}

		for(auto child : stmt->children())
			ret |= getExcepts(child, out);
		return ret;
	}

	std::vector<SourceLocation> getFunctionExcepts(const FunctionDecl *decl) {
		if(!decl->hasBody())
			return {};
		using TK = FunctionDecl::TemplatedKind;
		auto tk = decl->getTemplatedKind();
		if(tk == TK::TK_FunctionTemplate) // TODO: how to handle this?
			return {};

		std::vector<SourceLocation> out;
		getExcepts(decl->getBody(), &out);
		return out;
	}

  private:
	static Annotation localAnnotation(const Decl *decl) {
		// TODO: check for multiple conflicting annotations
		for(auto *attr : decl->attrs()) {
			if(attr->getKind() == clang::attr::Kind::Annotate) {
				AnnotateAttr *aattr = cast<AnnotateAttr>(attr);
				if(aattr->getAnnotation() == "except")
					return Annotation::except;
				if(aattr->getAnnotation() == "not_except")
					return Annotation::not_except;
				if(aattr->getAnnotation() == "inst_except")
					return Annotation::inst_except;
			}
		}
		return Annotation::none;
	}

	static const CXXRecordDecl *annotableParent(const Decl *decl) {
		using DK = Decl::Kind;
		auto dk = decl->getKind();

		if(dk == DK::CXXMethod || dk == DK::CXXConstructor || dk == DK::CXXDestructor) {
			auto *spec = cast<CXXMethodDecl>(decl);
			return spec->getParent();
		}

		return nullptr;
	}

	std::unordered_map<const Decl *, Info> decls;
	std::unordered_set<const TemplateDecl *> expecteds;
	ASTContext &ast_ctx;
};

class CheckErrorAttribsConsumer : public ASTConsumer {
	CompilerInstance &ci;

  public:
	CheckErrorAttribsConsumer(CompilerInstance &ci) : ci(ci) {}

	void reportError(const FunctionDecl *decl) {}

	struct Visitor : public RecursiveASTVisitor<Visitor> {
		Visitor(AnnoCtx &ctx) : anno_ctx(ctx) {}

		bool shouldVisitTemplateInstantiations() const { return true; }

		bool VisitFunctionDecl(FunctionDecl *decl) {
			auto first_decl = firstDeclaration(decl);
			if(first_decl != decl)
				return true;

			auto &info = anno_ctx.access(decl);

			if(!allOf(Annotation::none, info.body, info.decl, info.decl_local))
				DBG_PRINT("%s\n", info.describe(decl).c_str());

			if(info.missingAnnotation() || info.invalidInstExcept(decl))
				error_decls.emplace_back(decl);
			return true;
		}

		std::vector<const FunctionDecl *> error_decls;
		AnnoCtx &anno_ctx;
	};

	void HandleTranslationUnit(ASTContext &ctx) override {
		AnnoCtx anno_ctx(ctx);
		Visitor v(anno_ctx);
		v.TraverseDecl(ctx.getTranslationUnitDecl());

		if(v.error_decls.empty())
			return;
		DBG_PRINT("ERRORS: %d\n", (int)v.error_decls.size());

		auto &diags = ci.getDiagnostics();
		auto err_id = diags.getCustomDiagID(DiagnosticsEngine::Error, "Missing EXCEPT attribute");
		auto note_id = diags.getCustomDiagID(DiagnosticsEngine::Note, "Caused by this statement:");
		auto ref_id = diags.getCustomDiagID(
			DiagnosticsEngine::Note, "Referencing following function which may raise exceptions:");
		auto templ_id = diags.getCustomDiagID(DiagnosticsEngine::Error,
											  "INST_EXCEPT can only be placed on templates");
		auto more_notes_id =
			diags.getCustomDiagID(DiagnosticsEngine::Note, "Skipped %0 more instances...");
		int max_notes = 3;

		for(auto *decl : v.error_decls) {
			auto &info = anno_ctx.access(decl);

			auto loc = decl->getSourceRange().getBegin();
			if(info.invalidInstExcept(decl))
				diags.Report(loc, templ_id);

			if(info.missingAnnotation()) {
				auto locs = anno_ctx.getFunctionExcepts(decl);
				int skipped = (int)locs.size() / 2 > max_notes ? locs.size() / 2 - max_notes : 0;
				if(skipped > 1)
					locs.resize(max_notes * 2);

				diags.Report(loc, err_id);
				for(uint n = 0; n < locs.size(); n += 2) {
					diags.Report(locs[n + 0], note_id);
					diags.Report(locs[n + 1], ref_id);
				}

				if(skipped > 1)
					diags.Report({}, more_notes_id) << skipped;
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
