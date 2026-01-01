#include "lexer.hpp"
#include "parser.hpp"
#include "symbol_table_builder.hpp"
#include <gtest/gtest.h>

using namespace pecco;

class SymbolTableTest : public ::testing::Test {
protected:
  ScopedSymbolTable symbols;
  SymbolTableBuilder builder;
};

TEST_F(SymbolTableTest, CollectGlobalFunction) {
  std::string code = R"(
    func add(x : i32, y : i32) : i32 {
      return x + y;
    }
  )";

  Lexer lexer(code);
  auto tokens = lexer.tokenize_all();
  Parser parser(std::move(tokens));
  auto stmts = parser.parse_program();

  ASSERT_FALSE(parser.has_errors());
  ASSERT_TRUE(builder.collect(stmts, symbols));

  // Check function was collected
  EXPECT_TRUE(symbols.has_function("add"));
  auto funcs = symbols.find_functions("add");
  ASSERT_EQ(funcs.size(), 1);
  EXPECT_EQ(funcs[0].name, "add");
  EXPECT_EQ(funcs[0].param_types.size(), 2);
  EXPECT_EQ(funcs[0].param_types[0], "i32");
  EXPECT_EQ(funcs[0].param_types[1], "i32");
  EXPECT_EQ(funcs[0].return_type, "i32");
  EXPECT_EQ(funcs[0].origin, SymbolOrigin::User);
}

TEST_F(SymbolTableTest, CollectVariablesInScope) {
  std::string code = R"(
    func test() : i32 {
      let x = 10;
      let y = 20;
      return x + y;
    }
  )";

  Lexer lexer(code);
  auto tokens = lexer.tokenize_all();
  Parser parser(std::move(tokens));
  auto stmts = parser.parse_program();

  ASSERT_TRUE(builder.collect(stmts, symbols));

  // Check scope hierarchy
  auto *root = symbols.root_scope();
  ASSERT_NE(root, nullptr);

  // Should have one child scope (function test)
  EXPECT_EQ(root->children().size(), 1);
  auto *func_scope = root->children()[0];
  EXPECT_EQ(func_scope->description(), "function test");

  // Function scope should have one child (the block)
  EXPECT_EQ(func_scope->children().size(), 1);
  auto *block_scope = func_scope->children()[0];

  // Block should have 2 variables
  auto vars = block_scope->get_local_variables();
  EXPECT_EQ(vars.size(), 2);

  // Check variable names (order might vary)
  std::set<std::string> var_names;
  for (const auto &var : vars) {
    var_names.insert(var.name);
  }
  EXPECT_TRUE(var_names.count("x") > 0);
  EXPECT_TRUE(var_names.count("y") > 0);
}

TEST_F(SymbolTableTest, CollectNestedScopes) {
  std::string code = R"(
    func outer() : i32 {
      let a = 1;
      {
        let b = 2;
        {
          let c = 3;
        }
      }
      return a;
    }
  )";

  Lexer lexer(code);
  auto tokens = lexer.tokenize_all();
  Parser parser(std::move(tokens));
  auto stmts = parser.parse_program();

  ASSERT_TRUE(builder.collect(stmts, symbols));

  auto *root = symbols.root_scope();
  ASSERT_EQ(root->children().size(), 1);

  auto *func_scope = root->children()[0];
  EXPECT_EQ(func_scope->children().size(), 1);

  auto *block1 = func_scope->children()[0];
  auto vars1 = block1->get_local_variables();
  EXPECT_EQ(vars1.size(), 1);
  EXPECT_EQ(vars1[0].name, "a");

  ASSERT_EQ(block1->children().size(), 1);
  auto *block2 = block1->children()[0];
  auto vars2 = block2->get_local_variables();
  EXPECT_EQ(vars2.size(), 1);
  EXPECT_EQ(vars2[0].name, "b");

  ASSERT_EQ(block2->children().size(), 1);
  auto *block3 = block2->children()[0];
  auto vars3 = block3->get_local_variables();
  EXPECT_EQ(vars3.size(), 1);
  EXPECT_EQ(vars3[0].name, "c");
}

TEST_F(SymbolTableTest, DetectVariableRedefinition) {
  std::string code = R"(
    func test() : i32 {
      let x = 10;
      let x = 20;
      return x;
    }
  )";

  Lexer lexer(code);
  auto tokens = lexer.tokenize_all();
  Parser parser(std::move(tokens));
  auto stmts = parser.parse_program();

  ASSERT_FALSE(builder.collect(stmts, symbols));
  ASSERT_TRUE(builder.has_errors());

  auto errors = builder.errors();
  ASSERT_EQ(errors.size(), 1);
  EXPECT_NE(errors[0].message.find("already defined"), std::string::npos);
}

TEST_F(SymbolTableTest, DetectNestedFunction) {
  std::string code = R"(
    func outer() : i32 {
      func inner() : i32 {
        return 42;
      }
      return inner();
    }
  )";

  Lexer lexer(code);
  auto tokens = lexer.tokenize_all();
  Parser parser(std::move(tokens));
  auto stmts = parser.parse_program();

  ASSERT_FALSE(builder.collect(stmts, symbols));
  ASSERT_TRUE(builder.has_errors());

  auto errors = builder.errors();
  ASSERT_EQ(errors.size(), 1);
  EXPECT_NE(errors[0].message.find("Nested function"), std::string::npos);
}

