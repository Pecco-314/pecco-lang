#include "lexer.hpp"
#include "parser.hpp"
#include "semantic.hpp"
#include <gtest/gtest.h>

using namespace pecco;

class SemanticTest : public ::testing::Test {
protected:
  SemanticAnalyzer analyzer;
};

TEST_F(SemanticTest, LoadPrelude) {
  // Load prelude
  ASSERT_TRUE(analyzer.load_prelude(STDLIB_DIR "/prelude.pl"));

  // Print errors if any
  if (analyzer.has_errors()) {
    for (const auto &err : analyzer.errors()) {
      std::cerr << "Semantic error: " << err.message << "\n";
    }
  }

  ASSERT_FALSE(analyzer.has_errors());

  const auto &symtab = analyzer.symbol_table();

  // Check that core function is loaded
  EXPECT_TRUE(symtab.has_function("write"));

  // Check that operators are loaded
  EXPECT_TRUE(symtab.has_operator("+", OpPosition::Infix));
  EXPECT_TRUE(symtab.has_operator("-", OpPosition::Infix));
  EXPECT_TRUE(symtab.has_operator("-", OpPosition::Prefix));
  EXPECT_TRUE(symtab.has_operator("*", OpPosition::Infix));
  EXPECT_TRUE(symtab.has_operator("/", OpPosition::Infix));
  EXPECT_TRUE(symtab.has_operator("==", OpPosition::Infix));
  EXPECT_TRUE(symtab.has_operator("&&", OpPosition::Infix));
  EXPECT_TRUE(symtab.has_operator("!", OpPosition::Prefix));
}

TEST_F(SemanticTest, FindFunctionOverloads) {
  ASSERT_TRUE(analyzer.load_prelude(STDLIB_DIR "/prelude.pl"));

  const auto &symtab = analyzer.symbol_table();

  // Find write function
  auto write_funcs = symtab.find_functions("write");
  EXPECT_EQ(write_funcs.size(), 1);
  EXPECT_EQ(write_funcs[0].param_types.size(), 3);
  EXPECT_EQ(write_funcs[0].param_types[0], "i32");
  EXPECT_EQ(write_funcs[0].param_types[1], "string");
  EXPECT_EQ(write_funcs[0].param_types[2], "i32");
  EXPECT_EQ(write_funcs[0].return_type, "i32");
}

TEST_F(SemanticTest, FindOperatorOverloads) {
  ASSERT_TRUE(analyzer.load_prelude(STDLIB_DIR "/prelude.pl"));

  const auto &symtab = analyzer.symbol_table();

  // Find all + operators
  auto plus_ops = symtab.find_all_operators("+");
  // Should have i32 + i32 and f64 + f64
  EXPECT_GE(plus_ops.size(), 2);

  // Find infix + operator
  auto infix_plus = symtab.find_operator("+", OpPosition::Infix);
  ASSERT_TRUE(infix_plus.has_value());
  EXPECT_EQ(infix_plus->precedence, 70);
  EXPECT_EQ(infix_plus->assoc, Associativity::Left);
}

TEST_F(SemanticTest, CollectUserFunctions) {
  // Parse a simple function
  std::string source = "func add(x: i32, y: i32) : i32 { return x + y; }";
  Lexer lexer(source);
  auto tokens = lexer.tokenize_all();
  Parser parser(std::move(tokens));
  auto stmts = parser.parse_program();

  ASSERT_FALSE(parser.has_errors());

  // Collect declarations
  analyzer.collect_declarations(stmts);
  ASSERT_FALSE(analyzer.has_errors());

  const auto &symtab = analyzer.symbol_table();

  // Check that function is in symbol table
  EXPECT_TRUE(symtab.has_function("add"));
  auto add_funcs = symtab.find_functions("add");
  ASSERT_EQ(add_funcs.size(), 1);
  EXPECT_EQ(add_funcs[0].param_types.size(), 2);
  EXPECT_EQ(add_funcs[0].param_types[0], "i32");
  EXPECT_EQ(add_funcs[0].param_types[1], "i32");
  EXPECT_EQ(add_funcs[0].return_type, "i32");
  EXPECT_FALSE(add_funcs[0].is_declaration_only);
}

