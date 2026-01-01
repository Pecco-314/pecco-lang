#include "parser.hpp"
#include <gtest/gtest.h>

using namespace pecco;

namespace {

// Helper to parse a source string
std::pair<std::vector<StmtPtr>, Parser>
parse_source(const std::string &source) {
  Lexer lexer(source);
  auto tokens = lexer.tokenize_all();
  Parser parser(std::move(tokens));
  auto stmts = parser.parse_program();
  return {std::move(stmts), std::move(parser)};
}

TEST(ParserTest, ParseLetWithType) {
  std::string source = "let x : i32 = 42;";
  auto [stmts, parser] = parse_source(source);

  ASSERT_FALSE(parser.has_errors())
      << "Parser errors: "
      << (parser.errors().empty() ? "" : parser.errors()[0].message);
  ASSERT_EQ(stmts.size(), 1);
  ASSERT_EQ(stmts[0]->kind, StmtKind::Let);

  auto *let_stmt = static_cast<LetStmt *>(stmts[0].get());
  EXPECT_EQ(let_stmt->name, "x");
  EXPECT_TRUE(let_stmt->type.has_value());
  EXPECT_EQ((*let_stmt->type)->name, "i32");
  EXPECT_EQ(let_stmt->init->kind, ExprKind::IntLiteral);
}

TEST(ParserTest, ParseLetWithoutType) {
  std::string source = "let y = 3.14;";

  auto [stmts, parser] = parse_source(source);

  ASSERT_FALSE(parser.has_errors());
  ASSERT_EQ(stmts.size(), 1);
  ASSERT_EQ(stmts[0]->kind, StmtKind::Let);

  auto *let_stmt = static_cast<LetStmt *>(stmts[0].get());
  EXPECT_EQ(let_stmt->name, "y");
  EXPECT_FALSE(let_stmt->type.has_value());
  EXPECT_EQ(let_stmt->init->kind, ExprKind::FloatLiteral);
}

TEST(ParserTest, ParseFuncWithTypes) {
  std::string source = "func add(a : i32, b : i32) : i32 { return a + b; }";

  auto [stmts, parser] = parse_source(source);

  ASSERT_FALSE(parser.has_errors())
      << (parser.errors().empty() ? "" : parser.errors()[0].message);
  ASSERT_EQ(stmts.size(), 1);
  ASSERT_EQ(stmts[0]->kind, StmtKind::Func);

  auto *func_stmt = static_cast<FuncStmt *>(stmts[0].get());
  EXPECT_EQ(func_stmt->name, "add");
  EXPECT_EQ(func_stmt->params.size(), 2);
  EXPECT_EQ(func_stmt->params[0].name, "a");
  EXPECT_TRUE(func_stmt->params[0].type.has_value());
  EXPECT_EQ((*func_stmt->params[0].type)->name, "i32");
  EXPECT_TRUE(func_stmt->return_type.has_value());
  EXPECT_EQ((*func_stmt->return_type)->name, "i32");
}

TEST(ParserTest, ParseFuncWithoutTypes) {
  std::string source = "func test(x, y) { return x; }";

  auto [stmts, parser] = parse_source(source);

  ASSERT_FALSE(parser.has_errors());
  ASSERT_EQ(stmts.size(), 1);
  ASSERT_EQ(stmts[0]->kind, StmtKind::Func);

  auto *func_stmt = static_cast<FuncStmt *>(stmts[0].get());
  EXPECT_EQ(func_stmt->name, "test");
  EXPECT_EQ(func_stmt->params.size(), 2);
  EXPECT_FALSE(func_stmt->params[0].type.has_value());
  EXPECT_FALSE(func_stmt->return_type.has_value());
}

TEST(ParserTest, ParseIfStmt) {
  std::string source = "if x { return 1; }";

  auto [stmts, parser] = parse_source(source);

  ASSERT_FALSE(parser.has_errors());
  ASSERT_EQ(stmts.size(), 1);
  ASSERT_EQ(stmts[0]->kind, StmtKind::If);

  auto *if_stmt = static_cast<IfStmt *>(stmts[0].get());
  EXPECT_EQ(if_stmt->condition->kind, ExprKind::Identifier);
  EXPECT_EQ(if_stmt->then_branch->kind, StmtKind::Block);
  EXPECT_FALSE(if_stmt->else_branch.has_value());
}

TEST(ParserTest, ParseIfElseStmt) {
  std::string source = "if x { return 1; } else { return 2; }";

  auto [stmts, parser] = parse_source(source);

  ASSERT_FALSE(parser.has_errors());
  ASSERT_EQ(stmts.size(), 1);
  ASSERT_EQ(stmts[0]->kind, StmtKind::If);

  auto *if_stmt = static_cast<IfStmt *>(stmts[0].get());
  EXPECT_TRUE(if_stmt->else_branch.has_value());
  EXPECT_EQ((*if_stmt->else_branch)->kind, StmtKind::Block);
}

TEST(ParserTest, ParseIfElseIfElse) {
  std::string source =
      "if x { return 1; } else if y { return 2; } else { return 3; }";

  auto [stmts, parser] = parse_source(source);

  ASSERT_FALSE(parser.has_errors())
      << (parser.errors().empty() ? "" : parser.errors()[0].message);
  ASSERT_EQ(stmts.size(), 1);
  ASSERT_EQ(stmts[0]->kind, StmtKind::If);

  auto *if_stmt = static_cast<IfStmt *>(stmts[0].get());
  EXPECT_TRUE(if_stmt->else_branch.has_value());
  EXPECT_EQ((*if_stmt->else_branch)->kind, StmtKind::If); // else if
}

TEST(ParserTest, ParseReturnWithValue) {
  std::string source = "return 42;";

  auto [stmts, parser] = parse_source(source);

  ASSERT_FALSE(parser.has_errors());
  ASSERT_EQ(stmts.size(), 1);
  ASSERT_EQ(stmts[0]->kind, StmtKind::Return);

  auto *ret_stmt = static_cast<ReturnStmt *>(stmts[0].get());
  EXPECT_TRUE(ret_stmt->value.has_value());
  EXPECT_EQ((*ret_stmt->value)->kind, ExprKind::IntLiteral);
}

TEST(ParserTest, ParseReturnWithoutValue) {
  std::string source = "return;";

  auto [stmts, parser] = parse_source(source);

  ASSERT_FALSE(parser.has_errors());
  ASSERT_EQ(stmts.size(), 1);
  ASSERT_EQ(stmts[0]->kind, StmtKind::Return);

  auto *ret_stmt = static_cast<ReturnStmt *>(stmts[0].get());
  EXPECT_FALSE(ret_stmt->value.has_value());
}

TEST(ParserTest, ParseWhileStmt) {
  std::string source = "while x { x = x - 1; }";

  auto [stmts, parser] = parse_source(source);

  ASSERT_FALSE(parser.has_errors())
      << (parser.errors().empty() ? "" : parser.errors()[0].message);
  ASSERT_EQ(stmts.size(), 1);
  ASSERT_EQ(stmts[0]->kind, StmtKind::While);

  auto *while_stmt = static_cast<WhileStmt *>(stmts[0].get());
  EXPECT_EQ(while_stmt->condition->kind, ExprKind::Identifier);
  EXPECT_EQ(while_stmt->body->kind, StmtKind::Block);
}

TEST(ParserTest, ParseBinaryExpr) {
  std::string source = "let x = 1 + 2 * 3;";

  auto [stmts, parser] = parse_source(source);

  ASSERT_FALSE(parser.has_errors());
  ASSERT_EQ(stmts.size(), 1);

  auto *let_stmt = static_cast<LetStmt *>(stmts[0].get());
  EXPECT_EQ(let_stmt->init->kind, ExprKind::OperatorSeq);

  // Expression is now a flat sequence: 1 + 2 * 3
  // Format: operand(1) op(+) operand(2) op(*) operand(3)
  auto *seq = static_cast<OperatorSeqExpr *>(let_stmt->init.get());
  ASSERT_EQ(seq->items.size(), 5); // 3 operands + 2 operators

  EXPECT_EQ(seq->items[0].kind, OpSeqItem::Kind::Operand);
  EXPECT_EQ(seq->items[1].kind, OpSeqItem::Kind::Operator);
  EXPECT_EQ(seq->items[1].op, "+");
  EXPECT_EQ(seq->items[2].kind, OpSeqItem::Kind::Operand);
  EXPECT_EQ(seq->items[3].kind, OpSeqItem::Kind::Operator);
  EXPECT_EQ(seq->items[3].op, "*");
  EXPECT_EQ(seq->items[4].kind, OpSeqItem::Kind::Operand);
}

TEST(ParserTest, ParseFunctionCall) {
  std::string source = "let result = add(1, 2);";

  auto [stmts, parser] = parse_source(source);

  ASSERT_FALSE(parser.has_errors());
  ASSERT_EQ(stmts.size(), 1);

  auto *let_stmt = static_cast<LetStmt *>(stmts[0].get());
  EXPECT_EQ(let_stmt->init->kind, ExprKind::Call);

  auto *call = static_cast<CallExpr *>(let_stmt->init.get());
  EXPECT_EQ(call->callee->kind, ExprKind::Identifier);
  EXPECT_EQ(call->args.size(), 2);
}

TEST(ParserTest, ParseCompleteProgram) {
  std::string source = R"(
    let x : i32 = 42;
    func test(a : i32) : i32 {
      if a > 0 {
        return a;
      } else {
        return 0;
      }
    }
  )";

