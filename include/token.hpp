#pragma once

#include <cstddef>
#include <string>

namespace pecco {

enum class TokenKind {
  EndOfFile,
  Integer,
  Float,
  String,
  Identifier,
  Keyword,
  Operator,
  Punctuation,
  Comment,
  Error,
};

struct Token {
  TokenKind kind{TokenKind::EndOfFile};
  std::string lexeme{};  // Raw or decoded lexeme, depending on token type.
  std::size_t line{1};   // 1-based line number.
  std::size_t column{1}; // 1-based column number (start of token).
};

const char *to_string(TokenKind kind);

} // namespace pecco
