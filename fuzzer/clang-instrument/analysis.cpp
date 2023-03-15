#include "clang/AST/AST.h"
#include "clang/AST/ASTConsumer.h"
#include "clang/AST/OperationKinds.h"
#include "clang/AST/RecursiveASTVisitor.h"
#include "clang/Analysis/AnalysisContext.h"
#include "clang/Analysis/CFG.h"
using namespace clang;

#include "analysis.h"
#include "common/logger.h"
#include "goal.h"

#include <memory>
using namespace std;

namespace instr {

bool Analysis::skipFunctionDecl(FunctionDecl *FD) const {
#ifdef SKIP_DEPENDENT_TYPE
  if (isa<CXXMethodDecl>(FD)) {
    CXXMethodDecl *method_decl = dyn_cast<CXXMethodDecl>(FD);
    CXXRecordDecl *class_decl = method_decl->getParent();
    if (class_decl->isDependentType()) {
      return true;
    }
  }
#endif
  return !FD->hasBody() || FD->isDefaulted() || FD->isDeleted() ||
         FD->isPure() || FD->isInlined() ||
         (!(FD->hasBody() && FD->isThisDeclarationADefinition()));
}

OperationClassifier::block_summaries_t
Analysis::process(FunctionDecl *FD, ASTContext &context) const {
  if (!SM) {
    LOG(INFO) << "No source manager available. Skipping.";
    return OperationClassifier::block_summaries_t();
  }

  if (skipFunctionDecl(FD)) {
    LOG(INFO) << "Skipped function";
    return OperationClassifier::block_summaries_t();
  }

  CFG::BuildOptions bo;
  bo.PruneTriviallyFalseEdges = true;
  bo.AddEHEdges = true;

  AnalysisDeclContextManager manager(/*useUnoptimizedCFG*/ false);
  auto func_context = make_shared<AnalysisDeclContext>(&manager, FD, bo);

  // Force the creation of the CFG in all circumstances
  func_context->getCFGBuildOptions().setAllAlwaysAdd();

  if (func_context->getCFG()) {
    OperationClassifier classifier(context);
    classifier.classify(*func_context);
    classifier.debugPrint();
    return classifier.get_summaries();
  }

  return OperationClassifier::block_summaries_t();
}
}
