#include "declaration_collector.hpp"
#include "lexer.hpp"
#include "operator_resolver.hpp"
#include "parser.hpp"

#include <llvm/Support/CommandLine.h>
#include <llvm/Support/FileSystem.h>
#include <llvm/Support/MemoryBuffer.h>
#include <llvm/Support/WithColor.h>
#include <llvm/Support/raw_ostream.h>

#include <algorithm>
#include <set>
#include <sstream>
#include <system_error>

using namespace llvm;

static cl::opt<std::string>
    InputFilename(cl::Positional, cl::desc("<input file>"), cl::Required);

static cl::opt<bool> LexMode("lex",
                             cl::desc("Run lexer only and output tokens"));

static cl::opt<bool> ParseMode(
    "parse",
    cl::desc("Run parser only and output flat AST (no semantic analysis)"));

static cl::opt<bool>
    DumpAST("dump-ast", cl::desc("Dump resolved AST after semantic analysis"));

static cl::opt<bool>
    DumpSymbols("dump-symbols",
                cl::desc("Dump symbol table after semantic analysis"));

static void printToken(const pecco::Token &tok, raw_ostream &os) {
  os << "[" << pecco::to_string(tok.kind) << "] ";
  if (!tok.lexeme.empty() && tok.kind != pecco::TokenKind::EndOfFile) {
    os << "'" << tok.lexeme << "'";
  }
  os << " (line " << tok.line << ", col " << tok.column << ")\n";
}

static void printSourceLine(StringRef source, size_t line, size_t column,
                            size_t end_column, size_t error_offset,
                            raw_ostream &os) {
  // Split source into lines
  SmallVector<StringRef, 32> lines;
  source.split(lines, '\n');

  if (line < 1 || line > lines.size()) {
    return;
  }

  StringRef lineContent = lines[line - 1];
  os << "  " << line << " | " << lineContent << "\n";
  os << "    | ";

  // Print spaces before the highlight
  size_t start_col = column;
  size_t actual_error_col = column + error_offset;

  for (size_t i = 1; i < start_col; ++i) {
    os << " ";
  }

  // Highlight the token range if end_column is specified
  if (end_column > column + 1) {
    // Multi-character token
    for (size_t i = start_col; i < end_column; ++i) {
      if (error_offset > 0 && i == actual_error_col) {
        WithColor(os, raw_ostream::RED, true) << "^";
      } else {
        WithColor(os, raw_ostream::RED) << "~";
      }
    }
  } else {
    // Single character or point error
    WithColor(os, raw_ostream::RED, true) << "^";
  }

  os << "\n";
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
  bool hasError = false;
  for (const auto &tok : tokens) {
    if (tok.kind == pecco::TokenKind::Error) {
      hasError = true;
      WithColor::error(errs(), "plc")
          << "lexer error at " << filename << ":" << tok.line << ":"
          << tok.column << ": " << tok.lexeme << "\n";
      printSourceLine(buffer->getBuffer(), tok.line, tok.column, tok.end_column,
                      tok.error_offset, errs());
    } else {
      printToken(tok, outs());
    }
  }

  return hasError ? 1 : 0;
}

static void printStmt(const pecco::Stmt *stmt, raw_ostream &os, int indent);
static void printExpr(const pecco::Expr *expr, raw_ostream &os) {
  if (!expr) {
    os << "<null>";
    return;
  }

  // Use std::stringstream for C++ stream, then output to LLVM stream
  std::stringstream ss;
  expr->print(ss);
  os << ss.str();
}

static void printStmt(const pecco::Stmt *stmt, raw_ostream &os,
                      int indent = 0) {
  if (!stmt) {
    for (int i = 0; i < indent; ++i) {
      os << "  ";
    }
    os << "<null>\n";
    return;
  }

  // Use std::stringstream for C++ stream, then output to LLVM stream
  std::stringstream ss;
  stmt->print(ss, indent);
  os << ss.str();
}