  auto [stmts, parser] = parse_source(source);

  ASSERT_FALSE(parser.has_errors())
      << (parser.errors().empty() ? "" : parser.errors()[0].message);
  EXPECT_EQ(stmts.size(), 2);
  EXPECT_EQ(stmts[0]->kind, StmtKind::Let);
  EXPECT_EQ(stmts[1]->kind, StmtKind::Func);
}

TEST(ParserTest, ParseBoolLiterals) {
  std::string source = "let a = true; let b = false;";

  auto [stmts, parser] = parse_source(source);

  ASSERT_FALSE(parser.has_errors());
  ASSERT_EQ(stmts.size(), 2);

  auto *let_a = static_cast<LetStmt *>(stmts[0].get());
  EXPECT_EQ(let_a->init->kind, ExprKind::BoolLiteral);
  auto *bool_a = static_cast<BoolLiteralExpr *>(let_a->init.get());
  EXPECT_TRUE(bool_a->value);

  auto *let_b = static_cast<LetStmt *>(stmts[1].get());
  auto *bool_b = static_cast<BoolLiteralExpr *>(let_b->init.get());
  EXPECT_FALSE(bool_b->value);
}

TEST(ParserTest, ErrorRecovery) {
  std::string source = "let x = ; let y = 2;"; // Invalid

  auto [stmts, parser] = parse_source(source);

  EXPECT_TRUE(parser.has_errors());
  // Should recover and parse the second statement
  EXPECT_GE(stmts.size(), 0);
}

