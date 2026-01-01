#include "ast.hpp"
#include <iostream>

namespace pecco {

// Helper function for indentation
static void print_indent(std::ostream &os, int indent) {
  for (int i = 0; i < indent; ++i) {
    os << "  ";
  }
}

// Type print implementation

void Type::print(std::ostream &os) const { os << name; }

// Expression print implementations

void IntLiteralExpr::print(std::ostream &os) const {
  os << "IntLiteral(" << value << ")";
}

void FloatLiteralExpr::print(std::ostream &os) const {
  os << "FloatLiteral(" << value << ")";
}

void StringLiteralExpr::print(std::ostream &os) const {
  os << "StringLiteral(\"" << value << "\")";
}

void BoolLiteralExpr::print(std::ostream &os) const {
  os << "BoolLiteral(" << (value ? "true" : "false") << ")";
}

void IdentifierExpr::print(std::ostream &os) const {
  os << "Identifier(" << name << ")";
}

void BinaryExpr::print(std::ostream &os) const {
  os << "Binary(" << op << ", ";
  left->print(os);
  os << ", ";
  right->print(os);
  os << ")";
}

void UnaryExpr::print(std::ostream &os) const {
  os << "Unary(" << op << ", ";
  operand->print(os);
  os << ", ";
  switch (position) {
  case OpPosition::Prefix:
    os << "Prefix";
    break;
  case OpPosition::Postfix:
    os << "Postfix";
    break;
  default:
    os << "Unknown";
    break;
  }
  os << ")";
}

void OperatorSeqExpr::print(std::ostream &os) const {
  os << "OperatorSeq(";

  for (size_t i = 0; i < items.size(); ++i) {
    if (i > 0) {
      os << " ";
    }

    if (items[i].kind == OpSeqItem::Kind::Operator) {
      os << items[i].op;
    } else {
      items[i].operand->print(os);
    }
  }

  os << ")";
}

void CallExpr::print(std::ostream &os) const {
  os << "Call(";
  callee->print(os);
  os << ", [";
  for (size_t i = 0; i < args.size(); ++i) {
    if (i > 0)
      os << ", ";
    args[i]->print(os);
  }
  os << "])";
}

// Statement print implementations

void LetStmt::print(std::ostream &os, int indent) const {
  print_indent(os, indent);
  os << "Let(" << name;
  if (type) {
    os << " : ";
    (*type)->print(os);
  }
  os << " = ";
  init->print(os);
  os << ")\n";
}

void FuncStmt::print(std::ostream &os, int indent) const {
  print_indent(os, indent);
  os << "Func(" << name << "(";
  for (size_t i = 0; i < params.size(); ++i) {
    if (i > 0)
      os << ", ";
    os << params[i].name;
    if (params[i].type) {
      os << " : ";
      (*params[i].type)->print(os);
    }
  }
  os << ")";
  if (return_type) {
    os << " : ";
    (*return_type)->print(os);
  }
  os << ")\n";
  if (body) {
    (*body)->print(os, indent + 1);
  }
}

void OperatorDeclStmt::print(std::ostream &os, int indent) const {
  print_indent(os, indent);
  os << "OperatorDecl(";

  // Print position
  switch (position) {
  case OpPosition::Prefix:
    os << "prefix ";
    break;
  case OpPosition::Infix:
    os << "infix ";
    break;
  case OpPosition::Postfix:
    os << "postfix ";
    break;
  }

  os << op << "(";

  // Print parameters
  for (size_t i = 0; i < params.size(); ++i) {
    if (i > 0)
      os << ", ";
    os << params[i].name;
    if (params[i].type) {
      os << " : ";
      (*params[i].type)->print(os);
    }
  }
  os << ")";

  if (return_type) {
    os << " : ";
    (*return_type)->print(os);
  }

  // Print precedence and associativity only for infix operators
  if (position == OpPosition::Infix) {
    os << " prec " << precedence;
    if (assoc == Associativity::Right) {
      os << " assoc_right";
    }
    // assoc_left is default, can be omitted
  }

  os << ")\n";

  // Print body if present
  if (body) {
    (*body)->print(os, indent + 1);
  }
}

void IfStmt::print(std::ostream &os, int indent) const {
  print_indent(os, indent);
  os << "If(";
  condition->print(os);
  os << ")\n";
  then_branch->print(os, indent + 1);
  if (else_branch) {
    print_indent(os, indent);
    os << "Else\n";
    (*else_branch)->print(os, indent + 1);
  }
}

void ReturnStmt::print(std::ostream &os, int indent) const {
  print_indent(os, indent);
  os << "Return(";
  if (value) {
    (*value)->print(os);
  }
  os << ")\n";
}

void WhileStmt::print(std::ostream &os, int indent) const {
  print_indent(os, indent);
  os << "While(";
  condition->print(os);
  os << ")\n";
  body->print(os, indent + 1);
}

void ExprStmt::print(std::ostream &os, int indent) const {
  print_indent(os, indent);
  os << "Expr(";
  if (expr) {
    expr->print(os);
  } else {
    os << "<null>";
  }
  os << ")\n";
}

void BlockStmt::print(std::ostream &os, int indent) const {
  print_indent(os, indent);
  os << "Block\n";
  for (const auto &s : stmts) {
    s->print(os, indent + 1);
  }
}

} // namespace pecco
