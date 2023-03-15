#ifndef ELEMENTS_H
#define ELEMENTS_H

#include <cstdint>
#include <list>
#include <sstream>
#include <vector>

#include <cereal/types/polymorphic.hpp>

namespace instr {

typedef uint32_t element_id;
static const element_id ERROR_ID = 0x0;

class Element : public std::enable_shared_from_this<Element> {
  friend class cereal::access;

public:
  enum Kind {
    E_UNKNOWN = 0,
    E_SOURCE,
    E_FUNCTION,
    E_BLOCK,
    E_SUMMARY,
    E_CONDITION,
    E_END = 0xff
  };

private:
  element_id id; // unique id
  Kind kind = E_UNKNOWN;
  element_id parent_id = ERROR_ID;

public:
  Element() : id(ERROR_ID) {}

  Element(const element_id id, const Kind kind = E_UNKNOWN,
          const element_id parent_id = ERROR_ID)
      : id(id), kind(kind), parent_id(parent_id) {}

  Element(const Element &e) : id(e.id), kind(e.kind), parent_id(e.parent_id) {}

  Element &operator=(const Element &e) {
    if (&e == this)
      return *this;
    id = e.id;
    kind = e.kind;
    parent_id = e.parent_id;
    return *this;
  }

  virtual ~Element() = default;

  std::shared_ptr<Element> ptr() { return std::shared_ptr<Element>(this); }

  Kind getKind() { return kind; }

  bool isKind(const Kind _kind) { return _kind == kind; }

  bool hasParent() const { return parent_id != ERROR_ID; }

  element_id getParentId() const { return parent_id; }

  element_id getId() const { return id; }

  virtual std::string toString() {
    std::ostringstream oss;
    oss << "<Element " << id << "...>";
    return oss.str();
  }

  //
  // Map utils if the Element is stored as a key in ordered containers
  //
  struct element_hash {
    size_t operator()(const std::shared_ptr<Element> &e) const { return e->id; }

    size_t operator()(const Element &e) const { return e.id; }
  };

  struct element_equal {
    bool operator()(const std::shared_ptr<Element> &a,
                    const std::shared_ptr<Element> &b) const {
      return a->id == b->id;
    }

    bool operator()(const Element &a, const Element &b) const {
      return a.id == b.id;
    }
  };

  //
  // Serialization utils
  //
  template <class Archive> void serialize(Archive &archive) {
    archive(id, kind, parent_id);
  }
};

static const char *kindName(const Element::Kind kind) {
  switch (kind) {
  case Element::E_SOURCE:
    return "source";
  case Element::E_FUNCTION:
    return "function";
  case Element::E_BLOCK:
    return "block";
  case Element::E_SUMMARY:
    return "goal";
  default:
    return "unknown";
  }
}

struct SourceElement : public Element {
  std::string path;
  std::list<element_id> functions;

  SourceElement() : Element() {}

  SourceElement(const element_id id) : Element(id, Element::E_SOURCE) {}

  SourceElement(const SourceElement &s)
      : Element(s), path(s.path), functions(s.functions) {}

  SourceElement &operator=(const SourceElement &s) {
    if (&s == this)
      return *this;
    Element::operator=(s);
    path = s.path;
    functions = s.functions;
    return *this;
  }

  ~SourceElement() = default;

  std::string toString() {
    std::ostringstream oss;
    oss << "<SourceElement " << path << "...>";
    return oss.str();
  }

  //
  // Serialization utils
  //
  template <class Archive> void serialize(Archive &archive) {
    archive(cereal::base_class<Element>(this), path, functions);
  }
};

struct FunctionElement : public Element {
  std::string name;
  std::string signature;
  std::string mangled_name;

  uint16_t num_formals;
  std::vector<element_id> blocks;

  FunctionElement() : Element() {}

  FunctionElement(const element_id id, const element_id source_id)
      : Element(id, Element::E_FUNCTION, source_id) {}

  FunctionElement(const FunctionElement &f)
      : Element(f), name(f.name), signature(f.signature),
        mangled_name(f.mangled_name), num_formals(f.num_formals),
        blocks(f.blocks) {}

  FunctionElement &operator=(const FunctionElement &f) {
    if (&f == this)
      return *this;
    Element::operator=(f);
    name = f.name;
    signature = f.signature;
    mangled_name = f.mangled_name;
    num_formals = f.num_formals;
    blocks = f.blocks;
    return *this;
  }

  ~FunctionElement() = default;

  element_id getSourceId() const { return getParentId(); }

  std::string toString() {
    std::ostringstream oss;
    oss << "<FunctionElement " << name << " #b=" << blocks.size() << "...>";
    return oss.str();
  }

  //
  // Serialization utils
  //
  template <class Archive> void serialize(Archive &archive) {
    archive(cereal::base_class<Element>(this), name, signature, mangled_name,
            num_formals, blocks);
  }
};

struct BlockElement : public Element {
  uint32_t internal_block_id; // internal block id in the CFG
  std::vector<element_id> predecessor_ids;
  std::vector<element_id> summaries;
  std::vector<element_id> condition_literals;

  BlockElement() : Element() {}