TEST_F(SymbolTableTest, AllowShadowingInDifferentScopes) {
  std::string code = R"(
    func test() : i32 {
      let x = 10;
      {
        let x = 20;
        return x;
      }
    }
  )";

  Lexer lexer(code);
  auto tokens = lexer.tokenize_all();
  Parser parser(std::move(tokens));
  auto stmts = parser.parse_program();

  // Shadowing should be allowed (no error)
  ASSERT_TRUE(builder.collect(stmts, symbols));
  EXPECT_FALSE(builder.has_errors());
}

TEST_F(SymbolTableTest, CollectFunctionParameters) {
  std::string code = R"(
    func add(a : i32, b : i32) : i32 {
      return a + b;
    }
  )";

  Lexer lexer(code);
  auto tokens = lexer.tokenize_all();
  Parser parser(std::move(tokens));
  auto stmts = parser.parse_program();

  ASSERT_TRUE(builder.collect(stmts, symbols));

  auto *root = symbols.root_scope();
  ASSERT_EQ(root->children().size(), 1);

  auto *func_scope = root->children()[0];
  auto params = func_scope->get_local_variables();

  // Parameters should be in function scope
  EXPECT_EQ(params.size(), 2);

  std::set<std::string> param_names;
  for (const auto &p : params) {
    param_names.insert(p.name);
    EXPECT_EQ(p.type, "i32");
  }
  EXPECT_TRUE(param_names.count("a") > 0);
  EXPECT_TRUE(param_names.count("b") > 0);
}

TEST_F(SymbolTableTest, DetectMissingParameterType) {
  std::string code = R"(
    func compute(first : i32, second, third : i32) : i32 {
      return first + third;
    }
  )";

  Lexer lexer(code);
  auto tokens = lexer.tokenize_all();
  Parser parser(std::move(tokens));
  auto stmts = parser.parse_program();

  // Should fail: parameter 'second' missing type
  ASSERT_FALSE(builder.collect(stmts, symbols));
  ASSERT_TRUE(builder.has_errors());

  auto errors = builder.errors();
  ASSERT_EQ(errors.size(), 1);
  EXPECT_NE(errors[0].message.find("generics unimplemented"),
            std::string::npos);
  EXPECT_NE(errors[0].message.find("second"), std::string::npos);
  // Verify error points to the parameter location
  EXPECT_EQ(errors[0].column, 31);
}

TEST_F(SymbolTableTest, CollectOperatorDeclaration) {
  std::string code = R"(
    operator prefix <+> (x : i32) : i32;
    operator infix <*> (a : i32, b : i32) : i32 prec 80;
  )";

  Lexer lexer(code);
  auto tokens = lexer.tokenize_all();
  Parser parser(std::move(tokens));
  auto stmts = parser.parse_program();

  ASSERT_TRUE(builder.collect(stmts, symbols));

  // Check prefix operator
  auto prefix_op = symbols.find_operator("<+>", OpPosition::Prefix);
  ASSERT_TRUE(prefix_op.has_value());
  EXPECT_EQ(prefix_op->op, "<+>");
  EXPECT_EQ(prefix_op->signature.param_types.size(), 1);
  EXPECT_EQ(prefix_op->signature.return_type, "i32");
  EXPECT_EQ(prefix_op->origin, SymbolOrigin::User);

  // Check infix operator
  auto infix_op = symbols.find_operator("<*>", OpPosition::Infix);
  ASSERT_TRUE(infix_op.has_value());
  EXPECT_EQ(infix_op->op, "<*>");
  EXPECT_EQ(infix_op->signature.param_types.size(), 2);
  EXPECT_EQ(infix_op->precedence, 80);
  EXPECT_EQ(infix_op->origin, SymbolOrigin::User);
}

TEST_F(SymbolTableTest, PreludeSymbolsMarked) {
  // Load prelude
  ASSERT_TRUE(builder.load_prelude(STDLIB_DIR "/prelude.pec", symbols));

  // Check that prelude functions are marked
  auto write_funcs = symbols.find_functions("write");
  ASSERT_GT(write_funcs.size(), 0);
  for (const auto &func : write_funcs) {
    EXPECT_EQ(func.origin, SymbolOrigin::Prelude);
  }

  // Check that prelude operators are marked
  auto plus_op = symbols.find_operator("+", OpPosition::Infix);
  ASSERT_TRUE(plus_op.has_value());
  EXPECT_EQ(plus_op->origin, SymbolOrigin::Prelude);
}

TEST_F(SymbolTableTest, MultipleFunctionsAndScopes) {
  std::string code = R"(
    func func1() : i32 {
      let a = 1;
      return a;
    }

    func func2() : i32 {
      let b = 2;
      return b;
    }
  )";

  Lexer lexer(code);
  auto tokens = lexer.tokenize_all();
  Parser parser(std::move(tokens));
  auto stmts = parser.parse_program();

  ASSERT_TRUE(builder.collect(stmts, symbols));

  // Check both functions are collected
  EXPECT_TRUE(symbols.has_function("func1"));
  EXPECT_TRUE(symbols.has_function("func2"));

  // Check scope hierarchy
  auto *root = symbols.root_scope();
  EXPECT_EQ(root->children().size(), 2);

  // Each function has its own scope
  EXPECT_EQ(root->children()[0]->description(), "function func1");
  EXPECT_EQ(root->children()[1]->description(), "function func2");
}
