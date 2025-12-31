#pragma once

#include "ast.hpp"
#include "symbol_table.hpp"
#include <string>
#include <vector>

namespace pecco {

// Semantic analyzer - performs type checking and semantic analysis
class SemanticAnalyzer {
public:
  SemanticAnalyzer() = default;

  // First pass: collect all function and operator declarations
  void collect_declarations(const std::vector<StmtPtr> &stmts);

  // Load prelude (built-in operators and functions)
  bool load_prelude(const std::string &prelude_path);

  // Get the symbol table
  const SymbolTable &symbol_table() const { return symbol_table_; }

  // Transform operator sequences to expression trees
  ExprPtr resolve_operators(ExprPtr expr);

  // Check if there were any errors
  bool has_errors() const { return !errors_.empty(); }

  struct SemanticError {
    std::string message;
    size_t line;
    size_t column;
  };

  const std::vector<SemanticError> &errors() const { return errors_; }

private:
  // Process a single statement (declaration collection)
  void process_decl(const Stmt *stmt);

  // Process function declaration
  void process_func_decl(const FuncStmt *func);

  // Process operator declaration
  void process_operator_decl(const OperatorDeclStmt *op);

  // Helper: extract type name from Type
  std::string get_type_name(const Type *type) const;

  // Operator sequence resolution helpers
  ExprPtr resolve_operator_seq(const OperatorSeqExpr *seq);
  ExprPtr build_expr_tree(std::vector<ExprPtr> &operands,
                          std::vector<std::string> &operators, size_t start,
                          size_t end, int min_prec);

  // Report error
  void error(const std::string &message, size_t line = 0, size_t column = 0);

  SymbolTable symbol_table_;
  std::vector<SemanticError> errors_;
};

} // namespace pecco
