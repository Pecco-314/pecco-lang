#pragma once

#include "ast.hpp"
#include "symbol_table.hpp"
#include <string>
#include <vector>

namespace pecco {

// Resolves operator sequences in AST into proper expression trees
// Uses symbol table to look up operator precedence and associativity
class OperatorResolver {
public:
  // Resolve operators in an expression
  // Returns the resolved expression, or nullptr on error
  static ExprPtr resolve_expr(ExprPtr expr, const SymbolTable &symbol_table,
                              std::vector<std::string> &errors);

  // Resolve operators in a statement (recursively processes all expressions)
  static void resolve_stmt(Stmt *stmt, const SymbolTable &symbol_table,
                           std::vector<std::string> &errors);

private:
  OperatorResolver() = delete; // Static class, no instantiation

  static ExprPtr resolve_operator_seq(const OperatorSeqExpr *seq,
                                      const SymbolTable &symbol_table,
                                      std::vector<std::string> &errors);

  static ExprPtr build_infix_tree(std::vector<ExprPtr> &operands,
                                  std::vector<std::string> &operators,
                                  std::vector<int> &precedences,
                                  std::vector<Associativity> &assocs,
                                  std::vector<SourceLocation> &locations,
                                  size_t start, size_t end,
                                  std::vector<std::string> &errors);

  static void error(const std::string &message, size_t line, size_t column,
                    std::vector<std::string> &errors);
};

} // namespace pecco
