//===- PrintFunctionNames.cpp ---------------------------------------------===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//
//
// Example clang plugin which simply prints the names of all the top-level decls
// in the input file.
//
//===----------------------------------------------------------------------===//

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

namespace {

string getAnnotation(const Decl *decl) {
	for(auto *attr : decl->attrs()) {
		if(attr->getKind() == clang::attr::Kind::Annotate) {
			AnnotateAttr *aattr = cast<AnnotateAttr>(attr);
			return string(aattr->getAnnotation());
		}
	}
	return {};
}

bool hasExceptAttrib(const Decl *decl) {
	if(decl->getKind() == Decl::Kind::CXXMethod) {
		auto *spec = cast<CXXMethodDecl>(decl);
		if(getAnnotation(spec->getParent()) == "except")
			return true;
	}
	return getAnnotation(decl) == "except";
}

bool hasExceptAttrib(const Expr *expr) {
	// TODO: there is no need for recursion here, we're doing it already for Stmts
	if(expr->getStmtClass() == Stmt::StmtClass::DeclRefExprClass) {
		const auto *spec = cast<DeclRefExpr>(expr);
		return hasExceptAttrib(spec->getDecl());
	} else if(expr->getStmtClass() == Stmt::StmtClass::ImplicitCastExprClass) {
		const auto *spec = cast<ImplicitCastExpr>(expr);
		return hasExceptAttrib(spec->getSubExpr());
	} else if(expr->getStmtClass() == Stmt::StmtClass::UnresolvedLookupExprClass) {
		const auto *spec = cast<UnresolvedLookupExpr>(expr);
		return false;
	}
	return false;
}

bool hasExceptAttrib(const Stmt *stmt) {
	if(!stmt)
		return false;

	if(stmt->getStmtClass() == Stmt::StmtClass::CallExprClass) {
		const CallExpr *ce = cast<CallExpr>(stmt);
		const Expr *callee = ce->getCallee();
		if(hasExceptAttrib(callee))
			return true;
	}
	if(stmt->getStmtClass() == Stmt::StmtClass::CXXConstructExprClass) {
		const auto *expr = cast<CXXConstructExpr>(stmt);
		CXXConstructorDecl *constr = expr->getConstructor();
		if(hasExceptAttrib(constr))
			return true;
	}

	for(auto child : stmt->children())
		if(hasExceptAttrib(child))
			return true;
	return false;
}

bool hasMissingAttrib(const FunctionDecl *decl) {
	if(!decl->hasBody())
		return false;

	using TK = FunctionDecl::TemplatedKind;
	auto tk = decl->getTemplatedKind();
	if(tk == TK::TK_FunctionTemplate) // TODO: how to handle this?
		return false;
	return !hasExceptAttrib(decl) && hasExceptAttrib(decl->getBody());
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
	std::set<std::string> ParsedTemplates;

  public:
	CheckErrorAttribsConsumer(CompilerInstance &ci, std::set<std::string> ParsedTemplates)
		: ci(ci), ParsedTemplates(ParsedTemplates) {}

	void reportError(const FunctionDecl *decl) {
		auto &diags = ci.getDiagnostics();
		auto diag_id =
			diags.getCustomDiagID(DiagnosticsEngine::Error,
								  "Missing EXCEPT attribute (function may generate exceptions)");
		diags.Report(decl->getSourceRange().getBegin(), diag_id);
	}

	void HandleTranslationUnit(ASTContext &context) override {
		struct Visitor : public RecursiveASTVisitor<Visitor> {
			Visitor() {}

			bool shouldVisitTemplateInstantiations() const { return true; }

			bool VisitFunctionDecl(FunctionDecl *decl) {
				bool missing = hasMissingAttrib(decl);
				//if(hasExceptAttrib(decl) || missing) {
				//	printf("Function: %s%s\n", functionName(decl).c_str(),
				//		   missing ? " Missing attribute" : "");
				//}
				if(missing)
					error_decls.emplace_back(firstDeclaration(decl));
				return true;
			}

			std::vector<const FunctionDecl *> error_decls;
		} v;
		v.TraverseDecl(context.getTranslationUnitDecl());

		auto &edecls = v.error_decls;
		makeUnique(edecls);
		for(auto *decl : edecls)
			reportError(decl);

		clang::Sema &sema = ci.getSema();
	}
};

class CheckErrorAttribsAction : public PluginASTAction {
	std::set<std::string> ParsedTemplates;

  protected:
	std::unique_ptr<ASTConsumer> CreateASTConsumer(CompilerInstance &CI, llvm::StringRef) override {
		return llvm::make_unique<CheckErrorAttribsConsumer>(CI, ParsedTemplates);
	}

	bool ParseArgs(const CompilerInstance &CI, const std::vector<std::string> &args) override {
		for(unsigned i = 0, e = args.size(); i != e; ++i) {
			llvm::errs() << "PrintFunctionNames arg = " << args[i] << "\n";

			// Example error handling.
			DiagnosticsEngine &D = CI.getDiagnostics();
			if(args[i] == "-an-error") {
				unsigned DiagID =
					D.getCustomDiagID(DiagnosticsEngine::Error, "invalid argument '%0'");
				D.Report(DiagID) << args[i];
				return false;
			} else if(args[i] == "-parse-template") {
				if(i + 1 >= e) {
					D.Report(D.getCustomDiagID(DiagnosticsEngine::Error,
											   "missing -parse-template argument"));
					return false;
				}
				++i;
				ParsedTemplates.insert(args[i]);
			}
		}
		if(!args.empty() && args[0] == "help")
			PrintHelp(llvm::errs());

		return true;
	}
	void PrintHelp(llvm::raw_ostream &ros) {
		ros << "Help for PrintFunctionNames plugin goes here\n";
	}
};

}

static FrontendPluginRegistry::Add<CheckErrorAttribsAction>
	X("check-error-attribs", "Checks if error attribs are in place");
