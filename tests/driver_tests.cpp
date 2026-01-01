#include <gtest/gtest.h>

#include <array>
#include <cstdlib>
#include <fstream>
#include <sstream>
#include <string>
#include <sys/wait.h>

#ifndef PLC_BINARY
#define PLC_BINARY "./build/src/plc"
#endif

#ifndef TEST_FIXTURES_DIR
#define TEST_FIXTURES_DIR "./tests/fixtures"
#endif

namespace {

std::string runCommand(const std::string &cmd) {
  std::array<char, 128> buffer;
  std::string result;
  FILE *pipe = popen((cmd + " 2>&1").c_str(), "r");
  if (!pipe) {
    throw std::runtime_error("popen() failed!");
  }
  while (fgets(buffer.data(), buffer.size(), pipe) != nullptr) {
    result += buffer.data();
  }
  pclose(pipe);
  return result;
}

TEST(PlcDriverTest, LexSampleFile) {
  std::string cmd =
      std::string(PLC_BINARY) + " --lex " + TEST_FIXTURES_DIR + "/sample.pec";
  std::string output = runCommand(cmd);

  EXPECT_TRUE(output.find("[Keyword] 'func'") != std::string::npos);
  EXPECT_TRUE(output.find("[Identifier] 'add'") != std::string::npos);
  EXPECT_TRUE(output.find("[Identifier] 'i32'") != std::string::npos);
  EXPECT_TRUE(output.find("[Punctuation] ':'") != std::string::npos);
  EXPECT_TRUE(output.find("[Keyword] 'return'") != std::string::npos);
  EXPECT_TRUE(output.find("[Operator] '+'") != std::string::npos);
  EXPECT_TRUE(output.find("[Punctuation] ';'") != std::string::npos);
  EXPECT_TRUE(output.find("[EndOfFile]") != std::string::npos);
}

TEST(PlcDriverTest, DefaultModeCompilation) {
  // Default mode compiles and links, generates executable
  std::string exe_file = "exit_test";

  // Clean up any existing file
  std::remove(exe_file.c_str());

  std::string cmd =
      std::string(PLC_BINARY) + " " + TEST_FIXTURES_DIR + "/exit_test.pec";
  std::string output = runCommand(cmd);

  // Should generate executable
  EXPECT_TRUE(output.find("Executable generated:") != std::string::npos);

  // Check that executable was created
  std::ifstream file(exe_file);
  EXPECT_TRUE(file.good());
  file.close();

  // Clean up
  std::remove(exe_file.c_str());
}

TEST(PlcDriverTest, RunModeCompilation) {
  // --run mode compiles, links, and runs
  std::string cmd = std::string(PLC_BINARY) + " " + TEST_FIXTURES_DIR +
                    "/exit_test.pec --run";

  // Use system() to get exit code
  int exit_code = system(cmd.c_str());
  // system() returns exit_code << 8 on Unix
  int actual_exit = WEXITSTATUS(exit_code);

  // Should exit with code 42
  EXPECT_EQ(actual_exit, 42);
}

TEST(PlcDriverTest, FileNotFound) {
  std::string cmd = std::string(PLC_BINARY) + " --lex nonexistent.pec";
  std::string output = runCommand(cmd);

  EXPECT_TRUE(output.find("cannot open file") != std::string::npos);
  EXPECT_TRUE(output.find("No such file or directory") != std::string::npos);
}

TEST(PlcDriverTest, HelpMessage) {
  std::string cmd = std::string(PLC_BINARY) + " --help";
  std::string output = runCommand(cmd);

  EXPECT_TRUE(output.find("OVERVIEW: pecco-lang compiler") !=
              std::string::npos);
  EXPECT_TRUE(output.find("--lex") != std::string::npos);
  EXPECT_TRUE(output.find("--parse") != std::string::npos);
  EXPECT_TRUE(output.find("--dump-ast") != std::string::npos);
  EXPECT_TRUE(output.find("--dump-symbols") != std::string::npos);
}

TEST(PlcDriverTest, ParseSampleFile) {
  std::string cmd =
      std::string(PLC_BINARY) + " --parse " + TEST_FIXTURES_DIR + "/sample.pec";
  std::string output = runCommand(cmd);

  EXPECT_TRUE(output.find("AST:") != std::string::npos);
  EXPECT_TRUE(output.find("Func(add") != std::string::npos);
  EXPECT_TRUE(output.find("Return(") != std::string::npos);
}

TEST(PlcDriverTest, LexerErrorReporting) {
  std::string cmd = std::string(PLC_BINARY) + " --lex " + TEST_FIXTURES_DIR +
                    "/lex_error.pec";
  std::string output = runCommand(cmd);

  EXPECT_TRUE(output.find("lexer error") != std::string::npos);
  EXPECT_TRUE(output.find("lex_error.pec") != std::string::npos);
  // Should show line and column
  EXPECT_TRUE(output.find(":") != std::string::npos);
}

TEST(PlcDriverTest, ParserErrorReporting) {
  std::string cmd = std::string(PLC_BINARY) + " --parse " + TEST_FIXTURES_DIR +
                    "/parse_error.pec";
  std::string output = runCommand(cmd);

  EXPECT_TRUE(output.find("parse error") != std::string::npos);
  EXPECT_TRUE(output.find("parse_error.pec") != std::string::npos);
  // Should show error location
  EXPECT_TRUE(output.find(":") != std::string::npos);
}

TEST(PlcDriverTest, DumpSymbolTable) {
  std::string cmd = std::string(PLC_BINARY) + " " + TEST_FIXTURES_DIR +
                    "/sample.pec --dump-symbols";
  std::string output = runCommand(cmd);

  // Should show symbol table header
  EXPECT_TRUE(output.find("Symbol Table:") != std::string::npos);
  // Should show functions section
  EXPECT_TRUE(output.find("Functions:") != std::string::npos);
  // Should show the add function from sample.pec
  EXPECT_TRUE(output.find("add(i32, i32) : i32") != std::string::npos);
  // Should show operators section
  EXPECT_TRUE(output.find("Operators:") != std::string::npos);
  // Should show standard operators from prelude
  EXPECT_TRUE(output.find("infix +") != std::string::npos);
  EXPECT_TRUE(output.find("infix *") != std::string::npos);
}

TEST(PlcDriverTest, SemanticErrorReporting) {
  std::string cmd =
      std::string(PLC_BINARY) + " " + TEST_FIXTURES_DIR + "/semantic_error.pec";
  std::string output = runCommand(cmd);

  // Should show semantic error
  EXPECT_TRUE(output.find("semantic error") != std::string::npos);
  // Should show the file name
  EXPECT_TRUE(output.find("semantic_error.pec") != std::string::npos);
  // Should show line and column (line 5, column 18)
  EXPECT_TRUE(output.find(":5:18:") != std::string::npos);
  // Should show the error message about mixed associativity
  EXPECT_TRUE(output.find("Mixed associativity") != std::string::npos);
  // Should point to the conflicting operator
  EXPECT_TRUE(output.find("^") != std::string::npos);
}

TEST(PlcDriverTest, DumpResolvedAST) {
  std::string cmd = std::string(PLC_BINARY) + " " + TEST_FIXTURES_DIR +
                    "/sample.pec --dump-ast";
  std::string output = runCommand(cmd);

  // Should show resolved AST header
  EXPECT_TRUE(output.find("Resolved AST:") != std::string::npos);
  // Should show function declarations
  EXPECT_TRUE(output.find("Func(add") != std::string::npos);
  // Should show Binary operator node (OperatorSeq resolved to Binary)
  EXPECT_TRUE(output.find("Binary(+") != std::string::npos);
  // Should NOT contain OperatorSeq (it should be resolved)
  EXPECT_TRUE(output.find("OperatorSeq") == std::string::npos);
}

TEST(PlcDriverTest, EmitLLVM) {
  std::string cmd = std::string(PLC_BINARY) + " " + TEST_FIXTURES_DIR +
                    "/simple_ir_test.pec --emit-llvm";
  std::string output = runCommand(cmd);

  // Should contain LLVM IR module definition
  EXPECT_TRUE(output.find("ModuleID") != std::string::npos ||
              output.find("source_filename") != std::string::npos);

  // Should contain the __pecco_entry function
  EXPECT_TRUE(output.find("define i32 @__pecco_entry()") != std::string::npos);

  // Should contain the user-defined double function
  EXPECT_TRUE(output.find("define i32 @double(i32") != std::string::npos);

  // Should contain add instruction (x + x)
  EXPECT_TRUE(output.find("add") != std::string::npos);

  // Should contain function call
  EXPECT_TRUE(output.find("call i32 @double") != std::string::npos);

  // Should contain ret instruction
  EXPECT_TRUE(output.find("ret i32") != std::string::npos);
}

TEST(PlcDriverTest, EmitLLVMWithPrelude) {
  std::string cmd = std::string(PLC_BINARY) + " " + TEST_FIXTURES_DIR +
                    "/exit_test.pec --emit-llvm";
  std::string output = runCommand(cmd);

  // Should contain exit function declaration (external)
  EXPECT_TRUE(output.find("declare void @exit(i32)") != std::string::npos);

  // Should contain call to exit
  EXPECT_TRUE(output.find("call void @exit(i32 42)") != std::string::npos);

  // Should contain __pecco_entry
  EXPECT_TRUE(output.find("define i32 @__pecco_entry()") != std::string::npos);
}

TEST(PlcDriverTest, CompileOnlyMode) {
  std::string obj_file = std::string(TEST_FIXTURES_DIR) + "/test_compile.o";

  // Clean up any existing file
  std::remove(obj_file.c_str());

  std::string cmd = std::string(PLC_BINARY) + " " + TEST_FIXTURES_DIR +
                    "/simple_ir_test.pec --compile -o " + obj_file;
  std::string output = runCommand(cmd);

  // Check that object file was created
  std::ifstream file(obj_file);
  EXPECT_TRUE(file.good());
  file.close();

  // Clean up
  std::remove(obj_file.c_str());
}

TEST(PlcDriverTest, OptimizationFlag) {
  // Test --opt with --emit-llvm
  std::string cmd = std::string(PLC_BINARY) + " " + TEST_FIXTURES_DIR +
                    "/opt_test.pec --opt --emit-llvm";
  std::string output = runCommand(cmd);

  // Optimized version should simplify add_constant to a single add
  // add_constant(x) should become: add i32 %0, 3
  EXPECT_TRUE(output.find("add i32 %0, 3") != std::string::npos);

  // Should NOT have multiple alloca/load/store for intermediate variables
  // Count alloca instructions in add_constant - optimized should have 0 or 1
  size_t alloca_pos = 0;
  int alloca_count = 0;
  while ((alloca_pos = output.find("alloca", alloca_pos)) !=
         std::string::npos) {
    // Make sure it's within add_constant function
    size_t func_start = output.rfind("define i32 @add_constant", alloca_pos);
    size_t func_end = output.find("}", alloca_pos);
    size_t next_func = output.find("define", alloca_pos + 1);

    if (func_start != std::string::npos &&
        (next_func == std::string::npos || func_end < next_func)) {
      alloca_count++;
    }
    alloca_pos++;
  }

  // Optimized code should have very few allocas in add_constant
  EXPECT_LE(alloca_count, 1);
}

TEST(PlcDriverTest, OptimizationWithRun) {
  // Test that --opt with --run produces correct result
  std::string cmd = std::string(PLC_BINARY) + " " + TEST_FIXTURES_DIR +
                    "/opt_test.pec --opt --run";

  int exit_code = system(cmd.c_str());
  int actual_exit = WEXITSTATUS(exit_code);

  // add_constant(5) = 5 + 1 + 1 + 1 = 8
  EXPECT_EQ(actual_exit, 8);
}

TEST(PlcDriverTest, OptimizationConsistency) {
  // Verify optimized and unoptimized produce same result
  std::string cmd_unopt =
      std::string(PLC_BINARY) + " " + TEST_FIXTURES_DIR + "/opt_test.pec --run";
  std::string cmd_opt = std::string(PLC_BINARY) + " " + TEST_FIXTURES_DIR +
                        "/opt_test.pec --opt --run";

  int exit_unopt = WEXITSTATUS(system(cmd_unopt.c_str()));
  int exit_opt = WEXITSTATUS(system(cmd_opt.c_str()));

  // Both should produce the same result
  EXPECT_EQ(exit_unopt, exit_opt);
  EXPECT_EQ(exit_unopt, 8);
}

} // namespace

int main(int argc, char **argv) {
  ::testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}
