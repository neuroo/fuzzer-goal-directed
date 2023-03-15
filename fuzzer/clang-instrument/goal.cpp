#include "clang/AST/AST.h"
#include "clang/AST/ASTConsumer.h"
#include "clang/AST/OperationKinds.h"
#include "clang/AST/ParentMap.h"
#include "clang/AST/RecursiveASTVisitor.h"
#include "clang/AST/StmtVisitor.h"
#include "clang/Analysis/AnalysisContext.h"
#include "clang/Analysis/CFG.h"
#include "clang/Analysis/CFGStmtMap.h"

#include "llvm/Support/raw_ostream.h"
using namespace clang;

#include "common/elements.h"
#include "common/logger.h"
#include "goal.h"

namespace instr {

//
// Visitor to find all references to VarDecl
//
struct VarRefDeclFinder : public RecursiveASTVisitor<VarRefDeclFinder> {
  std::set<VarDecl *> var_decls;
  std::set<DeclRefExpr *> var_refs;

  bool VisitVarDecl(VarDecl *VD) {
    var_decls.insert(VD);
    return true;
  }

  bool VisitFunctionDecl(FunctionDecl *FD) {
    for (auto &param_decl : FD->parameters()) {
      var_decls.insert(param_decl);
    }
    return true;
  }

