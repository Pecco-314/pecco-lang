#include <gtest/gtest.h>

#include "codegen.hpp"
#include "lexer.hpp"
#include "operator_resolver.hpp"
#include "parser.hpp"
#include "symbol_table_builder.hpp"

#include <fstream>
#include <regex>

namespace {

// Helper to compile source code to IR
std::string compileToIR(const std::string &source) {
  pecco::Lexer lexer(source);
  auto tokens = lexer.tokenize_all();

  pecco::Parser parser(std::move(tokens));
  auto stmts = parser.parse_program();
  if (parser.has_errors()) {
    return "";
  }

  pecco::ScopedSymbolTable symbols;
  pecco::SymbolTableBuilder builder;

  // Load prelude first
  std::string prelude_path = std::string(STDLIB_DIR) + "/prelude.pec";
  std::ifstream prelude_file(prelude_path);
  if (prelude_file.is_open()) {
    std::string prelude_source((std::istreambuf_iterator<char>(prelude_file)),
                               std::istreambuf_iterator<char>());
    pecco::Lexer prelude_lexer(prelude_source);
    auto prelude_tokens = prelude_lexer.tokenize_all();
    pecco::Parser prelude_parser(std::move(prelude_tokens));
    auto prelude_stmts = prelude_parser.parse_program();
    if (!prelude_parser.has_errors()) {
      builder.collect(prelude_stmts, symbols);
    }
  }

  // Then collect user code
  if (!builder.collect(stmts, symbols)) {
    return "";
  }

  std::vector<std::string> resolve_errors;
  for (auto &stmt : stmts) {
    pecco::OperatorResolver::resolve_stmt(stmt.get(), symbols.symbol_table(),
                                          resolve_errors);
  }
  if (!resolve_errors.empty()) {
    return "";
  }

  pecco::CodeGen codegen("test_module");
  if (!codegen.generate(stmts, symbols)) {
    return "";
  }

  return codegen.get_ir();
}

// Helper to check if IR contains a pattern
bool irContains(const std::string &ir, const std::string &pattern) {
  return ir.find(pattern) != std::string::npos;
}

// Helper to check if IR matches regex
bool irMatches(const std::string &ir, const std::string &pattern) {
  std::regex re(pattern);
  return std::regex_search(ir, re);
}

// ===== Basic Literals =====

TEST(CodeGenTest, IntLiteral) {
  std::string source = "let x = 42;";
  std::string ir = compileToIR(source);

  ASSERT_FALSE(ir.empty());
  EXPECT_TRUE(irContains(ir, "i32 42"));
  EXPECT_TRUE(irContains(ir, "alloca i32"));
  EXPECT_TRUE(irContains(ir, "store i32 42"));
}

TEST(CodeGenTest, FloatLiteral) {
  std::string source = "let x = 3.14;";
  std::string ir = compileToIR(source);

  ASSERT_FALSE(ir.empty());
  EXPECT_TRUE(irContains(ir, "double"));
  EXPECT_TRUE(irContains(ir, "3.14"));
  EXPECT_TRUE(irContains(ir, "alloca double"));
}

TEST(CodeGenTest, BoolLiteral) {
  std::string source = "let flag = true;";
  std::string ir = compileToIR(source);

  ASSERT_FALSE(ir.empty());
  EXPECT_TRUE(irContains(ir, "i1"));
  EXPECT_TRUE(irContains(ir, "alloca i1"));
  EXPECT_TRUE(irMatches(ir, "store i1 (true|1)"));
}

TEST(CodeGenTest, StringLiteral) {
  std::string source = R"(let msg = "Hello, World!";)";
  std::string ir = compileToIR(source);

  ASSERT_FALSE(ir.empty());
  EXPECT_TRUE(irContains(ir, "ptr"));
  EXPECT_TRUE(irContains(ir, "Hello, World!"));
}

// ===== Arithmetic Operations =====

TEST(CodeGenTest, IntAddition) {
  std::string source = R"(
    let a = 10;
    let b = 20;
    let result = a + b;
  )";
  std::string ir = compileToIR(source);

  ASSERT_FALSE(ir.empty());
  EXPECT_TRUE(irContains(ir, "add i32") || irContains(ir, "add nsw i32"));
}

TEST(CodeGenTest, IntSubtraction) {
  std::string source = R"(
    let a = 100;
    let b = 30;
    let result = a - b;
  )";
  std::string ir = compileToIR(source);

  ASSERT_FALSE(ir.empty());
  EXPECT_TRUE(irContains(ir, "sub i32") || irContains(ir, "sub nsw i32"));
}

