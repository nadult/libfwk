// This plugin checks if functions which may call EXCEPT are also marked as EXCEPT or NOEXCEPT.

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

#define DBG_PRINT(...)
//#define DBG_PRINT(...) printf(__VA_ARGS__)

namespace {

// TODO: rename to something else ?
enum class ExceptAnnotation { none, except, inst_except, not_except };

const char *toString(ExceptAnnotation a) {
	static const char *strings[] = {"none", "except", "inst_except", "not_except"};
	return strings[int(a)];
}

template <class T, class... Args> constexpr bool anyOf(const T &value, const Args &...args) {
	return ((value == args) || ...);
}

template <class T, class... Args> constexpr bool allOf(const T &value, const Args &...args) {
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

	using ExAnno = ExceptAnnotation;

	struct Info {
		ExceptAnnotation decl_local, decl, body;
		const RecordDecl *parent = nullptr;
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
			if(decl == ExAnno::not_except)
				return false;
			if(decl == ExAnno::except)
				return true;
			if(returns_expected)
				return false;
			return body == ExAnno::except;
		}

		bool missingExceptAnnotation() const {
			return decl == ExAnno::none && body == ExAnno::except && !returns_expected;
		}

		bool invalidInstExcept(const FunctionDecl *fdecl) const {
			return decl == ExAnno::inst_except && !isTemplateSpec(fdecl);
		}
	};