  bool VisitDeclRefExpr(DeclRefExpr *DR) {
    if (DR->getDecl()) {
      // If there is no available declaration, we don't do anything.
      var_refs.insert(DR);
    }
    return true;
  }
};

// Prints all the operations
void OperationClassifier::debugPrint() {
  for (auto &op : operations) {
    LOG(INFO) << "- " << op.toString();
  }
}

//
// OperationClassifier
//

OperationClassifier::block_summaries_t OperationClassifier::get_summaries() {
  if (!created_summaries) {
    for (auto &operation : operations) {
      unsigned int block_id = operation.block_id;
      auto iter = summaries.find(block_id);
      if (iter == summaries.end()) {
        summaries.insert({block_id, std::vector<Operation>()});
        iter = summaries.find(block_id);
      }

      iter->second.push_back(operation);
    }
    created_summaries = true;
  }
  return summaries;
}

// Summarize the type of the current decl
TypeKind OperationClassifier::TypeOracle::decl_type(const VarDecl *v) {
  auto iter = type_kinds.find(v);
  if (iter != type_kinds.end())
    return iter->second;
  TypeKind kind = internal_decl_type(v);
  type_kinds.insert({v, kind});
  return kind;
}

TypeKind OperationClassifier::TypeOracle::internal_decl_type(const VarDecl *v) {
  const VarDecl *CV = v->getCanonicalDecl();
  QualType type = CV->getType();
  const Type *type_ptr = type.getTypePtr();
  if (!type_ptr) {
    LOG(INFO) << "Cannot get type ptr";
    return TY_UNKNOWN;
  }

  if (type_ptr->isIntegerType()) {
    return TY_INTEGER;
  } else if (type_ptr->isArrayType()) {
    return TY_BUFFER;
  } else if (type_ptr->isPointerType()) {
    return pointer_type(type, type_ptr);
  }

  return TY_UNKNOWN;
}

// This is the special case for a pointer. We need to inspect what's the
// pointee type.
TypeKind
OperationClassifier::TypeOracle::pointer_type(const QualType &qual_type,
                                              const Type *type_ptr) {
  QualType ptee_type = type_ptr->getPointeeType();
  const Type *ptee_type_ptr = ptee_type.getTypePtr();
  if (!ptee_type_ptr)
    return TY_UNKNOWN;

  if (ptee_type_ptr->isScalarType() || ptee_type_ptr->isVoidType() ||
      ptee_type_ptr->isPointerType()) {
    return TY_BUFFER;
  } else if (ptee_type_ptr->isStructureType()) {
    return TY_STRUCT;
  } else {
    LOG(INFO) << "Unknown type: " << qual_type.getAsString();
  }
  return TY_UNKNOWN;
}

// Given a current context, builds the information about what happens in the
// function. It's a summary of block level operations.
bool OperationClassifier::classify(AnalysisDeclContext &AC) {
  created_summaries = false;
  const Decl *D = AC.getDecl();

  if (!D || !isa<FunctionDecl>(D))
    return false;

  // XXX need to handle lambdas
  if (const FunctionDecl *CFD = dyn_cast<FunctionDecl>(D)) {
    FunctionDecl *FD = const_cast<FunctionDecl *>(CFD);
    AC.getCFGBuildOptions().setAllAlwaysAdd();
    if (!AC.getCFG())
      return false;

    VarRefDeclFinder var_ref_decl_finder;
    var_ref_decl_finder.TraverseDecl(FD);

    // Nothing to explore...
    if (var_ref_decl_finder.var_refs.empty())
      return true;

    std::unique_ptr<ParentMap> PM = llvm::make_unique<ParentMap>(FD->getBody());
    cfg_map = CFGStmtMap::Build(AC.getCFG(), PM.get());

    for (auto &var_ref : var_ref_decl_finder.var_refs) {
      // We don't care of anything but variables or parameters...
      if (VarDecl *var_decl = dyn_cast<VarDecl>(var_ref->getDecl())) {
        const TypeKind type_kind = type_oracle.decl_type(var_decl);
        if (type_kind == TY_UNKNOWN) {
          LOG(INFO) << "\tTY_UNKNOWN type for var: "
                    << var_decl->getQualifiedNameAsString();
          continue;
        }
        classify_use(type_kind, var_decl, var_ref, PM.get());
      } else {
        LOG(INFO) << "Non var Decl: " << var_ref->getDecl()->getNameAsString();
      }
    }

    delete cfg_map;
    cfg_map = nullptr;
  } else {
    LOG(INFO) << "No goal in lambdas yet";
  }

  return true;
}

// Returns the highest `Expr` that contains this `DeclRefExpr`. The idea is to
// be able to analyze the full expr that uses it.
const Stmt *OperationClassifier::find_containing_expr(const DeclRefExpr *RE,
                                                      ParentMap *parent_map) {
  const Stmt *op = RE;
  const Stmt *parent = nullptr;
  while (op) {
    parent = parent_map->getParent(op);
    if (!isa<Expr>(parent)) {
      break;
    }
    op = parent;
  }
  return op;
}

// Get the CFG block ID where the stmt belongs to
unsigned int OperationClassifier::get_block_id(const Stmt *stmt) {
  if (!cfg_map)
    return 0;
  const CFGBlock *block = cfg_map->getBlock(stmt);
  if (!block)
    return 0;
  return block->getBlockID();
}

// Once we get a DeclRefExpr, we need to find how it's being used... it can
// be a direct parent or something else.
void OperationClassifier::classify_use(const TypeKind type_kind,
                                       const VarDecl *var_decl,
                                       const DeclRefExpr *RE,
                                       ParentMap *parent_map) {
  const Stmt *stmt_op = find_containing_expr(RE, parent_map);
  if (!stmt_op) {
    LOG(INFO) << "Couldn't find containing expr...";
    return;
  }
  switch (type_kind) {
  case TY_INTEGER:
    classify_integer_use(var_decl, stmt_op, RE, parent_map);
    break;
  case TY_BUFFER:
    classify_buffer_use(var_decl, stmt_op, RE, parent_map);
    break;
  case TY_STRUCT:
    classify_struct_use(var_decl, stmt_op, RE, parent_map);
    break;
  default:
    break;
  }
}

// Returns true if `child` is actually a child of `parent`
bool isParentOf(ParentMap *parent_map, const Stmt *parent, const Stmt *child) {
  Stmt *current = const_cast<Stmt *>(child);
  while (current && parent_map->hasParent(current)) {
    if (current == parent)
      return true;
    current = parent_map->getParent(current);
  }
  return false;
}

bool isDontCareExpr(const Stmt *stmt) {
  return isa<CXXNewExpr>(stmt) || isa<CXXConstructExpr>(stmt);
}

// Unroll ImplicitCastExpr and ParentExpr
const Expr *unroll_expr(const Expr *expr) {
  const Expr *cur = expr;
  bool deepen = false;
  while (true) {
    deepen = false;
    if (isa<ImplicitCastExpr>(cur)) {
      const ImplicitCastExpr *cur_cast = dyn_cast<ImplicitCastExpr>(cur);
      cur = cur_cast->getSubExpr();
      deepen = true;
    } else if (isa<ParenExpr>(cur)) {
      const ParenExpr *cur_cast = dyn_cast<ParenExpr>(cur);
      cur = cur_cast->getSubExpr();
      deepen = true;
    }
    if (!deepen)
      break;
  }
  return cur;
}

void OperationClassifier::classify_any_binop(const VarDecl *var_decl,
                                             const BinaryOperator *bin_op,
                                             const DeclRefExpr *RE,
                                             ParentMap *parent_map,
                                             const TypeKind type_kind) {
  typedef BinaryOperatorKind Opcode;
  const Operator op =
      type_kind == TY_INTEGER ? OP_INTEGER_UNKNOWN : OP_BUFFER_UNKNOWN;

  if (bin_op->isCompoundAssignmentOp()) {
    // Only report something if DeclRefExpr is on the LHS of this operator
    if (isParentOf(parent_map, bin_op->getLHS(), RE)) {
      operations.insert(Operation::create(op, type_kind, get_block_id(bin_op),
                                          var_decl, bin_op));
    } else {
      LOG(INFO) << "skipped isCompoundAssignmentOp";
    }
  } else {
    // Only for non-logical operations
    const Opcode opc = bin_op->getOpcode();
    if (opc >= BO_Mul && opc <= BO_Shr) {
      // in {BO_Mul, BO_Div, BO_Rem, BO_Add, BO_Sub, BO_Shl, BO_Shr}
      operations.insert(Operation::create(op, type_kind, get_block_id(bin_op),
                                          var_decl, bin_op));
    } else {
      // inspect LHS or RHS
      const Stmt *stmt_op = unroll_expr(
          isParentOf(parent_map, bin_op->getLHS(), RE) ? bin_op->getLHS()
                                                       : bin_op->getRHS());
      switch (type_kind) {
      case TY_INTEGER:
        classify_integer_use(var_decl, stmt_op, RE, parent_map);
        break;
      case TY_BUFFER:
        classify_buffer_use(var_decl, stmt_op, RE, parent_map);
        break;
      case TY_STRUCT:
        classify_struct_use(var_decl, stmt_op, RE, parent_map);
        break;
      default:
        break;
      }
    }
  }
}

void OperationClassifier::classify_any_unaop(const VarDecl *var_decl,
                                             const UnaryOperator *unary_op,
                                             const DeclRefExpr *RE,
                                             ParentMap *parent_map,
                                             const TypeKind type_kind) {
  typedef UnaryOperatorKind Opcode;

  const Operator op =
      type_kind == TY_INTEGER ? OP_INTEGER_UNKNOWN : OP_BUFFER_UNKNOWN;

  if (unary_op->isIncrementOp() || unary_op->isDecrementOp()) {
    operations.insert(Operation::create(op, type_kind, get_block_id(unary_op),
                                        var_decl, unary_op));
  } else {
    const Opcode opc = unary_op->getOpcode();

    if (opc == UO_Not) {
      // The unary not
      operations.insert(Operation::create(op, type_kind, get_block_id(unary_op),
                                          var_decl, unary_op));
    } else if (opc == UO_Deref) {
      operations.insert(Operation::create(
          op == OP_BUFFER_UNKNOWN ? OP_BUFFER_READ : OP_BUFFER_UNKNOWN,
          type_kind, get_block_id(unary_op), var_decl, unary_op));
    } else {
      LOG(INFO) << "Unhandled type of unary operation: " << opc;
    }
  }
}

// Search for operations that can lead to wrap-around
void OperationClassifier::classify_integer_use(const VarDecl *var_decl,
                                               const Stmt *stmt,
                                               const DeclRefExpr *RE,
                                               ParentMap *parent_map) {
  if (isa<CallExpr>(stmt)) {
    // Unconditionally interesting
    operations.insert(Operation::create(OP_PASS_THROUGH, TY_INTEGER,
                                        get_block_id(stmt), var_decl, stmt));
  } else if (isa<BinaryOperator>(stmt)) {
    const BinaryOperator *bin_op = dyn_cast<BinaryOperator>(stmt);
    classify_any_binop(var_decl, bin_op, RE, parent_map, TY_INTEGER);
  } else if (isa<UnaryOperator>(stmt)) {
    const UnaryOperator *unary_op = dyn_cast<UnaryOperator>(stmt);
    classify_any_unaop(var_decl, unary_op, RE, parent_map, TY_INTEGER);
  } else if (isa<ExplicitCastExpr>(stmt)) {
    // Don't really care to analyze what it's for. It can get interesting
    // anyways. Might want to special cases casts from types of different sizes.
    operations.insert(Operation::create(OP_CAST_UNKNOWN, TY_INTEGER,
                                        get_block_id(stmt), var_decl, RE));
  } else if (isa<ImplicitCastExpr>(stmt) || isa<ParenExpr>(stmt)) {
    const Stmt *stmt_op = unroll_expr(static_cast<const Expr *>(stmt));
    classify_integer_use(var_decl, stmt_op, RE, parent_map);
  } else if (isa<ArraySubscriptExpr>(stmt)) {
    const ArraySubscriptExpr *array_sub = dyn_cast<ArraySubscriptExpr>(stmt);
    if (isParentOf(parent_map, array_sub->getBase(), RE)) {
      operations.insert(Operation::create(OP_INTEGER_MAY_OVERFLOW, TY_INTEGER,
                                          get_block_id(stmt), var_decl, RE));
    }
  } else {
    LOG(INFO) << "Unhandled type of integer operation: "
              << stmt->getStmtClassName();
  }
}

// Search for pointer arithmetic or casts
void OperationClassifier::classify_buffer_use(const VarDecl *var_decl,
                                              const Stmt *stmt,
                                              const DeclRefExpr *RE,
                                              ParentMap *parent_map) {
  if (isa<CallExpr>(stmt)) {
    // Unconditionally interesting
    operations.insert(Operation::create(OP_PASS_THROUGH, TY_BUFFER,
                                        get_block_id(stmt), var_decl, stmt));
  } else if (isa<ExplicitCastExpr>(stmt)) {
    if (isa<CStyleCastExpr>(stmt)) {
      // C-style cast is weird in C++, but can be used to cast ptrs
      // XXX inspect the type used to cast the expr.
      operations.insert(Operation::create(OP_CAST_UNSAFE, TY_BUFFER,
                                          get_block_id(stmt), var_decl, RE));
    } else {
      const Operator op =
          isa<CXXReinterpretCastExpr>(stmt) ? OP_CAST_UNSAFE : OP_CAST_UNKNOWN;
      operations.insert(
          Operation::create(op, TY_BUFFER, get_block_id(stmt), var_decl, RE));
    }
  } else if (isa<BinaryOperator>(stmt)) {
    const BinaryOperator *bin_op = dyn_cast<BinaryOperator>(stmt);
    classify_any_binop(var_decl, bin_op, RE, parent_map, TY_BUFFER);
  } else if (isa<UnaryOperator>(stmt)) {
    const UnaryOperator *unary_op = dyn_cast<UnaryOperator>(stmt);
    classify_any_unaop(var_decl, unary_op, RE, parent_map, TY_BUFFER);
  } else if (isa<ArraySubscriptExpr>(stmt)) {
    const ArraySubscriptExpr *array_sub = dyn_cast<ArraySubscriptExpr>(stmt);
    if (isParentOf(parent_map, array_sub->getBase(), RE)) {
      operations.insert(Operation::create(OP_BUFFER_UNKNOWN, TY_BUFFER,
                                          get_block_id(stmt), var_decl, RE));
    }
  } else if (isa<ImplicitCastExpr>(stmt) || isa<ParenExpr>(stmt)) {
    const Stmt *stmt_op = unroll_expr(static_cast<const Expr *>(stmt));
    classify_buffer_use(var_decl, stmt_op, RE, parent_map);
  } else {
    LOG(INFO) << "Unhandled type of buffer operation: "
              << stmt->getStmtClassName();
  }
}

void OperationClassifier::classify_struct_use(const VarDecl *var_decl,
                                              const Stmt *stmt,
                                              const DeclRefExpr *RE,
                                              ParentMap *parent_map) {
  // For structs, we really just care about weird casts
  if (isa<CallExpr>(stmt)) {
    // Unconditionally interesting
    operations.insert(Operation::create(OP_PASS_THROUGH, TY_STRUCT,
                                        get_block_id(stmt), var_decl, stmt));
  } else if (isa<ExplicitCastExpr>(stmt)) {
    if (isa<CStyleCastExpr>(stmt)) {
      // C-style cast is weird in C++, but can be used to cast ptrs
      // XXX inspect the type used to cast the expr.
      operations.insert(Operation::create(OP_CAST_UNSAFE, TY_BUFFER,
                                          get_block_id(stmt), var_decl, RE));
    } else {
      const Operator op =
          isa<CXXReinterpretCastExpr>(stmt) ? OP_CAST_UNSAFE : OP_CAST_UNKNOWN;
      operations.insert(
          Operation::create(op, TY_BUFFER, get_block_id(stmt), var_decl, RE));
    }
  } else if (isa<ImplicitCastExpr>(stmt) || isa<ParenExpr>(stmt)) {
    const Stmt *stmt_op = unroll_expr(static_cast<const Expr *>(stmt));
    classify_struct_use(var_decl, stmt_op, RE, parent_map);
  } else {
    LOG(INFO) << "Unhandled type of struct operation: "
              << stmt->getStmtClassName();
  }
}
}