TEST(CodeGenTest, IntMultiplication) {
  std::string source = R"(
    let a = 5;
    let b = 6;
    let result = a * b;
  )";
  std::string ir = compileToIR(source);

  ASSERT_FALSE(ir.empty());
  EXPECT_TRUE(irContains(ir, "mul i32") || irContains(ir, "mul nsw i32"));
}

TEST(CodeGenTest, IntDivision) {
  std::string source = R"(
    let a = 100;
    let b = 4;
    let result = a / b;
  )";
  std::string ir = compileToIR(source);

  ASSERT_FALSE(ir.empty());
  EXPECT_TRUE(irContains(ir, "sdiv i32"));
}

TEST(CodeGenTest, IntModulo) {
  std::string source = R"(
    let a = 17;
    let b = 5;
    let result = a % b;
  )";
  std::string ir = compileToIR(source);

  ASSERT_FALSE(ir.empty());
  EXPECT_TRUE(irContains(ir, "srem i32"));
}

TEST(CodeGenTest, FloatArithmetic) {
  std::string source = R"(
    let a = 3.14;
    let b = 2.86;
    let result = a + b;
  )";
  std::string ir = compileToIR(source);

  ASSERT_FALSE(ir.empty());
  EXPECT_TRUE(irContains(ir, "fadd double"));
}

TEST(CodeGenTest, ComplexArithmetic) {
  std::string source = R"(
    let a = 2;
    let b = 3;
    let c = 4;
    let d = 5;
    let result = a + b * c - d;
  )";
  std::string ir = compileToIR(source);

  ASSERT_FALSE(ir.empty());
  EXPECT_TRUE(irContains(ir, "mul i32") || irContains(ir, "mul nsw i32"));
  EXPECT_TRUE(irContains(ir, "add i32") || irContains(ir, "add nsw i32"));
  EXPECT_TRUE(irContains(ir, "sub i32") || irContains(ir, "sub nsw i32"));
}

// ===== Comparison Operations =====

TEST(CodeGenTest, IntEqual) {
  std::string source = R"(
    let a = 10;
    let b = 10;
    let result = a == b;
  )";
  std::string ir = compileToIR(source);

  ASSERT_FALSE(ir.empty());
  EXPECT_TRUE(irContains(ir, "icmp eq"));
}

TEST(CodeGenTest, IntNotEqual) {
  std::string source = R"(
    let a = 10;
    let b = 20;
    let result = a != b;
  )";
  std::string ir = compileToIR(source);

  ASSERT_FALSE(ir.empty());
  EXPECT_TRUE(irContains(ir, "icmp ne"));
}

TEST(CodeGenTest, IntLessThan) {
  std::string source = R"(
    let a = 5;
    let b = 10;
    let result = a < b;
  )";
  std::string ir = compileToIR(source);

  ASSERT_FALSE(ir.empty());
  EXPECT_TRUE(irContains(ir, "icmp slt"));
}

TEST(CodeGenTest, IntGreaterThan) {
  std::string source = R"(
    let a = 15;
    let b = 10;
    let result = a > b;
  )";
  std::string ir = compileToIR(source);

  ASSERT_FALSE(ir.empty());
  EXPECT_TRUE(irContains(ir, "icmp sgt"));
}

TEST(CodeGenTest, IntLessEqual) {
  std::string source = R"(
    let a = 10;
    let b = 10;
    let result = a <= b;
  )";
  std::string ir = compileToIR(source);

  ASSERT_FALSE(ir.empty());
  EXPECT_TRUE(irContains(ir, "icmp sle"));
}

TEST(CodeGenTest, IntGreaterEqual) {
  std::string source = R"(
    let a = 10;
    let b = 5;
    let result = a >= b;
  )";
  std::string ir = compileToIR(source);

  ASSERT_FALSE(ir.empty());
  EXPECT_TRUE(irContains(ir, "icmp sge"));
}

TEST(CodeGenTest, FloatComparison) {
  std::string source = R"(
    let a = 3.14;
    let b = 2.5;
    let result = a > b;
  )";
  std::string ir = compileToIR(source);

  ASSERT_FALSE(ir.empty());
  EXPECT_TRUE(irContains(ir, "fcmp ogt"));
}

// ===== Logical Operations =====

TEST(CodeGenTest, LogicalAnd) {
  std::string source = R"(
    let a = true;
    let b = false;
    let result = a && b;
  )";
  std::string ir = compileToIR(source);

  ASSERT_FALSE(ir.empty());
  EXPECT_TRUE(irContains(ir, "and i1"));
}

