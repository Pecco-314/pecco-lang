#include "lexer.hpp"
#include "parser.hpp"

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

static cl::opt<bool> ParseMode("parse", cl::desc("Run parser and output AST"));

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

static void printStmt(const pecco::Stmt *stmt, raw_ostream &os, int indent = 0);
static void printExpr(const pecco::Expr *expr, raw_ostream &os);

static void printIndent(raw_ostream &os, int indent) {
  for (int i = 0; i < indent; ++i) {
    os << "  ";
  }
}

static void printExpr(const pecco::Expr *expr, raw_ostream &os) {
  if (!expr) {
    os << "<null>";
    return;
  }

  switch (expr->kind) {
  case pecco::ExprKind::IntLiteral:
    os << "IntLiteral("
       << static_cast<const pecco::IntLiteralExpr *>(expr)->value << ")";
    break;
  case pecco::ExprKind::FloatLiteral:
    os << "FloatLiteral("
       << static_cast<const pecco::FloatLiteralExpr *>(expr)->value << ")";
    break;
  case pecco::ExprKind::StringLiteral:
    os << "StringLiteral(\""
       << static_cast<const pecco::StringLiteralExpr *>(expr)->value << "\")";
    break;
  case pecco::ExprKind::BoolLiteral:
    os << "BoolLiteral("
       << (static_cast<const pecco::BoolLiteralExpr *>(expr)->value ? "true"
                                                                    : "false")
       << ")";
    break;
  case pecco::ExprKind::Identifier:
    os << "Identifier("
       << static_cast<const pecco::IdentifierExpr *>(expr)->name << ")";
    break;
  case pecco::ExprKind::OperatorSeq: {
    auto *seq = static_cast<const pecco::OperatorSeqExpr *>(expr);
    os << "OperatorSeq(";
    for (size_t i = 0; i < seq->operands.size(); ++i) {
      if (i > 0) {
        os << " " << seq->operators[i - 1] << " ";
      }
      printExpr(seq->operands[i].get(), os);
    }
    os << ")";
    break;
  }
  case pecco::ExprKind::Call: {
    auto *call = static_cast<const pecco::CallExpr *>(expr);
    os << "Call(";
    printExpr(call->callee.get(), os);
    os << ", [";
    for (size_t i = 0; i < call->args.size(); ++i) {
      if (i > 0)
        os << ", ";
      printExpr(call->args[i].get(), os);
    }
    os << "])";
    break;
  }
  }
}

static void printStmt(const pecco::Stmt *stmt, raw_ostream &os, int indent) {
  if (!stmt) {
    printIndent(os, indent);
    os << "<null>\n";
    return;
  }

  printIndent(os, indent);
  switch (stmt->kind) {
  case pecco::StmtKind::Let: {
    auto *let = static_cast<const pecco::LetStmt *>(stmt);
    os << "Let(" << let->name;
    if (let->type) {
      os << " : " << (*let->type)->name;
    }
    os << " = ";
    printExpr(let->init.get(), os);
    os << ")\n";
    break;
  }
  case pecco::StmtKind::Func: {
    auto *func = static_cast<const pecco::FuncStmt *>(stmt);
    os << "Func(" << func->name << "(";
    for (size_t i = 0; i < func->params.size(); ++i) {
      if (i > 0)
        os << ", ";
      os << func->params[i].name;
      if (func->params[i].type) {
        os << " : " << (*func->params[i].type)->name;
      }
    }
    os << ")";
    if (func->return_type) {
      os << " : " << (*func->return_type)->name;
    }
    os << ")\n";
    printStmt(func->body.get(), os, indent + 1);
    break;
  }
  case pecco::StmtKind::If: {
    auto *ifstmt = static_cast<const pecco::IfStmt *>(stmt);
    os << "If(";
    printExpr(ifstmt->condition.get(), os);
    os << ")\n";
    printStmt(ifstmt->then_branch.get(), os, indent + 1);
    if (ifstmt->else_branch) {
      printIndent(os, indent);
      os << "Else\n";
      printStmt((*ifstmt->else_branch).get(), os, indent + 1);
    }
    break;
  }
  case pecco::StmtKind::Return: {
    auto *ret = static_cast<const pecco::ReturnStmt *>(stmt);
    os << "Return(";
    if (ret->value) {
      printExpr((*ret->value).get(), os);
    }
    os << ")\n";
    break;
  }
  case pecco::StmtKind::While: {
    auto *wh = static_cast<const pecco::WhileStmt *>(stmt);
    os << "While(";
    printExpr(wh->condition.get(), os);
    os << ")\n";
    printStmt(wh->body.get(), os, indent + 1);
    break;
  }
  case pecco::StmtKind::Expr: {
    auto *expr = static_cast<const pecco::ExprStmt *>(stmt);
    os << "Expr(";
    printExpr(expr->expr.get(), os);
    os << ")\n";
    break;
  }
  case pecco::StmtKind::Block: {
    auto *block = static_cast<const pecco::BlockStmt *>(stmt);
    os << "Block\n";
    for (const auto &s : block->stmts) {
      printStmt(s.get(), os, indent + 1);
    }
    break;
  }
  }
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

int main(int argc, char **argv) {
  cl::ParseCommandLineOptions(argc, argv, "pecco-lang compiler\n");

  if (LexMode) {
    return runLexer(InputFilename);
  }

  if (ParseMode) {
    return runParser(InputFilename);
  }

  WithColor::error(errs(), "plc") << "no operation mode specified\n";
  return 1;
}
