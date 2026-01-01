#pragma once

#include <map>
#include <optional>
#include <string>
#include <vector>

namespace pecco {

// Symbol origin marker
enum class SymbolOrigin {
  User,    // User-defined symbol
  Prelude, // From stdlib prelude
};

// Operator position types
enum class OpPosition {
  Prefix,  // -x, !x
  Infix,   // x + y, x * y
  Postfix, // x++
};

// Operator associativity (only for infix operators)
enum class Associativity {
  Left,  // Left-associative: a + b + c = (a + b) + c
  Right, // Right-associative: a = b = c means a = (b = c)
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

// Prefix operator information (no precedence/associativity needed)
struct PrefixOperatorInfo {
  std::string op;              // Operator symbol
  OperatorSignature signature; // Type signature

  PrefixOperatorInfo(std::string op, OperatorSignature signature)
      : op(std::move(op)), signature(std::move(signature)) {}
};

// Postfix operator information (no precedence/associativity needed)
struct PostfixOperatorInfo {
  std::string op;              // Operator symbol
  OperatorSignature signature; // Type signature

  PostfixOperatorInfo(std::string op, OperatorSignature signature)
      : op(std::move(op)), signature(std::move(signature)) {}
};

// Infix operator information (with precedence and associativity)
struct InfixOperatorInfo {
  std::string op;              // Operator symbol
  int precedence;              // Higher number = higher precedence (0-100)
  Associativity assoc;         // Associativity
  OperatorSignature signature; // Type signature

  InfixOperatorInfo(std::string op, int precedence, Associativity assoc,
                    OperatorSignature signature)
      : op(std::move(op)), precedence(precedence), assoc(assoc),
        signature(std::move(signature)) {}
};

// Unified operator info (for generic access)
struct OperatorInfo {
  std::string op;              // Operator symbol
  OpPosition position;         // Prefix/Infix/Postfix
  int precedence;              // Only valid for infix (0 for prefix/postfix)
  Associativity assoc;         // Only valid for infix
  OperatorSignature signature; // Type signature
  SymbolOrigin origin;         // Where this symbol comes from

  OperatorInfo(std::string op, OpPosition position, int precedence,
               Associativity assoc, OperatorSignature signature,
               SymbolOrigin origin = static_cast<SymbolOrigin>(0))
      : op(std::move(op)), position(position), precedence(precedence),
        assoc(assoc), signature(std::move(signature)), origin(origin) {}

  // Convenience constructors
  static OperatorInfo from_prefix(const PrefixOperatorInfo &info,
                                  SymbolOrigin origin = SymbolOrigin::User) {
    return OperatorInfo(info.op, OpPosition::Prefix, 0, Associativity::Left,
                        info.signature, origin);
  }

  static OperatorInfo from_postfix(const PostfixOperatorInfo &info,
                                   SymbolOrigin origin = SymbolOrigin::User) {
    return OperatorInfo(info.op, OpPosition::Postfix, 0, Associativity::Left,
                        info.signature, origin);
  }

  static OperatorInfo from_infix(const InfixOperatorInfo &info,
                                 SymbolOrigin origin = SymbolOrigin::User) {
    return OperatorInfo(info.op, OpPosition::Infix, info.precedence, info.assoc,
                        info.signature, origin);
  }
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
