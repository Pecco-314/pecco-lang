#include "lexer.hpp"
#include "operator_resolver.hpp"
#include "parser.hpp"
#include "scope.hpp"
#include "symbol_table_builder.hpp"
#include "type_checker.hpp"

#include <gtest/gtest.h>

using namespace pecco;

class TypeCheckerTest : public ::testing::Test {
protected:
  void SetUp() override {
    // Load prelude for each test
    builder.load_prelude(STDLIB_DIR "/prelude.pec", symbols);
  }

  ScopedSymbolTable symbols;
  SymbolTableBuilder builder;
  TypeChecker checker;

  bool parse_and_check(const std::string &code) {
    Lexer lexer(code);
    auto tokens = lexer.tokenize_all();
    Parser parser(std::move(tokens));
    stmts = parser.parse_program();

    if (parser.has_errors()) {
      std::cerr << "Parser errors:\n";
      for (const auto &err : parser.errors()) {
        std::cerr << "  " << err.message << "\n";
      }
      return false;
    }

    // Build symbol table
    if (!builder.collect(stmts, symbols)) {
      std::cerr << "Symbol table builder errors:\n";
      for (const auto &err : builder.errors()) {
        std::cerr << "  " << err.message << "\n";
      }
      return false;
    }

    // Resolve operators
    std::vector<std::string> resolve_errors;
    for (auto &stmt : stmts) {
      OperatorResolver::resolve_stmt(stmt.get(), symbols.symbol_table(),
                                     resolve_errors);
    }

    if (!resolve_errors.empty()) {
      std::cerr << "Operator resolver errors:\n";
      for (const auto &err : resolve_errors) {
        std::cerr << "  " << err << "\n";
      }
      return false;
    }

    // Type check
    bool result = checker.check(stmts, symbols);
    if (!result) {
      std::cerr << "Type checker errors:\n";
      for (const auto &err : checker.errors()) {
        std::cerr << "  " << err.message << "\n";
      }
    }
    return result;
  }

  std::vector<StmtPtr> stmts;
};

TEST_F(TypeCheckerTest, InferLiteralTypes) {
  std::string code = R"(
    func test() : i32 {
      let a = 42;
      let b = 3.14;
      let c = true;
      let d = "hello";
      return a;
    }
  )";

  ASSERT_TRUE(parse_and_check(code));
  EXPECT_FALSE(checker.has_errors());
}

TEST_F(TypeCheckerTest, InferBinaryOperatorTypes) {
  std::string code = R"(
    func test() : i32 {
      let x = 10 + 20;
      let y = 5 * 3;
      return x + y;
    }
  )";

  ASSERT_TRUE(parse_and_check(code));
  EXPECT_FALSE(checker.has_errors());
}

TEST_F(TypeCheckerTest, TypeMismatchInLetStatement) {
  std::string code = R"(
    func test() : i32 {
      let x : i32 = 3.14;
      return x;
    }
  )";

  ASSERT_FALSE(parse_and_check(code));
  ASSERT_TRUE(checker.has_errors());

  auto errors = checker.errors();
  ASSERT_EQ(errors.size(), 1);
  EXPECT_NE(errors[0].message.find("Type mismatch"), std::string::npos);
  EXPECT_NE(errors[0].message.find("i32"), std::string::npos);
  EXPECT_NE(errors[0].message.find("f64"), std::string::npos);
}

TEST_F(TypeCheckerTest, CorrectTypeAnnotation) {
  std::string code = R"(
    func test() : i32 {
      let x : i32 = 42;
      let y : f64 = 3.14;
      let z : bool = true;
      return x;
    }
  )";

  ASSERT_TRUE(parse_and_check(code));
  EXPECT_FALSE(checker.has_errors());
}

TEST_F(TypeCheckerTest, BoolConditionInIf) {
  std::string code = R"(
    func test(n : i32) : i32 {
      if n > 0 {
        return 1;
      } else {
        return 0;
      }
    }
  )";

  ASSERT_TRUE(parse_and_check(code));
  EXPECT_FALSE(checker.has_errors());
}

TEST_F(TypeCheckerTest, NonBoolConditionInIf) {
  std::string code = R"(
    func test(n : i32) : i32 {
      if 42 {
        return 1;
      } else {
        return 0;
      }
    }
  )";

  ASSERT_FALSE(parse_and_check(code));
  ASSERT_TRUE(checker.has_errors());

  auto errors = checker.errors();
  ASSERT_EQ(errors.size(), 1);
  EXPECT_NE(errors[0].message.find("If condition must be 'bool'"),
            std::string::npos);
}

TEST_F(TypeCheckerTest, BoolConditionInWhile) {
  std::string code = R"(
    func test(n : i32) : i32 {
      while n > 0 {
        n = n - 1;
      }
      return n;
    }
  )";

  ASSERT_TRUE(parse_and_check(code));
  EXPECT_FALSE(checker.has_errors());
}

TEST_F(TypeCheckerTest, NonBoolConditionInWhile) {
  std::string code = R"(
    func test(n : i32) : i32 {
      while 100 {
        n = n - 1;
      }
      return n;
    }
  )";

  ASSERT_FALSE(parse_and_check(code));
  ASSERT_TRUE(checker.has_errors());

  auto errors = checker.errors();
  ASSERT_EQ(errors.size(), 1);
  EXPECT_NE(errors[0].message.find("While condition must be 'bool'"),
            std::string::npos);
}

