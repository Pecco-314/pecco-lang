#include "lexer.hpp"

#include <llvm/Support/CommandLine.h>
#include <llvm/Support/FileSystem.h>
#include <llvm/Support/MemoryBuffer.h>
#include <llvm/Support/WithColor.h>
#include <llvm/Support/raw_ostream.h>

#include <system_error>

using namespace llvm;

static cl::opt<std::string>
    InputFilename(cl::Positional, cl::desc("<input file>"), cl::Required);

static cl::opt<bool> LexMode("lex", cl::desc("Run lexer and output tokens"));

static void printToken(const pecco::Token &tok, raw_ostream &os) {
  os << "[" << pecco::to_string(tok.kind) << "] ";
  if (!tok.lexeme.empty() && tok.kind != pecco::TokenKind::EndOfFile) {
    os << "'" << tok.lexeme << "'";
  }
  os << " (line " << tok.line << ", col " << tok.column << ")\n";
}

static int runLexer(StringRef filename) {
  auto bufferOrErr = MemoryBuffer::getFile(filename);
  if (std::error_code ec = bufferOrErr.getError()) {
    WithColor::error(errs(), "plc")
        << "cannot open file '" << filename << "': " << ec.message() << "\n";
    return 1;
  }

  std::unique_ptr<MemoryBuffer> buffer = std::move(*bufferOrErr);
  pecco::Lexer lexer(buffer->getBuffer());

  auto tokens = lexer.tokenize_all();
  for (const auto &tok : tokens) {
    printToken(tok, outs());
  }

  return 0;
}

int main(int argc, char **argv) {
  cl::ParseCommandLineOptions(argc, argv, "pecco-lang compiler\n");

  if (LexMode) {
    return runLexer(InputFilename);
  }

  WithColor::error(errs(), "plc") << "no operation mode specified\n";
  return 1;
}
