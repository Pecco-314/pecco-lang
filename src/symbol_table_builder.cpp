#include "symbol_table_builder.hpp"
#include "lexer.hpp"
#include "parser.hpp"
#include <fstream>
#include <sstream>

namespace pecco {

bool SymbolTableBuilder::collect(const std::vector<StmtPtr> &stmts,
                                 ScopedSymbolTable &symbols) {
  next_block_num_ = 0;
  for (const auto &stmt : stmts) {
    process_stmt(stmt.get(), symbols);
  }
  return !has_errors();
}

bool SymbolTableBuilder::load_prelude(const std::string &prelude_path,
                                      ScopedSymbolTable &symbols) {
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

  // Mark that we're collecting prelude symbols
  collecting_prelude_ = true;
  bool result = collect(stmts, symbols);
  collecting_prelude_ = false;

  return result;
}

void SymbolTableBuilder::process_stmt(const Stmt *stmt,
                                      ScopedSymbolTable &symbols) {
  if (!stmt)
    return;

  switch (stmt->kind) {
  case StmtKind::Func:
    process_func_decl(static_cast<const FuncStmt *>(stmt), symbols);
    break;
  case StmtKind::OperatorDecl:
    process_operator_decl(static_cast<const OperatorDeclStmt *>(stmt), symbols);
    break;
  case StmtKind::Let:
    process_let(static_cast<const LetStmt *>(stmt), symbols);
    break;
  case StmtKind::Block: {
    int block_num = next_block_num_++;
    process_block(static_cast<const BlockStmt *>(stmt), symbols, block_num);
    break;
  }
  case StmtKind::If: {
    auto *if_stmt = static_cast<const IfStmt *>(stmt);
    process_stmt(if_stmt->then_branch.get(), symbols);
    if (if_stmt->else_branch) {
      process_stmt(if_stmt->else_branch->get(), symbols);
    }
    break;
  }
  case StmtKind::While: {
    auto *while_stmt = static_cast<const WhileStmt *>(stmt);
    process_stmt(while_stmt->body.get(), symbols);
    break;
  }
  default:
    // Return, Expr: no declarations to collect
    break;
  }
}

void SymbolTableBuilder::process_func_decl(const FuncStmt *func,
                                           ScopedSymbolTable &symbols) {
  // Determine symbol origin
  SymbolOrigin origin =
      collecting_prelude_ ? SymbolOrigin::Prelude : SymbolOrigin::User;

  // Check for nested function (only allowed at global scope)
  if (symbols.current_scope()->kind() != ScopeKind::Global) {
    error("Nested function definitions are not yet supported (closures "
          "unimplemented)",
          func->loc.line, func->loc.column);
    return;
  }

  // Extract parameter types
  std::vector<std::string> param_types;
  for (const auto &param : func->params) {
    if (param.type) {
      param_types.push_back(get_type_name(param.type->get()));
    } else {
      // Generics not yet supported - require explicit types
      error("Function parameter '" + param.name +
                "' requires explicit type (generics unimplemented)",
            param.loc.line, param.loc.column);
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

  // Add to global symbol table
  FunctionSignature sig(func->name, param_types, return_type, is_decl_only,
                        origin);
  symbols.add_function(sig);

  // If function has a body, process it in its own scope
  if (func->body) {
    std::string desc = "function " + func->name;
    symbols.push_scope(ScopeKind::Function, desc);

    // Add parameters as variables in function scope
    for (const auto &param : func->params) {
      std::string type = param.type ? get_type_name(param.type->get()) : "";
      VariableBinding binding(param.name, type, param.loc.line,
                              param.loc.column, origin);
      symbols.add_variable(binding);
    }

    // Process function body
    process_stmt(func->body->get(), symbols);

    symbols.pop_scope();
  }
}

void SymbolTableBuilder::process_operator_decl(const OperatorDeclStmt *op,
                                               ScopedSymbolTable &symbols) {
  // Determine symbol origin
  SymbolOrigin origin =
      collecting_prelude_ ? SymbolOrigin::Prelude : SymbolOrigin::User;

  // Extract parameter types
  std::vector<std::string> param_types;
  for (const auto &param : op->params) {
    if (param.type) {
      param_types.push_back(get_type_name(param.type->get()));
    } else {
      error(
          "Operator parameter requires explicit type (generics unimplemented)",
          param.loc.line, param.loc.column);
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

  // Create operator info with origin
  OperatorInfo info(op->op, op->position, op->precedence, op->assoc, signature,
                    origin);

  // Add to symbol table
  symbols.add_operator(info);
}

void SymbolTableBuilder::process_let(const LetStmt *let,
                                     ScopedSymbolTable &symbols) {
  // Determine symbol origin
  SymbolOrigin origin =
      collecting_prelude_ ? SymbolOrigin::Prelude : SymbolOrigin::User;

  // Check for redefinition in current scope
  if (symbols.current_scope()->has_variable_local(let->name)) {
    error("Variable '" + let->name + "' already defined in current scope",
          let->loc.line, let->loc.column);
    return;
  }

  // Get type name if explicit type is provided
  std::string type_name;
  if (let->type) {
    type_name = get_type_name(let->type->get());
  }

  // Add variable to current scope
  VariableBinding binding(let->name, type_name, let->loc.line, let->loc.column,
                          origin);
  symbols.add_variable(binding);
}

void SymbolTableBuilder::process_block(const BlockStmt *block,
                                       ScopedSymbolTable &symbols,
                                       int block_num) {
  std::string desc = "block #" + std::to_string(block_num) + " at line " +
                     std::to_string(block->loc.line);
  symbols.push_scope(ScopeKind::Block, desc);

  for (const auto &stmt : block->stmts) {
    process_stmt(stmt.get(), symbols);
  }

  symbols.pop_scope();
}

std::string SymbolTableBuilder::get_type_name(const Type *type) const {
  if (!type)
    return "";

  switch (type->kind) {
  case TypeKind::Named:
    return type->name;
  default:
    return "";
  }
}

void SymbolTableBuilder::error(const std::string &message, size_t line,
                               size_t column) {
  errors_.push_back({message, line, column});
}

} // namespace pecco
