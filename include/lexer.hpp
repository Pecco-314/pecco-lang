#pragma once

#include "token.hpp"

#include <string>
#include <string_view>
#include <vector>

namespace pecco {

class Lexer {
public:
  explicit Lexer(std::string_view source);

  // Produce the next token; returns TokenKind::EndOfFile when input is
  // exhausted.
  Token next_token();

  // Reset the lexer with new source content.
  void reset(std::string_view source);

  // Convenience: tokenize entire input.
  std::vector<Token> tokenize_all();

private:
  Token lex_number();
  Token lex_identifier_or_keyword();
  Token lex_string();
  Token lex_operator();
  Token lex_punctuation_or_comment();

  void skip_whitespace();
  bool at_end() const;
  char peek() const;
  char advance();

  std::string source_{};
  std::size_t index_{0};
  std::size_t line_{1};
  std::size_t column_{1};
};

} // namespace pecco
