#include "lexer.hpp"

#include <gtest/gtest.h>

using pecco::Lexer;
using pecco::TokenKind;

namespace {

void expect_token(const pecco::Token &tok, TokenKind kind,
                  std::string_view lexeme) {
  EXPECT_EQ(tok.kind, kind);
  EXPECT_EQ(tok.lexeme, lexeme);
}

void expect_sequence(
    const std::vector<pecco::Token> &tokens,
    std::initializer_list<std::pair<TokenKind, std::string_view>> expected) {
  ASSERT_EQ(tokens.size(), expected.size() + 1);
  std::size_t i = 0;
  for (auto [kind, lexeme] : expected) {
    expect_token(tokens[i], kind, lexeme);
    ++i;
  }
  EXPECT_EQ(tokens.back().kind, TokenKind::EndOfFile);
}

TEST(LexerTest, IdentifiersKeywordsNumbers) {
  Lexer lexer("let foo = 123 456\nreturn foo");
  auto tokens = lexer.tokenize_all();
  expect_sequence(tokens, {
                              {TokenKind::Keyword, "let"},
                              {TokenKind::Identifier, "foo"},
                              {TokenKind::Operator, "="},
                              {TokenKind::Integer, "123"},
                              {TokenKind::Integer, "456"},
                              {TokenKind::Keyword, "return"},
                              {TokenKind::Identifier, "foo"},
                          });
}

TEST(LexerTest, Floats) {
  Lexer lexer("1.25 4e2 5.0e-1");
  auto tokens = lexer.tokenize_all();
  expect_sequence(tokens, {
                              {TokenKind::Float, "1.25"},
                              {TokenKind::Float, "4e2"},
                              {TokenKind::Float, "5.0e-1"},
                          });
}

TEST(LexerTest, StringsWithEscape) {
  Lexer lexer(R"TXT("hello\nworld" "quote:\"" "tab\tchar")TXT");
  auto tokens = lexer.tokenize_all();
  expect_sequence(tokens, {
                              {TokenKind::String, "hello\nworld"},
                              {TokenKind::String, "quote:\""},
                              {TokenKind::String, "tab\tchar"},
                          });
}

TEST(LexerTest, StringsEscapedQuotesAndBackslashes) {
  Lexer lexer(R"TXT("a\\\"b" "slash\\\\\"quote" "back\\\\")TXT");
  auto tokens = lexer.tokenize_all();
  expect_sequence(tokens, {
                              {TokenKind::String, "a\\\"b"},
                              {TokenKind::String, "slash\\\\\"quote"},
                              {TokenKind::String, "back\\\\"},
                          });
}

TEST(LexerTest, OperatorsAndPunctuation) {
  Lexer lexer("a++ == b && c || d -> f();");
  auto tokens = lexer.tokenize_all();
  expect_sequence(tokens, {
                              {TokenKind::Identifier, "a"},
                              {TokenKind::Operator, "++"},
                              {TokenKind::Operator, "=="},
                              {TokenKind::Identifier, "b"},
                              {TokenKind::Operator, "&&"},
                              {TokenKind::Identifier, "c"},
                              {TokenKind::Operator, "||"},
                              {TokenKind::Identifier, "d"},
                              {TokenKind::Operator, "->"},
                              {TokenKind::Identifier, "f"},
                              {TokenKind::Punctuation, "("},
                              {TokenKind::Punctuation, ")"},
                              {TokenKind::Punctuation, ";"},
                          });
}

TEST(LexerTest, Comments) {
  Lexer lexer("foo(bar, baz); # comment here\nqux");
  auto tokens = lexer.tokenize_all();
  expect_sequence(tokens, {
                              {TokenKind::Identifier, "foo"},
                              {TokenKind::Punctuation, "("},
                              {TokenKind::Identifier, "bar"},
                              {TokenKind::Punctuation, ","},
                              {TokenKind::Identifier, "baz"},
                              {TokenKind::Punctuation, ")"},
                              {TokenKind::Punctuation, ";"},
                              {TokenKind::Comment, " comment here"},
                              {TokenKind::Identifier, "qux"},
                          });
}

TEST(LexerTest, InvalidEscapeReportsError) {
  Lexer lexer("\"bad\\q\"");
  auto token = lexer.next_token();
  EXPECT_EQ(token.kind, TokenKind::Error);
}

TEST(LexerTest, NumbersRichVariants) {
  Lexer lexer(
      "0 007 1234567890 0.0 3.1415 6.022e23 1e-3 1e+3 1.0e0 42e+0 123abc");
  auto tokens = lexer.tokenize_all();
  expect_sequence(tokens, {
                              {TokenKind::Integer, "0"},
                              {TokenKind::Integer, "007"},
                              {TokenKind::Integer, "1234567890"},
                              {TokenKind::Float, "0.0"},
                              {TokenKind::Float, "3.1415"},
                              {TokenKind::Float, "6.022e23"},
                              {TokenKind::Float, "1e-3"},
                              {TokenKind::Float, "1e+3"},
                              {TokenKind::Float, "1.0e0"},
                              {TokenKind::Float, "42e+0"},
                              {TokenKind::Integer, "123"},
                              {TokenKind::Identifier, "abc"},
                          });
}

TEST(LexerTest, OperatorCombinationsSeparatedBySpaces) {
  Lexer lexer("a == b != c <= d >= e && f || g + - * / % ^ & | << >> ");
  auto tokens = lexer.tokenize_all();
  expect_sequence(tokens,
                  {
                      {TokenKind::Identifier, "a"}, {TokenKind::Operator, "=="},
                      {TokenKind::Identifier, "b"}, {TokenKind::Operator, "!="},
                      {TokenKind::Identifier, "c"}, {TokenKind::Operator, "<="},
                      {TokenKind::Identifier, "d"}, {TokenKind::Operator, ">="},
                      {TokenKind::Identifier, "e"}, {TokenKind::Operator, "&&"},
                      {TokenKind::Identifier, "f"}, {TokenKind::Operator, "||"},
                      {TokenKind::Identifier, "g"}, {TokenKind::Operator, "+"},
                      {TokenKind::Operator, "-"},   {TokenKind::Operator, "*"},
                      {TokenKind::Operator, "/"},   {TokenKind::Operator, "%"},
                      {TokenKind::Operator, "^"},   {TokenKind::Operator, "&"},
                      {TokenKind::Operator, "|"},   {TokenKind::Operator, "<<"},
                      {TokenKind::Operator, ">>"},
                  });
}

TEST(LexerTest, KeywordsAreNotPrefixes) {
  Lexer lexer("func func_ while while_");
  auto tokens = lexer.tokenize_all();
  expect_sequence(tokens, {
                              {TokenKind::Keyword, "func"},
                              {TokenKind::Identifier, "func_"},
                              {TokenKind::Keyword, "while"},
                              {TokenKind::Identifier, "while_"},
                          });
}

TEST(LexerTest, CommentAtEndOfFile) {
  Lexer lexer("foo # trailing comment");
  auto tokens = lexer.tokenize_all();
  expect_sequence(tokens, {
                              {TokenKind::Identifier, "foo"},
                              {TokenKind::Comment, " trailing comment"},
                          });
}

TEST(LexerTest, UnterminatedStringWithBackslash) {
  Lexer lexer("\"abc\\");
  auto tok = lexer.next_token();
  EXPECT_EQ(tok.kind, TokenKind::Error);
}

} // namespace

int main(int argc, char **argv) {
  ::testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}
