#include "operator.hpp"

namespace pecco {

void OperatorTable::add_operator(const OperatorInfo &info) {
  auto key = std::make_pair(info.op, info.position);
  operators_[key].push_back(info);
}

std::optional<OperatorInfo>
OperatorTable::find_operator(const std::string &op, OpPosition position) const {
  auto key = std::make_pair(op, position);
  auto it = operators_.find(key);
  if (it != operators_.end() && !it->second.empty()) {
    return it->second[0]; // Return first overload
  }
  return std::nullopt;
}

std::vector<OperatorInfo>
OperatorTable::find_operators(const std::string &op,
                              OpPosition position) const {
  auto key = std::make_pair(op, position);
  auto it = operators_.find(key);
  if (it != operators_.end()) {
    return it->second; // Return all overloads
  }
  return {};
}

std::vector<OperatorInfo>
OperatorTable::find_all_operators(const std::string &op) const {
  std::vector<OperatorInfo> result;
  for (const auto &[key, infos] : operators_) {
    if (key.first == op) {
      result.insert(result.end(), infos.begin(), infos.end());
    }
  }
  return result;
}

bool OperatorTable::has_operator(const std::string &op,
                                 OpPosition position) const {
  auto key = std::make_pair(op, position);
  return operators_.find(key) != operators_.end();
}

} // namespace pecco
