#include <gtest/gtest.h>

#include <array>
#include <cstdlib>
#include <fstream>
#include <sstream>
#include <string>

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
  std::string cmd =
      std::string(PLC_BINARY) + " " + TEST_FIXTURES_DIR + "/sample.pec";
  std::string output = runCommand(cmd);

  EXPECT_TRUE(output.find("Compilation successful") != std::string::npos);
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

} // namespace

int main(int argc, char **argv) {
  ::testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}
