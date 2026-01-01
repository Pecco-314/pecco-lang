#include "type_checker.hpp"
#include <sstream>

namespace pecco {

void TypeChecker::error(const std::string &msg, size_t line, size_t column) {
  errors_.emplace_back(msg, line, column);
}

std::string TypeChecker::get_type_name(const Type *type) const {
  if (!type)
    return "";
  return type->name;
}

bool TypeChecker::check(std::vector<StmtPtr> &stmts,
                        const ScopedSymbolTable &symbols) {
  symbols_ = &symbols;
  errors_.clear();
  scope_stack_.clear();

  // Push global scope
  push_scope();

  for (auto &stmt : stmts) {
    check_stmt(stmt.get());
  }

  pop_scope();

  return !has_errors();
}

void TypeChecker::push_scope() { scope_stack_.emplace_back(); }

void TypeChecker::pop_scope() {
  if (!scope_stack_.empty()) {
    scope_stack_.pop_back();
  }
}

void TypeChecker::add_variable_type(const std::string &name,
                                    const std::string &type) {
  if (!scope_stack_.empty()) {
    scope_stack_.back()[name] = type;
  }
}

std::string TypeChecker::lookup_variable_type(const std::string &name) const {
  // Search from innermost to outermost scope
  for (auto it = scope_stack_.rbegin(); it != scope_stack_.rend(); ++it) {
    auto found = it->find(name);
    if (found != it->end()) {
      return found->second;
    }
  }
  return ""; // Not found
}

void TypeChecker::check_stmt(Stmt *stmt) {
  if (!stmt)
    return;

  switch (stmt->kind) {
  case StmtKind::Let: {
    auto *let = static_cast<LetStmt *>(stmt);
    if (let->init) {
      std::string init_type = check_expr(let->init.get());

      // If variable has explicit type annotation, check compatibility
      if (let->type) {
        std::string declared_type = get_type_name(let->type.value().get());
        if (!init_type.empty() && init_type != declared_type) {
          std::ostringstream msg;
          msg << "Type mismatch: variable '" << let->name << "' declared as '"
              << declared_type << "' but initialized with '" << init_type
              << "'";
          error(msg.str(), let->init->loc.line, let->init->loc.column);
        }
        // Record the declared type
        add_variable_type(let->name, declared_type);
      } else {
        // Record the inferred type
        if (!init_type.empty()) {
          add_variable_type(let->name, init_type);
        }
      }
    }
    break;
  }

  case StmtKind::Func: {
    auto *func = static_cast<FuncStmt *>(stmt);
    if (func->body) {
      push_scope();

      // Add function parameters to scope
      for (const auto &param : func->params) {
        if (param.type) {
          std::string param_type = get_type_name(param.type.value().get());
          add_variable_type(param.name, param_type);
        }
      }

      check_stmt(func->body.value().get());
      pop_scope();
    }
    break;
  }

  case StmtKind::Return: {
    auto *ret = static_cast<ReturnStmt *>(stmt);
    if (ret->value) {
      check_expr(ret->value.value().get());
    }
    break;
  }

  case StmtKind::Expr: {
    auto *expr_stmt = static_cast<ExprStmt *>(stmt);
    check_expr(expr_stmt->expr.get());
    break;
  }

  case StmtKind::Block: {
    auto *block = static_cast<BlockStmt *>(stmt);
    push_scope();
    for (auto &s : block->stmts) {
      check_stmt(s.get());
    }
    pop_scope();
    break;
  }

  case StmtKind::If: {
    auto *if_stmt = static_cast<IfStmt *>(stmt);
    std::string cond_type = check_expr(if_stmt->condition.get());
    if (!cond_type.empty() && cond_type != "bool") {
      std::ostringstream msg;
      msg << "If condition must be 'bool', got '" << cond_type << "'";
      error(msg.str(), if_stmt->condition->loc.line,
            if_stmt->condition->loc.column);
    }

    // Each branch gets its own scope (handled by Block statements)
    check_stmt(if_stmt->then_branch.get());
    if (if_stmt->else_branch) {
      check_stmt(if_stmt->else_branch.value().get());
    }
    break;
  }

  case StmtKind::While: {
    auto *while_stmt = static_cast<WhileStmt *>(stmt);
    std::string cond_type = check_expr(while_stmt->condition.get());
    if (!cond_type.empty() && cond_type != "bool") {
      std::ostringstream msg;
      msg << "While condition must be 'bool', got '" << cond_type << "'";
      error(msg.str(), while_stmt->condition->loc.line,
            while_stmt->condition->loc.column);
    }

    check_stmt(while_stmt->body.get());
    break;
  }

  case StmtKind::OperatorDecl:
    // No type checking needed for operator declarations
    break;
  }
}

std::string TypeChecker::check_expr(Expr *expr) {
  if (!expr)
    return "";

  // If already inferred, return it
  if (!expr->inferred_type.empty()) {
    return expr->inferred_type;
  }

  std::string type;

  switch (expr->kind) {
  case ExprKind::IntLiteral:
    type = "i32";
    break;

  case ExprKind::FloatLiteral:
    type = "f64";
    break;

  case ExprKind::StringLiteral:
    type = "string";
    break;

  case ExprKind::BoolLiteral:
    type = "bool";
    break;

  case ExprKind::Identifier: {
    auto *ident = static_cast<IdentifierExpr *>(expr);
    // Look up variable type in scope stack
    type = lookup_variable_type(ident->name);
    if (type.empty()) {
      // Variable not found or type unknown
      // This could be valid for some cases, so don't error here
      // Just leave type as empty
    }
    break;
  }

  case ExprKind::Binary: {
    auto *binary = static_cast<BinaryExpr *>(expr);
    std::string left_type = check_expr(binary->left.get());
    std::string right_type = check_expr(binary->right.get());

    // Look up operator in symbol table
    auto ops = symbols_->find_operators(binary->op, OpPosition::Infix);

    if (ops.empty()) {
      std::ostringstream msg;
      msg << "No infix operator '" << binary->op << "' found";
      error(msg.str(), expr->loc.line, expr->loc.column);
      type = "";
    } else {
      // Try to find matching overload by types
      bool found = false;
      for (const auto &op_info : ops) {
        if (op_info.signature.param_types.size() == 2) {
          if ((left_type.empty() ||
               op_info.signature.param_types[0] == left_type) &&
              (right_type.empty() ||
               op_info.signature.param_types[1] == right_type)) {
            type = op_info.signature.return_type;
            found = true;
            break;
          }
        }
      }

      if (!found) {
        // Just use the first operator's return type
        type = ops[0].signature.return_type;
      }
    }
    break;
  }

  case ExprKind::Unary: {
    auto *unary = static_cast<UnaryExpr *>(expr);
    std::string operand_type = check_expr(unary->operand.get());

    // Look up operator in symbol table
    auto ops = symbols_->find_operators(unary->op, unary->position);

    if (ops.empty()) {
      std::ostringstream msg;
      msg << "No "
          << (unary->position == OpPosition::Prefix ? "prefix" : "postfix")
          << " operator '" << unary->op << "' found";
      error(msg.str(), expr->loc.line, expr->loc.column);
      type = "";
    } else {
      // Try to find matching overload by type
      bool found = false;
      for (const auto &op_info : ops) {
        if (op_info.signature.param_types.size() == 1) {
          if (operand_type.empty() ||
              op_info.signature.param_types[0] == operand_type) {
            type = op_info.signature.return_type;
            found = true;
            break;
          }
        }
      }

      if (!found) {
        // Just use the first operator's return type
        type = ops[0].signature.return_type;
      }
    }
    break;
  }

  case ExprKind::Call: {
    auto *call = static_cast<CallExpr *>(expr);

    // Check argument types
    std::vector<std::string> arg_types;
    for (auto &arg : call->args) {
      arg_types.push_back(check_expr(arg.get()));
    }

    // For now, we need the callee to be an identifier
    if (call->callee->kind != ExprKind::Identifier) {
      error("Function call callee must be an identifier", expr->loc.line,
            expr->loc.column);
      type = "";
      break;
    }

    auto *ident = static_cast<IdentifierExpr *>(call->callee.get());
    std::string func_name = ident->name;

    // Look up function in symbol table
    auto funcs = symbols_->find_functions(func_name);
    if (funcs.empty()) {
      std::ostringstream msg;
      msg << "Unknown function '" << func_name << "'";
      error(msg.str(), expr->loc.line, expr->loc.column);
      type = "";
    } else {
      // Try to find matching overload
      bool found = false;
      for (const auto &func : funcs) {
        if (func.param_types.size() == arg_types.size()) {
          bool match = true;
          for (size_t i = 0; i < arg_types.size(); ++i) {
            if (!arg_types[i].empty() && func.param_types[i] != arg_types[i]) {
              match = false;
              break;
            }
          }
          if (match) {
            type = func.return_type;
            found = true;
            break;
          }
        }
      }

      if (!found) {
        // Just use the first overload's return type
        type = funcs[0].return_type;
      }
    }
    break;
  }

  case ExprKind::OperatorSeq:
    // Should have been resolved already
    error("OperatorSeq should have been resolved before type checking",
          expr->loc.line, expr->loc.column);
    type = "";
    break;
  }

  // Store inferred type
  expr->inferred_type = type;
  return type;
}

} // namespace pecco
