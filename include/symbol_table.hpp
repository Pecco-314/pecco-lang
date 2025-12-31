#pragma once

#include "ast.hpp"
#include "operator.hpp"
#include <map>
#include <optional>
#include <string>
#include <vector>

namespace pecco {

// Function signature information
struct FunctionSignature {
  std::string name;
  std::vector<std::string> param_types; // Parameter type names
  std::string return_type;              // Return type name (empty for void)
  bool is_declaration_only;             // True if no body

  FunctionSignature(std::string name, std::vector<std::string> param_types,
                    std::string return_type, bool is_declaration_only = false)
      : name(std::move(name)), param_types(std::move(param_types)),
        return_type(std::move(return_type)),
        is_declaration_only(is_declaration_only) {}
};

// Symbol table for functions and operators
class SymbolTable {
public:
  // Add a function signature
  void add_function(const FunctionSignature &sig);

  // Add an operator
  void add_operator(const OperatorInfo &info);

  // Find function by name (returns all overloads)
  std::vector<FunctionSignature> find_functions(const std::string &name) const;

  // Find operator by symbol and position
  std::optional<OperatorInfo> find_operator(const std::string &op,
                                            OpPosition position) const;

  // Find all operators with given symbol and position (all overloads)
  std::vector<OperatorInfo> find_operators(const std::string &op,
                                           OpPosition position) const;

  // Find all operators with given symbol
  std::vector<OperatorInfo> find_all_operators(const std::string &op) const;

  // Check if function exists
  bool has_function(const std::string &name) const;

  // Check if operator exists
  bool has_operator(const std::string &op, OpPosition position) const;

  // Get all function names (for debugging/testing)
  std::vector<std::string> get_all_function_names() const;

  // Get all operators (for debugging/testing)
  std::vector<OperatorInfo> get_all_operators() const;

private:
  // Map: function_name -> list of overloads
  std::map<std::string, std::vector<FunctionSignature>> functions_;

  // Operator table
  OperatorTable operators_;
};

} // namespace pecco
