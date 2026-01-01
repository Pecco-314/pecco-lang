#include "scope.hpp"

namespace pecco {

// === Scope ===

void Scope::add_variable(const VariableBinding &binding) {
  variables_[binding.name] = binding;
}

bool Scope::has_variable_local(const std::string &name) const {
  return variables_.find(name) != variables_.end();
}

bool Scope::has_variable(const std::string &name) const {
  if (has_variable_local(name)) {
    return true;
  }
  if (parent_) {
    return parent_->has_variable(name);
  }
  return false;
}

std::optional<VariableBinding>
Scope::find_variable(const std::string &name) const {
  auto it = variables_.find(name);
  if (it != variables_.end()) {
    return it->second;
  }
  if (parent_) {
    return parent_->find_variable(name);
  }
  return std::nullopt;
}

std::vector<VariableBinding> Scope::get_local_variables() const {
  std::vector<VariableBinding> result;
  for (const auto &[name, binding] : variables_) {
    result.push_back(binding);
  }
  return result;
}

// === ScopedSymbolTable ===

void ScopedSymbolTable::push_scope(ScopeKind kind,
                                   const std::string &description) {
  auto new_scope = std::make_unique<Scope>(kind, current_scope_, description);
  Scope *new_scope_ptr = new_scope.get();

  // Link to parent
  if (current_scope_) {
    current_scope_->add_child(new_scope_ptr);
  }

  current_scope_ = new_scope_ptr;
  scopes_.push_back(std::move(new_scope));
}

void ScopedSymbolTable::pop_scope() {
  if (current_scope_ && current_scope_->parent()) {
    current_scope_ = current_scope_->parent();
  }
}

} // namespace pecco