  BlockElement(const element_id id, const element_id source_id)
      : Element(id, Element::E_BLOCK, source_id) {}

  BlockElement(const BlockElement &b)
      : Element(b), internal_block_id(b.internal_block_id),
        predecessor_ids(b.predecessor_ids), summaries(b.summaries),
        condition_literals(b.condition_literals) {}

  BlockElement &operator=(const BlockElement &b) {
    if (&b == this)
      return *this;
    Element::operator=(b);
    internal_block_id = b.internal_block_id;
    predecessor_ids = b.predecessor_ids;
    summaries = b.summaries;
    condition_literals = b.condition_literals;
    return *this;
  }

  ~BlockElement() = default;

  element_id getFunctionId() const { return getParentId(); }

  std::string toString() {
    std::ostringstream oss;
    oss << "<BlockElement " << internal_block_id << "...>";
    return oss.str();
  }

  //
  // Serialization utils
  //
  template <class Archive> void serialize(Archive &archive) {
    archive(cereal::base_class<Element>(this), internal_block_id,
            predecessor_ids, summaries, condition_literals);
  }
};

// Type info for the summary
enum TypeKind { TY_INTEGER = 1, TY_BUFFER, TY_STRUCT, TY_UNKNOWN };

static const char *typeName(const TypeKind type) {
  switch (type) {
  case TY_INTEGER:
    return "TY_INTEGER";
  case TY_BUFFER:
    return "TY_BUFFER";
  case TY_STRUCT:
    return "TY_STRUCT";
  case TY_UNKNOWN:
    return "TY_UNKNOWN";
  default:
    return "TY_UNKNOWN";
  }
}

// Type of operation in the summary
enum Operator {
  OP_BUFFER_READ = 1,
  OP_BUFFER_WRITE,
  OP_BUFFER_READ_WRITE,
  OP_BUFFER_UNKNOWN,
  OP_INTEGER_MAY_OVERFLOW,
  OP_INTEGER_UNKNOWN,
  OP_CAST_UNSAFE,
  OP_CAST_UNKNOWN,
  OP_PASS_THROUGH
};

static const char *operatorName(const Operator op) {
  switch (op) {
  case OP_BUFFER_READ:
    return "OP_BUFFER_READ";
  case OP_BUFFER_WRITE:
    return "OP_BUFFER_WRITE";
  case OP_BUFFER_READ_WRITE:
    return "OP_BUFFER_READ_WRITE";
  case OP_BUFFER_UNKNOWN:
    return "OP_BUFFER_UNKNOWN";
  case OP_INTEGER_MAY_OVERFLOW:
    return "OP_INTEGER_MAY_OVERFLOW";
  case OP_INTEGER_UNKNOWN:
    return "OP_INTEGER_UNKNOWN";
  case OP_CAST_UNSAFE:
    return "OP_CAST_UNSAFE";
  case OP_CAST_UNKNOWN:
    return "OP_CAST_UNKNOWN";
  case OP_PASS_THROUGH:
    return "OP_PASS_THROUGH";
  default:
    return "OP_UNKNOWN";
  }
}

struct SummaryElement : public Element {
  Operator op;
  TypeKind type_kind;

  SummaryElement() : Element() {}

  SummaryElement(const element_id id, const element_id block_id)
      : Element(id, Element::E_SUMMARY, block_id) {}

  SummaryElement(const SummaryElement &b)
      : Element(b), op(b.op), type_kind(b.type_kind) {}

  SummaryElement &operator=(const SummaryElement &b) {
    if (&b == this)
      return *this;
    Element::operator=(b);
    op = b.op;
    type_kind = b.type_kind;
    return *this;
  }

  ~SummaryElement() = default;

  // Returns the element_id that points to the BBL
  element_id getBlockId() const { return getParentId(); }

  std::string toString() {
    std::ostringstream oss;
    oss << "<SummaryElement " << typeName(type_kind) << " " << operatorName(op)
        << " block_element_id#" << getBlockId() << ">";
    return oss.str();
  }

  //
  // Serialization utils
  //
  template <class Archive> void serialize(Archive &archive) {
    archive(cereal::base_class<Element>(this), op, type_kind);
  }
};

struct ConditionElement : public Element {
  std::vector<std::string> string_literals; // XXX encode different types

  ConditionElement() : Element() {}

  ConditionElement(const element_id id, const element_id block_id)
      : Element(id, Element::E_CONDITION, block_id) {}

  ConditionElement(const ConditionElement &b)
      : Element(b), string_literals(b.string_literals) {}

  ConditionElement &operator=(const ConditionElement &b) {
    if (&b == this)
      return *this;
    Element::operator=(b);
    string_literals = b.string_literals;
    return *this;
  }

  ~ConditionElement() = default;

  // Returns the element_id that points to the BBL
  element_id getBlockId() const { return getParentId(); }

  std::string toString() {
    std::ostringstream oss;
    oss << "<ConditionElement [";
    for (auto &lit : string_literals) {
      oss << "\"" << lit << "\",";
    }
    oss << "]>";
    return oss.str();
  }

  //
  // Serialization utils
  //
  template <class Archive> void serialize(Archive &archive) {
    archive(cereal::base_class<Element>(this), string_literals);
  }
};
}

#endif
