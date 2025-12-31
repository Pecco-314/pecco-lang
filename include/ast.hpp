#pragma once

#include "operator.hpp"
#include <memory>
#include <optional>
#include <string>
#include <vector>

namespace pecco {

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

  explicit Type(std::string name)
      : kind(TypeKind::Named), name(std::move(name)) {}
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
  virtual ~Expr() = default;

protected:
  explicit Expr(ExprKind kind) : kind(kind) {}
};

struct IntLiteralExpr : public Expr {
  std::string value; // store as string, parse later

  explicit IntLiteralExpr(std::string value)
      : Expr(ExprKind::IntLiteral), value(std::move(value)) {}
};

struct FloatLiteralExpr : public Expr {
  std::string value;

  explicit FloatLiteralExpr(std::string value)
      : Expr(ExprKind::FloatLiteral), value(std::move(value)) {}
};

struct StringLiteralExpr : public Expr {
  std::string value;

  explicit StringLiteralExpr(std::string value)
      : Expr(ExprKind::StringLiteral), value(std::move(value)) {}
};

struct BoolLiteralExpr : public Expr {
  bool value;

  explicit BoolLiteralExpr(bool value)
      : Expr(ExprKind::BoolLiteral), value(value) {}
};

struct IdentifierExpr : public Expr {
  std::string name;

  explicit IdentifierExpr(std::string name)
      : Expr(ExprKind::Identifier), name(std::move(name)) {}
};

// Binary operation (infix operators)
struct BinaryExpr : public Expr {
  std::string op;      // Operator symbol
  ExprPtr left;        // Left operand
  ExprPtr right;       // Right operand
  OpPosition position; // Always Infix for binary

  BinaryExpr(std::string op, ExprPtr left, ExprPtr right)
      : Expr(ExprKind::Binary), op(std::move(op)), left(std::move(left)),
        right(std::move(right)), position(OpPosition::Infix) {}
};

// Unary operation (prefix/postfix operators)
struct UnaryExpr : public Expr {
  std::string op;      // Operator symbol
  ExprPtr operand;     // Operand
  OpPosition position; // Prefix or Postfix

  UnaryExpr(std::string op, ExprPtr operand, OpPosition position)
      : Expr(ExprKind::Unary), op(std::move(op)), operand(std::move(operand)),
        position(position) {}
};

// Represents a sequence of operands and operators (no precedence resolution)
// Example: "a + b * c" -> operands=[a, b, c], operators=["+", "*"]
struct OperatorSeqExpr : public Expr {
  std::vector<ExprPtr>
      operands; // Primary expressions (literals, identifiers, calls)
  std::vector<std::string> operators; // Operators between operands

  OperatorSeqExpr(std::vector<ExprPtr> operands,
                  std::vector<std::string> operators)
      : Expr(ExprKind::OperatorSeq), operands(std::move(operands)),
        operators(std::move(operators)) {}
};

struct CallExpr : public Expr {
  ExprPtr callee;
  std::vector<ExprPtr> args;

  CallExpr(ExprPtr callee, std::vector<ExprPtr> args)
      : Expr(ExprKind::Call), callee(std::move(callee)), args(std::move(args)) {
  }
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
  virtual ~Stmt() = default;

protected:
  explicit Stmt(StmtKind kind) : kind(kind) {}
};

struct LetStmt : public Stmt {
  std::string name;
  std::optional<TypePtr> type;
  ExprPtr init;

  LetStmt(std::string name, std::optional<TypePtr> type, ExprPtr init)
      : Stmt(StmtKind::Let), name(std::move(name)), type(std::move(type)),
        init(std::move(init)) {}
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
           std::optional<TypePtr> return_type, std::optional<StmtPtr> body)
      : Stmt(StmtKind::Func), name(std::move(name)), params(std::move(params)),
        return_type(std::move(return_type)), body(std::move(body)) {}
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
                   Associativity assoc, std::optional<StmtPtr> body)
      : Stmt(StmtKind::OperatorDecl), op(std::move(op)), position(position),
        params(std::move(params)), return_type(std::move(return_type)),
        precedence(precedence), assoc(assoc), body(std::move(body)) {}
};

struct IfStmt : public Stmt {
  ExprPtr condition;
  StmtPtr then_branch;
  std::optional<StmtPtr> else_branch;

  IfStmt(ExprPtr condition, StmtPtr then_branch,
         std::optional<StmtPtr> else_branch)
      : Stmt(StmtKind::If), condition(std::move(condition)),
        then_branch(std::move(then_branch)),
        else_branch(std::move(else_branch)) {}
};

struct ReturnStmt : public Stmt {
  std::optional<ExprPtr> value;

  explicit ReturnStmt(std::optional<ExprPtr> value)
      : Stmt(StmtKind::Return), value(std::move(value)) {}
};

struct WhileStmt : public Stmt {
  ExprPtr condition;
  StmtPtr body;

  WhileStmt(ExprPtr condition, StmtPtr body)
      : Stmt(StmtKind::While), condition(std::move(condition)),
        body(std::move(body)) {}
};

struct ExprStmt : public Stmt {
  ExprPtr expr;

  explicit ExprStmt(ExprPtr expr)
      : Stmt(StmtKind::Expr), expr(std::move(expr)) {}
};

struct BlockStmt : public Stmt {
  std::vector<StmtPtr> stmts;

  explicit BlockStmt(std::vector<StmtPtr> stmts)
      : Stmt(StmtKind::Block), stmts(std::move(stmts)) {}
};

} // namespace pecco
