#pragma once

#include "operator.hpp"
#include <memory>
#include <optional>
#include <ostream>
#include <string>
#include <vector>

namespace pecco {

// Source location information
struct SourceLocation {
  size_t line{0};       // 1-based line number (0 means unknown)
  size_t column{0};     // 1-based column number (0 means unknown)
  size_t end_column{0}; // 1-based end column (0 means unknown)

  SourceLocation() = default;
  SourceLocation(size_t line, size_t column, size_t end_column = 0)
      : line(line), column(column), end_column(end_column) {}
};

// Forward declarations
struct Expr;
struct Stmt;
struct Type;

using ExprPtr = std::unique_ptr<Expr>;
using StmtPtr = std::unique_ptr<Stmt>;
using TypePtr = std::unique_ptr<Type>;

// ===== Type =====

enum class TypeKind {
  Named, // i32, f64, bool, etc.
};

struct Type {
  TypeKind kind;
  std::string name; // for Named types
  SourceLocation loc;

  explicit Type(std::string name, SourceLocation loc = SourceLocation())
      : kind(TypeKind::Named), name(std::move(name)), loc(loc) {}

  void print(std::ostream &os) const;
};

// ===== Expression =====

enum class ExprKind {
  IntLiteral,
  FloatLiteral,
  StringLiteral,
  BoolLiteral,
  Identifier,
  Binary,      // Binary operation (infix)
  Unary,       // Unary operation (prefix/postfix)
  OperatorSeq, // Sequence of operands and operators (not yet resolved)
  Call,
};

struct Expr {
  ExprKind kind;
  SourceLocation loc;

  virtual ~Expr() = default;

  // Print expression to stream
  virtual void print(std::ostream &os) const = 0;

protected:
  explicit Expr(ExprKind kind, SourceLocation loc = SourceLocation())
      : kind(kind), loc(loc) {}
};

struct IntLiteralExpr : public Expr {
  std::string value; // store as string, parse later

  explicit IntLiteralExpr(std::string value,
                          SourceLocation loc = SourceLocation())
      : Expr(ExprKind::IntLiteral, loc), value(std::move(value)) {}

  void print(std::ostream &os) const override;
};

struct FloatLiteralExpr : public Expr {
  std::string value;

  explicit FloatLiteralExpr(std::string value,
                            SourceLocation loc = SourceLocation())
      : Expr(ExprKind::FloatLiteral, loc), value(std::move(value)) {}

  void print(std::ostream &os) const override;
};

struct StringLiteralExpr : public Expr {
  std::string value;

  explicit StringLiteralExpr(std::string value,
                             SourceLocation loc = SourceLocation())
      : Expr(ExprKind::StringLiteral, loc), value(std::move(value)) {}

  void print(std::ostream &os) const override;
};

struct BoolLiteralExpr : public Expr {
  bool value;

  explicit BoolLiteralExpr(bool value, SourceLocation loc = SourceLocation())
      : Expr(ExprKind::BoolLiteral, loc), value(value) {}

  void print(std::ostream &os) const override;
};

struct IdentifierExpr : public Expr {
  std::string name;

  explicit IdentifierExpr(std::string name,
                          SourceLocation loc = SourceLocation())
      : Expr(ExprKind::Identifier, loc), name(std::move(name)) {}

  void print(std::ostream &os) const override;
};

// Binary operation (infix operators)
struct BinaryExpr : public Expr {
  std::string op;      // Operator symbol
  ExprPtr left;        // Left operand
  ExprPtr right;       // Right operand
  OpPosition position; // Always Infix for binary

  BinaryExpr(std::string op, ExprPtr left, ExprPtr right,
             SourceLocation loc = SourceLocation())
      : Expr(ExprKind::Binary, loc), op(std::move(op)), left(std::move(left)),
        right(std::move(right)), position(OpPosition::Infix) {}

  void print(std::ostream &os) const override;
};

// Unary operation (prefix/postfix operators)
struct UnaryExpr : public Expr {
  std::string op;      // Operator symbol
  ExprPtr operand;     // Operand
  OpPosition position; // Prefix or Postfix

  UnaryExpr(std::string op, ExprPtr operand, OpPosition position,
            SourceLocation loc = SourceLocation())
      : Expr(ExprKind::Unary, loc), op(std::move(op)),
        operand(std::move(operand)), position(position) {}

  void print(std::ostream &os) const override;
};

// Item in operator sequence: either an operator or an operand
struct OpSeqItem {
  enum class Kind { Operator, Operand };

  Kind kind;
  std::string op;     // For operator
  ExprPtr operand;    // For operand
  SourceLocation loc; // Location of this item

  // Constructor for operator
  explicit OpSeqItem(std::string op, SourceLocation loc = SourceLocation())
      : kind(Kind::Operator), op(std::move(op)), operand(nullptr), loc(loc) {}

  // Constructor for operand
  explicit OpSeqItem(ExprPtr operand)
      : kind(Kind::Operand), op(""), operand(std::move(operand)),
        loc(operand ? operand->loc : SourceLocation()) {}

  // Move constructor
  OpSeqItem(OpSeqItem &&other) noexcept
      : kind(other.kind), op(std::move(other.op)),
        operand(std::move(other.operand)), loc(other.loc) {}

  // Move assignment
  OpSeqItem &operator=(OpSeqItem &&other) noexcept {
    if (this != &other) {
      kind = other.kind;
      op = std::move(other.op);
      operand = std::move(other.operand);
      loc = other.loc;
    }
    return *this;
  }