static int runParser(StringRef filename) {
  auto bufferOrErr = MemoryBuffer::getFile(filename);
  if (std::error_code ec = bufferOrErr.getError()) {
    WithColor::error(errs(), "plc")
        << "cannot open file '" << filename << "': " << ec.message() << "\n";
    return 1;
  }

  std::unique_ptr<MemoryBuffer> buffer = std::move(*bufferOrErr);
  StringRef sourceContent = buffer->getBuffer();

  pecco::Lexer lexer(sourceContent);
  auto tokens = lexer.tokenize_all();

  // Check for lexer errors
  bool hasLexerError = false;
  for (const auto &tok : tokens) {
    if (tok.kind == pecco::TokenKind::Error) {
      hasLexerError = true;
      WithColor::error(errs(), "plc")
          << "lexer error at " << filename << ":" << tok.line << ":"
          << tok.column << ": " << tok.lexeme << "\n";
      printSourceLine(sourceContent, tok.line, tok.column, tok.end_column,
                      tok.error_offset, errs());
    }
  }

  if (hasLexerError) {
    return 1;
  }

  pecco::Parser parser(std::move(tokens));
  auto stmts = parser.parse_program();

  if (parser.has_errors()) {
    for (const auto &err : parser.errors()) {
      WithColor::error(errs(), "plc")
          << "parse error at " << filename << ":" << err.line << ":"
          << err.column << ": " << err.message << "\n";
      printSourceLine(sourceContent, err.line, err.column, err.end_column, 0,
                      errs());
    }
    return 1;
  }

  WithColor(outs(), raw_ostream::GREEN, true) << "AST:\n";
  for (const auto &stmt : stmts) {
    printStmt(stmt.get(), outs(), 0);
  }

  return 0;
}

static void printSymbolTable(const pecco::SymbolTable &symtab,
                             raw_ostream &os) {
  WithColor(os, raw_ostream::CYAN, true) << "\nSymbol Table:\n";

  // Print functions
  WithColor(os, raw_ostream::CYAN, true) << "\nFunctions:\n";
  auto func_names = symtab.get_all_function_names();
  for (const auto &name : func_names) {
    auto funcs = symtab.find_functions(name);
    for (const auto &func : funcs) {
      os << "  " << name << "(";
      for (size_t i = 0; i < func.param_types.size(); ++i) {
        if (i > 0)
          os << ", ";
        os << func.param_types[i];
      }
      os << ")";
      if (!func.return_type.empty()) {
        os << " : " << func.return_type;
      }
      if (func.is_declaration_only) {
        os << " [declaration]";
      }
      os << "\n";
    }
  }

  // Print operators
  WithColor(os, raw_ostream::CYAN, true) << "\nOperators:\n";

  // Get all operators and sort by symbol
  auto all_ops = symtab.get_all_operators();
  std::sort(all_ops.begin(), all_ops.end(),
            [](const pecco::OperatorInfo &a, const pecco::OperatorInfo &b) {
              if (a.op != b.op)
                return a.op < b.op;
              return a.position < b.position;
            });

  for (const auto &info : all_ops) {
    os << "  ";
    switch (info.position) {
    case pecco::OpPosition::Prefix:
      os << "prefix ";
      break;
    case pecco::OpPosition::Infix:
      os << "infix ";
      break;
    case pecco::OpPosition::Postfix:
      os << "postfix ";
      break;
    }
    os << info.op << "(";
    for (size_t i = 0; i < info.signature.param_types.size(); ++i) {
      if (i > 0)
        os << ", ";
      os << info.signature.param_types[i];
    }
    os << ") : " << info.signature.return_type;
    if (info.position == pecco::OpPosition::Infix) {
      os << " [prec " << info.precedence;
      if (info.assoc == pecco::Associativity::Right) {
        os << ", assoc_right";
      }
      os << "]";
    }
    os << "\n";
  }
}