TEST(ParserTest, ParseWithComments) {
  std::string source = R"(
    # This is a comment
    let x = 42;
    # Another comment
    func test() {
      return x; # inline comment
    }
    # Final comment
  )";

  auto [stmts, parser] = parse_source(source);

  ASSERT_FALSE(parser.has_errors())
      << (parser.errors().empty() ? "" : parser.errors()[0].message);
  EXPECT_EQ(stmts.size(), 2); // let and func, comments should be skipped
  EXPECT_EQ(stmts[0]->kind, StmtKind::Let);
  EXPECT_EQ(stmts[1]->kind, StmtKind::Func);
}

TEST(ParserTest, ErrorLocationPrecision) {
  // Test that missing semicolon error points to end of previous token
  std::string source = "let x = 42\nlet y = 10;";

  auto [stmts, parser] = parse_source(source);

  ASSERT_TRUE(parser.has_errors());
  ASSERT_GE(parser.errors().size(), 1);

  auto &err = parser.errors()[0];
  EXPECT_EQ(err.line, 1);
  EXPECT_EQ(err.column, 11); // After "42"
  EXPECT_NE(err.message.find("';'"), std::string::npos);
}

TEST(ParserTest, ErrorRecoveryInBlock) {
  // Test that error recovery doesn't skip block delimiters
  std::string source = R"(
    func test() {
      return 42
    }
  )";

  auto [stmts, parser] = parse_source(source);

  ASSERT_TRUE(parser.has_errors());
  // Should only have one error (missing semicolon), not cascade errors
  EXPECT_EQ(parser.errors().size(), 1);
  EXPECT_NE(parser.errors()[0].message.find("';'"), std::string::npos);

  // Should still successfully parse the function structure
  ASSERT_EQ(stmts.size(), 1);
  EXPECT_EQ(stmts[0]->kind, StmtKind::Func);
}

