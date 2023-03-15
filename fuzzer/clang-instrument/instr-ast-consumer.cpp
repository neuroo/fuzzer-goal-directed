#include "clang/AST/AST.h"
#include "clang/AST/ASTConsumer.h"
#include "clang/AST/ParentMap.h"
#include "clang/AST/RecursiveASTVisitor.h"
#include "clang/Analysis/CFG.h"
#include "clang/Frontend/CompilerInstance.h"
#include "clang/Rewrite/Core/Rewriter.h"
#include "clang/Sema/Sema.h"
using namespace clang;

#include "analysis.h"
#include "common/logger.h"
#include "config.h"
#include "instr-ast-consumer.h"
#include "instrument.h"

#include <iostream>
#include <sstream>

namespace instr {

// XXX this will come from the config, parsed from the command line args
static const std::string serFileName = "models.xxx";

// XXX wrap into extern "C" {}
void InstrASTConsumer::writeExterns() {
  *output << "extern void __coverage_reach_block(const unsigned long, const "
             "unsigned int, const unsigned int);"
          << '\n' << "extern void __coverage_skip_block(const unsigned long, const "
                     "unsigned int, const unsigned int);"
          << '\n' << "extern void __coverage_enter_func(const unsigned long);"
          << '\n' << "extern void __coverage_exit_func(const unsigned long);" << '\n'
          << "extern void __coverage_kill(const unsigned long);" << '\n';
}

void InstrASTConsumer::createDependencies() {
  store = llvm::make_unique<Store>(serFileName);
  analysis = llvm::make_unique<Analysis>();
  registerFile();
}

void InstrASTConsumer::registerFile() {
  const std::string &path = in_file.str();

  // First, register to the list of sources and get a unique ID for the current
  // source file
  source_id = store->store().addSource(path);

  // Then create an element for the source
  auto e = Store::create(Element::E_SOURCE, source_id);

  // XXX since static_pointer_cast makes a copy, we only use it to store
  // information
  //     related to the actual pointee element SourceElement
  auto s = std::static_pointer_cast<SourceElement>(e);
  s->path = path;

  store->store().add(source_id, e);

  // XXX dump the current store
  LOG(INFO) << "Curent store" << std::endl << store->toString();
}

void InstrASTConsumer::HandleTranslationUnit(ASTContext &context) {
  rewrite.setSourceMgr(context.getSourceManager(), context.getLangOpts());
  SM = &context.getSourceManager();
  analysis->setSourceMgr(SM);

  mainFileID = SM->getMainFileID();

  LOG(INFO) << "processing context for file: "
            << SM->getFileEntryForID(mainFileID)->getName();

  InstrumentationVisitor instrumenter(context, SM, mainFileID, &rewrite, store,
                                      source_id, analysis);

  instrumenter.TraverseDecl(context.getTranslationUnitDecl());

  if (const RewriteBuffer *rewriteBuf =
          rewrite.getRewriteBufferFor(mainFileID)) {
    writeExterns();
    *output << std::string(rewriteBuf->begin(), rewriteBuf->end());
  } else {
    StringRef buffer = SM->getBufferData(mainFileID).data();
    *output << std::string(buffer);
  }
}
}