TEST(CodeGenTest, LogicalOr) {
  std::string source = R"(
    let a = true;
    let b = false;
    let result = a || b;
  )";
  std::string ir = compileToIR(source);

  ASSERT_FALSE(ir.empty());
  EXPECT_TRUE(irContains(ir, "or i1"));
}

TEST(CodeGenTest, LogicalNot) {
  std::string source = R"(
    let a = true;
    let result = !a;
  )";
  std::string ir = compileToIR(source);

  ASSERT_FALSE(ir.empty());
  EXPECT_TRUE(irContains(ir, "xor i1"));
}

// ===== Unary Operations =====

TEST(CodeGenTest, IntNegation) {
  std::string source = R"(
    let a = 42;
    let x = -a;
  )";
  std::string ir = compileToIR(source);

  ASSERT_FALSE(ir.empty());
  EXPECT_TRUE(irContains(ir, "sub i32 0") || irContains(ir, "sub nsw i32 0"));
}

TEST(CodeGenTest, FloatNegation) {
  std::string source = R"(
    let a = 3.14;
    let x = -a;
  )";
  std::string ir = compileToIR(source);

  ASSERT_FALSE(ir.empty());
  EXPECT_TRUE(irContains(ir, "fneg double"));
}

// ===== Variables =====

TEST(CodeGenTest, VariableDeclaration) {
  std::string source = "let x = 10;";
  std::string ir = compileToIR(source);

  ASSERT_FALSE(ir.empty());
  EXPECT_TRUE(irContains(ir, "alloca i32"));
  EXPECT_TRUE(irContains(ir, "store i32 10"));
}

TEST(CodeGenTest, VariableUsage) {
  std::string source = R"(
    let x = 10;
    let y = x;
  )";
  std::string ir = compileToIR(source);

  ASSERT_FALSE(ir.empty());
  EXPECT_TRUE(irContains(ir, "load i32"));
  EXPECT_TRUE(irContains(ir, "alloca i32"));
}

TEST(CodeGenTest, VariableArithmetic) {
  std::string source = R"(
    let a = 10;
    let b = 20;
    let sum = a + b;
  )";
  std::string ir = compileToIR(source);

  ASSERT_FALSE(ir.empty());
  EXPECT_TRUE(irContains(ir, "load i32"));
  EXPECT_TRUE(irContains(ir, "add i32"));
}

// ===== Functions =====

TEST(CodeGenTest, SimpleFunctionDefinition) {
  std::string source = R"(
    func add(a: i32, b: i32) : i32 {
      return a + b;
    }
  )";
  std::string ir = compileToIR(source);

  ASSERT_FALSE(ir.empty());
  EXPECT_TRUE(irContains(ir, "define i32 @add(i32"));
  EXPECT_TRUE(irContains(ir, "add i32"));
  EXPECT_TRUE(irContains(ir, "ret i32"));
}

TEST(CodeGenTest, FunctionWithMultipleParams) {
  std::string source = R"(
    func calculate(x: i32, y: i32, z: i32) : i32 {
      return x + y * z;
    }
  )";
  std::string ir = compileToIR(source);

  ASSERT_FALSE(ir.empty());
  EXPECT_TRUE(irContains(ir, "define i32 @calculate(i32"));
  EXPECT_TRUE(irContains(ir, "mul i32"));
  EXPECT_TRUE(irContains(ir, "add i32"));
}

TEST(CodeGenTest, FunctionCall) {
  std::string source = R"(
    func double(x: i32) : i32 {
      return x + x;
    }
    let result = double(21);
  )";
  std::string ir = compileToIR(source);

  ASSERT_FALSE(ir.empty());
  EXPECT_TRUE(irContains(ir, "define i32 @double"));
  EXPECT_TRUE(irContains(ir, "call i32 @double(i32 21)"));
}

TEST(CodeGenTest, RecursiveFunction) {
  std::string source = R"(
    func factorial(n: i32) : i32 {
      if (n <= 1) {
        return 1;
      }
      return n * factorial(n - 1);
    }
  )";
  std::string ir = compileToIR(source);

  ASSERT_FALSE(ir.empty());
  EXPECT_TRUE(irContains(ir, "define i32 @factorial"));
  EXPECT_TRUE(irContains(ir, "call i32 @factorial"));
  EXPECT_TRUE(irContains(ir, "icmp sle"));
  EXPECT_TRUE(irContains(ir, "br i1"));
}

TEST(CodeGenTest, VoidFunction) {
  std::string source = R"(
    func doSomething() : void {
      let x = 42;
    }
  )";
  std::string ir = compileToIR(source);

  ASSERT_FALSE(ir.empty());
  EXPECT_TRUE(irContains(ir, "define void @doSomething()"));
  EXPECT_TRUE(irContains(ir, "ret void"));
}

