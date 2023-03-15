#ifndef ANALYSIS_H
#define ANALYSIS_H

#include "clang/AST/AST.h"
#include "clang/AST/ASTConsumer.h"

#include "goal.h"

#include <memory>
#include <vector>

// Ignore templated methods
#define SKIP_DEPENDENT_TYPE 1

namespace instr {

class Analysis {
  clang::SourceManager *SM = nullptr;

public:
  Analysis(clang::SourceManager *SM = nullptr) : SM(SM) {}

  Analysis(const Analysis &a) = delete;
  Analysis &operator=(const Analysis &a) = delete;
  ~Analysis() = default;

  void setSourceMgr(clang::SourceManager *SM) { this->SM = SM; }

  bool skipFunctionDecl(clang::FunctionDecl *FD) const;
  OperationClassifier::block_summaries_t
  process(clang::FunctionDecl *FD, clang::ASTContext &context) const;
};
}

#endif
