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
  std::string lexeme{};      // Raw or decoded lexeme, depending on token type.
  std::size_t line{1};       // 1-based line number.
  std::size_t column{1};     // 1-based column number (start of token).
  std::size_t end_column{1}; // 1-based column number (end of token, exclusive).

  // For Error tokens: offset from column where the actual error occurs
  // For example, in "bad\qescape", column points to ", error_offset points to
  // \q
  std::size_t error_offset{0};
};

const char *to_string(TokenKind kind);

} // namespace pecco