TEST_F(SemanticTest, CollectFunctionDeclarations) {
  // Parse a function declaration (no body)
  std::string source = "func extern_func(x: i32) : i32;";
  Lexer lexer(source);
  auto tokens = lexer.tokenize_all();
  Parser parser(std::move(tokens));
  auto stmts = parser.parse_program();

  ASSERT_FALSE(parser.has_errors());

  // Collect declarations
  analyzer.collect_declarations(stmts);
  ASSERT_FALSE(analyzer.has_errors());

  const auto &symtab = analyzer.symbol_table();

  // Check that function is in symbol table
  EXPECT_TRUE(symtab.has_function("extern_func"));
  auto funcs = symtab.find_functions("extern_func");
  ASSERT_EQ(funcs.size(), 1);
  EXPECT_TRUE(funcs[0].is_declaration_only);
}

TEST_F(SemanticTest, CollectOperatorDeclarations) {
  // Parse an operator declaration
  std::string source =
      "infix operator + (a: i32, b: i32) : i32 prec 75 assoc left;";
  Lexer lexer(source);
  auto tokens = lexer.tokenize_all();
  Parser parser(std::move(tokens));
  auto stmts = parser.parse_program();

  if (parser.has_errors()) {
    for (const auto &err : parser.errors()) {
      std::cerr << "Parse error: " << err.message << "\n";
    }
  }

  ASSERT_FALSE(parser.has_errors());

  // Collect declarations
  analyzer.collect_declarations(stmts);
  ASSERT_FALSE(analyzer.has_errors());

  const auto &symtab = analyzer.symbol_table();

  // Check that operator is in symbol table
  EXPECT_TRUE(symtab.has_operator("+", OpPosition::Infix));
  auto ops = symtab.find_operators("+", OpPosition::Infix);
  ASSERT_GE(ops.size(), 1);
  // Find the one we just added
  bool found = false;
  for (const auto &op : ops) {
    if (op.precedence == 75 && op.assoc == Associativity::Left) {
      found = true;
      EXPECT_EQ(op.signature.param_types.size(), 2);
      EXPECT_EQ(op.signature.return_type, "i32");
      break;
    }
  }
  EXPECT_TRUE(found);
}

TEST_F(SemanticTest, ResolveSimpleExpression) {
  // Load prelude first
  ASSERT_TRUE(analyzer.load_prelude(STDLIB_DIR "/prelude.pl"));

  // Parse: 1 + 2
  std::string source = "let x = 1 + 2;";
  Lexer lexer(source);
  auto tokens = lexer.tokenize_all();
  Parser parser(std::move(tokens));
  auto stmts = parser.parse_program();
  ASSERT_FALSE(parser.has_errors());

  // Get the let statement
  ASSERT_EQ(stmts.size(), 1);
  auto *let = static_cast<LetStmt *>(stmts[0].get());

  // Initially it's an OperatorSeq
  EXPECT_EQ(let->init->kind, ExprKind::OperatorSeq);

  // Resolve operators
  let->init = analyzer.resolve_operators(std::move(let->init));

  // Now it should be Binary
  ASSERT_EQ(let->init->kind, ExprKind::Binary);
  auto *bin = static_cast<BinaryExpr *>(let->init.get());
  EXPECT_EQ(bin->op, "+");
  EXPECT_EQ(bin->left->kind, ExprKind::IntLiteral);
  EXPECT_EQ(bin->right->kind, ExprKind::IntLiteral);
}

