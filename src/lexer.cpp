#include "lexer.hpp"

#include <algorithm>
#include <array>
#include <cctype>
#include <optional>
#include <string_view>

namespace pecco {
namespace {

constexpr std::string_view kOperatorChars = "+-*/%=&|^!<>?.";
constexpr std::string_view kPunctuationChars = "(){}[],;:#";

constexpr std::array<std::string_view, 17> kKeywords = {
    "let",  "func",  "if",       "else",   "return",  "while",
    "true", "false", "operator", "prefix", "postfix", "infix",
    "prec", "assoc", "left",     "right",  "none",
};

bool is_identifier_start(char c) {
  return std::isalpha(static_cast<unsigned char>(c)) || c == '_';
}

bool is_identifier_part(char c) {
  return std::isalnum(static_cast<unsigned char>(c)) || c == '_';
}

bool is_operator_char(char c) {
  return kOperatorChars.find(c) != std::string_view::npos;
}

bool is_punctuation_char(char c) {
  return kPunctuationChars.find(c) != std::string_view::npos;
}

bool is_whitespace(char c) {
  switch (c) {
  case ' ': // space
  case '\t':
  case '\r':
  case '\n':
    return true;
  default:
    return false;
  }
}

std::optional<std::string>
decode_string_with_error_pos(std::string_view view, std::size_t &error_pos) {
  std::string out;
  out.reserve(view.size());

  for (std::size_t i = 0; i < view.size(); ++i) {
    char c = view[i];
    if (c != '\\') {
      out.push_back(c);
      continue;
    }

    if (i + 1 >= view.size()) {
      error_pos = i;
      return std::nullopt;
    }

    char next = view[++i];
    switch (next) {
    case '\\':
    case '"':
    case '\'':
      out.push_back(next);
      break;
    case 'n':
      out.push_back('\n');
      break;
    case 't':
      out.push_back('\t');
      break;
    case 'r':
      out.push_back('\r');
      break;
    case 'b':
      out.push_back('\b');
      break;
    case 'f':
      out.push_back('\f');
      break;
    case '0':
      out.push_back('\0');
      break;
    default:
      error_pos = i - 1; // Position of backslash
      return std::nullopt;
    }
  }

  return out;
}

std::optional<std::string> decode_string(std::string_view view) {
  std::size_t dummy = 0;
  return decode_string_with_error_pos(view, dummy);
}

Token make_error(std::string message, std::size_t line, std::size_t column,
                 std::size_t end_column, std::size_t error_offset) {
  Token tok;
  tok.kind = TokenKind::Error;
  tok.lexeme = std::move(message);
  tok.line = line;
  tok.column = column;
  tok.end_column = (end_column == 0) ? column + 1 : end_column;
  tok.error_offset = error_offset;
  return tok;
}

const char *to_string_impl(TokenKind kind) {
  switch (kind) {
  case TokenKind::EndOfFile:
    return "EndOfFile";
  case TokenKind::Integer:
    return "Integer";
  case TokenKind::Float:
    return "Float";
  case TokenKind::String:
    return "String";
  case TokenKind::Identifier:
    return "Identifier";
  case TokenKind::Keyword:
    return "Keyword";
  case TokenKind::Operator:
    return "Operator";
  case TokenKind::Punctuation:
    return "Punctuation";
  case TokenKind::Comment:
    return "Comment";
  case TokenKind::Error:
    return "Error";
  }
  return "Unknown";
}

} // namespace

Lexer::Lexer(std::string_view source) { reset(source); }

void Lexer::reset(std::string_view source) {
  source_ = std::string(source);
  index_ = 0;
  line_ = 1;
  column_ = 1;
}

Token Lexer::next_token() {
  skip_whitespace();

  if (at_end()) {
    return Token{TokenKind::EndOfFile, "", line_, column_};
  }

  char c = peek();
  if (std::isdigit(static_cast<unsigned char>(c))) {
    return lex_number();
  }
  if (is_identifier_start(c)) {
    return lex_identifier_or_keyword();
  }
  if (c == '"') {
    return lex_string();
  }
  if (is_operator_char(c)) {
    return lex_operator();
  }
  if (is_punctuation_char(c)) {
    return lex_punctuation_or_comment();
  }

  // Unknown character; advance and report error.
  std::size_t start_line = line_;
  std::size_t start_column = column_;
  std::string msg(1, c);
  advance();
  return make_error("Unexpected character: " + msg, start_line, start_column,
                    column_, 0);
}

std::vector<Token> Lexer::tokenize_all() {
  std::vector<Token> tokens;
  for (;;) {
    Token tok = next_token();
    tokens.push_back(tok);
    if (tok.kind == TokenKind::EndOfFile) {
      break;
    }
  }
  return tokens;
}

Token Lexer::lex_number() {
  std::size_t start_index = index_;
  std::size_t start_line = line_;
  std::size_t start_column = column_;

  bool saw_dot = false;
  bool saw_exponent = false;

  auto is_exp_char = [](char ch) { return ch == 'e' || ch == 'E'; };

  while (!at_end()) {
    char c = peek();
    if (std::isdigit(static_cast<unsigned char>(c))) {
      advance();
      continue;
    }

    if (c == '.' && !saw_dot && !saw_exponent) {
      saw_dot = true;
      advance();
      continue;
    }

    if (is_exp_char(c)) {
      std::size_t next_pos = index_ + 1;
      if (next_pos < source_.size()) {
        char next = source_[next_pos];
        if (std::isdigit(static_cast<unsigned char>(next)) || next == '+' ||
            next == '-') {
          saw_exponent = true;
          advance();
          if (peek() == '+' || peek() == '-') {
            advance();
          }
          continue;
        }
      }
    }
    break;
  }

  std::string_view number_view =
      std::string_view(source_).substr(start_index, index_ - start_index);

  Token tok;
  tok.kind = (saw_dot || saw_exponent) ? TokenKind::Float : TokenKind::Integer;
  tok.lexeme = std::string(number_view);
  tok.line = start_line;
  tok.column = start_column;
  tok.end_column = column_;
  return tok;
}

Token Lexer::lex_identifier_or_keyword() {
  std::size_t start_index = index_;
  std::size_t start_line = line_;
  std::size_t start_column = column_;

  advance(); // first character already validated
  while (!at_end() && is_identifier_part(peek())) {
    advance();
  }

  std::string_view view =
      std::string_view(source_).substr(start_index, index_ - start_index);
  bool is_keyword =
      std::find(kKeywords.begin(), kKeywords.end(), view) != kKeywords.end();
  Token tok;
  tok.kind = is_keyword ? TokenKind::Keyword : TokenKind::Identifier;
  tok.lexeme = std::string(view);
  tok.line = start_line;
  tok.column = start_column;
  tok.end_column = column_;
  return tok;
}

Token Lexer::lex_string() {
  std::size_t start_line = line_;
  std::size_t start_column = column_;

  advance(); // consume opening quote
  std::size_t content_start = index_;

  bool terminated = false;
  std::size_t backslashes = 0; // count of consecutive backslashes
  while (!at_end()) {
    char c = advance();
    if (c == '\\') {
      ++backslashes;
      continue;
    }

    bool escaped = (backslashes % 2) == 1;
    backslashes = 0;

    if (c == '"' && !escaped) {
      terminated = true;
      break;
    }
    if (c == '\n' && !escaped) {
      return make_error("Unterminated string literal", start_line, start_column,
                        column_, 0);
    }
  }

  if (!terminated) {
    return make_error("Unterminated string literal", start_line, start_column,
                      column_, 0);
  }

  std::size_t content_end =
      index_ - 1; // exclude closing quote already consumed
  std::string_view raw_content = std::string_view(source_).substr(
      content_start, content_end - content_start);

  // Try to decode and find error position
  std::size_t error_pos = 0;
  auto decoded = decode_string_with_error_pos(raw_content, error_pos);
  if (!decoded.has_value()) {
    // error_pos is relative to content_start, add 1 for opening quote
    return make_error("Invalid string escape", start_line, start_column,
                      column_, error_pos + 1);
  }

  Token tok;
  tok.kind = TokenKind::String;
  tok.lexeme = std::move(*decoded);
  tok.line = start_line;
  tok.column = start_column;
  tok.end_column = column_;
  return tok;
}

Token Lexer::lex_operator() {
  std::size_t start_index = index_;
  std::size_t start_line = line_;
  std::size_t start_column = column_;

  while (!at_end() && is_operator_char(peek())) {
    advance();
  }

  std::string_view view =
      std::string_view(source_).substr(start_index, index_ - start_index);
  Token tok;
  tok.kind = TokenKind::Operator;
  tok.lexeme = std::string(view);
  tok.line = start_line;
  tok.column = start_column;
  tok.end_column = column_;
  return tok;
}

Token Lexer::lex_punctuation_or_comment() {
  std::size_t start_line = line_;
  std::size_t start_column = column_;
  char c = advance();

  if (c == '#') {
    std::size_t comment_start = index_;
    while (!at_end() && peek() != '\n') {
      advance();
    }
    std::string_view view =
        std::string_view(source_).substr(comment_start, index_ - comment_start);
    if (!at_end() && peek() == '\n') {
      advance(); // consume newline to move to the next line
    }
    Token tok;
    tok.kind = TokenKind::Comment;
    tok.lexeme = std::string(view);
    tok.line = start_line;
    tok.column = start_column;
    tok.end_column = column_;
    return tok;
  }

  Token tok;
  tok.kind = TokenKind::Punctuation;
  tok.lexeme = std::string(1, c);
  tok.line = start_line;
  tok.column = start_column;
  tok.end_column = column_;
  return tok;
}

void Lexer::skip_whitespace() {
  while (!at_end() && is_whitespace(peek())) {
    advance();
  }
}

bool Lexer::at_end() const { return index_ >= source_.size(); }

char Lexer::peek() const { return source_[index_]; }

char Lexer::advance() {
  char c = source_[index_++];
  if (c == '\n') {
    ++line_;
    column_ = 1;
  } else {
    ++column_;
  }
  return c;
}

const char *to_string(TokenKind kind) { return to_string_impl(kind); }

} // namespace pecco
