#ifndef BLOCK_CONSUMER_H
#define BLOCK_CONSUMER_H

#include "clang/AST/AST.h"
#include "clang/AST/ASTConsumer.h"
#include "clang/Frontend/CompilerInstance.h"
#include "clang/Rewrite/Core/Rewriter.h"
using namespace clang;

#include <common/store.h>
#include <iostream>

#include "analysis.h"
#include "config.h"

namespace instr {

class InstrASTConsumer : public ASTConsumer {
  StringRef &in_file;
  raw_ostream *output;
  FileID mainFileID;

  SourceManager *SM = nullptr;

  Rewriter rewrite;
  Config &config;
  std::unique_ptr<Store> store;
  std::unique_ptr<Analysis> analysis;

  // Current source element_id
  element_id source_id = ERROR_ID;

public:
  InstrASTConsumer() = delete;
  InstrASTConsumer(const InstrASTConsumer &) = delete;

  // Move constructor
  InstrASTConsumer(InstrASTConsumer &&c)
      : in_file(c.in_file), output(c.output), config(c.config),
        store(std::move(c.store)), source_id(c.source_id) {}

  // Move assignment operator
  InstrASTConsumer &operator=(InstrASTConsumer &&c) {
    if (this != &c) {
      in_file = c.in_file;
      output = c.output;
      mainFileID = c.mainFileID;
      SM = c.SM;
      config = c.config;
      store = std::move(c.store);
      source_id = c.source_id;
    }
    return *this;
  }

  InstrASTConsumer(llvm::StringRef &in_file, raw_ostream *output,
                   Config &config)
      : in_file(in_file), output(output), config(config) {
    createDependencies();
  }

  void HandleTranslationUnit(ASTContext &context);

private:
  void createDependencies();

  void registerFile();

  void writeExterns();
};
}

#endif