TEST(CodeGenTest, FloatFunction) {
  std::string source = R"(
    func average(a: f64, b: f64) : f64 {
      return (a + b) / 2.0;
    }
  )";
  std::string ir = compileToIR(source);

  ASSERT_FALSE(ir.empty());
  EXPECT_TRUE(irContains(ir, "define double @average(double"));
  EXPECT_TRUE(irContains(ir, "fadd double"));
  EXPECT_TRUE(irContains(ir, "fdiv double"));
}

// ===== Control Flow =====

TEST(CodeGenTest, SimpleIf) {
  std::string source = R"(
    let x = 10;
    if (x > 5) {
      let y = 20;
    }
  )";
  std::string ir = compileToIR(source);

  ASSERT_FALSE(ir.empty());
  EXPECT_TRUE(irContains(ir, "icmp sgt"));
  EXPECT_TRUE(irContains(ir, "br i1"));
  EXPECT_TRUE(irMatches(ir, "label %then"));
  EXPECT_TRUE(irMatches(ir, "label %ifcont"));
}

TEST(CodeGenTest, IfElse) {
  std::string source = R"(
    let x = 10;
    if (x > 5) {
      let a = 1;
    } else {
      let b = 2;
    }
  )";
  std::string ir = compileToIR(source);

  ASSERT_FALSE(ir.empty());
  EXPECT_TRUE(irContains(ir, "br i1"));
  EXPECT_TRUE(irMatches(ir, "label %then"));
  EXPECT_TRUE(irMatches(ir, "label %else"));
  EXPECT_TRUE(irMatches(ir, "label %ifcont"));
}

TEST(CodeGenTest, NestedIf) {
  std::string source = R"(
    let x = 10;
    if (x > 5) {
      if (x < 15) {
        let y = 1;
      }
    }
  )";
  std::string ir = compileToIR(source);

  ASSERT_FALSE(ir.empty());
  // Should have multiple conditional branches
  std::regex br_regex("br i1");
  std::ptrdiff_t count =
      std::distance(std::sregex_iterator(ir.begin(), ir.end(), br_regex),
                    std::sregex_iterator());
  EXPECT_GE(count, 2);
}

// ===== Control Flow =====

TEST(CodeGenTest, WhileLoop) {
  std::string source = R"(
    let i = 0;
    while (i < 10) {
      i = i + 1;
    }
  )";
  std::string ir = compileToIR(source);

  ASSERT_FALSE(ir.empty());
  EXPECT_TRUE(irMatches(ir, "label %loop.cond"));
  EXPECT_TRUE(irMatches(ir, "label %loop.body"));
  EXPECT_TRUE(irMatches(ir, "label %loop.end"));
  EXPECT_TRUE(irContains(ir, "icmp slt"));
  EXPECT_TRUE(irContains(ir, "br i1"));
}

TEST(CodeGenTest, NestedLoop) {
  std::string source = R"(
    let i = 0;
    while (i < 3) {
      let j = 0;
      while (j < 3) {
        j = j + 1;
      }
      i = i + 1;
    }
  )";
  std::string ir = compileToIR(source);

  ASSERT_FALSE(ir.empty());
  // Should have multiple loop structures
  std::regex loop_regex("label %loop");
  std::ptrdiff_t count =
      std::distance(std::sregex_iterator(ir.begin(), ir.end(), loop_regex),
                    std::sregex_iterator());
  EXPECT_GE(count, 4); // At least 2 loops * 2 labels each
}

TEST(CodeGenTest, FunctionWithReturn) {
  std::string source = R"(
    func max(a: i32, b: i32) : i32 {
      if (a > b) {
        return a;
      }
      return b;
    }
  )";
  std::string ir = compileToIR(source);

  ASSERT_FALSE(ir.empty());
  EXPECT_TRUE(irContains(ir, "ret i32"));
  EXPECT_TRUE(irContains(ir, "icmp sgt"));
}

TEST(CodeGenTest, EarlyReturn) {
  std::string source = R"(
    func check(n: i32) : i32 {
      if (n < 0) {
        return 0;
      }
      return n;
    }
  )";
  std::string ir = compileToIR(source);

  ASSERT_FALSE(ir.empty());
  // Should have multiple return statements
  std::regex ret_regex("ret i32");
  std::ptrdiff_t count =
      std::distance(std::sregex_iterator(ir.begin(), ir.end(), ret_regex),
                    std::sregex_iterator());
  EXPECT_GE(count, 2);
}

// ===== Complex Expressions =====

