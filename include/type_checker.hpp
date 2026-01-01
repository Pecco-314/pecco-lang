#pragma once

#include "ast.hpp"
#include "scope.hpp"
#include <string>
#include <vector>

namespace pecco {

class TypeChecker {
public:
  struct Error {
    std::string message;
    size_t line;
    size_t column;

    Error(std::string msg, size_t l = 0, size_t c = 0)
        : message(std::move(msg)), line(l), column(c) {}
  };

  TypeChecker() = default;

  // Check types for all statements and infer expression types
  bool check(std::vector<StmtPtr> &stmts, const ScopedSymbolTable &symbols);

  bool has_errors() const { return !errors_.empty(); }
  const std::vector<Error> &errors() const { return errors_; }

private:
  std::vector<Error> errors_;
  const ScopedSymbolTable *symbols_ = nullptr;

  // Track variable types in current scope chain
  // Map from variable name to type
  std::vector<std::map<std::string, std::string>> scope_stack_;

  void error(const std::string &msg, size_t line = 0, size_t column = 0);

  // Scope management
  void push_scope();
  void pop_scope();
  void add_variable_type(const std::string &name, const std::string &type);
  std::string lookup_variable_type(const std::string &name) const;

  // Check statement types
  void check_stmt(Stmt *stmt);

  // Check and infer expression types, returns inferred type
  std::string check_expr(Expr *expr);

  // Helper: get type name from Type AST node
  std::string get_type_name(const Type *type) const;
};

} // namespace pecco
