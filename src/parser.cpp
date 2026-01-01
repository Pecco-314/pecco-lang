#include "parser.hpp"
#include <sstream>

namespace pecco {

Parser::Parser(std::vector<Token> tokens)
    : tokens_(std::move(tokens)), current_(0) {}

std::vector<StmtPtr> Parser::parse_program() {
  std::vector<StmtPtr> stmts;
  while (!at_end()) {
    auto stmt = parse_stmt();
    if (stmt) {
      stmts.push_back(std::move(stmt));
    } else {
      synchronize();
    }
  }
  return stmts;
}

// ===== Statement Parsing =====

StmtPtr Parser::parse_stmt() {
  Token tok = peek();

  if (tok.kind == TokenKind::Keyword) {
    if (tok.lexeme == "let") {
      return parse_let_stmt();
    } else if (tok.lexeme == "func") {
      return parse_func_stmt();
    } else if (tok.lexeme == "operator") {
      return parse_operator_decl();
    } else if (tok.lexeme == "if") {
      return parse_if_stmt();
    } else if (tok.lexeme == "return") {
      return parse_return_stmt();
    } else if (tok.lexeme == "while") {
      return parse_while_stmt();
    }
  }

  if (check(TokenKind::Punctuation) && peek().lexeme == "{") {
    return parse_block_stmt();
  }

  return parse_expr_stmt();
}

StmtPtr Parser::parse_let_stmt() {
  Token start_tok = peek();
  advance(); // consume 'let'

  if (!check(TokenKind::Identifier)) {
    error("Expected identifier after 'let'");
    return nullptr;
  }
  std::string name = advance().lexeme;

  // Optional type annotation
  std::optional<TypePtr> type;
  if (check(TokenKind::Punctuation) && peek().lexeme == ":") {
    advance(); // consume ':'
    type = parse_type_annotation();
    if (!type) {
      error("Expected type after ':'");
      return nullptr;
    }
  }

  // Expect '='
  if (!check(TokenKind::Operator) || peek().lexeme != "=") {
    error("Expected '=' in let statement");
    return nullptr;
  }
  advance(); // consume '='

  auto init = parse_expr();
  if (!init) {
    error("Expected expression after '='");
    return nullptr;
  }

  // Expect ';'
  if (!expect_token(TokenKind::Punctuation, ";",
                    "Expected ';' after let statement")) {
    return nullptr;
  }

  return std::make_unique<LetStmt>(std::move(name), std::move(type),
                                   std::move(init), token_loc(start_tok));
}

StmtPtr Parser::parse_func_stmt() {
  Token start_tok = peek();
  advance(); // consume 'func'

  if (!check(TokenKind::Identifier)) {
    error("Expected function name after 'func'");
    return nullptr;
  }
  std::string name = advance().lexeme;

  // Expect '('
  if (!check(TokenKind::Punctuation) || peek().lexeme != "(") {
    error("Expected '(' after function name");
    return nullptr;
  }
  advance(); // consume '('

  // Parse parameters
  std::vector<Parameter> params;
  while (!check(TokenKind::Punctuation) || peek().lexeme != ")") {
    if (!params.empty()) {
      if (!check(TokenKind::Punctuation) || peek().lexeme != ",") {
        error("Expected ',' between parameters");
        return nullptr;
      }
      advance(); // consume ','
    }

    if (!check(TokenKind::Identifier)) {
      error("Expected parameter name");
      return nullptr;
    }
    Token param_token = advance();
    std::string param_name = param_token.lexeme;
    SourceLocation param_loc(param_token.line, param_token.column);

    // Optional type annotation
    std::optional<TypePtr> param_type;
    if (check(TokenKind::Punctuation) && peek().lexeme == ":") {
      advance(); // consume ':'
      param_type = parse_type_annotation();
      if (!param_type) {
        error("Expected type after ':'");
        return nullptr;
      }
    }

    params.emplace_back(std::move(param_name), std::move(param_type),
                        param_loc);
  }

  if (!check(TokenKind::Punctuation) || peek().lexeme != ")") {
    error("Expected ')' after parameters");
    return nullptr;
  }
  advance(); // consume ')'

  // Optional return type
  std::optional<TypePtr> return_type;
  if (check(TokenKind::Punctuation) && peek().lexeme == ":") {
    advance(); // consume ':'
    return_type = parse_type_annotation();
    if (!return_type) {
      error("Expected return type after ':'");
      return nullptr;
    }
  }

  // Function body or semicolon
  std::optional<StmtPtr> body;
  if (check(TokenKind::Punctuation) && peek().lexeme == "{") {
    // Function definition with body
    auto block = parse_block_stmt();
    if (!block) {
      error("Expected function body");
      return nullptr;
    }
    body = std::move(block);
  } else if (check(TokenKind::Punctuation) && peek().lexeme == ";") {
    // Function declaration without body
    advance(); // consume ';'
    // body remains nullopt
  } else {
    error("Expected '{' or ';' after function signature");
    return nullptr;
  }

  return std::make_unique<FuncStmt>(std::move(name), std::move(params),
                                    std::move(return_type), std::move(body),
                                    token_loc(start_tok));
}

StmtPtr Parser::parse_operator_decl() {
  // Expect 'operator' keyword (already consumed by parse_stmt)
  Token start_tok = peek();
  advance(); // consume 'operator'

  // Parse operator position: prefix/infix/postfix
  if (!check(TokenKind::Keyword)) {
    error("Expected operator position ('prefix', 'infix', or 'postfix')");
    return nullptr;
  }

  std::string position_str = advance().lexeme;
  OpPosition position;
  if (position_str == "prefix") {
    position = OpPosition::Prefix;
  } else if (position_str == "infix") {
    position = OpPosition::Infix;
  } else if (position_str == "postfix") {
    position = OpPosition::Postfix;
  } else {
    error("Expected 'prefix', 'infix', or 'postfix'");
    return nullptr;
  }

  // Expect operator symbol
  if (!check(TokenKind::Operator)) {
    error("Expected operator symbol");
    return nullptr;
  }
  std::string op = advance().lexeme;

  // Expect '('
  if (!check(TokenKind::Punctuation) || peek().lexeme != "(") {
    error("Expected '(' after operator symbol");
    return nullptr;
  }
  advance(); // consume '('

  // Parse parameters
  std::vector<Parameter> params;
  while (!check(TokenKind::Punctuation) || peek().lexeme != ")") {
    if (!params.empty()) {
      if (!check(TokenKind::Punctuation) || peek().lexeme != ",") {
        error("Expected ',' between parameters");
        return nullptr;
      }
      advance(); // consume ','
    }

    if (!check(TokenKind::Identifier)) {
      error("Expected parameter name");
      return nullptr;
    }
    Token param_token = advance();
    std::string param_name = param_token.lexeme;
    SourceLocation param_loc(param_token.line, param_token.column);

    // Require type annotation for operators
    if (!check(TokenKind::Punctuation) || peek().lexeme != ":") {
      error("Expected ':' after parameter name (generics unimplemented for "
            "operators)");
      return nullptr;
    }
    advance(); // consume ':'

    auto param_type = parse_type_annotation();
    if (!param_type) {
      error("Expected type after ':'");
      return nullptr;
    }

    params.emplace_back(std::move(param_name), std::move(param_type),
                        param_loc);
  }

  // Validate parameter count based on position
  if (position == OpPosition::Prefix && params.size() != 1) {
    error("Prefix operator must have exactly 1 parameter");
    return nullptr;
  }
  if (position == OpPosition::Infix && params.size() != 2) {
    error("Infix operator must have exactly 2 parameters");
    return nullptr;
  }
  if (position == OpPosition::Postfix && params.size() != 1) {
    error("Postfix operator must have exactly 1 parameter");
    return nullptr;
  }

  if (!check(TokenKind::Punctuation) || peek().lexeme != ")") {
    error("Expected ')' after parameters");
    return nullptr;
  }
  advance(); // consume ')'

  // Require return type
  if (!check(TokenKind::Punctuation) || peek().lexeme != ":") {
    error(
        "Expected ':' after parameters (generics unimplemented for operators)");
    return nullptr;
  }
  advance(); // consume ':'

  auto return_type = parse_type_annotation();
  if (!return_type) {
    error("Expected return type after ':'");
    return nullptr;
  }

  // Parse precedence and associativity (only for infix operators)
  int precedence = 0;
  Associativity assoc = Associativity::Left;

  if (position == OpPosition::Infix) {
    // Infix: require prec, optional assoc (default: left)
    // Expect 'prec' keyword
    if (!check(TokenKind::Keyword) || peek().lexeme != "prec") {
      error("Expected 'prec' keyword for infix operator");
      return nullptr;
    }
    advance(); // consume 'prec'

    // Expect precedence value (integer)
    if (!check(TokenKind::Integer)) {
      error("Expected integer precedence value");
      return nullptr;
    }
    precedence = std::stoi(advance().lexeme);

    // Optional associativity (default: left)
    if (check(TokenKind::Keyword)) {
      std::string assoc_str = peek().lexeme;
      if (assoc_str == "assoc_left") {
        advance();
        assoc = Associativity::Left;
      } else if (assoc_str == "assoc_right") {
        advance();
        assoc = Associativity::Right;
      }
      // If not assoc_left or assoc_right, don't consume and use default
    }
    // Default is already set to Associativity::Left above
  }
  // Prefix and postfix operators don't need precedence or associativity
  // They are handled by greedy algorithm in semantic analysis

  // Check for function body or semicolon
  std::optional<StmtPtr> body;
  if (check(TokenKind::Punctuation) && peek().lexeme == "{") {
    // Definition with body
    body = parse_block_stmt();
    if (!body) {
      error("Expected function body");
      return nullptr;
    }
  } else {
    // Declaration only - expect semicolon
    if (!expect_token(TokenKind::Punctuation, ";",
                      "Expected ';' after operator declaration")) {
      return nullptr;
    }
  }

  return std::make_unique<OperatorDeclStmt>(
      std::move(op), position, std::move(params), std::move(return_type),
      precedence, assoc, std::move(body), token_loc(start_tok));
}

StmtPtr Parser::parse_if_stmt() {
  Token start_tok = peek();
  advance(); // consume 'if'

  auto condition = parse_expr();
  if (!condition) {
    error("Expected condition after 'if'");
    return nullptr;
  }

  auto then_branch = parse_block_stmt();
  if (!then_branch) {
    error("Expected block after if condition");
    return nullptr;
  }

  std::optional<StmtPtr> else_branch;
  if (check(TokenKind::Keyword) && peek().lexeme == "else") {
    advance(); // consume 'else'

    // Check for 'else if'
    if (check(TokenKind::Keyword) && peek().lexeme == "if") {
      else_branch = parse_if_stmt();
    } else {
      else_branch = parse_block_stmt();
    }

    if (!else_branch) {
      error("Expected block or if statement after 'else'");
      return nullptr;
    }
  }

  return std::make_unique<IfStmt>(std::move(condition), std::move(then_branch),
                                  std::move(else_branch), token_loc(start_tok));
}

StmtPtr Parser::parse_return_stmt() {
  Token start_tok = peek();
  advance(); // consume 'return'

  std::optional<ExprPtr> value;
  // Check if there's an expression before the semicolon
  if (!check(TokenKind::Punctuation) || peek().lexeme != ";") {
    auto expr = parse_expr();
    if (!expr) {
      error("Expected expression or ';' after 'return'");
      return nullptr;
    }
    value = std::move(expr);
  }

  // Expect ';'
  if (!expect_token(TokenKind::Punctuation, ";",
                    "Expected ';' after return statement")) {
    return nullptr;
  }

  return std::make_unique<ReturnStmt>(std::move(value), token_loc(start_tok));
}

StmtPtr Parser::parse_while_stmt() {
  Token start_tok = peek();
  advance(); // consume 'while'

  auto condition = parse_expr();
  if (!condition) {
    error("Expected condition after 'while'");
    return nullptr;
  }

  auto body = parse_block_stmt();
  if (!body) {
    error("Expected block after while condition");
    return nullptr;
  }

  return std::make_unique<WhileStmt>(std::move(condition), std::move(body),
                                     token_loc(start_tok));
}

StmtPtr Parser::parse_block_stmt() {
  Token start_tok = peek();
  if (!check(TokenKind::Punctuation) || peek().lexeme != "{") {
    error("Expected '{'");
    return nullptr;
  }
  advance(); // consume '{'

  std::vector<StmtPtr> stmts;
  while (!at_end() &&
         (!check(TokenKind::Punctuation) || peek().lexeme != "}")) {
    auto stmt = parse_stmt();
    if (stmt) {
      stmts.push_back(std::move(stmt));
    } else {
      synchronize();
    }
  }

  if (!check(TokenKind::Punctuation) || peek().lexeme != "}") {
    error("Expected '}'");
    return nullptr;
  }
  advance(); // consume '}'

  return std::make_unique<BlockStmt>(std::move(stmts), token_loc(start_tok));
}

StmtPtr Parser::parse_expr_stmt() {
  auto expr = parse_expr();
  if (!expr) {
    error("Expected expression");
    return nullptr;
  }

  SourceLocation loc = expr->loc; // Save location before moving

  // Expect ';'
  if (!expect_token(TokenKind::Punctuation, ";",
                    "Expected ';' after expression")) {
    return nullptr;
  }

  return std::make_unique<ExprStmt>(std::move(expr), loc);
}

// ===== Expression Parsing =====

// ===== Expression Parsing =====

ExprPtr Parser::parse_expr() {
  Token start_tok = peek();

  // Parse expression as a flat sequence of operands and operators
  // We don't distinguish prefix/infix/postfix here - that's semantic analysis's
  // job Strategy: Alternately collect operators and primaries Rule: primaries
  // cannot be consecutive (must have operators between them)
  //       operators can be consecutive
  // This handles: -5++, -- ++x, 5 + 3, -5++ + --3--, etc.

  std::vector<OpSeqItem> items;

  bool last_was_primary = false;

  while (!at_end()) {
    if (check(TokenKind::Operator)) {
      // Collect operator
      Token op_tok = peek();
      items.push_back(OpSeqItem(op_tok.lexeme, token_loc(op_tok)));
      advance();
      last_was_primary = false;
    } else if (can_start_primary()) {
      // Check if we have two consecutive primaries (error)
      if (last_was_primary) {
        // Two primaries in a row - stop here
        break;
      }

      // Parse primary
      auto operand = parse_primary_expr();
      if (!operand) {
        return nullptr;
      }
      items.push_back(OpSeqItem(std::move(operand)));
      last_was_primary = true;
    } else {
      // Can't continue
      break;
    }
  }

  // Must have at least one item
  if (items.empty()) {
    error("Expected expression");
    return nullptr;
  }

  // If only one item and it's an operand, return it directly
  if (items.size() == 1 && items[0].kind == OpSeqItem::Kind::Operand) {
    return std::move(items[0].operand);
  }

  // Otherwise return an OperatorSeqExpr
  return std::make_unique<OperatorSeqExpr>(std::move(items),
                                           token_loc(start_tok));
}

ExprPtr Parser::parse_primary_expr() {
  Token tok = peek();

  // Literals
  if (tok.kind == TokenKind::Integer) {
    advance();
    return std::make_unique<IntLiteralExpr>(tok.lexeme, token_loc(tok));
  }

  if (tok.kind == TokenKind::Float) {
    advance();
    return std::make_unique<FloatLiteralExpr>(tok.lexeme, token_loc(tok));
  }

  if (tok.kind == TokenKind::String) {
    advance();
    return std::make_unique<StringLiteralExpr>(tok.lexeme, token_loc(tok));
  }

  if (tok.kind == TokenKind::Keyword) {
    if (tok.lexeme == "true") {
      advance();
      return std::make_unique<BoolLiteralExpr>(true, token_loc(tok));
    }
    if (tok.lexeme == "false") {
      advance();
      return std::make_unique<BoolLiteralExpr>(false, token_loc(tok));
    }
  }

  // Identifier
  if (tok.kind == TokenKind::Identifier) {
    advance();
    auto expr = std::make_unique<IdentifierExpr>(tok.lexeme, token_loc(tok));

    // Check for function call
    if (check(TokenKind::Punctuation) && peek().lexeme == "(") {
      return parse_call_expr(std::move(expr));
    }

    return expr;
  }

  // Parenthesized expression
  if (tok.kind == TokenKind::Punctuation && tok.lexeme == "(") {
    advance(); // consume '('
    auto expr = parse_expr();
    if (!expr) {
      error("Expected expression after '('");
      return nullptr;
    }
    if (!check(TokenKind::Punctuation) || peek().lexeme != ")") {
      error("Expected ')' after expression");
      return nullptr;
    }
    advance(); // consume ')'
    return expr;
  }

  error("Unexpected token in expression: " + tok.lexeme);
  return nullptr;
}

ExprPtr Parser::parse_call_expr(ExprPtr callee) {
  Token start_tok = peek();
  advance(); // consume '('

  std::vector<ExprPtr> args;
  while (!check(TokenKind::Punctuation) || peek().lexeme != ")") {
    if (!args.empty()) {
      if (!check(TokenKind::Punctuation) || peek().lexeme != ",") {
        error("Expected ',' between arguments");
        return nullptr;
      }
      advance(); // consume ','
    }

    auto arg = parse_expr();
    if (!arg) {
      error("Expected argument expression");
      return nullptr;
    }
    args.push_back(std::move(arg));
  }

  if (!check(TokenKind::Punctuation) || peek().lexeme != ")") {
    error("Expected ')' after arguments");
    return nullptr;
  }
  advance(); // consume ')'

  return std::make_unique<CallExpr>(std::move(callee), std::move(args),
                                    token_loc(start_tok));
}

// ===== Type Parsing =====

std::optional<TypePtr> Parser::parse_type_annotation() {
  if (!check(TokenKind::Identifier)) {
    error("Expected type name");
    return std::nullopt;
  }
  Token tok = peek();
  std::string type_name = advance().lexeme;
  return std::make_unique<Type>(std::move(type_name), token_loc(tok));
}

// ===== Helper Functions =====

Token Parser::peek() const {
  // Skip comments
  size_t idx = current_;
  while (idx < tokens_.size() && tokens_[idx].kind == TokenKind::Comment) {
    ++idx;
  }

  if (idx >= tokens_.size()) {
    return tokens_.back(); // Should be EndOfFile
  }
  return tokens_[idx];
}

Token Parser::advance() {
  // Skip comments before advancing
  while (current_ < tokens_.size() &&
         tokens_[current_].kind == TokenKind::Comment) {
    ++current_;
  }

  if (current_ < tokens_.size()) {
    ++current_;
  }

  return tokens_[current_ - 1];
}

bool Parser::check(TokenKind kind) const {
  if (at_end()) {
    return false;
  }
  return peek().kind == kind;
}

bool Parser::match(TokenKind kind) {
  if (check(kind)) {
    advance();
    return true;
  }
  return false;
}

bool Parser::expect(TokenKind kind, const std::string &message) {
  if (check(kind)) {
    advance();
    return true;
  }
  error(message);
  return false;
}

bool Parser::expect_token(TokenKind kind, const std::string &lexeme,
                          const std::string &message) {
  if (check(kind) && peek().lexeme == lexeme) {
    advance();
    return true;
  }
  // Token is missing, point to where it should be inserted (end of previous
  // token)
  error_at_previous_end(message);
  return false;
}

bool Parser::expect_keyword(const std::string &keyword,
                            const std::string &message) {
  if (check(TokenKind::Keyword) && peek().lexeme == keyword) {
    advance();
    return true;
  }
  error(message);
  return false;
}

bool Parser::at_end() const {
  // Skip comments to check if we're at end
  size_t idx = current_;
  while (idx < tokens_.size() && tokens_[idx].kind == TokenKind::Comment) {
    ++idx;
  }

  return idx >= tokens_.size() ||
         (idx < tokens_.size() && tokens_[idx].kind == TokenKind::EndOfFile);
}

bool Parser::can_start_primary() const {
  if (at_end())
    return false;

  Token tok = peek();

  // Literals
  if (tok.kind == TokenKind::Integer || tok.kind == TokenKind::Float ||
      tok.kind == TokenKind::String) {
    return true;
  }

  // Identifiers
  if (tok.kind == TokenKind::Identifier) {
    return true;
  }

  // Boolean literals
  if (tok.kind == TokenKind::Keyword &&
      (tok.lexeme == "true" || tok.lexeme == "false")) {
    return true;
  }

  // Parenthesized expressions
  if (tok.kind == TokenKind::Punctuation && tok.lexeme == "(") {
    return true;
  }

  return false;
}

void Parser::error(const std::string &message) {
  Token tok = peek();
  ParseError err;
  err.message = message;
  err.line = tok.line;
  err.column = tok.column;
  err.end_column = tok.end_column;
  errors_.push_back(err);
}

void Parser::error_at_previous_end(const std::string &message) {
  // Error should point to the end of the previous non-comment token
  if (current_ == 0) {
    error(message);
    return;
  }

  // Find the previous non-comment token
  size_t idx = current_ > 0 ? current_ - 1 : 0;
  while (idx > 0 && tokens_[idx].kind == TokenKind::Comment) {
    --idx;
  }

  Token prev = tokens_[idx];
  ParseError err;
  err.message = message;
  err.line = prev.line;
  err.column = prev.end_column; // Point to end of previous token
  err.end_column = prev.end_column + 1;
  errors_.push_back(err);
}

void Parser::synchronize() {
  // Skip tokens until we find a statement boundary
  while (!at_end()) {
    Token tok = peek();

    // Semicolon is a statement boundary
    if (tok.kind == TokenKind::Punctuation && tok.lexeme == ";") {
      advance();
      return;
    }

    // Block end is a statement boundary (don't consume it)
    if (tok.kind == TokenKind::Punctuation && tok.lexeme == "}") {
      return;
    }

    // Keywords that start statements
    if (tok.kind == TokenKind::Keyword) {
      if (tok.lexeme == "let" || tok.lexeme == "func" || tok.lexeme == "if" ||
          tok.lexeme == "return" || tok.lexeme == "while") {
        return;
      }
    }

    advance();
  }
}

} // namespace pecco
