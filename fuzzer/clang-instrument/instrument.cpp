#include "clang/AST/AST.h"
#include "clang/AST/ASTConsumer.h"
#include "clang/AST/ParentMap.h"
#include "clang/AST/RecursiveASTVisitor.h"
#include "clang/Analysis/CFG.h"
#include "clang/Basic/FileManager.h"
#include "clang/Basic/SourceManager.h"
#include "clang/Frontend/CompilerInstance.h"
#include "clang/Lex/Lexer.h"
#include "clang/Lex/Preprocessor.h"
#include "clang/Rewrite/Core/Rewriter.h"
#include "clang/Sema/Sema.h"
#include "llvm/ADT/STLExtras.h"
using namespace clang;

#include "common/logger.h"
#include "instrument.h"

#include <iostream>
#include <map>
#include <sstream>
#include <string>
#include <vector>

// #define DUMP_CFG_OUTPUT
#define ONLY_INSTR_COMPILED_FILES

namespace instr {

bool InstrumentationVisitor::shouldInstrumentFunctionDecl(
    const FunctionDecl *FD) {
  return FD->hasBody();
}

bool InstrumentationVisitor::isInMainFile(const FileID &funcFileID) {
  return mainFileID == funcFileID;
}

SourceLocation InstrumentationVisitor::expand_loc(const SourceLocation &loc) {
  return rewrite->getSourceMgr().getFileLoc(loc);
}

bool InstrumentationVisitor::HandleStmt(Stmt *stmt) {
  if (!stmt)
    return false;

  FullSourceLoc fullLocation = context.getFullLoc(stmt->getLocStart());

#ifdef ONLY_INSTR_COMPILED_FILES
  FileID currentFileID = fullLocation.getFileID();
  if (!isInMainFile(currentFileID)) {
    return true;
  }
#endif

  if (isa<ForStmt>(stmt)) {
    ForStmt *forStmt = cast<ForStmt>(stmt);
    Stmt *body = forStmt->getBody();
    EnsureBracesControlStmt(body);
    InsertBlockDirective(body, findBlockIdForStmt(body));
  } else if (isa<CXXForRangeStmt>(stmt)) {
    CXXForRangeStmt *forStmt = cast<CXXForRangeStmt>(stmt);
    Stmt *body = forStmt->getBody();
    EnsureBracesControlStmt(body);
    InsertBlockDirective(body, findBlockIdForStmt(body));
  } else if (isa<WhileStmt>(stmt)) {
    WhileStmt *whileStmt = cast<WhileStmt>(stmt);
    Stmt *body = whileStmt->getBody();
    EnsureBracesControlStmt(body);
    InsertBlockDirective(body, findBlockIdForStmt(body));
  } else if (isa<DoStmt>(stmt)) {
    DoStmt *doStmt = cast<DoStmt>(stmt);
    Stmt *body = doStmt->getBody();
    EnsureBracesControlStmt(body);
    InsertBlockDirective(body, findBlockIdForStmt(body));
  } else if (isa<IfStmt>(stmt)) {
    // get both branches
    IfStmt *ifStmt = cast<IfStmt>(stmt);
    Stmt *thenStmt = ifStmt->getThen();
    EnsureBracesControlStmt(thenStmt);
    InsertBlockDirective(thenStmt, findBlockIdForStmt(thenStmt));

    Stmt *elseStmt = ifStmt->getElse();
    if (elseStmt) {
      EnsureBracesControlStmt(elseStmt);
      InsertBlockDirective(elseStmt, findBlockIdForStmt(elseStmt));
    } else {
      // add an else stmt with a __coverage_skip_block(...)
      InsertSkippedBlockDirective(ifStmt);
    }
  } else if (isa<SwitchStmt>(stmt)) {
    SwitchStmt *switchStmt = cast<SwitchStmt>(stmt);
    Stmt *body = switchStmt->getBody();
    EnsureBracesControlStmt(body);
  } else if (isa<CaseStmt>(stmt)) {
    CaseStmt *caseStmt = cast<CaseStmt>(stmt);
    Stmt *body = caseStmt->getSubStmt();
    InsertBlockDirective(body, findBlockIdForStmt(body));
  } else if (isa<DefaultStmt>(stmt)) {
    DefaultStmt *defaultStmt = cast<DefaultStmt>(stmt);
    Stmt *body = defaultStmt->getSubStmt();
    InsertBlockDirective(body, findBlockIdForStmt(body));
  } else if (isa<CXXTryStmt>(stmt)) {
    CXXTryStmt *tryStmt = cast<CXXTryStmt>(stmt);
    Stmt *body = tryStmt->getTryBlock();
    InsertBlockDirective(body, findBlockIdForStmt(body));
  } else if (isa<CXXCatchStmt>(stmt)) {
    CXXCatchStmt *catchStmt = cast<CXXCatchStmt>(stmt);
    Stmt *body = catchStmt->getHandlerBlock();
    InsertBlockDirective(body, findBlockIdForStmt(body));
  } else if (isa<LabelStmt>(stmt)) {
    LabelStmt *labelStmt = cast<LabelStmt>(stmt);
    Stmt *body = labelStmt->getSubStmt();
    InsertBlockDirective(body, findBlockIdForStmt(body));
  } else if (isa<ReturnStmt>(stmt)) {
    ReturnStmt *returnStmt = cast<ReturnStmt>(stmt);
    HandleReturnStmt(returnStmt);
  } else if (isa<LambdaExpr>(stmt)) {
    LambdaExpr *lambdaExpr = cast<LambdaExpr>(stmt);
    HandleLambdaExpr(lambdaExpr);
    return false;
  }

  // That's our own recursive visitor...
  for (auto &child : stmt->children()) {
    if (child) {
      HandleStmt(child);
    }
  }

  return true;
}

void InstrumentationVisitor::EnsureBracesControlStmt(Stmt *stmt) {
  if (isa<CompoundStmt>(stmt))
    return;

  SourceLocation start = stmt->getLocStart(), end = stmt->getLocEnd();

  rewrite->InsertTextBefore(expand_loc(start), "{");
  // MeasureTokenLength gets us past the last token, and adding 1 gets
  // us past the ';'
  int offset = Lexer::MeasureTokenLength(end, rewrite->getSourceMgr(),
                                         rewrite->getLangOpts()) +
               1;
  SourceLocation adapted_end = end.getLocWithOffset(offset);
  rewrite->InsertTextBefore(expand_loc(adapted_end), "}");
}

void InstrumentationVisitor::InsertSkippedBlockDirective(IfStmt *ifStmt) {
  return;

  if (ifStmt->getElse())
    return;

  SourceLocation end = ifStmt->getLocEnd();
  int offset = Lexer::MeasureTokenLength(end, rewrite->getSourceMgr(),
                                         rewrite->getLangOpts()) +
               1;
  SourceLocation adapted_end = end.getLocWithOffset(offset);

  unsigned int block_id = findBlockIdForStmt(ifStmt->getThen());
  if (block_id < 1) {
    block_id = 0;
  }

  std::ostringstream oss;
  oss << "else { __coverage_skip_block(" << cfg_stack.id() << ", __coverage_pred_block, "
      << block_id << "); __coverage_pred_block = " << block_id << "; } ";

  rewrite->InsertTextBefore(expand_loc(adapted_end), oss.str());
}

void InstrumentationVisitor::InsertBlockDirective(const Stmt *S,
                                                  const unsigned int block_id) {
  SourceLocation start = S->getLocStart();
  if (isa<CompoundStmt>(S)) {
    LOG(INFO) << "Compound, old location = " << start.printToString(*SM);
    start = start.getLocWithOffset(1);
  }

  std::ostringstream oss;
  oss << " __coverage_reach_block(" << cfg_stack.id() << ", __coverage_pred_block, "
      << block_id << "); __coverage_pred_block = " << block_id << "; ";

  rewrite->InsertText(expand_loc(start), oss.str());
}

unsigned int InstrumentationVisitor::findBlockIdForStmt(Stmt *s) {
  if (!s || !cfg_stack.map()) {
    return 0;
  }

  Stmt *stmt = s;
  if (InstrumentationUtils::isControlFlowStmt(stmt)) {
    auto first_child = stmt->child_begin();
    stmt = first_child == stmt->child_end() ? s : *first_child;
  }

  const CFGBlock *block = cfg_stack.map()->getBlock(stmt);
  if (!block) {
    LOG(ERROR) << "Cannot find block mapped to stmt";
    return 0;
  }
  return block->getBlockID();
}

void InstrumentationVisitor::VisitCallableBody(Stmt *body) {
  // Just unroll the visitor on the body...
  HandleStmt(body);
}

bool InstrumentationVisitor::HandleLambdaExpr(LambdaExpr *LE) {
  const std::string lambdaName = InstrumentationUtils::getNameForLambda(LE, SM);
  if (instrumented.find(lambdaName) != instrumented.end())
    return true;
  instrumented.insert(lambdaName);

  FullSourceLoc fullLocation = context.getFullLoc(LE->getLocStart());

#ifdef ONLY_INSTR_COMPILED_FILES
  FileID currentFileID = fullLocation.getFileID();
  if (!isInMainFile(currentFileID)) {
    LOG(INFO) << "!isInMainFile " << lambdaName;
    return true;
  }
#endif

  element_id func_id = createSharedFunctionInformation(lambdaName);

  if (InstrumentationVisitor::FunctionInformation *FI =
          createFunctionInformation(LE, lambdaName, func_id)) {
    createStoredFunctionInfo(FI);

    cfg_stack.push(FI);
    VisitCallableBody(LE->getBody());
    cfg_stack.pop();
  }
  return true;
}

bool InstrumentationVisitor::VisitFunctionDecl(FunctionDecl *FD) {
  const std::string functionName = FD->getQualifiedNameAsString();
  if (instrumented.find(functionName) != instrumented.end())
    return true;
  instrumented.insert(functionName);

  if (!shouldInstrumentFunctionDecl(FD)) {
    LOG(INFO) << "!shouldInstrumentFunctionDecl " << functionName;
    return true;
  }

  FullSourceLoc fullLocation = context.getFullLoc(FD->getLocStart());

#ifdef ONLY_INSTR_COMPILED_FILES
  FileID currentFileID = fullLocation.getFileID();
  if (!isInMainFile(currentFileID)) {
    LOG(INFO) << "!isInMainFile " << functionName;
    return true;
  }
#endif

  element_id func_id = createSharedFunctionInformation(functionName);

  if (InstrumentationVisitor::FunctionInformation *FI =
          createFunctionInformation(FD, functionName, func_id)) {
    createStoredFunctionInfo(FI);

    cfg_stack.push(FI);
    VisitCallableBody(FD->getBody());
    cfg_stack.pop();
  }
  return true;
}

element_id InstrumentationVisitor::createSharedFunctionInformation(
    const std::string &name) {
  // Need to cleanup all of this...
  LOG(INFO) << "Current store: " << store->toString();

  element_id func_id = store->store().getNextId();
  auto e = Store::create(Element::E_FUNCTION, func_id, /*global???*/ source_id);
  auto func_elmt = std::static_pointer_cast<FunctionElement>(e);
  func_elmt->name = name;

  LOG(INFO) << "created " << func_elmt->toString();

  store->store().add(func_id, e);
  return func_id;
}

// Register block infos and the summaries (goals) associated to them
void InstrumentationVisitor::createStoredFunctionInfo(FunctionInformation *FI) {
  if (!FI->cfg) {
    LOG(ERROR) << "No CFG for the function: " << FI->func_id;
    return;
  }

  // Get the summary
  OperationClassifier::block_summaries_t summaries;
  if (FI->FD) {
    summaries = analysis->process(FI->FD, context);
  } else {
    LOG(INFO) << "LambdaExpr not supported yet";
  }

  auto f = store->store().elements[FI->func_id];
  auto func_elmt = std::static_pointer_cast<FunctionElement>(f);

  std::map<unsigned int, element_id> block_element_ids;

  // Add the CFG block info and associate the summaries
  for (auto &block : *FI->cfg) {
    element_id cur_block_id = store->store().getNextId();
    auto e = Store::create(Element::E_BLOCK, cur_block_id, FI->func_id);
    auto block_elmt = std::static_pointer_cast<BlockElement>(e);
    block_elmt->internal_block_id = block->getBlockID();
    block_element_ids[block_elmt->internal_block_id] = cur_block_id;

    LOG(INFO) << "Add block (internal id=" << block_elmt->internal_block_id
              << ") elmt=" << cur_block_id << " to func " << FI->func_id;
    func_elmt->blocks.push_back(cur_block_id);

    // Find all summaries linked to this BBL
    auto iter = summaries.find(block_elmt->internal_block_id);
    if (iter != summaries.end()) {
      for (auto &summary : iter->second) {
        element_id summary_id = store->store().getNextId();
        auto es = Store::create(Element::E_SUMMARY, summary_id, cur_block_id);
        auto summary_elmt = std::static_pointer_cast<SummaryElement>(es);
        summary_elmt->op = summary.op;
        summary_elmt->type_kind = summary.type_kind;

        store->store().add(summary_id, es);

        block_elmt->summaries.push_back(summary_id);
      }
    }

    std::map<unsigned int, std::vector<std::string>> block_pred_literals;

    // Add Block predecessors
    auto block_pred_iter = block->pred_begin(),
         block_pred_end = block->pred_end();
    for (; block_pred_iter != block_pred_end; ++block_pred_iter) {
      const unsigned int pred_id = (*block_pred_iter)->getBlockID();
      auto pred_block_elmt_id_iter = block_element_ids.find(pred_id);
      if (pred_block_elmt_id_iter != block_element_ids.end()) {
        block_elmt->predecessor_ids.push_back(pred_block_elmt_id_iter->second);
      } else {
        LOG(INFO) << "No predecessor found in map for "
                  << block_elmt->internal_block_id;
      }
    }

    // XXX add literals

    // Insert our block
    store->store().add(cur_block_id, e);
  }
}

// Specialization of `createFunctionInformation` for `LambdaExpr`
InstrumentationVisitor::FunctionInformation *
InstrumentationVisitor::createFunctionInformation(LambdaExpr *LE,
                                                  const std::string &name,
                                                  const element_id func_id) {
  FunctionInformation *FI = new FunctionInformation();
  FI->func_id = func_id;
  FI->FD = nullptr;
  FI->LE = LE;

  // Quid of handling signatures of lambda expr
  if (LE->hasExplicitResultType()) {

  } else {
    // ????
  }

  SourceLocation start = LE->getBody()->getLocStart();
  std::ostringstream oss;

  oss << "unsigned int __coverage_pred_block = 0; __coverage_enter_func(" << func_id
      << ");";
  rewrite->InsertTextBefore(expand_loc(start), oss.str());

  if (!hasReturnForEpilogue(LE->getBody())) {
    SourceLocation end = LE->getBody()->getLocEnd();

    std::ostringstream ross;
    ross << " __coverage_exit_func(" << func_id << "); ";
    rewrite->InsertText(expand_loc(end), ross.str());
  }

  InstrumentationVisitor::FunctionInformation *fd_parent =
      cfg_stack.getNonLambdaContainer();
  if (!fd_parent) {
    LOG(ERROR) << "Cannot find parent function for lambda: " << name;

  } else {
    FI->cfg = InstrumentationUtils::buildCFG(LE, context, fd_parent->FD);
  }

#ifdef DUMP_CFG_OUTPUT
  std::cout << "CFG for: " << name << std::endl;
  FI->cfg->dump(context.getLangOpts(), true);
#endif

  FI->pm = llvm::make_unique<ParentMap>(LE->getBody());
  FI->map = CFGStmtMap::Build(FI->cfg.get(), FI->pm.get());

  return FI;
}

// For a `FunctionDecl` or `CXXMethodDecl`, compute information required for the
// instrumentation. Access to type info, etc.
InstrumentationVisitor::FunctionInformation *
InstrumentationVisitor::createFunctionInformation(FunctionDecl *FD,
                                                  const std::string &name,
                                                  const element_id func_id) {

  InstrumentationVisitor::FunctionInformation *FI =
      new InstrumentationVisitor::FunctionInformation();
  FI->func_id = func_id;
  FI->FD = FD;
  FI->LE = nullptr;

  // Insert prelude in function. This prelude registers a local variable in each
  // function that helps tracking the previous BBL. The combination of the
  // previous
  // BBL and current BBL is used to know the CFG edge.
  SourceLocation start = FD->getBody()->getLocStart().getLocWithOffset(1);
  std::ostringstream oss;

  FI->hasRetValue = InstrumentationUtils::hasNonVoidReturnValue(FD);
  FI->requiresLocalReturnVariable =
      !InstrumentationUtils::canInstrumentReturnGlobally(FD);

  if (FI->hasRetValue && !FI->requiresLocalReturnVariable) {
    FI->hasRetValue = true;
    oss << InstrumentationUtils::getLiteralReturnType(FD)
        << " __coverage_ret_value;";
  }

  if (FI->requiresLocalReturnVariable) {
    FI->qualifiedReturnType = InstrumentationUtils::getLiteralReturnType(FD);
  }

  oss << "unsigned int __coverage_pred_block = 0; __coverage_enter_func(" << func_id
      << ");";
  rewrite->InsertTextBefore(expand_loc(start), oss.str());

  // If the last token isn't a return, we still need to inject something to know
  // this is the exit of the function
  if (!hasReturnForEpilogue(FD->getBody())) {
    SourceLocation end = FD->getBody()->getLocEnd();
    std::ostringstream ross;
    ross << " __coverage_exit_func(" << func_id << "); ";
    rewrite->InsertText(expand_loc(end), ross.str());
  }

  FI->cfg = InstrumentationUtils::buildCFG(FD, context);

#ifdef DUMP_CFG_OUTPUT
  std::cout << "CFG for: " << name << std::endl;
  FI->cfg->dump(context.getLangOpts(), true);
#endif

  FI->pm = llvm::make_unique<ParentMap>(FD->getBody());
  FI->map = CFGStmtMap::Build(FI->cfg.get(), FI->pm.get());

  return FI;
}

bool InstrumentationVisitor::hasReturnForEpilogue(Stmt *stmt) {
  Stmt *last_stmt = nullptr;
  for (auto &child : stmt->children()) {
    if (child)
      last_stmt = child;
  }
  return last_stmt && isa<ReturnStmt>(last_stmt);
}

std::string InstrumentationVisitor::getReturnStmtInstrumentation() {
  std::ostringstream oss;
  oss << " __coverage_exit_func(" << cfg_stack.id() << "); ";
  return oss.str();
}

bool InstrumentationVisitor::HandleReturnStmt(ReturnStmt *RS) {
  if (!cfg_stack.isLambdaExpr() &&
      InstrumentationUtils::isInsideLambda(
          RS, cfg_stack.getNonLambdaContainer()->pm.get()))
    return true;

  if (instrumented_returns.find(RS) != instrumented_returns.end())
    return true;
  instrumented_returns.insert(RS);

  SourceLocation start = RS->getLocStart();
  if (!cfg_stack.hasRetValue()) {
    // No return value, we just prepend our code.
    rewrite->InsertTextAfter(expand_loc(start), getReturnStmtInstrumentation());
  } else {
    // Capture the expression and assign it to __coverage_ret_value. We might create
    // a new variable if we cannot just create a variable (e.g., reference
    // types)
    Expr *retValueExpr = RS->getRetValue();
    const std::string retValueLiteral =
        InstrumentationUtils::getLiteralExpr(SM, rewrite, retValueExpr);

    std::ostringstream oss;
    std::string ret_variable_name = "__coverage_ret_value";

    if (cfg_stack.requiresLocalReturnVariable()) {
      oss << cfg_stack.qualifiedReturnType() << " ";
      ret_variable_name =
          "__coverage_ret_value_" + std::to_string(cfg_stack.loc_ret_counter());
    }

    oss << ret_variable_name << " = (" << retValueLiteral << ");"
        << getReturnStmtInstrumentation();
    std::string replValue = oss.str();

    // Insert the assignment to `__coverage_ret_value` before the current return
    // statement.
    rewrite->InsertTextAfter(expand_loc(start), replValue);

    // Change the expression of the return statement to be only
    // `__coverage_ret_value`
    SourceLocation expr_loc = retValueExpr->getLocStart();
    rewrite->RemoveText(expand_loc(expr_loc),
                        rewrite->getRangeSize(retValueExpr->getSourceRange()));
    rewrite->InsertTextAfter(expand_loc(expr_loc), ret_variable_name);
  }
  return true;
}

//
// InstrumentationUtils
//

bool InstrumentationUtils::isInsideLambda(Stmt *stmt, ParentMap *PM) {
  Stmt *current = stmt;
  while (current) {
    if (isa<LambdaExpr>(current))
      return true;
    current = PM->getParent(current);
  }
  return false;
}

std::string InstrumentationUtils::getLiteralReturnType(FunctionDecl *FD) {
  SplitQualType type = FD->getReturnType().split();
  std::string value = QualType::getAsString(type);
  if (value == "_Bool") {
    value = "bool";
  }
  return value;
}

bool InstrumentationUtils::hasNonVoidReturnValue(FunctionDecl *FD) {
  return InstrumentationUtils::getLiteralReturnType(FD) != "void";
}

// When it's a reference type, we cannot use a variable to capture the return
// type and assign it, so we'll need to create variable in the scope of the
// return.
bool InstrumentationUtils::canInstrumentReturnGlobally(FunctionDecl *FD) {
  return !FD->getReturnType().getTypePtr()->isReferenceType();
}

std::string InstrumentationUtils::getLiteralExpr(SourceManager *SM,
                                                 Rewriter *rewrite,
                                                 const clang::Stmt *S) {
  int length = rewrite->getRangeSize(S->getSourceRange());
  if (length < 0) {
    return std::string("");
  }
  const char *buffer = SM->getCharacterData(S->getLocStart());
  return std::string(buffer, length);
}

bool InstrumentationUtils::isControlFlowStmt(Stmt *stmt) {
  return stmt != nullptr && (isa<CompoundStmt>(stmt));
}

std::unique_ptr<CFG> InstrumentationUtils::buildCFG(FunctionDecl *FD,
                                                    ASTContext &context) {
  CFG::BuildOptions bo;
  bo.PruneTriviallyFalseEdges = false;
  bo.AddEHEdges = true;

  return CFG::buildCFG(FD, FD->getBody(), &context, bo);
}

std::unique_ptr<CFG> InstrumentationUtils::buildCFG(LambdaExpr *LE,
                                                    ASTContext &context,
                                                    FunctionDecl *FD) {
  CFG::BuildOptions bo;
  bo.PruneTriviallyFalseEdges = false;
  bo.AddEHEdges = true;
  bo.setAllAlwaysAdd();

  return CFG::buildCFG(FD, LE->getBody(), &context, bo);
}

std::string InstrumentationUtils::getNameForLambda(LambdaExpr *LE,
                                                   SourceManager *SM) {
  SourceLocation start = LE->getLocStart();

  std::ostringstream oss;
  oss << "<lambda_" << start.printToString(*SM) << ">";
  return oss.str();
}
}