TEST(CodeGenTest, ComplexExpression) {
  std::string source = R"(
    let a = 10;
    let b = 20;
    let c = 30;
    let d = 5;
    let e = 2;
    let result = (a + b) * (c - d) / e;
  )";
  std::string ir = compileToIR(source);

  ASSERT_FALSE(ir.empty());
  EXPECT_TRUE(irContains(ir, "add i32") || irContains(ir, "add nsw i32"));
  EXPECT_TRUE(irContains(ir, "sub i32") || irContains(ir, "sub nsw i32"));
  EXPECT_TRUE(irContains(ir, "mul i32") || irContains(ir, "mul nsw i32"));
  EXPECT_TRUE(irContains(ir, "sdiv i32"));
}

TEST(CodeGenTest, BooleanExpression) {
  std::string source = R"(
    let a = 10;
    let b = 5;
    let c = 20;
    let d = 30;
    let e = false;
    let result = (a > b) && (c < d) || e;
  )";
  std::string ir = compileToIR(source);

  ASSERT_FALSE(ir.empty());
  EXPECT_TRUE(irContains(ir, "icmp sgt"));
  EXPECT_TRUE(irContains(ir, "icmp slt"));
  EXPECT_TRUE(irContains(ir, "and i1"));
  EXPECT_TRUE(irContains(ir, "or i1"));
}

TEST(CodeGenTest, MixedTypeExpression) {
  std::string source = R"(
    let a = 10;
    let b = 20;
    let result = (a + b) > 25;
  )";
  std::string ir = compileToIR(source);

  ASSERT_FALSE(ir.empty());
  EXPECT_TRUE(irContains(ir, "add i32"));
  EXPECT_TRUE(irContains(ir, "icmp sgt"));
}

// ===== External Functions (Prelude) =====

TEST(CodeGenTest, ExitFunction) {
  std::string source = "exit(42);";
  std::string ir = compileToIR(source);

  ASSERT_FALSE(ir.empty());
  EXPECT_TRUE(irContains(ir, "declare void @exit(i32)"));
  EXPECT_TRUE(irContains(ir, "call void @exit(i32 42)"));
}

TEST(CodeGenTest, WriteFunction) {
  std::string source = R"(write(1, "Hello", 5);)";
  std::string ir = compileToIR(source);

  ASSERT_FALSE(ir.empty());
  EXPECT_TRUE(irContains(ir, "declare i32 @write(i32, ptr, i32)"));
  EXPECT_TRUE(irContains(ir, "call i32 @write"));
  EXPECT_TRUE(irContains(ir, "Hello"));
}

// ===== Entry Point =====

TEST(CodeGenTest, EntryPointGeneration) {
  std::string source = "let x = 42;";
  std::string ir = compileToIR(source);

  ASSERT_FALSE(ir.empty());
  EXPECT_TRUE(irContains(ir, "define i32 @__pecco_entry()"));
  EXPECT_TRUE(irContains(ir, "ret i32 0"));
}

TEST(CodeGenTest, TopLevelStatements) {
  std::string source = R"(
    let x = 10;
    let y = 20;
    let z = x + y;
  )";
  std::string ir = compileToIR(source);

  ASSERT_FALSE(ir.empty());
  EXPECT_TRUE(irContains(ir, "define i32 @__pecco_entry()"));
  // All statements should be in entry function
  EXPECT_TRUE(irContains(ir, "alloca i32"));
}

// ===== Scoping =====

TEST(CodeGenTest, BlockScoping) {
  std::string source = R"(
    let x = 10;
    {
      let y = 20;
      let z = x + y;
    }
  )";
  std::string ir = compileToIR(source);

  ASSERT_FALSE(ir.empty());
  // Should have multiple allocas
  std::regex alloca_regex("alloca i32");
  std::ptrdiff_t count =
      std::distance(std::sregex_iterator(ir.begin(), ir.end(), alloca_regex),
                    std::sregex_iterator());
  EXPECT_GE(count, 3);
}

TEST(CodeGenTest, FunctionLocalVariables) {
  std::string source = R"(
    func test(n: i32) : i32 {
      let x = n + 1;
      let y = x * 2;
      return y;
    }
  )";
  std::string ir = compileToIR(source);

  ASSERT_FALSE(ir.empty());
  EXPECT_TRUE(irContains(ir, "define i32 @test"));
  // Should have allocas for parameters and local variables
  EXPECT_TRUE(irContains(ir, "alloca i32"));
}

// ===== Edge Cases =====

