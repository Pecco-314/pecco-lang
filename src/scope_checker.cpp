#include "scope_checker.hpp"
#include <sstream>

namespace pecco {

bool ScopeChecker::check(const std::vector<StmtPtr> &stmts,
                         ScopedSymbolTable &symbols) {
  for (const auto &stmt : stmts) {
    check_stmt(stmt.get(), symbols);
  }
  return !has_errors();
}

void ScopeChecker::check_stmt(const Stmt *stmt, ScopedSymbolTable &symbols) {
  if (!stmt)
    return;

  switch (stmt->kind) {
  case StmtKind::Func:
    // Check for nested function definition (not yet supported)
    if (symbols.current_scope()->kind() != ScopeKind::Global) {
      error("Nested function definitions are not yet supported (closures "
            "unimplemented)",
            static_cast<const FuncStmt *>(stmt)->loc.line,
            static_cast<const FuncStmt *>(stmt)->loc.column);
      return;
    }
    check_func(static_cast<const FuncStmt *>(stmt), symbols);
    break;
  case StmtKind::Block:
    check_block(static_cast<const BlockStmt *>(stmt), symbols);
    break;
  case StmtKind::Let:
    check_let(static_cast<const LetStmt *>(stmt), symbols);
    break;
  case StmtKind::If: {
    auto *if_stmt = static_cast<const IfStmt *>(stmt);
    check_expr(if_stmt->condition.get(), symbols);
    check_stmt(if_stmt->then_branch.get(), symbols);
    if (if_stmt->else_branch) {
      check_stmt(if_stmt->else_branch->get(), symbols);
    }
    break;
  }
  case StmtKind::While: {
    auto *while_stmt = static_cast<const WhileStmt *>(stmt);
    check_expr(while_stmt->condition.get(), symbols);
    check_stmt(while_stmt->body.get(), symbols);
    break;
  }
  case StmtKind::Return: {
    auto *ret = static_cast<const ReturnStmt *>(stmt);
    if (ret->value) {
      check_expr(ret->value->get(), symbols);
    }
    break;
  }
  case StmtKind::Expr: {
    auto *expr_stmt = static_cast<const ExprStmt *>(stmt);
    check_expr(expr_stmt->expr.get(), symbols);
    break;
  }
  case StmtKind::OperatorDecl:
    // Operator declarations are already processed in declaration collection
    break;
  }
}

void ScopeChecker::check_func(const FuncStmt *func,
                              ScopedSymbolTable &symbols) {
  if (!func->body) {
    // Declaration only, no body to check
    return;
  }

  // Enter function scope
  symbols.push_scope(ScopeKind::Function);

  // Add parameters to current scope
  for (const auto &param : func->params) {
    std::string type_name;
    if (param.type) {
      // Type is a unique_ptr<Type>, access through get()
      if (param.type->get()->kind == TypeKind::Named) {
        type_name = param.type->get()->name;
      }
    }
    symbols.add_variable(VariableBinding(param.name, type_name, func->loc.line,
                                         func->loc.column));
  }

  // Check function body (body is optional<StmtPtr>)
  if (func->body) {
    check_stmt(func->body->get(), symbols);
  }

  // Exit function scope
  symbols.pop_scope();
}

void ScopeChecker::check_block(const BlockStmt *block,
                               ScopedSymbolTable &symbols) {
  // Enter block scope
  symbols.push_scope(ScopeKind::Block);

  // Check all statements in block
  for (const auto &stmt : block->stmts) {
    check_stmt(stmt.get(), symbols);
  }

  // Exit block scope
  symbols.pop_scope();
}

void ScopeChecker::check_let(const LetStmt *let, ScopedSymbolTable &symbols) {
  // Check if variable already exists in current scope
  if (symbols.current_scope()->has_variable_local(let->name)) {
    std::ostringstream oss;
    oss << "Variable '" << let->name << "' already defined in current scope";
    error(oss.str(), let->loc.line, let->loc.column);
    return;
  }

  // Check initializer expression
  if (let->init) {
    check_expr(let->init.get(), symbols);
  }

  // Add variable to current scope
  std::string type_name;
  if (let->type) {
    if (let->type->get()->kind == TypeKind::Named) {
      type_name = let->type->get()->name;
    }
  }
  symbols.add_variable(
      VariableBinding(let->name, type_name, let->loc.line, let->loc.column));
}

void ScopeChecker::check_expr(const Expr *expr, ScopedSymbolTable &symbols) {
  if (!expr)
    return;

  // TODO: Full expression type checking
  // For now, just check identifier references
  if (expr->kind == ExprKind::Identifier) {
    auto *ident = static_cast<const IdentifierExpr *>(expr);
    if (!symbols.has_variable(ident->name) &&
        !symbols.has_function(ident->name)) {
      std::ostringstream oss;
      oss << "Undefined variable or function '" << ident->name << "'";
      error(oss.str(), expr->loc.line, expr->loc.column);
    }
  } else if (expr->kind == ExprKind::Call) {
    auto *call = static_cast<const CallExpr *>(expr);
    check_expr(call->callee.get(), symbols);
    for (const auto &arg : call->args) {
      check_expr(arg.get(), symbols);
    }
  } else if (expr->kind == ExprKind::Binary) {
    auto *bin = static_cast<const BinaryExpr *>(expr);
    check_expr(bin->left.get(), symbols);
    check_expr(bin->right.get(), symbols);
  } else if (expr->kind == ExprKind::Unary) {
    auto *unary = static_cast<const UnaryExpr *>(expr);
    check_expr(unary->operand.get(), symbols);
  } else if (expr->kind == ExprKind::OperatorSeq) {
    auto *seq = static_cast<const OperatorSeqExpr *>(expr);
    for (const auto &item : seq->items) {
      if (item.kind == OpSeqItem::Kind::Operand) {
        check_expr(item.operand.get(), symbols);
      }
    }
  }
}

void ScopeChecker::error(const std::string &message, size_t line,
                         size_t column) {
  errors_.push_back({message, line, column});
}

} // namespace pecco
