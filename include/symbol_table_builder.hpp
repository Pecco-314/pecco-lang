#pragma once

#include "ast.hpp"
#include "scope.hpp"
#include <string>
#include <vector>

namespace pecco {

// Builds a hierarchical symbol table from AST
// This is Phase 1 of semantic analysis: collects all symbols (functions,
// operators, variables) across all scopes and validates declarations
class SymbolTableBuilder {
public:
  SymbolTableBuilder() = default;

  // Collect declarations from a list of statements
  // Returns true on success, false if there were errors
  bool collect(const std::vector<StmtPtr> &stmts, ScopedSymbolTable &symbols);

  // Load prelude file and collect its declarations (marked as prelude origin)
  // Returns true on success, false if there were errors
  bool load_prelude(const std::string &prelude_path,
                    ScopedSymbolTable &symbols);

  // Check if there were any errors
  bool has_errors() const { return !errors_.empty(); }

  struct Error {
    std::string message;
    size_t line;
    size_t column;
  };

  const std::vector<Error> &errors() const { return errors_; }

private:
  void process_stmt(const Stmt *stmt, ScopedSymbolTable &symbols);
  void process_func_decl(const FuncStmt *func, ScopedSymbolTable &symbols);
  void process_operator_decl(const OperatorDeclStmt *op,
                             ScopedSymbolTable &symbols);
  void process_let(const LetStmt *let, ScopedSymbolTable &symbols);
  void process_block(const BlockStmt *block, ScopedSymbolTable &symbols,
                     int block_num);
  std::string get_type_name(const Type *type) const;

  void error(const std::string &message, size_t line = 0, size_t column = 0);

  std::vector<Error> errors_;
  bool collecting_prelude_ = false; // Track if we're loading prelude
  int next_block_num_ = 0;          // For generating block descriptions
};

} // namespace pecco