TEST(CodeGenTest, EmptyProgram) {
  std::string source = "";
  std::string ir = compileToIR(source);

  ASSERT_FALSE(ir.empty());
  EXPECT_TRUE(irContains(ir, "define i32 @__pecco_entry()"));
  EXPECT_TRUE(irContains(ir, "ret i32 0"));
}

TEST(CodeGenTest, OnlyFunctionDefinition) {
  std::string source = R"(
    func add(a: i32, b: i32) : i32 {
      return a + b;
    }
  )";
  std::string ir = compileToIR(source);

  ASSERT_FALSE(ir.empty());
  EXPECT_TRUE(irContains(ir, "define i32 @add"));
  EXPECT_TRUE(irContains(ir, "define i32 @__pecco_entry()"));
}

TEST(CodeGenTest, LargeInteger) {
  std::string source = "let x = 2147483647;"; // INT32_MAX
  std::string ir = compileToIR(source);

  ASSERT_FALSE(ir.empty());
  EXPECT_TRUE(irContains(ir, "2147483647"));
}

TEST(CodeGenTest, MultipleReturns) {
  std::string source = R"(
    func classify(n: i32) : i32 {
      if (n < 0) {
        return -1;
      }
      if (n > 0) {
        return 1;
      }
      return 0;
    }
  )";
  std::string ir = compileToIR(source);

  ASSERT_FALSE(ir.empty());
  std::regex ret_regex("ret i32");
  std::ptrdiff_t count =
      std::distance(std::sregex_iterator(ir.begin(), ir.end(), ret_regex),
                    std::sregex_iterator());
  EXPECT_GE(count, 3);
}

// ===== Integration Tests =====

TEST(CodeGenTest, FibonacciRecursive) {
  std::string source = R"(
    func fib(n: i32) : i32 {
      if (n <= 1) {
        return n;
      }
      return fib(n - 1) + fib(n - 2);
    }
    let result = fib(10);
  )";
  std::string ir = compileToIR(source);

  ASSERT_FALSE(ir.empty());
  EXPECT_TRUE(irContains(ir, "define i32 @fib"));
  EXPECT_TRUE(irContains(ir, "call i32 @fib"));
  EXPECT_TRUE(irContains(ir, "icmp sle"));
  EXPECT_TRUE(irContains(ir, "add i32"));
}

TEST(CodeGenTest, IterativeSum) {
  std::string source = R"(
    func sum(n: i32) : i32 {
      let result = 0;
      let i = 1;
      while (i <= n) {
        result = result + i;
        i = i + 1;
      }
      return result;
    }
  )";
  std::string ir = compileToIR(source);

  ASSERT_FALSE(ir.empty());
  EXPECT_TRUE(irContains(ir, "define i32 @sum"));
  EXPECT_TRUE(irMatches(ir, "label %loop"));
  EXPECT_TRUE(irContains(ir, "add i32") || irContains(ir, "add nsw i32"));
  EXPECT_TRUE(irContains(ir, "icmp sle"));
}

TEST(CodeGenTest, ComplexControlFlow) {
  std::string source = R"(
    func process(x: i32, y: i32) : i32 {
      if (x > y) {
        while (x > 0) {
          x = x - 1;
        }
        return x;
      } else {
        if (y > 0) {
          return y;
        }
        return 0;
      }
    }
  )";
  std::string ir = compileToIR(source);

  ASSERT_FALSE(ir.empty());
  EXPECT_TRUE(irContains(ir, "define i32 @process"));
  EXPECT_TRUE(irContains(ir, "br i1"));
  EXPECT_TRUE(irMatches(ir, "label %then"));
  EXPECT_TRUE(irMatches(ir, "label %else"));
  EXPECT_TRUE(irMatches(ir, "label %loop"));
}

// ===== Assignment Operators =====

TEST(CodeGenTest, SimpleAssignment) {
  std::string source = R"(
    let x = 10;
    x = 20;
  )";
  std::string ir = compileToIR(source);

  ASSERT_FALSE(ir.empty());
  EXPECT_TRUE(irContains(ir, "store i32 10"));
  EXPECT_TRUE(irContains(ir, "store i32 20"));
}

TEST(CodeGenTest, CompoundAssignmentAdd) {
  std::string source = R"(
    let x = 10;
    x += 5;
  )";
  std::string ir = compileToIR(source);

  ASSERT_FALSE(ir.empty());
  EXPECT_TRUE(irContains(ir, "load i32"));
  EXPECT_TRUE(irContains(ir, "add i32") || irContains(ir, "add nsw i32"));
  EXPECT_TRUE(irContains(ir, "store i32"));
}

