#pragma once

#include "ast.hpp"
#include "error.hpp"
#include "scope.hpp"

#include <llvm/IR/IRBuilder.h>
#include <llvm/IR/LLVMContext.h>
#include <llvm/IR/Module.h>
#include <llvm/IR/Value.h>

#include <map>
#include <memory>
#include <string>
#include <vector>

namespace pecco {

class CodeGen {
public:
  explicit CodeGen(const std::string &module_name = "pecco_module");

  // 生成整个模块的 LLVM IR
  bool generate(std::vector<StmtPtr> &stmts, const ScopedSymbolTable &symbols);

  // 获取生成的模块
  llvm::Module *get_module() { return module_.get(); }

  // 转移模块所有权（用于 JIT）
  std::unique_ptr<llvm::Module> take_module() { return std::move(module_); }

  // 错误处理
  bool has_errors() const { return !errors_.empty(); }
  const std::vector<Error> &errors() const { return errors_; }

  // 输出 IR 到字符串
  std::string get_ir() const;

private:
  // LLVM 核心组件
  llvm::LLVMContext context_;
  std::unique_ptr<llvm::Module> module_;
  llvm::IRBuilder<> builder_;

  // 符号表引用
  const ScopedSymbolTable *symbols_;

  // 变量作用域栈：每层是变量名到 LLVM Value* 的映射
  std::vector<std::map<std::string, llvm::Value *>> value_stack_;

  // 函数表：函数名到 LLVM Function* 的映射
  std::map<std::string, llvm::Function *> functions_;

  // 当前正在生成的函数
  llvm::Function *current_function_;

  // 错误列表
  std::vector<Error> errors_;

  // 类型映射：Pecco 类型名到 LLVM 类型
  llvm::Type *get_llvm_type(const std::string &type_name);

  // 作用域管理
  void push_scope();
  void pop_scope();
  void add_variable(const std::string &name, llvm::Value *value);
  llvm::Value *lookup_variable(const std::string &name);

  // 语句生成
  void gen_stmt(Stmt *stmt);
  void gen_func_stmt(FuncStmt *func);
  void gen_operator_stmt(OperatorDeclStmt *op_decl);
  void gen_let_stmt(LetStmt *let);
  void gen_return_stmt(ReturnStmt *ret);
  void gen_expr_stmt(ExprStmt *expr_stmt);
  void gen_block_stmt(BlockStmt *block);
  void gen_if_stmt(IfStmt *if_stmt);
  void gen_while_stmt(WhileStmt *while_stmt);

  // 表达式生成：返回 LLVM Value*
  llvm::Value *gen_expr(Expr *expr);
  llvm::Value *gen_int_literal(IntLiteralExpr *lit);
  llvm::Value *gen_float_literal(FloatLiteralExpr *lit);
  llvm::Value *gen_string_literal(StringLiteralExpr *lit);
  llvm::Value *gen_bool_literal(BoolLiteralExpr *lit);
  llvm::Value *gen_identifier(IdentifierExpr *ident);
  llvm::Value *gen_binary_expr(BinaryExpr *binary);
  llvm::Value *gen_unary_expr(UnaryExpr *unary);
  llvm::Value *gen_call_expr(CallExpr *call);

  // 错误报告
  void error(const std::string &msg, size_t line, size_t column);
};

} // namespace pecco
