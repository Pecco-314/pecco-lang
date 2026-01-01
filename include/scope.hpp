#pragma once

#include "symbol_table.hpp"
#include <map>
#include <memory>
#include <optional>
#include <string>
#include <vector>

namespace pecco {

// Scope type
enum class ScopeKind {
  Global,   // File-level scope (functions, operators)
  Function, // Function body scope (includes parameters)
  Block,    // Block scope
};

// Variable binding information
struct VariableBinding {
  std::string name;
  std::string type;    // Type name (empty if not yet inferred)
  size_t line;         // Line where defined
  size_t column;       // Column where defined
  SymbolOrigin origin; // Where this symbol comes from

  VariableBinding() : line(0), column(0), origin(SymbolOrigin::User) {}

  VariableBinding(std::string name, std::string type, size_t line,
                  size_t column, SymbolOrigin origin = SymbolOrigin::User)
      : name(std::move(name)), type(std::move(type)), line(line),
        column(column), origin(origin) {}
};

// Scope: manages variables and nested scopes
// Each scope has a unique ID for tracking in hierarchical symbol table
class Scope {
public:
  explicit Scope(ScopeKind kind, Scope *parent = nullptr,
                 std::string description = "")
      : kind_(kind), parent_(parent), description_(std::move(description)) {}

  // Add a variable binding to current scope
  void add_variable(const VariableBinding &binding);

  // Check if variable exists in current scope only
  bool has_variable_local(const std::string &name) const;

  // Check if variable exists in current scope or any parent scope
  bool has_variable(const std::string &name) const;

  // Find variable in current scope or parent scopes
  std::optional<VariableBinding> find_variable(const std::string &name) const;

  // Get all variables in current scope (for debugging)
  std::vector<VariableBinding> get_local_variables() const;

  // Get scope kind
  ScopeKind kind() const { return kind_; }

  // Get parent scope
  Scope *parent() const { return parent_; }

  // Get scope description (e.g., "function main", "block in line 10")
  const std::string &description() const { return description_; }

  // Add child scope (for hierarchical tracking)
  void add_child(Scope *child) { children_.push_back(child); }

  // Get child scopes
  const std::vector<Scope *> &children() const { return children_; }

private:
  ScopeKind kind_;
  Scope *parent_;           // nullptr for global scope
  std::string description_; // Human-readable description
  std::map<std::string, VariableBinding> variables_;
  std::vector<Scope *> children_; // Child scopes
};

// Scoped symbol table: combines SymbolTable (global) with Scope (local)
class ScopedSymbolTable {
public:
  ScopedSymbolTable() : global_symbols_(), current_scope_(nullptr) {
    // Create global scope
    global_scope_ = std::make_unique<Scope>(ScopeKind::Global);
    current_scope_ = global_scope_.get();
  }

  // === Global symbols (functions, operators) ===

  void add_function(const FunctionSignature &sig) {
    global_symbols_.add_function(sig);
  }

  void add_operator(const OperatorInfo &info) {
    global_symbols_.add_operator(info);
  }

  std::vector<FunctionSignature> find_functions(const std::string &name) const {
    return global_symbols_.find_functions(name);
  }

  std::optional<OperatorInfo> find_operator(const std::string &op,
                                            OpPosition position) const {
    return global_symbols_.find_operator(op, position);
  }

  std::vector<OperatorInfo> find_operators(const std::string &op,
                                           OpPosition position) const {
    return global_symbols_.find_operators(op, position);
  }

  std::vector<OperatorInfo> find_all_operators(const std::string &op) const {
    return global_symbols_.find_all_operators(op);
  }

  bool has_function(const std::string &name) const {
    return global_symbols_.has_function(name);
  }

  bool has_operator(const std::string &op, OpPosition position) const {
    return global_symbols_.has_operator(op, position);
  }

  // Get underlying SymbolTable (for operator resolution)
  const SymbolTable &symbol_table() const { return global_symbols_; }

  // === Scoped variables ===

  void add_variable(const VariableBinding &binding) {
    if (current_scope_) {
      current_scope_->add_variable(binding);
    }
  }

  bool has_variable(const std::string &name) const {
    return current_scope_ && current_scope_->has_variable(name);
  }

  std::optional<VariableBinding> find_variable(const std::string &name) const {
    if (current_scope_) {
      return current_scope_->find_variable(name);
    }
    return std::nullopt;
  }

  // === Scope management ===

  // Enter a new scope with description
  void push_scope(ScopeKind kind, const std::string &description = "");

  // Exit current scope
  void pop_scope();

  // Get current scope
  Scope *current_scope() const { return current_scope_; }

  // Get root scope (for hierarchical traversal)
  Scope *root_scope() const { return global_scope_.get(); }

private:
  SymbolTable global_symbols_;                 // Global functions and operators
  std::unique_ptr<Scope> global_scope_;        // Root scope
  Scope *current_scope_;                       // Current active scope
  std::vector<std::unique_ptr<Scope>> scopes_; // All created scopes
};

} // namespace pecco
