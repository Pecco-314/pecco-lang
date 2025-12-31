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
      std::string(PLC_BINARY) + " --lex " + TEST_FIXTURES_DIR + "/sample.pl";
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
      std::string(PLC_BINARY) + " " + TEST_FIXTURES_DIR + "/sample.pl";
  std::string output = runCommand(cmd);

  EXPECT_TRUE(output.find("Compilation successful") != std::string::npos);
}

TEST(PlcDriverTest, FileNotFound) {
  std::string cmd = std::string(PLC_BINARY) + " --lex nonexistent.pl";
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
      std::string(PLC_BINARY) + " --parse " + TEST_FIXTURES_DIR + "/sample.pl";
  std::string output = runCommand(cmd);

  EXPECT_TRUE(output.find("AST:") != std::string::npos);
  EXPECT_TRUE(output.find("Func(add") != std::string::npos);
  EXPECT_TRUE(output.find("Return(") != std::string::npos);
}

TEST(PlcDriverTest, LexerErrorReporting) {
  std::string cmd =
      std::string(PLC_BINARY) + " --lex " + TEST_FIXTURES_DIR + "/lex_error.pl";
  std::string output = runCommand(cmd);

  EXPECT_TRUE(output.find("lexer error") != std::string::npos);
  EXPECT_TRUE(output.find("lex_error.pl") != std::string::npos);
  // Should show line and column
  EXPECT_TRUE(output.find(":") != std::string::npos);
}

TEST(PlcDriverTest, ParserErrorReporting) {
  std::string cmd = std::string(PLC_BINARY) + " --parse " + TEST_FIXTURES_DIR +
                    "/parse_error.pl";
  std::string output = runCommand(cmd);

  EXPECT_TRUE(output.find("parse error") != std::string::npos);
  EXPECT_TRUE(output.find("parse_error.pl") != std::string::npos);
  // Should show error location
  EXPECT_TRUE(output.find(":") != std::string::npos);
}

} // namespace

int main(int argc, char **argv) {
  ::testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}