TEST(CodeGenTest, CompoundAssignmentSub) {
  std::string source = R"(
    let x = 10;
    x -= 3;
  )";
  std::string ir = compileToIR(source);

  ASSERT_FALSE(ir.empty());
  EXPECT_TRUE(irContains(ir, "sub i32") || irContains(ir, "sub nsw i32"));
}

TEST(CodeGenTest, CompoundAssignmentMul) {
  std::string source = R"(
    let x = 10;
    x *= 2;
  )";
  std::string ir = compileToIR(source);

  ASSERT_FALSE(ir.empty());
  EXPECT_TRUE(irContains(ir, "mul i32") || irContains(ir, "mul nsw i32"));
}

TEST(CodeGenTest, CompoundAssignmentDiv) {
  std::string source = R"(
    let x = 20;
    x /= 4;
  )";
  std::string ir = compileToIR(source);

  ASSERT_FALSE(ir.empty());
  EXPECT_TRUE(irContains(ir, "sdiv i32"));
}

TEST(CodeGenTest, CompoundAssignmentMod) {
  std::string source = R"(
    let x = 17;
    x %= 5;
  )";
  std::string ir = compileToIR(source);

  ASSERT_FALSE(ir.empty());
  EXPECT_TRUE(irContains(ir, "srem i32"));
}

TEST(CodeGenTest, FloatAssignment) {
  std::string source = R"(
    let x = 3.14;
    x += 2.0;
  )";
  std::string ir = compileToIR(source);

  ASSERT_FALSE(ir.empty());
  EXPECT_TRUE(irContains(ir, "fadd double"));
}

// ===== Bitwise Operators =====

TEST(CodeGenTest, BitwiseAnd) {
  std::string source = R"(
    let a = 12;
    let b = 10;
    let result = a & b;
  )";
  std::string ir = compileToIR(source);

  ASSERT_FALSE(ir.empty());
  EXPECT_TRUE(irContains(ir, "and i32"));
}

TEST(CodeGenTest, BitwiseOr) {
  std::string source = R"(
    let a = 12;
    let b = 10;
    let result = a | b;
  )";
  std::string ir = compileToIR(source);

  ASSERT_FALSE(ir.empty());
  EXPECT_TRUE(irContains(ir, "or i32"));
}

TEST(CodeGenTest, BitwiseXor) {
  std::string source = R"(
    let a = 12;
    let b = 10;
    let result = a ^ b;
  )";
  std::string ir = compileToIR(source);

  ASSERT_FALSE(ir.empty());
  EXPECT_TRUE(irContains(ir, "xor i32"));
}

TEST(CodeGenTest, LeftShift) {
  std::string source = R"(
    let a = 3;
    let b = 2;
    let result = a << b;
  )";
  std::string ir = compileToIR(source);

  ASSERT_FALSE(ir.empty());
  EXPECT_TRUE(irContains(ir, "shl i32"));
}

TEST(CodeGenTest, RightShift) {
  std::string source = R"(
    let a = 12;
    let b = 2;
    let result = a >> b;
  )";
  std::string ir = compileToIR(source);

  ASSERT_FALSE(ir.empty());
  EXPECT_TRUE(irContains(ir, "ashr i32"));
}

TEST(CodeGenTest, BitwiseComplex) {
  std::string source = R"(
    let a = 15;
    let b = 7;
    let c = 3;
    let result = (a & b) | (c << 2);
  )";
  std::string ir = compileToIR(source);

  ASSERT_FALSE(ir.empty());
  EXPECT_TRUE(irContains(ir, "and i32"));
  EXPECT_TRUE(irContains(ir, "or i32"));
  EXPECT_TRUE(irContains(ir, "shl i32"));
}

// ===== Power Operator =====

// Note: Integer power removed - will be implemented in stdlib as fast
// exponentiation

TEST(CodeGenTest, FloatPower) {
  std::string source = R"(
    let a = 2.0;
    let b = 3.0;
    let result = a ** b;
  )";
  std::string ir = compileToIR(source);

  ASSERT_FALSE(ir.empty());
  EXPECT_TRUE(irContains(ir, "call double @llvm.pow"));
}

// ===== Assignment in Expressions =====

TEST(CodeGenTest, AssignmentReturnsValue) {
  std::string source = R"(
    let x = 0;
    let y = (x = 10);
  )";
  std::string ir = compileToIR(source);

  ASSERT_FALSE(ir.empty());
  EXPECT_TRUE(irContains(ir, "store i32 10"));
}

TEST(CodeGenTest, ChainedAssignment) {
  std::string source = R"(
    let x = 0;
    let y = 0;
    let z = 0;
    z = y = x = 42;
  )";
  std::string ir = compileToIR(source);

  ASSERT_FALSE(ir.empty());
  EXPECT_TRUE(irContains(ir, "store i32 42"));
}

