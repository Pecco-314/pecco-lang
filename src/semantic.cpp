#include "semantic.hpp"
#include "lexer.hpp"
#include "parser.hpp"
#include <fstream>
#include <sstream>

namespace pecco {

void SemanticAnalyzer::collect_declarations(const std::vector<StmtPtr> &stmts) {
  for (const auto &stmt : stmts) {
    process_decl(stmt.get());
  }
}

bool SemanticAnalyzer::load_prelude(const std::string &prelude_path) {
  // Load prelude.pl
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
  collect_declarations(stmts);

  return true;
}

void SemanticAnalyzer::process_decl(const Stmt *stmt) {
  if (!stmt)
    return;

  switch (stmt->kind) {
  case StmtKind::Func:
    process_func_decl(static_cast<const FuncStmt *>(stmt));
    break;
  case StmtKind::OperatorDecl:
    process_operator_decl(static_cast<const OperatorDeclStmt *>(stmt));
    break;
  case StmtKind::Block: {
    // Recursively process statements in blocks
    auto *block = static_cast<const BlockStmt *>(stmt);
    for (const auto &s : block->stmts) {
      process_decl(s.get());
    }
    break;
  }
  default:
    // Other statements are ignored in declaration collection phase
    break;
  }
}

void SemanticAnalyzer::process_func_decl(const FuncStmt *func) {
  // Extract parameter types
  std::vector<std::string> param_types;
  for (const auto &param : func->params) {
    if (param.type) {
      param_types.push_back(get_type_name(param.type->get()));
    } else {
      // Type inference not yet supported - require explicit types
      error("Function parameter '" + param.name +
                "' must have explicit type annotation",
            0, 0);
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
  symbol_table_.add_function(sig);
}

void SemanticAnalyzer::process_operator_decl(const OperatorDeclStmt *op) {
  // Extract parameter types
  std::vector<std::string> param_types;
  for (const auto &param : op->params) {
    if (param.type) {
      param_types.push_back(get_type_name(param.type->get()));
    } else {
      error("Operator parameter must have explicit type annotation", 0, 0);
      return;
    }
  }

  // Extract return type
  std::string return_type;
  if (op->return_type) {
    return_type = get_type_name(op->return_type->get());
  } else {
    error("Operator must have explicit return type", 0, 0);
    return;
  }

  // Create operator signature
  OperatorSignature signature(param_types, return_type);

  // Create operator info
  OperatorInfo info(op->op, op->position, op->precedence, op->assoc, signature);

  // Add to symbol table
  symbol_table_.add_operator(info);
}

std::string SemanticAnalyzer::get_type_name(const Type *type) const {
  if (!type)
    return "";

  switch (type->kind) {
  case TypeKind::Named:
    return type->name;
  default:
    return "";
  }
}

void SemanticAnalyzer::error(const std::string &message, size_t line,
                             size_t column) {
  errors_.push_back({message, line, column});
}

// Transform operator sequences to expression trees
ExprPtr SemanticAnalyzer::resolve_operators(ExprPtr expr) {
  if (!expr)
    return nullptr;

  switch (expr->kind) {
  case ExprKind::OperatorSeq:
    return resolve_operator_seq(static_cast<OperatorSeqExpr *>(expr.get()));

  case ExprKind::Call: {
    auto *call = static_cast<CallExpr *>(expr.get());
    // Recursively resolve callee
    call->callee = resolve_operators(std::move(call->callee));
    // Recursively resolve arguments
    for (auto &arg : call->args) {
      arg = resolve_operators(std::move(arg));
    }
    return expr;
  }

  // Literals and identifiers don't need resolution
  default:
    return expr;
  }
}

ExprPtr SemanticAnalyzer::resolve_operator_seq(const OperatorSeqExpr *seq) {
  // Make copies of operands and operators since we need ownership
  std::vector<ExprPtr> operands;
  operands.reserve(seq->operands.size());

  for (size_t i = 0; i < seq->operands.size(); ++i) {
    // Clone the operand (we can't move from const)
    const Expr *operand = seq->operands[i].get();
    ExprPtr cloned;

    // Clone based on type
    switch (operand->kind) {
    case ExprKind::IntLiteral:
      cloned = std::make_unique<IntLiteralExpr>(
          static_cast<const IntLiteralExpr *>(operand)->value);
      break;
    case ExprKind::FloatLiteral:
      cloned = std::make_unique<FloatLiteralExpr>(
          static_cast<const FloatLiteralExpr *>(operand)->value);
      break;
    case ExprKind::StringLiteral:
      cloned = std::make_unique<StringLiteralExpr>(
          static_cast<const StringLiteralExpr *>(operand)->value);
      break;
    case ExprKind::BoolLiteral:
      cloned = std::make_unique<BoolLiteralExpr>(
          static_cast<const BoolLiteralExpr *>(operand)->value);
      break;
    case ExprKind::Identifier:
      cloned = std::make_unique<IdentifierExpr>(
          static_cast<const IdentifierExpr *>(operand)->name);
      break;
    default:
      // For more complex expressions, we'd need proper cloning
      error("Complex nested expressions not yet supported in operator "
            "resolution");
      return nullptr;
    }

    operands.push_back(std::move(cloned));
  }

  // Copy operators
  std::vector<std::string> operators = seq->operators;

  // If only one operand, just return it
  if (operands.size() == 1) {
    return std::move(operands[0]);
  }

  // Build expression tree using precedence climbing
  return build_expr_tree(operands, operators, 0, operands.size() - 1, 0);
}

ExprPtr SemanticAnalyzer::build_expr_tree(std::vector<ExprPtr> &operands,
                                          std::vector<std::string> &operators,
                                          size_t start, size_t end,
                                          int min_prec) {
  // Base case: single operand
  if (start == end) {
    return std::move(operands[start]);
  }

  // Find the operator with lowest precedence to split on
  // For left-associative: rightmost operator with lowest precedence
  // For right-associative: leftmost operator with lowest precedence
  int lowest_prec = 1000;
  size_t split_pos = start;
  bool found = false;

  // Scan operators between start and end
  for (size_t i = start; i < end; ++i) {
    const std::string &op = operators[i];

    // Look up operator info
    auto op_info = symbol_table_.find_operator(op, OpPosition::Infix);
    if (!op_info) {
      error("Unknown operator: " + op);
      continue;
    }

    int prec = op_info->precedence;

    // For left-associative: take rightmost operator with lowest precedence
    // For right-associative: take leftmost operator with lowest precedence
    if (prec < lowest_prec) {
      lowest_prec = prec;
      split_pos = i;
      found = true;
    } else if (prec == lowest_prec && op_info->assoc == Associativity::Left) {
      // For left-assoc with same precedence, take the rightmost
      split_pos = i;
    }
  }

  if (!found) {
    // No operator found, just return first operand
    return std::move(operands[start]);
  }

  // Recursively build left and right subtrees
  ExprPtr left = build_expr_tree(operands, operators, start, split_pos, 0);
  ExprPtr right = build_expr_tree(operands, operators, split_pos + 1, end, 0);

  return std::make_unique<BinaryExpr>(operators[split_pos], std::move(left),
                                      std::move(right));
}

} // namespace pecco
