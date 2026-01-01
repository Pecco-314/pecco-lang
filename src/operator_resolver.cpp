#include "operator_resolver.hpp"
#include <functional>

namespace pecco {

ExprPtr OperatorResolver::resolve_expr(ExprPtr expr,
                                       const SymbolTable &symbol_table,
                                       std::vector<std::string> &errors) {
  if (!expr)
    return nullptr;

  switch (expr->kind) {
  case ExprKind::OperatorSeq:
    return resolve_operator_seq(static_cast<OperatorSeqExpr *>(expr.get()),
                                symbol_table, errors);

  case ExprKind::Call: {
    auto *call = static_cast<CallExpr *>(expr.get());
    // Recursively resolve callee
    call->callee = resolve_expr(std::move(call->callee), symbol_table, errors);
    // Recursively resolve arguments
    for (auto &arg : call->args) {
      arg = resolve_expr(std::move(arg), symbol_table, errors);
    }
    return expr;
  }

  // Literals and identifiers don't need resolution
  default:
    return expr;
  }
}

void OperatorResolver::resolve_stmt(Stmt *stmt, const SymbolTable &symbol_table,
                                    std::vector<std::string> &errors) {
  if (!stmt)
    return;

  switch (stmt->kind) {
  case StmtKind::Let: {
    auto *let = static_cast<LetStmt *>(stmt);
    if (let->init) {
      let->init = resolve_expr(std::move(let->init), symbol_table, errors);
    }
    break;
  }
  case StmtKind::Func: {
    auto *func = static_cast<FuncStmt *>(stmt);
    if (func->body) {
      resolve_stmt(func->body->get(), symbol_table, errors);
    }
    break;
  }
  case StmtKind::OperatorDecl: {
    auto *op = static_cast<OperatorDeclStmt *>(stmt);
    if (op->body) {
      resolve_stmt(op->body->get(), symbol_table, errors);
    }
    break;
  }
  case StmtKind::If: {
    auto *if_stmt = static_cast<IfStmt *>(stmt);
    if (if_stmt->condition) {
      if_stmt->condition =
          resolve_expr(std::move(if_stmt->condition), symbol_table, errors);
    }
    if (if_stmt->then_branch) {
      resolve_stmt(if_stmt->then_branch.get(), symbol_table, errors);
    }
    if (if_stmt->else_branch) {
      resolve_stmt(if_stmt->else_branch->get(), symbol_table, errors);
    }
    break;
  }
  case StmtKind::Return: {
    auto *ret = static_cast<ReturnStmt *>(stmt);
    if (ret->value) {
      ret->value = resolve_expr(std::move(*ret->value), symbol_table, errors);
    }
    break;
  }
  case StmtKind::While: {
    auto *while_stmt = static_cast<WhileStmt *>(stmt);
    if (while_stmt->condition) {
      while_stmt->condition =
          resolve_expr(std::move(while_stmt->condition), symbol_table, errors);
    }
    if (while_stmt->body) {
      resolve_stmt(while_stmt->body.get(), symbol_table, errors);
    }
    break;
  }
  case StmtKind::Expr: {
    auto *expr_stmt = static_cast<ExprStmt *>(stmt);
    if (expr_stmt->expr) {
      expr_stmt->expr =
          resolve_expr(std::move(expr_stmt->expr), symbol_table, errors);
    }
    break;
  }
  case StmtKind::Block: {
    auto *block = static_cast<BlockStmt *>(stmt);
    for (auto &s : block->stmts) {
      resolve_stmt(s.get(), symbol_table, errors);
    }
    break;
  }
  default:
    break;
  }
}

ExprPtr
OperatorResolver::resolve_operator_seq(const OperatorSeqExpr *seq,
                                       const SymbolTable &symbol_table,
                                       std::vector<std::string> &errors) {
  // Helper to clone an operand
  std::function<ExprPtr(const Expr *)> clone_operand =
      [&](const Expr *operand) -> ExprPtr {
    switch (operand->kind) {
    case ExprKind::IntLiteral:
      return std::make_unique<IntLiteralExpr>(
          static_cast<const IntLiteralExpr *>(operand)->value, operand->loc);
    case ExprKind::FloatLiteral:
      return std::make_unique<FloatLiteralExpr>(
          static_cast<const FloatLiteralExpr *>(operand)->value, operand->loc);
    case ExprKind::StringLiteral:
      return std::make_unique<StringLiteralExpr>(
          static_cast<const StringLiteralExpr *>(operand)->value, operand->loc);
    case ExprKind::BoolLiteral:
      return std::make_unique<BoolLiteralExpr>(
          static_cast<const BoolLiteralExpr *>(operand)->value, operand->loc);
    case ExprKind::Identifier:
      return std::make_unique<IdentifierExpr>(
          static_cast<const IdentifierExpr *>(operand)->name, operand->loc);
    case ExprKind::OperatorSeq:
      // Recursively resolve nested operator sequences (from parentheses)
      return resolve_operator_seq(static_cast<const OperatorSeqExpr *>(operand),
                                  symbol_table, errors);
    case ExprKind::Call: {
      // Clone call expression
      auto *call = static_cast<const CallExpr *>(operand);
      auto cloned_callee = clone_operand(call->callee.get());
      std::vector<ExprPtr> cloned_args;
      for (const auto &arg : call->args) {
        cloned_args.push_back(clone_operand(arg.get()));
      }
      return std::make_unique<CallExpr>(std::move(cloned_callee),
                                        std::move(cloned_args), operand->loc);
    }
    default:
      error("Cannot clone expression of type " +
                std::to_string(static_cast<int>(operand->kind)),
            seq->loc.line, seq->loc.column, errors);
      return nullptr;
    }
  };

  // Step 1: Greedy algorithm to fold prefix and postfix operators
  // Result: operand infix operand infix operand ...

  std::vector<ExprPtr>
      operands; // Folded expressions (with prefix/postfix applied)
  std::vector<std::string> infix_ops;
  std::vector<int> infix_precs;
  std::vector<Associativity> infix_assocs;
  std::vector<SourceLocation> infix_locs; // Locations of infix operators

  size_t idx = 0;

  while (idx < seq->items.size()) {
    // Read prefix operators
    std::vector<std::string> prefix_ops;
    while (idx < seq->items.size() &&
           seq->items[idx].kind == OpSeqItem::Kind::Operator) {
      const auto &op = seq->items[idx].op;
      auto op_info = symbol_table.find_operator(op, OpPosition::Prefix);
      if (!op_info) {
        // Not a valid prefix operator
        error("Operator '" + op + "' cannot be used as prefix operator here",
              seq->items[idx].loc.line, seq->items[idx].loc.column, errors);
        return nullptr;
      }
      prefix_ops.push_back(op);
      idx++;
    }

    // Read primary operand
    if (idx >= seq->items.size() ||
        seq->items[idx].kind != OpSeqItem::Kind::Operand) {
      error("Expected operand after prefix operators", seq->loc.line,
            seq->loc.column, errors);
      return nullptr;
    }
    ExprPtr current = clone_operand(seq->items[idx].operand.get());
    if (!current) {
      return nullptr;
    }
    idx++;

    // Apply prefix operators (right to left)
    for (int i = static_cast<int>(prefix_ops.size()) - 1; i >= 0; --i) {
      current = std::make_unique<UnaryExpr>(prefix_ops[i], std::move(current),
                                            OpPosition::Prefix, seq->loc);
    }

    // Read postfix operators (greedily until we hit something that can't be
    // postfix)
    while (idx < seq->items.size() &&
           seq->items[idx].kind == OpSeqItem::Kind::Operator) {
      const auto &op = seq->items[idx].op;

      // Check if this can be a postfix operator
      auto postfix_info = symbol_table.find_operator(op, OpPosition::Postfix);
      if (!postfix_info) {
        // Can't be postfix, must stop here
        break;
      }

      // Greedy: apply as postfix
      current = std::make_unique<UnaryExpr>(op, std::move(current),
                                            OpPosition::Postfix, seq->loc);
      idx++;
    }

    // Now current is: prefix* operand postfix*
    operands.push_back(std::move(current));

    // Read infix operator (if any)
    if (idx < seq->items.size()) {
      if (seq->items[idx].kind != OpSeqItem::Kind::Operator) {
        error("Expected infix operator between operands", seq->loc.line,
              seq->loc.column, errors);
        return nullptr;
      }

      const auto &op = seq->items[idx].op;
      auto op_info = symbol_table.find_operator(op, OpPosition::Infix);
      if (!op_info) {
        error("Operator '" + op + "' cannot be used as infix operator",
              seq->items[idx].loc.line, seq->items[idx].loc.column, errors);
        return nullptr;
      }

      infix_ops.push_back(op);
      infix_precs.push_back(op_info->precedence);
      infix_assocs.push_back(op_info->assoc);
      infix_locs.push_back(seq->items[idx].loc); // Record operator location
      idx++;
    }
  }

  // Validate: infix_ops.size() should be operands.size() - 1
  if (infix_ops.size() != operands.size() - 1) {
    error("Operator sequence structure error: " +
              std::to_string(infix_ops.size()) + " infix operators for " +
              std::to_string(operands.size()) + " operands",
          seq->loc.line, seq->loc.column, errors);
    return nullptr;
  }

  // Step 2: Build infix tree by precedence and associativity
  if (operands.size() == 1) {
    return std::move(operands[0]);
  }

  auto result = build_infix_tree(operands, infix_ops, infix_precs, infix_assocs,
                                 infix_locs, 0, operands.size() - 1, errors);
  if (!result) {
    // Error already reported in build_infix_tree
    return nullptr;
  }
  return result;
}

ExprPtr OperatorResolver::build_infix_tree(
    std::vector<ExprPtr> &operands, std::vector<std::string> &operators,
    std::vector<int> &precedences, std::vector<Associativity> &assocs,
    std::vector<SourceLocation> &locations, size_t start, size_t end,
    std::vector<std::string> &errors) {
  // Base case: single operand
  if (start == end) {
    return std::move(operands[start]);
  }

  // Find the operator with lowest precedence to split on
  // For equal precedence:
  //   - Left-associative: use the rightmost one
  //   - Right-associative: use the leftmost one
  //   - Mixed associativity at same precedence: ERROR

  int lowest_prec = 1000;
  size_t split_pos = start;
  bool found = false;
  Associativity lowest_prec_assoc = Associativity::Left;

  // Scan operators between start and end
  // Operator i is between operands[i] and operands[i+1]
  for (size_t i = start; i < end; ++i) {
    int prec = precedences[i];
    Associativity assoc = assocs[i];

    if (prec < lowest_prec) {
      // Found new lowest precedence
      lowest_prec = prec;
      split_pos = i;
      lowest_prec_assoc = assoc;
      found = true;
    } else if (prec == lowest_prec) {
      // Same precedence - check for mixed associativity
      if (assoc != lowest_prec_assoc) {
        error(
            "Mixed associativity at same precedence level: operator '" +
                operators[i] + "' (" +
                (assoc == Associativity::Left ? "assoc_left" : "assoc_right") +
                ") conflicts with operator '" + operators[split_pos] + "' (" +
                (lowest_prec_assoc == Associativity::Left ? "assoc_left"
                                                          : "assoc_right") +
                ") at precedence " + std::to_string(prec),
            locations[i].line, locations[i].column, errors);
        return nullptr;
      }

      // Same precedence and same associativity
      if (assoc == Associativity::Left) {
        // Left-associative: prefer rightmost
        split_pos = i;
      }
      // Right-associative: keep leftmost (don't update split_pos)
    }
  }

  if (!found) {
    // Should not happen if validation passed
    return std::move(operands[start]);
  }

  // Recursively build left and right subtrees
  const std::string &split_op = operators[split_pos];

  ExprPtr left = build_infix_tree(operands, operators, precedences, assocs,
                                  locations, start, split_pos, errors);
  if (!left) {
    return nullptr;
  }

  ExprPtr right = build_infix_tree(operands, operators, precedences, assocs,
                                   locations, split_pos + 1, end, errors);
  if (!right) {
    return nullptr;
  }

  return std::make_unique<BinaryExpr>(split_op, std::move(left),
                                      std::move(right), locations[split_pos]);
}

void OperatorResolver::error(const std::string &message, size_t line,
                             size_t column, std::vector<std::string> &errors) {
  std::string full_msg = message;
  if (line > 0) {
    full_msg = "at " + std::to_string(line) + ":" + std::to_string(column) +
               ": " + message;
  }
  errors.push_back(full_msg);
}

} // namespace pecco
