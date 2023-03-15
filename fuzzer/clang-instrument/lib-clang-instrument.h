#ifndef LIB_CLANG_INSTRUMENT_H
#define LIB_CLANG_INSTRUMENT_H

#include "clang/AST/AST.h"
#include "clang/AST/ASTConsumer.h"
#include "clang/AST/RecursiveASTVisitor.h"
#include "clang/Frontend/CompilerInstance.h"
#include "clang/Frontend/FrontendPluginRegistry.h"
#include "clang/Sema/Sema.h"
#include "llvm/Support/raw_ostream.h"

#include <iostream>
#include <memory>

#include "config.h"
#include <common/store.h>

using namespace clang;

namespace instr {

class ClangInstrumenter : public PluginASTAction {
private:
  Config config;
  raw_ostream *output;

protected:
  std::unique_ptr<ASTConsumer> CreateASTConsumer(CompilerInstance &,
                                                 llvm::StringRef);

  element_id registerFile(Store &store, const std::string &path);

  bool ParseArgs(const CompilerInstance &, const std::vector<std::string> &);

  void PrintHelp(llvm::raw_ostream &);
};
}

#endif
