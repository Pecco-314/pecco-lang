#include "declaration_collector.hpp"
#include "lexer.hpp"
#include "parser.hpp"
#include <fstream>
#include <sstream>

namespace pecco {

bool DeclarationCollector::collect(const std::vector<StmtPtr> &stmts,
                                   SymbolTable &symbol_table) {
  for (const auto &stmt : stmts) {
    process_stmt(stmt.get(), symbol_table);
  }
  return !has_errors();
}

bool DeclarationCollector::load_prelude(const std::string &prelude_path,
                                        SymbolTable &symbol_table) {
  // Load prelude.pec
  std::ifstream stream(prelude_path);
  if (!stream) {
    error("Failed to open prelude file: " + prelude_path);
    return false;
  }

  std::stringstream buffer;
  buffer << stream.rdbuf();
  std::string content = buffer.str();

  // Lex and parse prelude file
  Lexer lexer(content);
  auto tokens = lexer.tokenize_all();

  // Check for lexer errors
  for (const auto &tok : tokens) {
    if (tok.kind == TokenKind::Error) {
      error("Lexer error in prelude: " + tok.lexeme, tok.line, tok.column);
      return false;
    }
  }

  Parser parser(std::move(tokens));
  auto stmts = parser.parse_program();

  if (parser.has_errors()) {
    for (const auto &err : parser.errors()) {
      error("Parse error in prelude: " + err.message, err.line, err.column);
    }
    return false;
  }

  // Collect declarations from prelude
  return collect(stmts, symbol_table);
}

void DeclarationCollector::process_stmt(const Stmt *stmt,
                                        SymbolTable &symbol_table) {
  if (!stmt)
    return;

  switch (stmt->kind) {
  case StmtKind::Func:
    process_func_decl(static_cast<const FuncStmt *>(stmt), symbol_table);
    break;
  case StmtKind::OperatorDecl:
    process_operator_decl(static_cast<const OperatorDeclStmt *>(stmt),
                          symbol_table);
    break;
  case StmtKind::Block: {
    // Recursively process statements in blocks
    auto *block = static_cast<const BlockStmt *>(stmt);
    for (const auto &s : block->stmts) {
      process_stmt(s.get(), symbol_table);
    }
    break;
  }
  default:
    // Other statements are ignored in declaration collection phase
    break;
  }
}

void DeclarationCollector::process_func_decl(const FuncStmt *func,
                                             SymbolTable &symbol_table) {
  // Extract parameter types
  std::vector<std::string> param_types;
  for (const auto &param : func->params) {
    if (param.type) {
      param_types.push_back(get_type_name(param.type->get()));
    } else {
      // Type inference not yet supported - require explicit types
      error("Function parameter '" + param.name +
                "' must have explicit type annotation",
            func->loc.line, func->loc.column);
      return;
    }
  }

  // Extract return type
  std::string return_type;
  if (func->return_type) {
    return_type = get_type_name(func->return_type->get());
  } else {
    return_type = ""; // void
  }

  // Check if it's a declaration only (no body)
  bool is_decl_only = !func->body;

  // Add to symbol table
  FunctionSignature sig(func->name, param_types, return_type, is_decl_only);
  symbol_table.add_function(sig);
}

void DeclarationCollector::process_operator_decl(const OperatorDeclStmt *op,
                                                 SymbolTable &symbol_table) {
  // Extract parameter types
  std::vector<std::string> param_types;
  for (const auto &param : op->params) {
    if (param.type) {
      param_types.push_back(get_type_name(param.type->get()));
    } else {
      error("Operator parameter must have explicit type annotation",
            op->loc.line, op->loc.column);
      return;
    }
  }

  // Extract return type
  std::string return_type;
  if (op->return_type) {
    return_type = get_type_name(op->return_type->get());
  } else {
    error("Operator must have explicit return type", op->loc.line,
          op->loc.column);
    return;
  }

  // Create operator signature
  OperatorSignature signature(param_types, return_type);

  // Create operator info
  OperatorInfo info(op->op, op->position, op->precedence, op->assoc, signature);

  // Add to symbol table
  symbol_table.add_operator(info);
}

std::string DeclarationCollector::get_type_name(const Type *type) const {
  if (!type)
    return "";

  switch (type->kind) {
  case TypeKind::Named:
    return type->name;
  default:
    return "";
  }
}

void DeclarationCollector::error(const std::string &message, size_t line,
                                 size_t column) {
  errors_.push_back({message, line, column});
}

} // namespace pecco