// ===== User-Defined Operators =====

TEST(CodeGenTest, UserDefinedIntegerPower) {
  std::string source = R"(
    operator infix **(a: i32, n: i32) : i32 prec 90 assoc_right {
      let ans = 1;
      let base = a;
      let exp = n;
      while exp != 0 {
        if exp % 2 == 1 {
          ans *= base;
        }
        base *= base;
        exp /= 2;
      }
      return ans;
    }
    let result = 3 ** 4;
  )";
  std::string ir = compileToIR(source);

  ASSERT_FALSE(ir.empty());
  // 检查 operator 函数被定义（使用 mangled name）
  EXPECT_TRUE(irContains(ir, "define i32 @\"**$i32$i32\""));
  // 检查 operator 被调用
  EXPECT_TRUE(irContains(ir, "call i32 @\"**$i32$i32\""));
}

TEST(CodeGenTest, UserDefinedUnaryOperator) {
  std::string source = R"(
    operator prefix +(x: i32) : i32 {
      return x * 2;
    }
    let result = +5;
  )";
  std::string ir = compileToIR(source);

  ASSERT_FALSE(ir.empty());
  // 检查 operator 函数被定义
  EXPECT_TRUE(irContains(ir, "define i32 @\"+$i32\""));
  // 检查 operator 被调用
  EXPECT_TRUE(irContains(ir, "call i32 @\"+$i32\""));
}

TEST(CodeGenTest, UserDefinedOperatorWithMultipleParameters) {
  std::string source = R"(
    operator infix %%(a: i32, b: i32) : i32 prec 80 assoc_left {
      return a % b;
    }
    let result = 17 %% 5;
  )";
  std::string ir = compileToIR(source);

  ASSERT_FALSE(ir.empty());
  // 检查 operator 函数被定义
  EXPECT_TRUE(irContains(ir, "define i32 @\"%%$i32$i32\""));
  // 检查 operator 被调用
  EXPECT_TRUE(irContains(ir, "call i32 @\"%%$i32$i32\""));
}

TEST(CodeGenTest, OperatorOverloadingByType) {
  std::string source = R"(
    operator infix ***(a: i32, b: i32) : i32 prec 85 assoc_left {
      return a * b * b;
    }
    operator infix ***(a: f64, b: f64) : f64 prec 85 assoc_left {
      return a * b * b;
    }
    let int_result = 3 *** 4;
    let float_result = 2.0 *** 3.0;
  )";
  std::string ir = compileToIR(source);

  ASSERT_FALSE(ir.empty());
  // 检查两个重载都被定义
  EXPECT_TRUE(irContains(ir, "define i32 @\"***$i32$i32\""));
  EXPECT_TRUE(irContains(ir, "define double @\"***$f64$f64\""));
  // 检查两个重载都被调用
  EXPECT_TRUE(irContains(ir, "call i32 @\"***$i32$i32\""));
  EXPECT_TRUE(irContains(ir, "call double @\"***$f64$f64\""));
}

TEST(CodeGenTest, UserDefinedOperatorCallsOtherOperator) {
  std::string source = R"(
    operator infix ^^(a: i32, b: i32) : i32 prec 90 assoc_right {
      let result = 1;
      let i = 0;
      while i < b {
        result *= a;
        i += 1;
      }
      return result;
    }
    let result = 2 ^^ 5;
  )";
  std::string ir = compileToIR(source);

  ASSERT_FALSE(ir.empty());
  // 检查 operator 函数被定义
  EXPECT_TRUE(irContains(ir, "define i32 @\"^^$i32$i32\""));
  // 检查使用了内置操作符（如 *=, +=）
  EXPECT_TRUE(irContains(ir, "mul i32") || irContains(ir, "add i32"));
}

TEST(CodeGenTest, BuiltinOperatorsPrioritized) {
  std::string source = R"(
    let a = 10;
    let b = 20;
    let sum = a + b;
    let product = a * b;
  )";
  std::string ir = compileToIR(source);

  ASSERT_FALSE(ir.empty());
  // 内置操作符应该使用 LLVM 指令，而不是函数调用
  EXPECT_TRUE(irContains(ir, "add i32"));
  EXPECT_TRUE(irContains(ir, "mul i32"));
  // 不应该有 operator 函数调用
  EXPECT_FALSE(irContains(ir, "call i32 @\"+$i32$i32\""));
  EXPECT_FALSE(irContains(ir, "call i32 @\"*$i32$i32\""));
}

} // namespace

int main(int argc, char **argv) {
  ::testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}
