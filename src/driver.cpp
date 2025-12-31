#include "lexer.hpp"
#include "parser.hpp"
#include "semantic.hpp"

#include <llvm/Support/CommandLine.h>
#include <llvm/Support/FileSystem.h>
#include <llvm/Support/MemoryBuffer.h>
#include <llvm/Support/WithColor.h>
#include <llvm/Support/raw_ostream.h>

#include <set>
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

// Forward declaration
static void resolveStmtOperators(pecco::Stmt *stmt,
                                 pecco::SemanticAnalyzer &analyzer);

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
  case pecco::ExprKind::Binary: {
    auto *bin = static_cast<const pecco::BinaryExpr *>(expr);
    os << "Binary(" << bin->op << ", ";
    printExpr(bin->left.get(), os);
    os << ", ";
    printExpr(bin->right.get(), os);
    os << ")";
    break;
  }
  case pecco::ExprKind::Unary: {
    auto *un = static_cast<const pecco::UnaryExpr *>(expr);
    os << "Unary(" << un->op << ", ";
    printExpr(un->operand.get(), os);
    os << ")";
    break;
  }
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
    if (func->body) {
      printStmt((*func->body).get(), os, indent + 1);
    }
    break;
  }
  case pecco::StmtKind::OperatorDecl: {
    auto *op = static_cast<const pecco::OperatorDeclStmt *>(stmt);
    os << "OperatorDecl(";
    // Print position
    switch (op->position) {
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
    os << op->op << "(";
    // Print parameters
    for (size_t i = 0; i < op->params.size(); ++i) {
      if (i > 0)
        os << ", ";
      os << op->params[i].name;
      if (op->params[i].type) {
        os << " : " << (*op->params[i].type)->name;
      }
    }
    os << ")";
    if (op->return_type) {
      os << " : " << (*op->return_type)->name;
    }
    // Print precedence and associativity for infix operators
    if (op->position == pecco::OpPosition::Infix) {
      os << " prec " << op->precedence << " assoc ";
      switch (op->assoc) {
      case pecco::Associativity::Left:
        os << "left";
        break;
      case pecco::Associativity::Right:
        os << "right";
        break;
      case pecco::Associativity::None:
        os << "none";
        break;
      }
    }
    os << ")\n";
    // Print body if present
    if (op->body) {
      printStmt((*op->body).get(), os, indent + 1);
    }
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
  // Collect unique operator symbols
  std::set<std::string> op_symbols;
  const std::string test_ops[] = {"+",  "-",  "*",  "/",  "%",  "**", "&",
                                  "|",  "^",  "<<", ">>", "&&", "||", "!",
                                  "==", "!=", "<",  ">",  "<=", ">="};
  for (const auto &op : test_ops) {
    auto ops = symtab.find_all_operators(op);
    if (!ops.empty()) {
      op_symbols.insert(op);
    }
  }

  for (const auto &op : op_symbols) {
    auto ops = symtab.find_all_operators(op);
    for (const auto &info : ops) {
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
        os << " [prec " << info.precedence << ", ";
        switch (info.assoc) {
        case pecco::Associativity::Left:
          os << "left";
          break;
        case pecco::Associativity::Right:
          os << "right";
          break;
        case pecco::Associativity::None:
          os << "none";
          break;
        }
        os << "]";
      }
      os << "\n";
    }
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
  pecco::SemanticAnalyzer analyzer;

  // Load prelude
  if (!analyzer.load_prelude(STDLIB_DIR "/prelude.pl")) {
    WithColor::error(errs(), "plc") << "failed to load prelude\n";
    if (analyzer.has_errors()) {
      for (const auto &err : analyzer.errors()) {
        errs() << "  " << err.message << "\n";
      }
    }
    return 1;
  }

  // Collect declarations from user code
  analyzer.collect_declarations(stmts);

  if (analyzer.has_errors()) {
    for (const auto &err : analyzer.errors()) {
      WithColor::error(errs(), "plc")
          << "semantic error at " << filename << ":" << err.line << ":"
          << err.column << ": " << err.message << "\n";
    }
    return 1;
  }

  // Resolve operator sequences in all statements
  for (auto &stmt : stmts) {
    resolveStmtOperators(stmt.get(), analyzer);
  }

  // Output based on flags
  if (DumpAST) {
    WithColor(outs(), raw_ostream::GREEN, true) << "Resolved AST:\n";
    for (const auto &stmt : stmts) {
      printStmt(stmt.get(), outs(), 0);
    }
  }

  if (DumpSymbols) {
    printSymbolTable(analyzer.symbol_table(), outs());
  }

  if (!DumpAST && !DumpSymbols) {
    // Default: just report success
    WithColor(outs(), raw_ostream::GREEN, true)
        << "Compilation successful (semantic analysis complete)\n";
  }

  return 0;
}

static void resolveStmtOperators(pecco::Stmt *stmt,
                                 pecco::SemanticAnalyzer &analyzer) {
  if (!stmt)
    return;

  switch (stmt->kind) {
  case pecco::StmtKind::Let: {
    auto *let = static_cast<pecco::LetStmt *>(stmt);
    let->init = analyzer.resolve_operators(std::move(let->init));
    break;
  }
  case pecco::StmtKind::Func: {
    auto *func = static_cast<pecco::FuncStmt *>(stmt);
    if (func->body) {
      resolveStmtOperators((*func->body).get(), analyzer);
    }
    break;
  }
  case pecco::StmtKind::If: {
    auto *ifstmt = static_cast<pecco::IfStmt *>(stmt);
    ifstmt->condition =
        analyzer.resolve_operators(std::move(ifstmt->condition));
    resolveStmtOperators(ifstmt->then_branch.get(), analyzer);
    if (ifstmt->else_branch) {
      resolveStmtOperators((*ifstmt->else_branch).get(), analyzer);
    }
    break;
  }
  case pecco::StmtKind::Return: {
    auto *ret = static_cast<pecco::ReturnStmt *>(stmt);
    if (ret->value) {
      *ret->value = analyzer.resolve_operators(std::move(*ret->value));
    }
    break;
  }
  case pecco::StmtKind::While: {
    auto *wh = static_cast<pecco::WhileStmt *>(stmt);
    wh->condition = analyzer.resolve_operators(std::move(wh->condition));
    resolveStmtOperators(wh->body.get(), analyzer);
    break;
  }
  case pecco::StmtKind::Expr: {
    auto *expr = static_cast<pecco::ExprStmt *>(stmt);
    expr->expr = analyzer.resolve_operators(std::move(expr->expr));
    break;
  }
  case pecco::StmtKind::Block: {
    auto *block = static_cast<pecco::BlockStmt *>(stmt);
    for (auto &s : block->stmts) {
      resolveStmtOperators(s.get(), analyzer);
    }
    break;
  }
  case pecco::StmtKind::OperatorDecl:
    // Operator declarations don't have expressions to resolve
    break;
  }
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
