#include "FunctionDumper.h"
#include <boost/interprocess/sync/interprocess_semaphore.hpp>
#include <iostream>
#include <fstream>
#include <iterator>
#include <string>
#include <algorithm> 

using namespace clang;
using namespace ento;
using namespace llvm;

namespace clangcms {
[[edm::thread_safe]] static boost::interprocess::interprocess_semaphore file_mutex(1);

class FDumper : public clang::StmtVisitor<FDumper> {
  clang::ento::BugReporter &BR;
  clang::AnalysisDeclContext *AC;

public:
  FDumper(clang::ento::BugReporter &br, clang::AnalysisDeclContext *ac )
    : BR(br),
      AC(ac) {}

  const clang::Stmt * ParentStmt(const Stmt *S) {
  	const Stmt * P = AC->getParentMap().getParentIgnoreParens(S);
	if (!P) return 0;
	return P;
  }


  void VisitChildren(clang::Stmt *S );
  void VisitStmt( clang::Stmt *S) { VisitChildren(S); }
  void VisitCallExpr( CallExpr *CE ); 
 
};

void FDumper::VisitChildren( clang::Stmt *S) {
  for (clang::Stmt::child_iterator I = S->child_begin(), E = S->child_end(); I!=E; ++I)
    if (clang::Stmt *child = *I) {
      Visit(child);
    }
}


void FDumper::VisitCallExpr( CallExpr *CE ) {
	LangOptions LangOpts;
	LangOpts.CPlusPlus = true;
	PrintingPolicy Policy(LangOpts);
	const Decl * D = AC->getDecl();
	std::string mdname =""; 
	if (const NamedDecl * ND = llvm::dyn_cast<NamedDecl>(D)) mdname = support::getQualifiedName(*ND);
	FunctionDecl * FD = CE->getDirectCallee();
	if (!FD) return;
 	const char *sfile=BR.getSourceManager().getPresumedLoc(CE->getExprLoc()).getFilename();
 	if (!support::isCmsLocalFile(sfile)) return;
	std::string sname(sfile);
	if ( sname.find("/test/") != std::string::npos) return;
 	std::string mname = support::getQualifiedName(*FD);
	const char * pPath = std::getenv("LOCALRT");
	std::string tname = ""; 
	if ( pPath != NULL ) tname += std::string(pPath);
	tname+="/tmp/function-dumper.txt.unsorted";
	std::string ostring = "function '"+ mdname +  "' " + "calls function '" + mname + "'\n"; 
	file_mutex.wait();
	std::fstream file(tname.c_str(),std::ios::in|std::ios::out|std::ios::app);
	std::string filecontents((std::istreambuf_iterator<char>(file)),std::istreambuf_iterator<char>() );
	if ( filecontents.find(ostring)  == std::string::npos ) {
		file<<ostring;
		file.close();
		file_mutex.post();
	} else {
		file.close();
		file_mutex.post();
	}
}

void FunctionDumper::checkASTDecl(const CXXMethodDecl *MD, AnalysisManager& mgr,
                    BugReporter &BR) const {

 	const char *sfile=BR.getSourceManager().getPresumedLoc(MD->getLocation()).getFilename();
   	if (!support::isCmsLocalFile(sfile)) return;
	std::string sname(sfile);
	if ( sname.find("/test/") != std::string::npos) return;
	if (!MD->doesThisDeclarationHaveABody()) return;
	FDumper walker(BR, mgr.getAnalysisDeclContext(MD));
	walker.Visit(MD->getBody());
        std::string mname = support::getQualifiedName(*MD);
	const char * pPath = std::getenv("LOCALRT");
	std::string tname=""; 
	if ( pPath != NULL ) tname += std::string(pPath);
	tname += "/tmp/function-dumper.txt.unsorted";
        for (auto I = MD->begin_overridden_methods(), E = MD->end_overridden_methods(); I!=E; ++I) {
		std::string oname = support::getQualifiedName(*(*I));
		std::string ostring = "function '" +  mname + "' " + "overrides function '" + oname + "'\n";
		file_mutex.wait();
		std::fstream file(tname.c_str(),std::ios::in|std::ios::out|std::ios::app);
		std::string filecontents((std::istreambuf_iterator<char>(file)),std::istreambuf_iterator<char>() );
		if ( filecontents.find(ostring) == std::string::npos) {
			file<<ostring;
			file.close();
			file_mutex.post();
		} else {
			file.close();
			file_mutex.post();
		}
	}
       	return;
} 

void FunctionDumper::checkASTDecl(const FunctionTemplateDecl *TD, AnalysisManager& mgr,
                    BugReporter &BR) const {

 	const char *sfile=BR.getSourceManager().getPresumedLoc(TD->getLocation ()).getFilename();
   	if (!support::isCmsLocalFile(sfile)) return;
	std::string sname(sfile);
	if ( sname.find("/test/") != std::string::npos) return;
  
	for (FunctionTemplateDecl::spec_iterator I = const_cast<clang::FunctionTemplateDecl *>(TD)->spec_begin(), 
			E = const_cast<clang::FunctionTemplateDecl *>(TD)->spec_end(); I != E; ++I) 
		{
			if (I->doesThisDeclarationHaveABody()) {
				FDumper walker(BR, mgr.getAnalysisDeclContext(*I));
				walker.Visit(I->getBody());
				}
		}	
	return;
}



}