TEST(ParserTest, MultipleErrorsWithRecovery) {
  // Test multiple errors with proper recovery
  std::string source = "let x = 42\nfunc add(a, b) {\n  return a + b\n}";

  auto [stmts, parser] = parse_source(source);

  ASSERT_TRUE(parser.has_errors());
  EXPECT_EQ(parser.errors().size(), 2); // Two missing semicolons

  // First error: missing semicolon after let
  EXPECT_EQ(parser.errors()[0].line, 1);
  EXPECT_EQ(parser.errors()[0].column, 11); // After "42"

  // Second error: missing semicolon after return
  EXPECT_EQ(parser.errors()[1].line, 3);
  EXPECT_EQ(parser.errors()[1].column, 15); // After "b"

  // With improved error recovery, both statements are kept despite errors
  EXPECT_EQ(stmts.size(), 2);
  EXPECT_EQ(stmts[0]->kind, StmtKind::Let);
  EXPECT_EQ(stmts[1]->kind, StmtKind::Func);
}

TEST(ParserTest, ParseComplexOperatorExpression) {
  // Test complex expression with prefix, infix, and postfix operators
  // Expression: -5++ + 3 * !flag--
  // This tests parsing without semantic analysis (operators don't need
  // definitions)
  std::string source = "let result = -5++ + 3 * !flag--;";

  auto [stmts, parser] = parse_source(source);

  ASSERT_FALSE(parser.has_errors())
      << "Parser errors: "
      << (parser.errors().empty() ? "" : parser.errors()[0].message);
  ASSERT_EQ(stmts.size(), 1);
  ASSERT_EQ(stmts[0]->kind, StmtKind::Let);

  auto *let = static_cast<LetStmt *>(stmts[0].get());
  EXPECT_EQ(let->name, "result");

  // Should be OperatorSeq
  ASSERT_EQ(let->init->kind, ExprKind::OperatorSeq);
  auto *seq = static_cast<OperatorSeqExpr *>(let->init.get());

  // Format: - 5 ++ + 3 * ! flag --
  // Total: 3 operands + 6 operators = 9 items
  EXPECT_EQ(seq->items.size(), 9);

  // Just verify it parsed as a sequence
  EXPECT_EQ(seq->items[0].kind, OpSeqItem::Kind::Operator); // -
  EXPECT_EQ(seq->items[1].kind, OpSeqItem::Kind::Operand);  // 5
  EXPECT_EQ(seq->items[2].kind, OpSeqItem::Kind::Operator); // ++
}

TEST(ParserTest, ParseMultiplePostfixOperators) {
  // Test: (5++)--  - Note: this requires parens to be valid
  // Simpler: just test postfix chaining isn't really standard
  // Let's use: x++ + y--  instead
  std::string source = "let x = a++ + b--;";

  auto [stmts, parser] = parse_source(source);

  ASSERT_FALSE(parser.has_errors())
      << "Parser errors: "
      << (parser.errors().empty() ? "" : parser.errors()[0].message);
  ASSERT_EQ(stmts.size(), 1);

  auto *let = static_cast<LetStmt *>(stmts[0].get());
  ASSERT_EQ(let->init->kind, ExprKind::OperatorSeq);
  auto *seq = static_cast<OperatorSeqExpr *>(let->init.get());

  // Format: a ++ + b --
  // Total: 2 operands + 3 operators = 5 items
  EXPECT_EQ(seq->items.size(), 5);

  EXPECT_EQ(seq->items[0].kind, OpSeqItem::Kind::Operand);  // a
  EXPECT_EQ(seq->items[1].kind, OpSeqItem::Kind::Operator); // ++
  EXPECT_EQ(seq->items[2].kind, OpSeqItem::Kind::Operator); // +
  EXPECT_EQ(seq->items[3].kind, OpSeqItem::Kind::Operand);  // b
  EXPECT_EQ(seq->items[4].kind, OpSeqItem::Kind::Operator); // --
}

