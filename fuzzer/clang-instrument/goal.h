#ifndef GOAL_H
#define GOAL_H

#include "clang/AST/AST.h"
#include "clang/AST/ASTConsumer.h"
#include "clang/AST/OperationKinds.h"
#include "clang/Analysis/AnalysisContext.h"
#include "clang/Analysis/CFG.h"
#include "clang/Analysis/CFGStmtMap.h"

#include "common/elements.h"

#include <map>
#include <memory>
#include <set>
#include <unordered_set>
namespace instr {

struct OperationClassifier {
  // Represent an interesting operation in the source code. The operation
  // involves a given `Operator`, a known variable `var`, as well as a
  // location where it happens `ref`. When this gets serialized, we remove
  // all references to the AST.
  struct Operation {
    Operator op;
    TypeKind type_kind;
    unsigned int block_id;
    const VarDecl *var;
    const Stmt *ref;

    Operation(const Operator op, const TypeKind type_kind,
              const unsigned int block_id, const VarDecl *var,
              const Stmt *ref = nullptr)
        : op(op), type_kind(type_kind), block_id(block_id), var(var), ref(ref) {
    }

    Operation(const Operation &o)
        : op(o.op), type_kind(o.type_kind), block_id(o.block_id), var(o.var),
          ref(o.ref) {}
    Operation &operator=(const Operation &o) {
      if (&o != this) {
        op = o.op;
        type_kind = o.type_kind;
        block_id = o.block_id;
        var = o.var;
        ref = o.ref;
      }
      return *this;
    }

    std::string toString() const {
      std::ostringstream oss;
      oss << "<Operation " << operatorName(op) << " " << typeName(type_kind)
          << " '" << var->getQualifiedNameAsString() << "' @ "
          << ref->getLocStart().printToString(
                 var->getASTContext().getSourceManager())
          << " block #" << block_id << ">";
      return oss.str();
    }

    static Operation create(const Operator op, const TypeKind type_kind,
                            const unsigned int block_id, const VarDecl *var,
                            const Stmt *ref = nullptr) {
      return Operation(op, type_kind, block_id, var, ref);
    }

    static Operation create(const Operator op, const TypeKind type_kind,
                            const VarDecl *var, const Stmt *ref = nullptr) {
      return Operation(op, type_kind, 0, var, ref);
    }
  };

  struct hash_operation {
    size_t operator()(const Operation &o) const {
      using namespace std;
      return hash<unsigned int>()(o.op) ^ hash<unsigned int>()(o.type_kind) ^
             hash<unsigned int>()(o.block_id) ^
             hash<uintptr_t>()(reinterpret_cast<uintptr_t>(o.var)) ^
             hash<uintptr_t>()(reinterpret_cast<uintptr_t>(o.ref));
    }
  };

  struct equal_operation {
    bool operator()(const Operation &o1, const Operation &o2) const {
      return o1.op == o2.op && o1.type_kind == o2.type_kind &&
             o1.block_id == o2.block_id && o1.var == o2.var && o1.ref == o2.ref;
    }
  };

  // Helper that summarizes the type of a VarDecl and caches it.
  struct TypeOracle {
    std::map<const VarDecl *, TypeKind> type_kinds;

    TypeKind decl_type(const VarDecl *v);

  private:
    TypeKind internal_decl_type(const VarDecl *v);

    TypeKind pointer_type(const QualType &qual_type, const Type *type_ptr);
  };

  typedef std::unordered_set<Operation, hash_operation, equal_operation>
      operations_t;

  // XXX Add a way to integrate literals found in conditions
  typedef std::map<unsigned int, std::vector<Operation>> block_summaries_t;

  ASTContext &context;
  CFGStmtMap *cfg_map = nullptr;
  TypeOracle type_oracle;
  operations_t operations;

  bool created_summaries = false;
  block_summaries_t summaries;

  OperationClassifier() = delete;
  OperationClassifier(const OperationClassifier &) = delete;
  OperationClassifier &operator=(const OperationClassifier &) = delete;

  OperationClassifier(ASTContext &context) : context(context) {}

  bool classify(AnalysisDeclContext &AC);

  block_summaries_t get_summaries();

  void debugPrint();

private:
  void classify_use(const TypeKind type_kind, const VarDecl *var_decl,
                    const DeclRefExpr *RE, ParentMap *parent_map);

  void classify_integer_use(const VarDecl *var_decl, const Stmt *stmt,
                            const DeclRefExpr *RE, ParentMap *parent_map);

  void classify_buffer_use(const VarDecl *var_decl, const Stmt *stmt,
                           const DeclRefExpr *RE, ParentMap *parent_map);

  void classify_struct_use(const VarDecl *var_decl, const Stmt *stmt,
                           const DeclRefExpr *RE, ParentMap *parent_map);

  void classify_any_binop(const VarDecl *var_decl, const BinaryOperator *bin_op,
                          const DeclRefExpr *RE, ParentMap *parent_map,
                          const TypeKind type_kind);

  void classify_any_unaop(const VarDecl *var_decl,
                          const UnaryOperator *unary_op, const DeclRefExpr *RE,
                          ParentMap *parent_map, const TypeKind type_kind);

  const Stmt *find_containing_expr(const DeclRefExpr *RE,
                                   ParentMap *parent_map);

  unsigned int get_block_id(const Stmt *stmt);
};
}

#endif