  // Delete copy
  OpSeqItem(const OpSeqItem &) = delete;
  OpSeqItem &operator=(const OpSeqItem &) = delete;
};

// Represents a sequence of operands and operators (no precedence resolution)
// Example: "-5" -> items=[Operator(-), Operand(5)]
//          "x++" -> items=[Operand(x), Operator(++)]
//          "a + b * c" -> items=[Operand(a), Operator(+), Operand(b),
//          Operator(*), Operand(c)]
struct OperatorSeqExpr : public Expr {
  std::vector<OpSeqItem> items; // Sequence of operators and operands

  explicit OperatorSeqExpr(std::vector<OpSeqItem> items,
                           SourceLocation loc = SourceLocation())
      : Expr(ExprKind::OperatorSeq, loc), items(std::move(items)) {}

  void print(std::ostream &os) const override;
};

struct CallExpr : public Expr {
  ExprPtr callee;
  std::vector<ExprPtr> args;

  CallExpr(ExprPtr callee, std::vector<ExprPtr> args,
           SourceLocation loc = SourceLocation())
      : Expr(ExprKind::Call, loc), callee(std::move(callee)),
        args(std::move(args)) {}

  void print(std::ostream &os) const override;
};

// ===== Statement =====

enum class StmtKind {
  Let,
  Func,
  OperatorDecl,
  If,
  Return,
  While,
  Expr,
  Block,
};

struct Stmt {
  StmtKind kind;
  SourceLocation loc;

  virtual ~Stmt() = default;

  // Print statement to stream
  virtual void print(std::ostream &os, int indent = 0) const = 0;

protected:
  explicit Stmt(StmtKind kind, SourceLocation loc = SourceLocation())
      : kind(kind), loc(loc) {}
};

struct LetStmt : public Stmt {
  std::string name;
  std::optional<TypePtr> type;
  ExprPtr init;

  LetStmt(std::string name, std::optional<TypePtr> type, ExprPtr init,
          SourceLocation loc = SourceLocation())
      : Stmt(StmtKind::Let, loc), name(std::move(name)), type(std::move(type)),
        init(std::move(init)) {}

  void print(std::ostream &os, int indent = 0) const override;
};

struct Parameter {
  std::string name;
  std::optional<TypePtr> type;

  Parameter(std::string name, std::optional<TypePtr> type)
      : name(std::move(name)), type(std::move(type)) {}
};

struct FuncStmt : public Stmt {
  std::string name;
  std::vector<Parameter> params;
  std::optional<TypePtr> return_type;
  std::optional<StmtPtr> body; // Optional body for declarations

  FuncStmt(std::string name, std::vector<Parameter> params,
           std::optional<TypePtr> return_type, std::optional<StmtPtr> body,
           SourceLocation loc = SourceLocation())
      : Stmt(StmtKind::Func, loc), name(std::move(name)),
        params(std::move(params)), return_type(std::move(return_type)),
        body(std::move(body)) {}

  void print(std::ostream &os, int indent = 0) const override;
};

struct OperatorDeclStmt : public Stmt {
  std::string op;                     // Operator symbol
  OpPosition position;                // Prefix/Infix/Postfix
  std::vector<Parameter> params;      // Parameters
  std::optional<TypePtr> return_type; // Return type
  int precedence;                     // Precedence level
  Associativity assoc;                // Associativity
  std::optional<StmtPtr> body;        // Optional body (for definition)

  OperatorDeclStmt(std::string op, OpPosition position,
                   std::vector<Parameter> params,
                   std::optional<TypePtr> return_type, int precedence,
                   Associativity assoc, std::optional<StmtPtr> body,
                   SourceLocation loc = SourceLocation())
      : Stmt(StmtKind::OperatorDecl, loc), op(std::move(op)),
        position(position), params(std::move(params)),
        return_type(std::move(return_type)), precedence(precedence),
        assoc(assoc), body(std::move(body)) {}

  void print(std::ostream &os, int indent = 0) const override;
};

struct IfStmt : public Stmt {
  ExprPtr condition;
  StmtPtr then_branch;
  std::optional<StmtPtr> else_branch;

  IfStmt(ExprPtr condition, StmtPtr then_branch,
         std::optional<StmtPtr> else_branch,
         SourceLocation loc = SourceLocation())
      : Stmt(StmtKind::If, loc), condition(std::move(condition)),
        then_branch(std::move(then_branch)),
        else_branch(std::move(else_branch)) {}

  void print(std::ostream &os, int indent = 0) const override;
};

struct ReturnStmt : public Stmt {
  std::optional<ExprPtr> value;

  explicit ReturnStmt(std::optional<ExprPtr> value,
                      SourceLocation loc = SourceLocation())
      : Stmt(StmtKind::Return, loc), value(std::move(value)) {}

  void print(std::ostream &os, int indent = 0) const override;
};

struct WhileStmt : public Stmt {
  ExprPtr condition;
  StmtPtr body;

  WhileStmt(ExprPtr condition, StmtPtr body,
            SourceLocation loc = SourceLocation())
      : Stmt(StmtKind::While, loc), condition(std::move(condition)),
        body(std::move(body)) {}

  void print(std::ostream &os, int indent = 0) const override;
};

struct ExprStmt : public Stmt {
  ExprPtr expr;

  explicit ExprStmt(ExprPtr expr, SourceLocation loc = SourceLocation())
      : Stmt(StmtKind::Expr, loc), expr(std::move(expr)) {}

  void print(std::ostream &os, int indent = 0) const override;
};

struct BlockStmt : public Stmt {
  std::vector<StmtPtr> stmts;

  explicit BlockStmt(std::vector<StmtPtr> stmts,
                     SourceLocation loc = SourceLocation())
      : Stmt(StmtKind::Block, loc), stmts(std::move(stmts)) {}

  void print(std::ostream &os, int indent = 0) const override;
};

} // namespace pecco
