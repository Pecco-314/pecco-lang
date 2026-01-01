#pragma once

#include "ast.hpp"
#include "lexer.hpp"
#include <string>
#include <vector>

namespace pecco {

class Parser {
public:
  explicit Parser(std::vector<Token> tokens);

  // Parse a complete program (list of statements)
  std::vector<StmtPtr> parse_program();

  // Check if there were any parse errors
  bool has_errors() const { return !errors_.empty(); }

  struct ParseError {
    std::string message;
    size_t line;
    size_t column;
    size_t end_column; // For range highlighting
  };

  const std::vector<ParseError> &errors() const { return errors_; }

private:
  // Statement parsing
  StmtPtr parse_stmt();
  StmtPtr parse_let_stmt();
  StmtPtr parse_func_stmt();
  StmtPtr parse_operator_decl(); // Parse operator declaration
  StmtPtr parse_if_stmt();
  StmtPtr parse_return_stmt();
  StmtPtr parse_while_stmt();
  StmtPtr parse_block_stmt();
  StmtPtr parse_expr_stmt();

  // Expression parsing (flat sequence, no precedence)
  ExprPtr parse_expr();
  ExprPtr parse_primary_expr();
  ExprPtr parse_call_expr(ExprPtr callee);

  // Type parsing
  std::optional<TypePtr> parse_type_annotation();

  // Helper functions
  Token peek() const;
  Token advance();
  bool check(TokenKind kind) const;
  bool match(TokenKind kind);
  bool expect(TokenKind kind, const std::string &message);
  bool expect_token(TokenKind kind, const std::string &lexeme,
                    const std::string &message);
  bool expect_keyword(const std::string &keyword, const std::string &message);
  bool at_end() const;
  bool
  can_start_primary() const; // Check if current token can start a primary expr

  void error(const std::string &message);
  void error_at_previous_end(
      const std::string &message); // Error after previous token
  void synchronize();

  // Convert token to source location
  SourceLocation token_loc(const Token &tok) const {
    return SourceLocation(tok.line, tok.column, tok.end_column);
  }

  std::vector<Token> tokens_;
  std::size_t current_;
  std::vector<ParseError> errors_;
};

} // namespace pecco
