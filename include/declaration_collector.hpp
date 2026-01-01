#pragma once

#include "ast.hpp"
#include "symbol_table.hpp"
#include <string>
#include <vector>

namespace pecco {

// Collects function and operator declarations from AST and builds a symbol
// table
class DeclarationCollector {
public:
  DeclarationCollector() = default;

  // Collect declarations from a list of statements
  // Returns true on success, false if there were errors
  bool collect(const std::vector<StmtPtr> &stmts, SymbolTable &symbol_table);

  // Load prelude file and collect its declarations
  // Returns true on success, false if there were errors
  bool load_prelude(const std::string &prelude_path, SymbolTable &symbol_table);

  // Check if there were any errors
  bool has_errors() const { return !errors_.empty(); }

  struct Error {
    std::string message;
    size_t line;
    size_t column;
  };

  const std::vector<Error> &errors() const { return errors_; }

private:
  void process_stmt(const Stmt *stmt, SymbolTable &symbol_table);
  void process_func_decl(const FuncStmt *func, SymbolTable &symbol_table);
  void process_operator_decl(const OperatorDeclStmt *op,
                             SymbolTable &symbol_table);
  std::string get_type_name(const Type *type) const;

  void error(const std::string &message, size_t line = 0, size_t column = 0);

  std::vector<Error> errors_;
};

} // namespace pecco
