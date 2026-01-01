#include "symbol_table.hpp"

namespace pecco {

void SymbolTable::add_function(const FunctionSignature &sig) {
  functions_[sig.name].push_back(sig);
}

void SymbolTable::add_operator(const OperatorInfo &info) {
  operators_.add_operator(info);
}

std::vector<FunctionSignature>
SymbolTable::find_functions(const std::string &name) const {
  auto it = functions_.find(name);
  if (it != functions_.end()) {
    return it->second;
  }
  return {};
}

std::optional<OperatorInfo>
SymbolTable::find_operator(const std::string &op, OpPosition position) const {
  return operators_.find_operator(op, position);
}

std::vector<OperatorInfo>
SymbolTable::find_operators(const std::string &op, OpPosition position) const {
  return operators_.find_operators(op, position);
}

std::vector<OperatorInfo>
SymbolTable::find_all_operators(const std::string &op) const {
  return operators_.find_all_operators(op);
}

bool SymbolTable::has_function(const std::string &name) const {
  return functions_.find(name) != functions_.end();
}

bool SymbolTable::has_operator(const std::string &op,
                               OpPosition position) const {
  return operators_.has_operator(op, position);
}

std::vector<std::string> SymbolTable::get_all_function_names() const {
  std::vector<std::string> names;
  for (const auto &[name, _] : functions_) {
    names.push_back(name);
  }
  return names;
}

std::vector<OperatorInfo> SymbolTable::get_all_operators() const {
  std::vector<OperatorInfo> result;
  for (const auto &[key, overloads] : operators_.get_operators()) {
    for (const auto &op : overloads) {
      result.push_back(op);
    }
  }
  return result;
}

} // namespace pecco
