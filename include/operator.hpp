#pragma once

#include <map>
#include <optional>
#include <string>
#include <vector>

namespace pecco {

// Operator position types
enum class OpPosition {
  Prefix,  // -x, !x
  Infix,   // x + y, x * y
  Postfix, // x++
};

// Operator associativity
enum class Associativity {
  Left,  // Left-associative
  Right, // Right-associative
  None,  // Non-associative (e.g., comparison operators)
};

// Type information for operator signature
struct OperatorSignature {
  std::vector<std::string> param_types; // Parameter types
  std::string return_type;              // Return type

  OperatorSignature(std::vector<std::string> param_types,
                    std::string return_type)
      : param_types(std::move(param_types)),
        return_type(std::move(return_type)) {}
};

// Operator information
struct OperatorInfo {
  std::string op;              // Operator symbol
  OpPosition position;         // Prefix/Infix/Postfix
  int precedence;              // Higher number = higher precedence (0-100)
  Associativity assoc;         // Associativity
  OperatorSignature signature; // Type signature

  OperatorInfo(std::string op, OpPosition position, int precedence,
               Associativity assoc, OperatorSignature signature)
      : op(std::move(op)), position(position), precedence(precedence),
        assoc(assoc), signature(std::move(signature)) {}
};

// Operator table for symbol resolution
class OperatorTable {
public:
  // Add an operator to the table
  void add_operator(const OperatorInfo &info);

  // Find operator by symbol and position (returns first match)
  std::optional<OperatorInfo> find_operator(const std::string &op,
                                            OpPosition position) const;

  // Find all operators with given symbol and position (all overloads)
  std::vector<OperatorInfo> find_operators(const std::string &op,
                                           OpPosition position) const;

  // Find all operators with given symbol (different positions)
  std::vector<OperatorInfo> find_all_operators(const std::string &op) const;

  // Check if an operator exists
  bool has_operator(const std::string &op, OpPosition position) const;

  // Get all operators (for iteration)
  const std::map<std::pair<std::string, OpPosition>,
                 std::vector<OperatorInfo>> &
  get_operators() const {
    return operators_;
  }

private:
  // Key: (operator_symbol, position) -> list of overloads
  std::map<std::pair<std::string, OpPosition>, std::vector<OperatorInfo>>
      operators_;
};

// Default operator precedences (standard precedence levels)
namespace precedence {
constexpr int ASSIGNMENT = 10;     // = += -= etc (not yet implemented)
constexpr int LOGICAL_OR = 20;     // ||
constexpr int LOGICAL_AND = 30;    // &&
constexpr int BITWISE_OR = 40;     // |
constexpr int BITWISE_XOR = 45;    // ^
constexpr int BITWISE_AND = 50;    // &
constexpr int EQUALITY = 55;       // == !=
constexpr int RELATIONAL = 60;     // < > <= >=
constexpr int SHIFT = 65;          // << >>
constexpr int ADDITIVE = 70;       // + -
constexpr int MULTIPLICATIVE = 80; // * / %
constexpr int POWER = 90;          // **
constexpr int UNARY = 95;          // - ! (prefix)
} // namespace precedence

} // namespace pecco
