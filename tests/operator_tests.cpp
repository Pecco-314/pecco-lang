#include "lexer.hpp"
#include "operator_resolver.hpp"
#include "parser.hpp"
#include "symbol_table_builder.hpp"
#include <gtest/gtest.h>

using namespace pecco;

class OperatorTest : public ::testing::Test {
protected:
  ScopedSymbolTable symbol_table;
  SymbolTableBuilder builder;
  std::vector<std::string> errors;
};

TEST_F(OperatorTest, LoadPrelude) {
  // Load prelude
  ASSERT_TRUE(builder.load_prelude(STDLIB_DIR "/prelude.pec", symbol_table));

  // Print errors if any
  if (builder.has_errors()) {
    for (const auto &err : builder.errors()) {
      std::cerr << "Semantic error: " << err.message << "\n";
    }
  }

  ASSERT_FALSE(builder.has_errors());

  const auto &symtab = symbol_table.symbol_table();

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

TEST_F(OperatorTest, FindFunctionOverloads) {
  ASSERT_TRUE(builder.load_prelude(STDLIB_DIR "/prelude.pec", symbol_table));

  const auto &symtab = symbol_table.symbol_table();

  // Find write function
  auto write_funcs = symtab.find_functions("write");
  EXPECT_EQ(write_funcs.size(), 1);
  EXPECT_EQ(write_funcs[0].param_types.size(), 3);
  EXPECT_EQ(write_funcs[0].param_types[0], "i32");
  EXPECT_EQ(write_funcs[0].param_types[1], "string");
  EXPECT_EQ(write_funcs[0].param_types[2], "i32");
  EXPECT_EQ(write_funcs[0].return_type, "i32");
}

TEST_F(OperatorTest, FindOperatorOverloads) {
  ASSERT_TRUE(builder.load_prelude(STDLIB_DIR "/prelude.pec", symbol_table));

  const auto &symtab = symbol_table.symbol_table();

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

TEST_F(OperatorTest, CollectUserFunctions) {
  // Parse a simple function
  std::string source = "func add(x: i32, y: i32) : i32 { return x + y; }";
  Lexer lexer(source);
  auto tokens = lexer.tokenize_all();
  Parser parser(std::move(tokens));
  auto stmts = parser.parse_program();

  ASSERT_FALSE(parser.has_errors());

  // Collect declarations
  builder.collect(stmts, symbol_table);
  ASSERT_FALSE(builder.has_errors());

  const auto &symtab = symbol_table.symbol_table();

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

TEST_F(OperatorTest, CollectFunctionDeclarations) {
  // Parse a function declaration (no body)
  std::string source = "func extern_func(x: i32) : i32;";
  Lexer lexer(source);
  auto tokens = lexer.tokenize_all();
  Parser parser(std::move(tokens));
  auto stmts = parser.parse_program();

  ASSERT_FALSE(parser.has_errors());

  // Collect declarations
  builder.collect(stmts, symbol_table);
  ASSERT_FALSE(builder.has_errors());

  const auto &symtab = symbol_table.symbol_table();

  // Check that function is in symbol table
  EXPECT_TRUE(symtab.has_function("extern_func"));
  auto funcs = symtab.find_functions("extern_func");
  ASSERT_EQ(funcs.size(), 1);
  EXPECT_TRUE(funcs[0].is_declaration_only);
}

TEST_F(OperatorTest, CollectOperatorDeclarations) {
  // Parse an operator declaration
  std::string source =
      "operator infix +(a: i32, b: i32) : i32 prec 75 { return a + b; }";
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
  builder.collect(stmts, symbol_table);
  ASSERT_FALSE(builder.has_errors());

  const auto &symtab = symbol_table.symbol_table();

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

TEST_F(OperatorTest, ResolveSimpleExpression) {
  // Load prelude first
  ASSERT_TRUE(builder.load_prelude(STDLIB_DIR "/prelude.pec", symbol_table));

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
  let->init = OperatorResolver::resolve_expr(
      std::move(let->init), symbol_table.symbol_table(), errors);

  // Now it should be Binary
  ASSERT_EQ(let->init->kind, ExprKind::Binary);
  auto *bin = static_cast<BinaryExpr *>(let->init.get());
  EXPECT_EQ(bin->op, "+");
  EXPECT_EQ(bin->left->kind, ExprKind::IntLiteral);
  EXPECT_EQ(bin->right->kind, ExprKind::IntLiteral);
}

TEST_F(OperatorTest, ResolvePrecedence) {
  // Load prelude first
  ASSERT_TRUE(builder.load_prelude(STDLIB_DIR "/prelude.pec", symbol_table));

  // Parse: 1 + 2 * 3  (should be: 1 + (2 * 3))
  std::string source = "let x = 1 + 2 * 3;";
  Lexer lexer(source);
  auto tokens = lexer.tokenize_all();
  Parser parser(std::move(tokens));
  auto stmts = parser.parse_program();
  ASSERT_FALSE(parser.has_errors());

  auto *let = static_cast<LetStmt *>(stmts[0].get());
  let->init = OperatorResolver::resolve_expr(
      std::move(let->init), symbol_table.symbol_table(), errors);

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

TEST_F(OperatorTest, ResolveLeftAssociativity) {
  // Load prelude first
  ASSERT_TRUE(builder.load_prelude(STDLIB_DIR "/prelude.pec", symbol_table));

  // Parse: 10 - 5 - 2  (should be: (10 - 5) - 2)
  std::string source = "let x = 10 - 5 - 2;";
  Lexer lexer(source);
  auto tokens = lexer.tokenize_all();
  Parser parser(std::move(tokens));
  auto stmts = parser.parse_program();
  ASSERT_FALSE(parser.has_errors());

  auto *let = static_cast<LetStmt *>(stmts[0].get());
  let->init = OperatorResolver::resolve_expr(
      std::move(let->init), symbol_table.symbol_table(), errors);

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

TEST_F(OperatorTest, ResolveRightAssociativity) {
  // Load prelude first
  ASSERT_TRUE(builder.load_prelude(STDLIB_DIR "/prelude.pec", symbol_table));

  // Parse: 2 ** 3 ** 2  (should be: 2 ** (3 ** 2))
  std::string source = "let x = 2 ** 3 ** 2;";
  Lexer lexer(source);
  auto tokens = lexer.tokenize_all();
  Parser parser(std::move(tokens));
  auto stmts = parser.parse_program();
  ASSERT_FALSE(parser.has_errors());

  auto *let = static_cast<LetStmt *>(stmts[0].get());
  let->init = OperatorResolver::resolve_expr(
      std::move(let->init), symbol_table.symbol_table(), errors);

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
// 在 semantic_tests.cpp 末尾添加的测试

// Test custom operators with standard operators
TEST_F(OperatorTest, ResolveCustomOperators) {
  // Load prelude first
  ASSERT_TRUE(builder.load_prelude(STDLIB_DIR "/prelude.pec", symbol_table));

  // Define custom operators with function bodies:
  // 1. prefix + (unary plus, identity)
  // 2. infix <> (not equal, for integers)
  std::string custom_ops = R"(
    operator prefix +(x: i32) : i32 {
      return x;
    }

    operator infix <>(a: i32, b: i32) : bool prec 60 {
      return a != b;
    }
  )";

  Lexer lex_ops(custom_ops);
  auto tok_ops = lex_ops.tokenize_all();
  Parser parser_ops(std::move(tok_ops));
  auto op_stmts = parser_ops.parse_program();
  ASSERT_FALSE(parser_ops.has_errors());

  // Collect custom operators
  builder.collect(op_stmts, symbol_table);

  // Now test expressions using custom operators
  // Test 1: +5 (prefix plus)
  std::string source1 = "let a = +5;";
  Lexer lexer1(source1);
  auto tokens1 = lexer1.tokenize_all();
  Parser parser1(std::move(tokens1));
  auto stmts1 = parser1.parse_program();
  ASSERT_FALSE(parser1.has_errors());

  auto *let1 = static_cast<LetStmt *>(stmts1[0].get());
  let1->init = OperatorResolver::resolve_expr(
      std::move(let1->init), symbol_table.symbol_table(), errors);

  // Should be Unary(+, IntLiteral(5))
  ASSERT_EQ(let1->init->kind, ExprKind::Unary);
  auto *unary = static_cast<UnaryExpr *>(let1->init.get());
  EXPECT_EQ(unary->op, "+");
  EXPECT_EQ(unary->position, OpPosition::Prefix);
  EXPECT_EQ(unary->operand->kind, ExprKind::IntLiteral);

  // Test 2: a <> b (custom not-equal)
  std::string source2 = "let c = a <> b;";
  Lexer lexer2(source2);
  auto tokens2 = lexer2.tokenize_all();
  Parser parser2(std::move(tokens2));
  auto stmts2 = parser2.parse_program();
  ASSERT_FALSE(parser2.has_errors());

  auto *let2 = static_cast<LetStmt *>(stmts2[0].get());
  let2->init = OperatorResolver::resolve_expr(
      std::move(let2->init), symbol_table.symbol_table(), errors);

  // Should be Binary(<>, Identifier(a), Identifier(b))
  ASSERT_EQ(let2->init->kind, ExprKind::Binary);
  auto *binary = static_cast<BinaryExpr *>(let2->init.get());
  EXPECT_EQ(binary->op, "<>");
  EXPECT_EQ(binary->left->kind, ExprKind::Identifier);
  EXPECT_EQ(binary->right->kind, ExprKind::Identifier);
}

// Test mixing custom and standard operators with correct precedence
TEST_F(OperatorTest, ResolveMixedCustomOperators) {
  // Load prelude
  ASSERT_TRUE(builder.load_prelude(STDLIB_DIR "/prelude.pec", symbol_table));

  // Define custom operator with different precedence and function body
  // infix +* with precedence 75 (between + and *)
  std::string custom_ops = R"(
    operator infix +*(a: i32, b: i32) : i32 prec 75 {
      return a + a * b;
    }
  )";

  Lexer lex_ops(custom_ops);
  auto tok_ops = lex_ops.tokenize_all();
  Parser parser_ops(std::move(tok_ops));
  auto op_stmts = parser_ops.parse_program();
  ASSERT_FALSE(parser_ops.has_errors());
  builder.collect(op_stmts, symbol_table);

  // Test: 1 + 2 +* 3 * 4
  // Precedence: * (80) > +* (75) > + (70)
  // Should be: 1 + (2 +* (3 * 4))
  std::string source = "let x = 1 + 2 +* 3 * 4;";
  Lexer lexer(source);
  auto tokens = lexer.tokenize_all();
  Parser parser(std::move(tokens));
  auto stmts = parser.parse_program();
  ASSERT_FALSE(parser.has_errors());

  auto *let = static_cast<LetStmt *>(stmts[0].get());
  let->init = OperatorResolver::resolve_expr(
      std::move(let->init), symbol_table.symbol_table(), errors);

  // Root should be: Binary(+, ...)
  ASSERT_EQ(let->init->kind, ExprKind::Binary);
  auto *root = static_cast<BinaryExpr *>(let->init.get());
  EXPECT_EQ(root->op, "+");
  EXPECT_EQ(root->left->kind, ExprKind::IntLiteral);

  // Right should be: Binary(+*, ...)
  ASSERT_EQ(root->right->kind, ExprKind::Binary);
  auto *mid = static_cast<BinaryExpr *>(root->right.get());
  EXPECT_EQ(mid->op, "+*");
  EXPECT_EQ(mid->left->kind, ExprKind::IntLiteral);

  // Right of +* should be: Binary(*, ...)
  ASSERT_EQ(mid->right->kind, ExprKind::Binary);
  auto *mult = static_cast<BinaryExpr *>(mid->right.get());
  EXPECT_EQ(mult->op, "*");
  EXPECT_EQ(mult->left->kind, ExprKind::IntLiteral);
  EXPECT_EQ(mult->right->kind, ExprKind::IntLiteral);
}

// Test prefix operator with infix operators
TEST_F(OperatorTest, ResolvePrefixWithInfix) {
  // Load prelude
  ASSERT_TRUE(builder.load_prelude(STDLIB_DIR "/prelude.pec", symbol_table));

  // Test: -5 + 10
  // Should be: Binary(+, Unary(-, IntLiteral(5)), IntLiteral(10))
  std::string source = "let x = -5 + 10;";
  Lexer lexer(source);
  auto tokens = lexer.tokenize_all();
  Parser parser(std::move(tokens));
  auto stmts = parser.parse_program();
  ASSERT_FALSE(parser.has_errors());

  auto *let = static_cast<LetStmt *>(stmts[0].get());
  let->init = OperatorResolver::resolve_expr(
      std::move(let->init), symbol_table.symbol_table(), errors);

  // Root should be: Binary(+, ...)
  ASSERT_EQ(let->init->kind, ExprKind::Binary);
  auto *binary = static_cast<BinaryExpr *>(let->init.get());
  EXPECT_EQ(binary->op, "+");

  // Left should be: Unary(-, IntLiteral(5))
  ASSERT_EQ(binary->left->kind, ExprKind::Unary);
  auto *unary = static_cast<UnaryExpr *>(binary->left.get());
  EXPECT_EQ(unary->op, "-");
  EXPECT_EQ(unary->position, OpPosition::Prefix);
  EXPECT_EQ(unary->operand->kind, ExprKind::IntLiteral);

  // Right should be: IntLiteral(10)
  EXPECT_EQ(binary->right->kind, ExprKind::IntLiteral);
}
TEST_F(OperatorTest, ResolvePostfixOperator) {
  // Load prelude
  ASSERT_TRUE(builder.load_prelude(STDLIB_DIR "/prelude.pec", symbol_table));

  // Define postfix ++ operator with body (n += 1)
  std::string source = R"(
operator postfix ++(n: i32) : i32 {
  return n += 1;
}

let x = 5++;
)";
  Lexer lexer(source);
  auto tokens = lexer.tokenize_all();
  Parser parser(std::move(tokens));
  auto stmts = parser.parse_program();
  ASSERT_FALSE(parser.has_errors());

  // Collect operator declarations
  builder.collect(stmts, symbol_table);

  // Then resolve let statement
  auto *let = static_cast<LetStmt *>(stmts[1].get());
  let->init = OperatorResolver::resolve_expr(
      std::move(let->init), symbol_table.symbol_table(), errors);

  // Should be: Unary(++, IntLiteral(5), Postfix)
  ASSERT_EQ(let->init->kind, ExprKind::Unary);
  auto *unary = static_cast<UnaryExpr *>(let->init.get());
  EXPECT_EQ(unary->op, "++");
  EXPECT_EQ(unary->position, OpPosition::Postfix);
  EXPECT_EQ(unary->operand->kind, ExprKind::IntLiteral);
  auto *lit = static_cast<IntLiteralExpr *>(unary->operand.get());
  EXPECT_EQ(lit->value, "5");
}

TEST_F(OperatorTest, ResolvePostfixWithInfix) {
  // Load prelude
  ASSERT_TRUE(builder.load_prelude(STDLIB_DIR "/prelude.pec", symbol_table));

  // Define postfix ++ operator
  std::string source = R"(
operator postfix ++(n: i32) : i32 {
  return n += 1;
}

let x = 5++ + 3;
)";
  Lexer lexer(source);
  auto tokens = lexer.tokenize_all();
  Parser parser(std::move(tokens));
  auto stmts = parser.parse_program();
  ASSERT_FALSE(parser.has_errors());

  // Collect operator declarations
  builder.collect(stmts, symbol_table);

  // Resolve let statement
  auto *let = static_cast<LetStmt *>(stmts[1].get());
  let->init = OperatorResolver::resolve_expr(
      std::move(let->init), symbol_table.symbol_table(), errors);

  // Should be: Binary(+, Unary(++, IntLiteral(5), Postfix), IntLiteral(3))
  ASSERT_EQ(let->init->kind, ExprKind::Binary);
  auto *binary = static_cast<BinaryExpr *>(let->init.get());
  EXPECT_EQ(binary->op, "+");

  // Left should be: Unary(++, IntLiteral(5), Postfix)
  ASSERT_EQ(binary->left->kind, ExprKind::Unary);
  auto *unary = static_cast<UnaryExpr *>(binary->left.get());
  EXPECT_EQ(unary->op, "++");
  EXPECT_EQ(unary->position, OpPosition::Postfix);
  EXPECT_EQ(unary->operand->kind, ExprKind::IntLiteral);

  // Right should be: IntLiteral(3)
  EXPECT_EQ(binary->right->kind, ExprKind::IntLiteral);
}

TEST_F(OperatorTest, ResolveComplexMixedOperators) {
  // Load prelude
  ASSERT_TRUE(builder.load_prelude(STDLIB_DIR "/prelude.pec", symbol_table));

  // Define prefix and postfix ++ operator
  std::string source = R"(
operator prefix ++(n: i32) : i32 {
  return n += 1;
}

operator postfix ++(n: i32) : i32 {
  return n += 1;
}

let x = 1;
let y = 2;
let result = ++ x ++ ++ + ++ ++ y ++;
)";
  Lexer lexer(source);
  auto tokens = lexer.tokenize_all();
  Parser parser(std::move(tokens));
  auto stmts = parser.parse_program();

  // Print errors if any
  if (parser.has_errors()) {
    for (const auto &err : parser.errors()) {
      std::cerr << "Parser error: " << err.message << "\n";
    }
  }
  ASSERT_FALSE(parser.has_errors());

  // Collect operator declarations
  builder.collect(stmts, symbol_table);

  // Resolve let statement for result
  auto *let = static_cast<LetStmt *>(stmts[4].get());
  let->init = OperatorResolver::resolve_expr(
      std::move(let->init), symbol_table.symbol_table(), errors);

  // Print errors if any
  if (builder.has_errors()) {
    for (const auto &err : builder.errors()) {
      std::cerr << "Semantic error: " << err.message << "\n";
    }
  }
  ASSERT_FALSE(builder.has_errors());

  // Should be: Binary(+, left_part, right_part)
  ASSERT_EQ(let->init->kind, ExprKind::Binary);
  auto *binary = static_cast<BinaryExpr *>(let->init.get());
  EXPECT_EQ(binary->op, "+");

  // Left part: ++ x ++ ++
  // Greedy parse: prefix(++), operand(x), postfix(++), postfix(++)
  // Structure: ((++prefix x)++)++)  - outer to inner: postfix, postfix, prefix
  ASSERT_EQ(binary->left->kind, ExprKind::Unary);
  auto *left_outer_postfix = static_cast<UnaryExpr *>(binary->left.get());
  EXPECT_EQ(left_outer_postfix->op, "++");
  EXPECT_EQ(left_outer_postfix->position, OpPosition::Postfix);

  ASSERT_EQ(left_outer_postfix->operand->kind, ExprKind::Unary);
  auto *left_inner_postfix =
      static_cast<UnaryExpr *>(left_outer_postfix->operand.get());
  EXPECT_EQ(left_inner_postfix->op, "++");
  EXPECT_EQ(left_inner_postfix->position, OpPosition::Postfix);

  ASSERT_EQ(left_inner_postfix->operand->kind, ExprKind::Unary);
  auto *left_prefix =
      static_cast<UnaryExpr *>(left_inner_postfix->operand.get());
  EXPECT_EQ(left_prefix->op, "++");
  EXPECT_EQ(left_prefix->position, OpPosition::Prefix);
  EXPECT_EQ(left_prefix->operand->kind, ExprKind::Identifier);

  // Right part: ++ ++ y ++
  // Greedy parse: prefix(++), prefix(++), operand(y), postfix(++)
  // Structure: (++prefix (++prefix y))++postfix  - 3 Unary layers
  ASSERT_EQ(binary->right->kind, ExprKind::Unary);
  auto *right_outer_postfix = static_cast<UnaryExpr *>(binary->right.get());
  EXPECT_EQ(right_outer_postfix->op, "++");
  EXPECT_EQ(right_outer_postfix->position, OpPosition::Postfix);

  ASSERT_EQ(right_outer_postfix->operand->kind, ExprKind::Unary);
  auto *right_prefix1 =
      static_cast<UnaryExpr *>(right_outer_postfix->operand.get());
  EXPECT_EQ(right_prefix1->op, "++");
  EXPECT_EQ(right_prefix1->position, OpPosition::Prefix);

  ASSERT_EQ(right_prefix1->operand->kind, ExprKind::Unary);
  auto *right_prefix2 = static_cast<UnaryExpr *>(right_prefix1->operand.get());
  EXPECT_EQ(right_prefix2->op, "++");
  EXPECT_EQ(right_prefix2->position, OpPosition::Prefix);

  // Bottom identifier
  EXPECT_EQ(right_prefix2->operand->kind, ExprKind::Identifier);
}

TEST_F(OperatorTest, RejectMixedAssociativity) {
  // Load prelude
  ASSERT_TRUE(builder.load_prelude(STDLIB_DIR "/prelude.pec", symbol_table));

  // Define two operators with same precedence but different associativity
  std::string source = R"(
operator infix +< (a: i32, b: i32) : i32 prec 70 {}
operator infix +> (a: i32, b: i32) : i32 prec 70 assoc_right {}

let x = a +< b +> c;
)";
  Lexer lexer(source);
  auto tokens = lexer.tokenize_all();
  Parser parser(std::move(tokens));
  auto stmts = parser.parse_program();

  ASSERT_FALSE(parser.has_errors());

  // Collect operator declarations
  builder.collect(stmts, symbol_table);

  // Try to resolve - should fail with mixed associativity error
  auto *let = static_cast<LetStmt *>(stmts[2].get());
  let->init = OperatorResolver::resolve_expr(
      std::move(let->init), symbol_table.symbol_table(), errors);

  // Should have errors
  ASSERT_TRUE(!errors.empty());

  // Check error message contains relevant info
  // errors vector already available
  ASSERT_GT(errors.size(), 0);
  EXPECT_TRUE(errors[0].find("Mixed associativity") != std::string::npos);
  EXPECT_TRUE(errors[0].find("precedence 70") != std::string::npos);

  // Check error location points to the conflicting operator
  // Line/column info embedded in error string  // "let x = a +< b +> c;" is
  // line 5
  // "+>" starts at column 18
}
