#include "clang/Frontend/FrontendPluginRegistry.h"
#include "clang/AST/AST.h"
#include "clang/AST/ASTConsumer.h"
#include "clang/AST/RecursiveASTVisitor.h"
#include "clang/Frontend/CompilerInstance.h"
#include "clang/Sema/Sema.h"
#include "llvm/Support/raw_ostream.h"
using namespace clang;

#include <iostream>
#include <memory>
#include <set>
#include <string>

#include "common/logger.h"
INITIALIZE_EASYLOGGINGPP; // Just once at the root

#include "common/store.h"
#include "instr-ast-consumer.h"
#include "lib-clang-instrument.h"

namespace instr {

// Create the instance of the `BlockConsumer` which handles the AST for
// transformations
std::unique_ptr<ASTConsumer>
ClangInstrumenter::CreateASTConsumer(CompilerInstance &CI,
                                     llvm::StringRef in_file) {
  setupLogger();

  LOG(INFO) << "Entering file: " << in_file.str();

  if ((output = CI.createDefaultOutputFile(false, in_file, "cpp"))) {
    return llvm::make_unique<InstrASTConsumer>(in_file, output, config);
  }
  return nullptr;
}

bool ClangInstrumenter::ParseArgs(const CompilerInstance &CI,
                                  const std::vector<std::string> &args) {
  // TBD
  return true;
}

void ClangInstrumenter::PrintHelp(llvm::raw_ostream &ros) {
  // TBD
}
}

static FrontendPluginRegistry::Add<instr::ClangInstrumenter>
    X("instrument", "Enable instrumentation");
