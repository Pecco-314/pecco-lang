#pragma once

#include "ast.hpp"
#include "scope.hpp"
#include <string>
#include <vector>

namespace pecco {

struct ScopeError {
  std::string message;
  size_t line;
  size_t column;
};

// Scope checker: checks variable scoping and detects unimplemented features
class ScopeChecker {
public:
  ScopeChecker() = default;

  // Check scopes for all statements
  bool check(const std::vector<StmtPtr> &stmts, ScopedSymbolTable &symbols);

  // Check if there are errors
  bool has_errors() const { return !errors_.empty(); }

  // Get errors
  const std::vector<ScopeError> &errors() const { return errors_; }

private:
  // Check a single statement
  void check_stmt(const Stmt *stmt, ScopedSymbolTable &symbols);

  // Check function body
  void check_func(const FuncStmt *func, ScopedSymbolTable &symbols);

  // Check block
  void check_block(const BlockStmt *block, ScopedSymbolTable &symbols);

  // Check let statement
  void check_let(const LetStmt *let, ScopedSymbolTable &symbols);

  // Check expression (stub for now)
  void check_expr(const Expr *expr, ScopedSymbolTable &symbols);

  // Report error
  void error(const std::string &message, size_t line, size_t column);

  std::vector<ScopeError> errors_;
};

} // namespace pecco