static int runCompile(StringRef filename) {
  auto bufferOrErr = MemoryBuffer::getFile(filename);
  if (std::error_code ec = bufferOrErr.getError()) {
    WithColor::error(errs(), "plc")
        << "cannot open file '" << filename << "': " << ec.message() << "\n";
    return 1;
  }

  std::unique_ptr<MemoryBuffer> buffer = std::move(*bufferOrErr);
  StringRef sourceContent = buffer->getBuffer();

  // Lex
  pecco::Lexer lexer(sourceContent);
  auto tokens = lexer.tokenize_all();

  // Check for lexer errors
  bool hasLexerError = false;
  for (const auto &tok : tokens) {
    if (tok.kind == pecco::TokenKind::Error) {
      hasLexerError = true;
      WithColor::error(errs(), "plc")
          << "lexer error at " << filename << ":" << tok.line << ":"
          << tok.column << ": " << tok.lexeme << "\n";
      printSourceLine(sourceContent, tok.line, tok.column, tok.end_column,
                      tok.error_offset, errs());
    }
  }

  if (hasLexerError) {
    return 1;
  }

  // Parse
  pecco::Parser parser(std::move(tokens));
  auto stmts = parser.parse_program();

  if (parser.has_errors()) {
    for (const auto &err : parser.errors()) {
      WithColor::error(errs(), "plc")
          << "parse error at " << filename << ":" << err.line << ":"
          << err.column << ": " << err.message << "\n";
      printSourceLine(sourceContent, err.line, err.column, err.end_column, 0,
                      errs());
    }
    return 1;
  }

  // Semantic analysis
  // Step 1: Build symbol table
  pecco::SymbolTable symbol_table;
  pecco::DeclarationCollector collector;

  // Load prelude
  if (!collector.load_prelude(STDLIB_DIR "/prelude.pec", symbol_table)) {
    WithColor::error(errs(), "plc") << "failed to load prelude\n";
    if (collector.has_errors()) {
      for (const auto &err : collector.errors()) {
        errs() << "  " << err.message << "\n";
      }
    }
    return 1;
  }

  // Collect declarations from user code
  if (!collector.collect(stmts, symbol_table)) {
    for (const auto &err : collector.errors()) {
      WithColor::error(errs(), "plc")
          << "semantic error at " << filename << ":" << err.line << ":"
          << err.column << ": " << err.message << "\n";
    }
    return 1;
  }

  // Step 2: Resolve operator sequences
  std::vector<std::string> resolve_errors;
  for (auto &stmt : stmts) {
    pecco::OperatorResolver::resolve_stmt(stmt.get(), symbol_table,
                                          resolve_errors);
  }

  // Check for errors after resolution
  if (!resolve_errors.empty()) {
    for (const auto &err : resolve_errors) {
      // Parse error format: "at line:col: message"
      if (err.find("at ") == 0) {
        size_t pos = err.find(": ");
        if (pos != std::string::npos) {
          std::string location = err.substr(3, pos - 3);
          std::string message = err.substr(pos + 2);

          // Parse line:col
          size_t colon = location.find(':');
          if (colon != std::string::npos) {
            size_t line = std::stoul(location.substr(0, colon));
            size_t column = std::stoul(location.substr(colon + 1));

            WithColor::error(errs(), "plc")
                << "semantic error at " << filename << ":" << line << ":"
                << column << ": " << message << "\n";
            printSourceLine(sourceContent, line, column, 0, 0, errs());
            continue;
          }
        }
      }
      // Fallback: print error as is
      WithColor::error(errs(), "plc") << "semantic error: " << err << "\n";
    }
    return 1;
  }

  // Output based on flags
  if (DumpAST) {
    WithColor(outs(), raw_ostream::GREEN, true) << "Resolved AST:\n";
    for (const auto &stmt : stmts) {
      printStmt(stmt.get(), outs(), 0);
    }
  }

  if (DumpSymbols) {
    printSymbolTable(symbol_table, outs());
  }

  if (!DumpAST && !DumpSymbols) {
    // Default: just report success
    WithColor(outs(), raw_ostream::GREEN, true)
        << "Compilation successful (semantic analysis complete)\n";
  }

  return 0;
}

int main(int argc, char **argv) {
  cl::ParseCommandLineOptions(argc, argv, "pecco-lang compiler\n");

  if (LexMode) {
    return runLexer(InputFilename);
  }

  if (ParseMode) {
    return runParser(InputFilename);
  }

  // Default: run full compilation (semantic analysis)
  return runCompile(InputFilename);
}
