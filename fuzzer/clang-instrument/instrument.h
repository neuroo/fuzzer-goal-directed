#ifndef INSTRUMENT_H
#define INSTRUMENT_H

#include "clang/AST/AST.h"
#include "clang/AST/ASTConsumer.h"
#include "clang/Analysis/CFG.h"
#include "clang/Analysis/CFGStmtMap.h"
#include "clang/Frontend/CompilerInstance.h"
#include "clang/Rewrite/Core/Rewriter.h"
#include "clang/Sema/Sema.h"
using namespace clang;

#include <list>
#include <memory>
#include <set>
#include <stack>
#include <vector>

#include "analysis.h"
#include "common/logger.h"
#include "common/store.h"
#include "config.h"

namespace instr {

struct InstrumentationVisitor
    : public RecursiveASTVisitor<InstrumentationVisitor> {
  ASTContext &context;
  SourceManager *SM = nullptr;
  FileID &mainFileID;
  Rewriter *rewrite = nullptr;

  std::set<std::string> instrumented;
  std::set<ReturnStmt *> instrumented_returns;

  std::unique_ptr<Analysis> &analysis;
  std::unique_ptr<Store> &store;

  // Everything we need to know about a given function. We need to order them
  // as a stack since we perform a traversal and want to know information about
  // the current function or lambda expr.
  struct FunctionInformation {
    element_id func_id = ERROR_ID;

    FunctionDecl *FD = nullptr; // stays null in a lambda
    LambdaExpr *LE = nullptr;

    CFGStmtMap *map = nullptr;
    std::unique_ptr<CFG> cfg;
    std::unique_ptr<ParentMap> pm;

    bool hasRetValue = false;
    bool requiresLocalReturnVariable = false;
    std::string qualifiedReturnType;
    unsigned int loc_ret_counter = 0;

    FunctionInformation() = default;
    FunctionInformation(const FunctionInformation &f) = default;
    FunctionInformation &operator=(const FunctionInformation &f) = default;
    ~FunctionInformation() {
      if (map)
        delete map;
      if (pm)
        pm.release();
      if (cfg)
        cfg.release();
    }

    bool isLambdaExpr() const { return LE == nullptr && FD != nullptr; }
  };

  // THe stack captures information gathered during the traversal of functions,
  // methods and lambdas.
  struct CFGStack {
    std::list<FunctionInformation *> M;

    CFGStmtMap *map() { return M.empty() ? nullptr : M.front()->map; }

    CFG *cfg() { return M.empty() ? nullptr : M.front()->cfg.get(); }

    ParentMap *PM() { return M.empty() ? nullptr : M.front()->pm.get(); }

    FunctionInformation *getNonLambdaContainer() {
      for (auto &elmt : M) {
        if (elmt->FD != nullptr)
          return elmt;
      }
      return nullptr;
    }

    element_id id() const { return M.empty() ? ERROR_ID : M.front()->func_id; }

    // returns true if the top is a lambda expr
    bool isLambdaExpr() const {
      return M.empty() ? false : M.front()->isLambdaExpr();
    }

    bool hasRetValue() const {
      return M.empty() ? false : M.front()->hasRetValue;
    }

    bool requiresLocalReturnVariable() const {
      return M.empty() ? false : M.front()->requiresLocalReturnVariable;
    }

    std::string qualifiedReturnType() const {
      return M.empty() ? "" : M.front()->qualifiedReturnType;
    }

    unsigned int loc_ret_counter() const {
      if (M.empty())
        return 0;
      M.front()->loc_ret_counter++;
      return M.front()->loc_ret_counter;
    }

    void push(FunctionInformation *FI) { M.push_front(FI); }

    void pop() {
      FunctionInformation *FI = M.front();
      if (FI)
        delete FI;
      M.pop_front();
    }
  };
  CFGStack cfg_stack;

  element_id source_id = ERROR_ID;

  InstrumentationVisitor(ASTContext &context, SourceManager *SM,
                         FileID &mainFileID, Rewriter *rewrite,
                         std::unique_ptr<Store> &store, element_id source_id,
                         std::unique_ptr<Analysis> &analysis)
      : context(context), SM(SM), mainFileID(mainFileID), rewrite(rewrite),
        analysis(analysis), store(store), source_id(source_id) {
    LOG(INFO) << "Create InstrumentationVisitor";
  }

  ~InstrumentationVisitor() { LOG(INFO) << "Destruct InstrumentationVisitor"; }

  // Limit the instrumentation to non-inlined functions
  bool shouldInstrumentFunctionDecl(const FunctionDecl *FD);

  // Used to limit the instrumentation to the current TU
  bool isInMainFile(const FileID &funcFileID);

  // Callback from the visitor on functions, method, and lambda expression.
  // These are really the entry points to the visitor since it'll call all other
  // handlers (Stmt/ReturnStmt) on their bodies
  bool VisitFunctionDecl(FunctionDecl *FD);

  void VisitCallableBody(Stmt *body);

  //
  bool HandleStmt(Stmt *stmt);

  bool HandleReturnStmt(ReturnStmt *stmt);

  bool HandleLambdaExpr(LambdaExpr *LE);

  // Instrumentation root at the function level
  InstrumentationVisitor::FunctionInformation *
  createFunctionInformation(FunctionDecl *, const std::string &,
                            const element_id);
  InstrumentationVisitor::FunctionInformation *
  createFunctionInformation(LambdaExpr *, const std::string &,
                            const element_id);

  // Compute and store information for a given function
  element_id createSharedFunctionInformation(const std::string &functionName);

  void createStoredFunctionInfo(FunctionInformation *FI);

private:
  SourceLocation expand_loc(const SourceLocation &);

  bool hasReturnForEpilogue(Stmt *stmt);

  std::string getReturnStmtInstrumentation();

  void InsertSkippedBlockDirective(IfStmt *ifStmt);

  void EnsureBracesControlStmt(Stmt *stmt);

  unsigned int findBlockIdForStmt(Stmt *stmt);

  void InsertBlockDirective(const Stmt *S, const unsigned int block_id);

  void rewriteReturnStatements(FunctionDecl *FD);
};

struct InstrumentationUtils {
  static bool isInsideLambda(Stmt *stmt, ParentMap *PM);

  static std::unique_ptr<clang::CFG> buildCFG(FunctionDecl *FD,
                                              ASTContext &context);

  static std::unique_ptr<clang::CFG>
  buildCFG(LambdaExpr *LE, ASTContext &context, FunctionDecl *FD);

  static std::string getLiteralReturnType(FunctionDecl *FD);

  static bool hasNonVoidReturnValue(FunctionDecl *FD);

  static bool canInstrumentReturnGlobally(FunctionDecl *FD);

  static bool isControlFlowStmt(Stmt *stmt);

  static std::string getLiteralExpr(SourceManager *SM, Rewriter *rewrite,
                                    const clang::Stmt *S);

  static std::string getNameForLambda(LambdaExpr *LE, SourceManager *SM);
};
}

#endif