TEST_F(SemanticTest, ResolvePrecedence) {
  // Load prelude first
  ASSERT_TRUE(analyzer.load_prelude(STDLIB_DIR "/prelude.pl"));

  // Parse: 1 + 2 * 3  (should be: 1 + (2 * 3))
  std::string source = "let x = 1 + 2 * 3;";
  Lexer lexer(source);
  auto tokens = lexer.tokenize_all();
  Parser parser(std::move(tokens));
  auto stmts = parser.parse_program();
  ASSERT_FALSE(parser.has_errors());

  auto *let = static_cast<LetStmt *>(stmts[0].get());
  let->init = analyzer.resolve_operators(std::move(let->init));

  // Should be Binary(+, IntLiteral(1), Binary(*, IntLiteral(2), IntLiteral(3)))
  ASSERT_EQ(let->init->kind, ExprKind::Binary);
  auto *add = static_cast<BinaryExpr *>(let->init.get());
  EXPECT_EQ(add->op, "+");
  EXPECT_EQ(add->left->kind, ExprKind::IntLiteral);

  ASSERT_EQ(add->right->kind, ExprKind::Binary);
  auto *mul = static_cast<BinaryExpr *>(add->right.get());
  EXPECT_EQ(mul->op, "*");
  EXPECT_EQ(mul->left->kind, ExprKind::IntLiteral);
  EXPECT_EQ(mul->right->kind, ExprKind::IntLiteral);
}

TEST_F(SemanticTest, ResolveLeftAssociativity) {
  // Load prelude first
  ASSERT_TRUE(analyzer.load_prelude(STDLIB_DIR "/prelude.pl"));

  // Parse: 10 - 5 - 2  (should be: (10 - 5) - 2)
  std::string source = "let x = 10 - 5 - 2;";
  Lexer lexer(source);
  auto tokens = lexer.tokenize_all();
  Parser parser(std::move(tokens));
  auto stmts = parser.parse_program();
  ASSERT_FALSE(parser.has_errors());

  auto *let = static_cast<LetStmt *>(stmts[0].get());
  let->init = analyzer.resolve_operators(std::move(let->init));

  // Should be Binary(-, Binary(-, IntLiteral(10), IntLiteral(5)),
  // IntLiteral(2))
  ASSERT_EQ(let->init->kind, ExprKind::Binary);
  auto *outer = static_cast<BinaryExpr *>(let->init.get());
  EXPECT_EQ(outer->op, "-");
  EXPECT_EQ(outer->right->kind, ExprKind::IntLiteral);

  ASSERT_EQ(outer->left->kind, ExprKind::Binary);
  auto *inner = static_cast<BinaryExpr *>(outer->left.get());
  EXPECT_EQ(inner->op, "-");
  EXPECT_EQ(inner->left->kind, ExprKind::IntLiteral);
  EXPECT_EQ(inner->right->kind, ExprKind::IntLiteral);
}

TEST_F(SemanticTest, ResolveRightAssociativity) {
  // Load prelude first
  ASSERT_TRUE(analyzer.load_prelude(STDLIB_DIR "/prelude.pl"));

  // Parse: 2 ** 3 ** 2  (should be: 2 ** (3 ** 2))
  std::string source = "let x = 2 ** 3 ** 2;";
  Lexer lexer(source);
  auto tokens = lexer.tokenize_all();
  Parser parser(std::move(tokens));
  auto stmts = parser.parse_program();
  ASSERT_FALSE(parser.has_errors());

  auto *let = static_cast<LetStmt *>(stmts[0].get());
  let->init = analyzer.resolve_operators(std::move(let->init));

  // Should be Binary(**, IntLiteral(2), Binary(**, IntLiteral(3),
  // IntLiteral(2)))
  ASSERT_EQ(let->init->kind, ExprKind::Binary);
  auto *outer = static_cast<BinaryExpr *>(let->init.get());
  EXPECT_EQ(outer->op, "**");
  EXPECT_EQ(outer->left->kind, ExprKind::IntLiteral);

  ASSERT_EQ(outer->right->kind, ExprKind::Binary);
  auto *inner = static_cast<BinaryExpr *>(outer->right.get());
  EXPECT_EQ(inner->op, "**");
  EXPECT_EQ(inner->left->kind, ExprKind::IntLiteral);
  EXPECT_EQ(inner->right->kind, ExprKind::IntLiteral);
}