	Info &access(const Decl *decl) {
		auto it = decls.find(decl);
		if(it != decls.end())
			return it->second;

		auto &ref = decls[decl];
		ref.decl = ref.decl_local = localExceptAnnotation(decl);

		ref.parent = annotableParent(decl);
		if(ref.parent && ref.decl_local == ExAnno::none) {
			auto &pinfo = access(ref.parent);
			ref.decl = pinfo.decl;
		}

		// TODO: what should we do in case of recursion?
		ref.body = ref.decl;
		if(isa<FunctionDecl>(decl)) {
			auto *spec = cast<FunctionDecl>(decl);
			ref.returns_expected = isExpectedType(spec->getReturnType());
			if(spec->hasBody()) {
				if(getExcepts(spec->getBody(), nullptr, spec))
					ref.body = ExAnno::except;
			}
		}
		if(isa<CXXConstructorDecl>(decl)) {
			auto *spec = cast<CXXConstructorDecl>(decl);
			for(auto *init : spec->inits())
				if(getExcepts(init->getInit(), nullptr, spec))
					ref.body = ExAnno::except;
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

	bool getExcepts(const Stmt *stmt, std::vector<SourceLocation> *out,
					const FunctionDecl *func_decl) {
		if(!stmt)
			return false;
		bool ret = false;

		using SC = Stmt::StmtClass;
		auto clazz = stmt->getStmtClass();

		if(clazz == SC::DeclRefExprClass) {
			const auto *spec = cast<DeclRefExpr>(stmt);
			if(access(spec->getDecl()).canRaise()) {
				if(out) {
					out->emplace_back(spec->getSourceRange().getBegin());
					out->emplace_back(spec->getDecl()->getSourceRange().getBegin());
				}
				return true;
			}
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
			ret |= getExcepts(child, out, func_decl);
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
		getExcepts(decl->getBody(), &out, decl);
		if(isa<CXXConstructorDecl>(decl)) {
			auto *spec = cast<CXXConstructorDecl>(decl);
			for(auto *init : spec->inits())
				getExcepts(init->getInit(), &out, spec);
		}

		return out;
	}

  private:
	static ExAnno localExceptAnnotation(const Decl *decl) {
		// TODO: check for multiple conflicting annotations
		for(auto *attr : decl->attrs()) {
			if(attr->getKind() == clang::attr::Kind::Annotate) {
				AnnotateAttr *aattr = cast<AnnotateAttr>(attr);
				if(aattr->getAnnotation() == "except")
					return ExAnno::except;
				if(aattr->getAnnotation() == "not_except")
					return ExAnno::not_except;
				if(aattr->getAnnotation() == "inst_except")
					return ExAnno::inst_except;
			}
		}
		return ExAnno::none;
	}

	static const RecordDecl *annotableParent(const Decl *decl) {
		using DK = Decl::Kind;
		auto dk = decl->getKind();
		if(dk == DK::CXXMethod || dk == DK::CXXConstructor || dk == DK::CXXDestructor)
			return cast<CXXMethodDecl>(decl)->getParent();
		return nullptr;
	}

	std::unordered_map<const Decl *, Info> decls;
	std::unordered_set<const TemplateDecl *> expecteds;
	ASTContext &ast_ctx;
};

class CheckFwkExceptionsConsumer : public ASTConsumer {
	CompilerInstance &ci;

  public:
	CheckFwkExceptionsConsumer(CompilerInstance &ci) : ci(ci) {}

	struct Visitor : public RecursiveASTVisitor<Visitor> {
		Visitor(AnnoCtx &ctx) : anno_ctx(ctx) {}

		bool shouldVisitTemplateInstantiations() const { return true; }

		bool VisitFunctionDecl(FunctionDecl *decl) {
			auto first_decl = firstDeclaration(decl);
			if(first_decl != decl)
				return true;

			auto &info = anno_ctx.access(decl);

			if(!allOf(ExceptAnnotation::none, info.body, info.decl, info.decl_local))
				DBG_PRINT("%s\n", info.describe(decl).c_str());

			if(info.missingExceptAnnotation() || info.invalidInstExcept(decl))
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
		auto err_id =
			diags.getCustomDiagID(DiagnosticsEngine::Error, "Missing EXCEPT attribute in: %q0");
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

			if(info.missingExceptAnnotation()) {
				auto locs = anno_ctx.getFunctionExcepts(decl);
				int skipped = (int)locs.size() / 2 > max_notes ? locs.size() / 2 - max_notes : 0;
				if(skipped > 1)
					locs.resize(max_notes * 2);

				diags.Report(loc, err_id) << decl;

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

class PrintTypeAliasesConsumer : public ASTConsumer {
  public:
	struct Visitor : public RecursiveASTVisitor<Visitor> {
		Visitor(ASTContext &ctx) : ctx(ctx) {}

		bool shouldVisitTemplateInstantiations() const { return true; }

		bool validDecl(Decl *decl) {
			if(!decl->isDefinedOutsideFunctionOrMethod())
				return false;
			auto lex_ctx = decl->getLexicalDeclContext();
			if((!lex_ctx->isNamespace() && !lex_ctx->isTranslationUnit()) ||
			   lex_ctx->isInlineNamespace())
				return false;
			return true;
		}

		bool VisitTypedefNameDecl(TypedefNameDecl *decl) {
			if(!validDecl(decl))
				return true;

			string base_type_name;
			decl->getTypeSourceInfo()->getType().getAsStringInternal(base_type_name,
																	 ctx.getLangOpts());

			printf("%s -> %s\n", decl->getNameAsString().c_str(), base_type_name.c_str());
			return true;
		}

		bool VisitUsingDecl(UsingDecl *decl) {
			if(!validDecl(decl))
				return true;

			// TODO: handle it
			return true;
		}

		const ASTContext &ctx;
	};

	void HandleTranslationUnit(ASTContext &ctx) override {
		Visitor v(ctx);
		v.TraverseDecl(ctx.getTranslationUnitDecl());
	}
};

class CheckFwkExceptionsAction : public PluginASTAction {
  protected:
	std::unique_ptr<ASTConsumer> CreateASTConsumer(CompilerInstance &CI, llvm::StringRef) override {
		return llvm::make_unique<CheckFwkExceptionsConsumer>(CI);
	}

	bool ParseArgs(const CompilerInstance &CI, const std::vector<std::string> &args) override {
		return true;
	}
};

class PrintTypeAliasesAction : public PluginASTAction {
  protected:
	std::unique_ptr<ASTConsumer> CreateASTConsumer(CompilerInstance &CI, llvm::StringRef) override {
		return llvm::make_unique<PrintTypeAliasesConsumer>();
	}
	bool ParseArgs(const CompilerInstance &CI, const std::vector<std::string> &args) override {
		return true;
	}
};

}

static FrontendPluginRegistry::Add<CheckFwkExceptionsAction>
	X1("fwk-check-exceptions", "Checks if exception attributes are in place");
static FrontendPluginRegistry::Add<PrintTypeAliasesAction> X2("fwk-print-type-aliases",
															  "Prints type aliases to file");