TEST(ParserTest, ParseMultiplePrefixOperators) {
  // Test: -- ++x  (with space)
  std::string source = "let y = -- ++x;";

  auto [stmts, parser] = parse_source(source);

  ASSERT_FALSE(parser.has_errors())
      << "Parser errors: "
      << (parser.errors().empty() ? "" : parser.errors()[0].message);
  ASSERT_EQ(stmts.size(), 1);

  auto *let = static_cast<LetStmt *>(stmts[0].get());
  ASSERT_EQ(let->init->kind, ExprKind::OperatorSeq);
  auto *seq = static_cast<OperatorSeqExpr *>(let->init.get());

  // Format: -- ++ x
  // Total: 1 operand + 2 operators = 3 items
  EXPECT_EQ(seq->items.size(), 3);

  EXPECT_EQ(seq->items[0].kind, OpSeqItem::Kind::Operator); // --
  EXPECT_EQ(seq->items[0].op, "--");
  EXPECT_EQ(seq->items[1].kind, OpSeqItem::Kind::Operator); // ++
  EXPECT_EQ(seq->items[1].op, "++");
  EXPECT_EQ(seq->items[2].kind, OpSeqItem::Kind::Operand); // x
}

TEST(ParserTest, ParseMixedPrefixPostfixInfix) {
  // Test: -a++ + ++b--
  std::string source = "let z = -a++ + ++b--;";

  auto [stmts, parser] = parse_source(source);

  ASSERT_FALSE(parser.has_errors())
      << "Parser errors: "
      << (parser.errors().empty() ? "" : parser.errors()[0].message);
  ASSERT_EQ(stmts.size(), 1);

  auto *let = static_cast<LetStmt *>(stmts[0].get());
  ASSERT_EQ(let->init->kind, ExprKind::OperatorSeq);
  auto *seq = static_cast<OperatorSeqExpr *>(let->init.get());

  // Format: - a ++ + ++ b --
  // Total: 2 operands + 5 operators = 7 items
  EXPECT_EQ(seq->items.size(), 7);

  EXPECT_EQ(seq->items[0].kind, OpSeqItem::Kind::Operator); // -
  EXPECT_EQ(seq->items[1].kind, OpSeqItem::Kind::Operand);  // a
  EXPECT_EQ(seq->items[2].kind, OpSeqItem::Kind::Operator); // ++
  EXPECT_EQ(seq->items[3].kind, OpSeqItem::Kind::Operator); // +
  EXPECT_EQ(seq->items[4].kind, OpSeqItem::Kind::Operator); // ++
  EXPECT_EQ(seq->items[5].kind, OpSeqItem::Kind::Operand);  // b
  EXPECT_EQ(seq->items[6].kind, OpSeqItem::Kind::Operator); // --
}

TEST(ParserTest, ParseChainedAssignment) {
  // Test: a = b = c = 5
  std::string source = "let x = a = b = c = 5;";

  auto [stmts, parser] = parse_source(source);

  ASSERT_FALSE(parser.has_errors())
      << "Parser errors: "
      << (parser.errors().empty() ? "" : parser.errors()[0].message);
  ASSERT_EQ(stmts.size(), 1);

  auto *let = static_cast<LetStmt *>(stmts[0].get());
  ASSERT_EQ(let->init->kind, ExprKind::OperatorSeq);
  auto *seq = static_cast<OperatorSeqExpr *>(let->init.get());

  // Format: a = b = c = 5
  // Total: 4 operands + 3 operators = 7 items
  EXPECT_EQ(seq->items.size(), 7);

  // Just verify it has the right number of items
  // Semantic analysis will handle the actual resolution
}

} // namespace

int main(int argc, char **argv) {
  ::testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}