TEST_F(TypeCheckerTest, FunctionCallTypeInference) {
  std::string code = R"(
    func add(a : i32, b : i32) : i32 {
      return a + b;
    }

    func test() : i32 {
      let result = add(10, 20);
      return result;
    }
  )";

  ASSERT_TRUE(parse_and_check(code));
  EXPECT_FALSE(checker.has_errors());
}

TEST_F(TypeCheckerTest, ComplexExpressionTypes) {
  std::string code = R"(
    func factorial(n : i32) : i32 {
      if n == 0 {
        return 1;
      } else {
        return n * factorial(n - 1);
      }
    }

    func test() : i32 {
      let x = 42;
      let result = factorial(5) + x;
      return result;
    }
  )";

  ASSERT_TRUE(parse_and_check(code));
  EXPECT_FALSE(checker.has_errors());
}

TEST_F(TypeCheckerTest, MultipleTypeErrors) {
  std::string code = R"(
    func test() : i32 {
      let x : i32 = 3.14;
      let y : f64 = 100;
      if x {
        return 0;
      }
      return 1;
    }
  )";

  ASSERT_FALSE(parse_and_check(code));
  ASSERT_TRUE(checker.has_errors());

  auto errors = checker.errors();
  EXPECT_GE(errors.size(), 2); // At least 2 errors
}

TEST_F(TypeCheckerTest, ComparisonOperatorsReturnBool) {
  std::string code = R"(
    func test(a : i32, b : i32) : bool {
      let eq = a == b;
      let ne = a != b;
      let lt = a < b;
      let gt = a > b;
      return eq;
    }
  )";

  ASSERT_TRUE(parse_and_check(code));
  EXPECT_FALSE(checker.has_errors());
}

TEST_F(TypeCheckerTest, LogicalOperatorsWithBool) {
  std::string code = R"(
    func test(a : bool, b : bool) : bool {
      let and_result = a && b;
      let or_result = a || b;
      let not_result = !a;
      return and_result || or_result;
    }
  )";

  ASSERT_TRUE(parse_and_check(code));
  EXPECT_FALSE(checker.has_errors());
}

TEST_F(TypeCheckerTest, VariableTypePropagation) {
  std::string code = R"(
    func test() : i32 {
      let x = 42;
      let y = x;
      return y;
    }
  )";

  ASSERT_TRUE(parse_and_check(code));
  EXPECT_FALSE(checker.has_errors());
}

TEST_F(TypeCheckerTest, CrossScopeTypePropagation) {
  std::string code = R"(
    func test() : i32 {
      let x = 42;
      if true {
        let y = x;
        return y;
      }
      return 0;
    }
  )";

  ASSERT_TRUE(parse_and_check(code));
  EXPECT_FALSE(checker.has_errors());
}

TEST_F(TypeCheckerTest, CrossScopeTypeError) {
  std::string code = R"(
    func test() : i32 {
      let x = 3.14;
      if true {
        let y : i32 = x;
        return y;
      }
      return 0;
    }
  )";

  ASSERT_FALSE(parse_and_check(code));
  ASSERT_TRUE(checker.has_errors());

  auto errors = checker.errors();
  ASSERT_EQ(errors.size(), 1);
  EXPECT_NE(errors[0].message.find("Type mismatch"), std::string::npos);
  EXPECT_NE(errors[0].message.find("f64"), std::string::npos);
}

TEST_F(TypeCheckerTest, NestedScopeVariables) {
  std::string code = R"(
    func test() : i32 {
      let a = 10;
      {
        let b = 20;
        {
          let c = a + b;
          return c;
        }
      }
    }
  )";

  ASSERT_TRUE(parse_and_check(code));
  EXPECT_FALSE(checker.has_errors());
}

TEST_F(TypeCheckerTest, FunctionParameterTypePropagation) {
  std::string code = R"(
    func add(x : i32, y : i32) : i32 {
      let sum = x + y;
      return sum;
    }
  )";

  ASSERT_TRUE(parse_and_check(code));
  EXPECT_FALSE(checker.has_errors());
}

TEST_F(TypeCheckerTest, ParameterTypeError) {
  std::string code = R"(
    func test(x : i32) : f64 {
      let y : f64 = x;
      return y;
    }
  )";

  ASSERT_FALSE(parse_and_check(code));
  ASSERT_TRUE(checker.has_errors());

  auto errors = checker.errors();
  ASSERT_EQ(errors.size(), 1);
  EXPECT_NE(errors[0].message.find("Type mismatch"), std::string::npos);
}

TEST_F(TypeCheckerTest, ChainedVariableAssignment) {
  std::string code = R"(
    func test() : i32 {
      let a = 100;
      let b = a;
      let c = b;
      let d = c;
      return d;
    }
  )";

  ASSERT_TRUE(parse_and_check(code));
  EXPECT_FALSE(checker.has_errors());
}

TEST_F(TypeCheckerTest, MixedTypeExpression) {
  std::string code = R"(
    func test() : i32 {
      let x = 10;
      let y = 20;
      let result = (x + y) * 2;
      return result;
    }
  )";

  ASSERT_TRUE(parse_and_check(code));
  EXPECT_FALSE(checker.has_errors());
}
