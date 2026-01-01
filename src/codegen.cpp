#include "codegen.hpp"

#include <llvm/IR/Verifier.h>
#include <llvm/Support/raw_ostream.h>

#include <sstream>

namespace pecco {

CodeGen::CodeGen(const std::string &module_name)
    : builder_(context_), current_function_(nullptr) {
  module_ = std::make_unique<llvm::Module>(module_name, context_);
}

llvm::Type *CodeGen::get_llvm_type(const std::string &type_name) {
  if (type_name == "i32") {
    return llvm::Type::getInt32Ty(context_);
  } else if (type_name == "f64") {
    return llvm::Type::getDoubleTy(context_);
  } else if (type_name == "bool") {
    return llvm::Type::getInt1Ty(context_);
  } else if (type_name == "string") {
    return llvm::Type::getInt8PtrTy(context_);
  } else if (type_name == "void") {
    return llvm::Type::getVoidTy(context_);
  }
  return nullptr;
}

void CodeGen::push_scope() { value_stack_.emplace_back(); }

void CodeGen::pop_scope() {
  if (!value_stack_.empty()) {
    value_stack_.pop_back();
  }
}

void CodeGen::add_variable(const std::string &name, llvm::Value *value) {
  if (!value_stack_.empty()) {
    value_stack_.back()[name] = value;
  }
}

llvm::Value *CodeGen::lookup_variable(const std::string &name) {
  // 从内到外查找变量
  for (auto it = value_stack_.rbegin(); it != value_stack_.rend(); ++it) {
    auto found = it->find(name);
    if (found != it->end()) {
      return found->second;
    }
  }
  return nullptr;
}

void CodeGen::error(const std::string &msg, size_t line, size_t column) {
  errors_.emplace_back(msg, line, column);
}

bool CodeGen::generate(std::vector<StmtPtr> &stmts,
                       const ScopedSymbolTable &symbols) {
  symbols_ = &symbols;
  errors_.clear();
  value_stack_.clear();
  functions_.clear();
  current_function_ = nullptr;

  // 首先从 symbol table 中声明所有函数（包括 prelude 和用户定义的）
  auto func_names = symbols.symbol_table().get_all_function_names();
  for (const auto &func_name : func_names) {
    auto funcs = symbols.symbol_table().find_functions(func_name);
    for (const auto &func_info : funcs) {
      // 构建参数类型列表
      std::vector<llvm::Type *> param_types;
      for (const auto &param_type : func_info.param_types) {
        llvm::Type *ty = get_llvm_type(param_type);
        if (!ty) {
          error("Unknown type: " + param_type, 0, 0);
          return false;
        }
        param_types.push_back(ty);
      }

      // 获取返回类型
      llvm::Type *return_type = get_llvm_type(func_info.return_type);
      if (!return_type) {
        error("Unknown return type: " + func_info.return_type, 0, 0);
        return false;
      }

      // 创建函数声明
      llvm::FunctionType *func_type =
          llvm::FunctionType::get(return_type, param_types, false);
      llvm::Function *llvm_func = llvm::Function::Create(
          func_type, llvm::Function::ExternalLinkage, func_name, module_.get());

      functions_[func_name] = llvm_func;
    }
  }

  // 声明所有 operator（将它们作为函数）
  auto all_operators = symbols.symbol_table().get_all_operators();
  for (const auto &op_info : all_operators) {
    // 构建参数类型列表
    std::vector<llvm::Type *> param_types;
    for (const auto &param_type : op_info.signature.param_types) {
      llvm::Type *ty = get_llvm_type(param_type);
      if (!ty) {
        error("Unknown type: " + param_type, 0, 0);
        return false;
      }
      param_types.push_back(ty);
    }

    // 获取返回类型
    llvm::Type *return_type = get_llvm_type(op_info.signature.return_type);
    if (!return_type) {
      error("Unknown return type: " + op_info.signature.return_type, 0, 0);
      return false;
    }

    // 生成 mangled name 用于区分重载：op_symbol$type1$type2...
    std::string mangled_name = op_info.op;
    for (const auto &param_type : op_info.signature.param_types) {
      mangled_name += "$" + param_type;
    }

    // 创建函数声明（使用 mangled name）
    llvm::FunctionType *func_type =
        llvm::FunctionType::get(return_type, param_types, false);
    llvm::Function *llvm_func =
        llvm::Function::Create(func_type, llvm::Function::ExternalLinkage,
                               mangled_name, module_.get());

    functions_[mangled_name] = llvm_func;
  }

  // 创建隐式入口函数 __pecco_entry
  llvm::FunctionType *entry_type =
      llvm::FunctionType::get(llvm::Type::getInt32Ty(context_), false);
  llvm::Function *entry_func =
      llvm::Function::Create(entry_type, llvm::Function::ExternalLinkage,
                             "__pecco_entry", module_.get());
  current_function_ = entry_func;

  // 创建入口基本块
  llvm::BasicBlock *entry_bb =
      llvm::BasicBlock::Create(context_, "entry", entry_func);
  builder_.SetInsertPoint(entry_bb);

  // 创建全局作用域
  push_scope();

  // 生成所有顶层语句
  for (auto &stmt : stmts) {
    if (stmt->kind == StmtKind::Func) {
      // 函数定义单独处理
      auto *func = static_cast<FuncStmt *>(stmt.get());
      if (func->body) {
        gen_func_stmt(func);
      }
    } else if (stmt->kind == StmtKind::OperatorDecl) {
      // 操作符定义单独处理
      auto *op_decl = static_cast<OperatorDeclStmt *>(stmt.get());
      if (op_decl->body) {
        gen_operator_stmt(op_decl);
      }
    } else {
      // 顶层语句在入口函数中执行
      gen_stmt(stmt.get());
    }
  }

  // 确保在入口函数的正确位置添加返回语句
  if (builder_.GetInsertBlock()->getParent() == entry_func &&
      !builder_.GetInsertBlock()->getTerminator()) {
    builder_.CreateRet(llvm::ConstantInt::get(context_, llvm::APInt(32, 0)));
  }

  pop_scope();

  // 验证模块
  std::string error_str;
  llvm::raw_string_ostream error_stream(error_str);
  if (llvm::verifyModule(*module_, &error_stream)) {
    error("LLVM module verification failed: " + error_str, 0, 0);
    return false;
  }

  return !has_errors();
}

std::string CodeGen::get_ir() const {
  std::string ir_str;
  llvm::raw_string_ostream stream(ir_str);
  module_->print(stream, nullptr);
  return ir_str;
}

void CodeGen::gen_stmt(Stmt *stmt) {
  if (!stmt)
    return;

  switch (stmt->kind) {
  case StmtKind::Let:
    gen_let_stmt(static_cast<LetStmt *>(stmt));
    break;
  case StmtKind::Return:
    gen_return_stmt(static_cast<ReturnStmt *>(stmt));
    break;
  case StmtKind::Expr:
    gen_expr_stmt(static_cast<ExprStmt *>(stmt));
    break;
  case StmtKind::Block:
    gen_block_stmt(static_cast<BlockStmt *>(stmt));
    break;
  case StmtKind::If:
    gen_if_stmt(static_cast<IfStmt *>(stmt));
    break;
  case StmtKind::While:
    gen_while_stmt(static_cast<WhileStmt *>(stmt));
    break;
  case StmtKind::Func:
    // 函数定义在 generate 中处理
    break;
  case StmtKind::OperatorDecl:
    // 操作符声明不需要生成代码
    break;
  }
}

void CodeGen::gen_func_stmt(FuncStmt *func) {
  // 函数已经在 generate 中声明，这里生成函数体
  llvm::Function *llvm_func = functions_[func->name];
  if (!llvm_func) {
    error("Function not found: " + func->name, func->loc.line,
          func->loc.column);
    return;
  }

  // 保存当前函数状态和插入点
  llvm::Function *saved_function = current_function_;
  llvm::BasicBlock *saved_block = builder_.GetInsertBlock();
  current_function_ = llvm_func;

  // 创建函数入口块
  llvm::BasicBlock *bb = llvm::BasicBlock::Create(context_, "entry", llvm_func);
  builder_.SetInsertPoint(bb);

  // 创建新作用域
  push_scope();

  // 为每个参数创建 alloca 并存储参数值
  size_t idx = 0;
  for (auto &arg : llvm_func->args()) {
    llvm::AllocaInst *alloca =
        builder_.CreateAlloca(arg.getType(), nullptr, arg.getName());
    builder_.CreateStore(&arg, alloca);
    add_variable(func->params[idx].name, alloca);
    idx++;
  }

  // 生成函数体
  if (func->body) {
    gen_stmt(func->body.value().get());
  }

  // 检查是否有返回语句，如果没有且返回类型是 void，添加 ret void
  llvm::BasicBlock *current_bb = builder_.GetInsertBlock();
  if (current_bb && !current_bb->getTerminator()) {
    if (func->return_type.value()->name == "void") {
      builder_.CreateRetVoid();
    } else {
      // 如果函数应该返回值但没有 return，这是个错误
      // 但为了生成有效的 IR，我们返回一个默认值
      llvm::Type *ret_type = get_llvm_type(func->return_type.value()->name);
      if (ret_type->isIntegerTy()) {
        builder_.CreateRet(llvm::ConstantInt::get(ret_type, 0));
      } else if (ret_type->isDoubleTy()) {
        builder_.CreateRet(llvm::ConstantFP::get(ret_type, 0.0));
      }
    }
  }

  pop_scope();

  // 恢复之前的函数和插入点
  current_function_ = saved_function;
  if (saved_block) {
    builder_.SetInsertPoint(saved_block);
  }
}

void CodeGen::gen_operator_stmt(OperatorDeclStmt *op_decl) {
  // 生成 mangled name（与声明时相同）
  std::string mangled_name = op_decl->op;
  for (const auto &param : op_decl->params) {
    if (param.type) {
      mangled_name += "$" + param.type.value()->name;
    }
  }

  // Operator 已经在 generate 中声明，这里生成函数体
  llvm::Function *llvm_func = functions_[mangled_name];
  if (!llvm_func) {
    error("Operator function not found: " + op_decl->op, op_decl->loc.line,
          op_decl->loc.column);
    return;
  }

  // 保存当前函数状态和插入点
  llvm::Function *saved_function = current_function_;
  llvm::BasicBlock *saved_block = builder_.GetInsertBlock();
  current_function_ = llvm_func;

  // 创建函数入口块
  llvm::BasicBlock *bb = llvm::BasicBlock::Create(context_, "entry", llvm_func);
  builder_.SetInsertPoint(bb);

  // 创建新作用域
  push_scope();

  // 为每个参数创建 alloca 并存储参数值
  size_t idx = 0;
  for (auto &arg : llvm_func->args()) {
    llvm::AllocaInst *alloca =
        builder_.CreateAlloca(arg.getType(), nullptr, arg.getName());
    builder_.CreateStore(&arg, alloca);
    add_variable(op_decl->params[idx].name, alloca);
    idx++;
  }

  // 生成函数体
  if (op_decl->body) {
    gen_stmt(op_decl->body.value().get());
  }

  // 检查是否有返回语句，如果没有且返回类型是 void，添加 ret void
  llvm::BasicBlock *current_bb = builder_.GetInsertBlock();
  if (current_bb && !current_bb->getTerminator()) {
    if (op_decl->return_type && op_decl->return_type.value()->name == "void") {
      builder_.CreateRetVoid();
    } else {
      // 如果函数应该返回值但没有 return，这是个错误
      // 但为了生成有效的 IR，我们返回一个默认值
      llvm::Type *ret_type = get_llvm_type(op_decl->return_type.value()->name);
      if (ret_type->isIntegerTy()) {
        builder_.CreateRet(llvm::ConstantInt::get(ret_type, 0));
      } else if (ret_type->isDoubleTy()) {
        builder_.CreateRet(llvm::ConstantFP::get(ret_type, 0.0));
      }
    }
  }

  pop_scope();

  // 恢复之前的函数和插入点
  current_function_ = saved_function;
  if (saved_block) {
    builder_.SetInsertPoint(saved_block);
  }
}

void CodeGen::gen_let_stmt(LetStmt *let) {
  // 生成初始化表达式
  llvm::Value *init_val = nullptr;
  if (let->init) {
    init_val = gen_expr(let->init.get());
    if (!init_val)
      return;
  }

  // 创建 alloca
  llvm::Type *var_type = init_val ? init_val->getType() : nullptr;
  if (let->type) {
    // 如果有显式类型注解，使用它
    var_type = get_llvm_type(let->type.value()->name);
  }

  if (!var_type) {
    error("Cannot determine type for variable: " + let->name, let->loc.line,
          let->loc.column);
    return;
  }

  llvm::AllocaInst *alloca =
      builder_.CreateAlloca(var_type, nullptr, let->name);

  // 如果有初始值，存储它
  if (init_val) {
    builder_.CreateStore(init_val, alloca);
  }

  // 注册变量
  add_variable(let->name, alloca);
}

void CodeGen::gen_return_stmt(ReturnStmt *ret) {
  if (ret->value) {
    llvm::Value *val = gen_expr(ret->value.value().get());
    if (val) {
      builder_.CreateRet(val);
    }
  } else {
    builder_.CreateRetVoid();
  }
}

void CodeGen::gen_expr_stmt(ExprStmt *expr_stmt) {
  gen_expr(expr_stmt->expr.get());
}

void CodeGen::gen_block_stmt(BlockStmt *block) {
  push_scope();
  for (auto &stmt : block->stmts) {
    gen_stmt(stmt.get());
  }
  pop_scope();
}

void CodeGen::gen_if_stmt(IfStmt *if_stmt) {
  llvm::Value *cond = gen_expr(if_stmt->condition.get());
  if (!cond)
    return;

  llvm::Function *func = current_function_;

  // 创建基本块
  llvm::BasicBlock *then_bb = llvm::BasicBlock::Create(context_, "then", func);
  llvm::BasicBlock *else_bb = nullptr;
  llvm::BasicBlock *merge_bb =
      llvm::BasicBlock::Create(context_, "ifcont", func);

  if (if_stmt->else_branch) {
    else_bb = llvm::BasicBlock::Create(context_, "else", func);
    builder_.CreateCondBr(cond, then_bb, else_bb);
  } else {
    builder_.CreateCondBr(cond, then_bb, merge_bb);
  }

  // 生成 then 分支
  builder_.SetInsertPoint(then_bb);
  gen_stmt(if_stmt->then_branch.get());

  // 如果 then 分支没有 terminator，添加 br 到 merge
  if (!builder_.GetInsertBlock()->getTerminator()) {
    builder_.CreateBr(merge_bb);
  }

  // 生成 else 分支（如果有）
  if (if_stmt->else_branch) {
    builder_.SetInsertPoint(else_bb);
    gen_stmt(if_stmt->else_branch.value().get());

    if (!builder_.GetInsertBlock()->getTerminator()) {
      builder_.CreateBr(merge_bb);
    }
  }

  // 继续在 merge 块
  builder_.SetInsertPoint(merge_bb);
}

void CodeGen::gen_while_stmt(WhileStmt *while_stmt) {
  llvm::Function *func = current_function_;

  // 创建基本块
  llvm::BasicBlock *loop_cond =
      llvm::BasicBlock::Create(context_, "loop.cond", func);
  llvm::BasicBlock *loop_body =
      llvm::BasicBlock::Create(context_, "loop.body", func);
  llvm::BasicBlock *loop_end =
      llvm::BasicBlock::Create(context_, "loop.end", func);

  // 跳转到条件判断
  builder_.CreateBr(loop_cond);

  // 生成条件判断
  builder_.SetInsertPoint(loop_cond);
  llvm::Value *cond = gen_expr(while_stmt->condition.get());
  if (!cond)
    return;

  builder_.CreateCondBr(cond, loop_body, loop_end);

  // 生成循环体
  builder_.SetInsertPoint(loop_body);
  gen_stmt(while_stmt->body.get());

  // 循环体结束后跳回条件判断
  if (!builder_.GetInsertBlock()->getTerminator()) {
    builder_.CreateBr(loop_cond);
  }

  // 继续在循环结束块
  builder_.SetInsertPoint(loop_end);
}

llvm::Value *CodeGen::gen_expr(Expr *expr) {
  if (!expr)
    return nullptr;

  switch (expr->kind) {
  case ExprKind::IntLiteral:
    return gen_int_literal(static_cast<IntLiteralExpr *>(expr));
  case ExprKind::FloatLiteral:
    return gen_float_literal(static_cast<FloatLiteralExpr *>(expr));
  case ExprKind::StringLiteral:
    return gen_string_literal(static_cast<StringLiteralExpr *>(expr));
  case ExprKind::BoolLiteral:
    return gen_bool_literal(static_cast<BoolLiteralExpr *>(expr));
  case ExprKind::Identifier:
    return gen_identifier(static_cast<IdentifierExpr *>(expr));
  case ExprKind::Binary:
    return gen_binary_expr(static_cast<BinaryExpr *>(expr));
  case ExprKind::Unary:
    return gen_unary_expr(static_cast<UnaryExpr *>(expr));
  case ExprKind::Call:
    return gen_call_expr(static_cast<CallExpr *>(expr));
  case ExprKind::OperatorSeq:
    error("OperatorSeq should have been resolved before codegen",
          expr->loc.line, expr->loc.column);
    return nullptr;
  }
  return nullptr;
}

llvm::Value *CodeGen::gen_int_literal(IntLiteralExpr *lit) {
  int64_t val = std::stoll(lit->value);
  return llvm::ConstantInt::get(context_, llvm::APInt(32, val, true));
}

llvm::Value *CodeGen::gen_float_literal(FloatLiteralExpr *lit) {
  double val = std::stod(lit->value);
  return llvm::ConstantFP::get(llvm::Type::getDoubleTy(context_), val);
}

llvm::Value *CodeGen::gen_string_literal(StringLiteralExpr *lit) {
  // 创建全局字符串常量
  return builder_.CreateGlobalStringPtr(lit->value);
}

llvm::Value *CodeGen::gen_bool_literal(BoolLiteralExpr *lit) {
  return llvm::ConstantInt::get(context_, llvm::APInt(1, lit->value ? 1 : 0));
}

llvm::Value *CodeGen::gen_identifier(IdentifierExpr *ident) {
  llvm::Value *var = lookup_variable(ident->name);
  if (!var) {
    error("Undefined variable: " + ident->name, ident->loc.line,
          ident->loc.column);
    return nullptr;
  }
  // Load 值从 alloca 指针
  llvm::Type *elem_type = llvm::cast<llvm::AllocaInst>(var)->getAllocatedType();
  return builder_.CreateLoad(elem_type, var, ident->name);
}

llvm::Value *CodeGen::gen_binary_expr(BinaryExpr *binary) {
  std::string op = binary->op;

  // 赋值操作符需要特殊处理（左值语义）
  if (op == "=" || op == "+=" || op == "-=" || op == "*=" || op == "/=" ||
      op == "%=") {
    // 左操作数必须是变量（左值）
    if (binary->left->kind != ExprKind::Identifier) {
      error("Left side of assignment must be a variable", binary->loc.line,
            binary->loc.column);
      return nullptr;
    }

    IdentifierExpr *var_expr =
        static_cast<IdentifierExpr *>(binary->left.get());
    llvm::Value *var = lookup_variable(var_expr->name);
    if (!var) {
      error("Undefined variable: " + var_expr->name, binary->loc.line,
            binary->loc.column);
      return nullptr;
    }

    llvm::Value *right_val = gen_expr(binary->right.get());
    if (!right_val)
      return nullptr;

    // 处理复合赋值操作符 (+=, -=, 等)
    if (op != "=") {
      // 先加载当前值
      llvm::Type *elem_type =
          llvm::cast<llvm::AllocaInst>(var)->getAllocatedType();
      llvm::Value *left_val =
          builder_.CreateLoad(elem_type, var, var_expr->name);

      // 执行相应的操作
      if (op == "+=") {
        if (left_val->getType()->isIntegerTy()) {
          right_val = builder_.CreateAdd(left_val, right_val, "addtmp");
        } else if (left_val->getType()->isDoubleTy()) {
          right_val = builder_.CreateFAdd(left_val, right_val, "addtmp");
        }
      } else if (op == "-=") {
        if (left_val->getType()->isIntegerTy()) {
          right_val = builder_.CreateSub(left_val, right_val, "subtmp");
        } else if (left_val->getType()->isDoubleTy()) {
          right_val = builder_.CreateFSub(left_val, right_val, "subtmp");
        }
      } else if (op == "*=") {
        if (left_val->getType()->isIntegerTy()) {
          right_val = builder_.CreateMul(left_val, right_val, "multmp");
        } else if (left_val->getType()->isDoubleTy()) {
          right_val = builder_.CreateFMul(left_val, right_val, "multmp");
        }
      } else if (op == "/=") {
        if (left_val->getType()->isIntegerTy()) {
          right_val = builder_.CreateSDiv(left_val, right_val, "divtmp");
        } else if (left_val->getType()->isDoubleTy()) {
          right_val = builder_.CreateFDiv(left_val, right_val, "divtmp");
        }
      } else if (op == "%=") {
        if (left_val->getType()->isIntegerTy()) {
          right_val = builder_.CreateSRem(left_val, right_val, "modtmp");
        }
      }
    }

    // 存储新值
    builder_.CreateStore(right_val, var);
    // 赋值表达式返回赋值后的值
    return right_val;
  }

  // 对于非赋值操作符，正常求值两个操作数
  llvm::Value *left = gen_expr(binary->left.get());
  llvm::Value *right = gen_expr(binary->right.get());

  if (!left || !right)
    return nullptr;

  // 算术操作符
  if (op == "+") {
    if (left->getType()->isIntegerTy()) {
      return builder_.CreateAdd(left, right, "addtmp");
    } else if (left->getType()->isDoubleTy()) {
      return builder_.CreateFAdd(left, right, "addtmp");
    }
  } else if (op == "-") {
    if (left->getType()->isIntegerTy()) {
      return builder_.CreateSub(left, right, "subtmp");
    } else if (left->getType()->isDoubleTy()) {
      return builder_.CreateFSub(left, right, "subtmp");
    }
  } else if (op == "*") {
    if (left->getType()->isIntegerTy()) {
      return builder_.CreateMul(left, right, "multmp");
    } else if (left->getType()->isDoubleTy()) {
      return builder_.CreateFMul(left, right, "multmp");
    }
  } else if (op == "/") {
    if (left->getType()->isIntegerTy()) {
      return builder_.CreateSDiv(left, right, "divtmp");
    } else if (left->getType()->isDoubleTy()) {
      return builder_.CreateFDiv(left, right, "divtmp");
    }
  } else if (op == "%") {
    if (left->getType()->isIntegerTy()) {
      return builder_.CreateSRem(left, right, "modtmp");
    }
  }
  // 幂运算
  else if (op == "**") {
    if (left->getType()->isDoubleTy() && right->getType()->isDoubleTy()) {
      llvm::Function *pow_func =
          llvm::Intrinsic::getDeclaration(module_.get(), llvm::Intrinsic::pow,
                                          {llvm::Type::getDoubleTy(context_)});
      return builder_.CreateCall(pow_func, {left, right}, "powtmp");
    }
  }
  // 位运算操作符
  else if (op == "&") {
    if (left->getType()->isIntegerTy()) {
      return builder_.CreateAnd(left, right, "andtmp");
    }
  } else if (op == "|") {
    if (left->getType()->isIntegerTy()) {
      return builder_.CreateOr(left, right, "ortmp");
    }
  } else if (op == "^") {
    if (left->getType()->isIntegerTy()) {
      return builder_.CreateXor(left, right, "xortmp");
    }
  } else if (op == "<<") {
    if (left->getType()->isIntegerTy()) {
      return builder_.CreateShl(left, right, "shltmp");
    }
  } else if (op == ">>") {
    if (left->getType()->isIntegerTy()) {
      return builder_.CreateAShr(left, right, "ashrtmp");
    }
  }
  // 比较操作符
  else if (op == "==") {
    if (left->getType()->isIntegerTy()) {
      return builder_.CreateICmpEQ(left, right, "eqtmp");
    } else if (left->getType()->isDoubleTy()) {
      return builder_.CreateFCmpOEQ(left, right, "eqtmp");
    }
  } else if (op == "!=") {
    if (left->getType()->isIntegerTy()) {
      return builder_.CreateICmpNE(left, right, "netmp");
    } else if (left->getType()->isDoubleTy()) {
      return builder_.CreateFCmpONE(left, right, "netmp");
    }
  } else if (op == "<") {
    if (left->getType()->isIntegerTy()) {
      return builder_.CreateICmpSLT(left, right, "lttmp");
    } else if (left->getType()->isDoubleTy()) {
      return builder_.CreateFCmpOLT(left, right, "lttmp");
    }
  } else if (op == "<=") {
    if (left->getType()->isIntegerTy()) {
      return builder_.CreateICmpSLE(left, right, "letmp");
    } else if (left->getType()->isDoubleTy()) {
      return builder_.CreateFCmpOLE(left, right, "letmp");
    }
  } else if (op == ">") {
    if (left->getType()->isIntegerTy()) {
      return builder_.CreateICmpSGT(left, right, "gttmp");
    } else if (left->getType()->isDoubleTy()) {
      return builder_.CreateFCmpOGT(left, right, "gttmp");
    }
  } else if (op == ">=") {
    if (left->getType()->isIntegerTy()) {
      return builder_.CreateICmpSGE(left, right, "getmp");
    } else if (left->getType()->isDoubleTy()) {
      return builder_.CreateFCmpOGE(left, right, "getmp");
    }
  }
  // 逻辑操作符
  else if (op == "&&") {
    return builder_.CreateAnd(left, right, "andtmp");
  } else if (op == "||") {
    return builder_.CreateOr(left, right, "ortmp");
  }

  // 如果不是内置 operator，检查是否是用户定义的 operator
  auto ops = symbols_->find_operators(op, OpPosition::Infix);
  if (!ops.empty()) {
    // 需要找到匹配类型的 operator
    // 根据操作数的 LLVM 类型推断 Pecco 类型
    std::string left_type, right_type;
    if (left->getType()->isIntegerTy(32)) {
      left_type = "i32";
    } else if (left->getType()->isDoubleTy()) {
      left_type = "f64";
    } else if (left->getType()->isIntegerTy(1)) {
      left_type = "bool";
    }

    if (right->getType()->isIntegerTy(32)) {
      right_type = "i32";
    } else if (right->getType()->isDoubleTy()) {
      right_type = "f64";
    } else if (right->getType()->isIntegerTy(1)) {
      right_type = "bool";
    }

    // 查找匹配的 operator 重载
    for (const auto &op_info : ops) {
      if (op_info.signature.param_types.size() == 2 &&
          op_info.signature.param_types[0] == left_type &&
          op_info.signature.param_types[1] == right_type) {
        // 构造 mangled name
        std::string mangled_name = op + "$" + left_type + "$" + right_type;
        llvm::Function *op_func = module_->getFunction(mangled_name);
        if (op_func) {
          // 找到了 operator 函数
          std::vector<llvm::Value *> args = {left, right};
          return builder_.CreateCall(op_func, args, "optmp");
        }
      }
    }
  }

  error("Unknown binary operator: " + op, binary->loc.line, binary->loc.column);
  return nullptr;
}

llvm::Value *CodeGen::gen_unary_expr(UnaryExpr *unary) {
  llvm::Value *operand = gen_expr(unary->operand.get());
  if (!operand)
    return nullptr;

  std::string op = unary->op;

  // 前缀操作符
  if (unary->position == OpPosition::Prefix) {
    if (op == "-") {
      // 负号
      if (operand->getType()->isIntegerTy()) {
        return builder_.CreateNeg(operand, "negtmp");
      } else if (operand->getType()->isDoubleTy()) {
        return builder_.CreateFNeg(operand, "negtmp");
      }
    } else if (op == "!") {
      // 逻辑取反
      return builder_.CreateNot(operand, "nottmp");
    }
  }
  // 后缀操作符（如果有）
  else if (unary->position == OpPosition::Postfix) {
    // 当前没有后缀操作符
  }

  // 检查是否是用户定义的 operator（将其作为函数调用）
  auto ops = symbols_->find_operators(op, unary->position);
  if (!ops.empty()) {
    // 根据操作数的 LLVM 类型推断 Pecco 类型
    std::string operand_type;
    if (operand->getType()->isIntegerTy(32)) {
      operand_type = "i32";
    } else if (operand->getType()->isDoubleTy()) {
      operand_type = "f64";
    } else if (operand->getType()->isIntegerTy(1)) {
      operand_type = "bool";
    }

    // 查找匹配的 operator 重载
    for (const auto &op_info : ops) {
      if (op_info.signature.param_types.size() == 1 &&
          op_info.signature.param_types[0] == operand_type) {
        // 构造 mangled name
        std::string mangled_name = op + "$" + operand_type;
        llvm::Function *op_func = module_->getFunction(mangled_name);
        if (op_func) {
          // 找到了 operator 函数
          std::vector<llvm::Value *> args = {operand};
          return builder_.CreateCall(op_func, args, "optmp");
        }
      }
    }
  }

  error("Unknown unary operator: " + op, unary->loc.line, unary->loc.column);
  return nullptr;
}

llvm::Value *CodeGen::gen_call_expr(CallExpr *call) {
  // 获取被调用函数名
  if (call->callee->kind != ExprKind::Identifier) {
    error("Function call callee must be an identifier", call->loc.line,
          call->loc.column);
    return nullptr;
  }

  auto *ident = static_cast<IdentifierExpr *>(call->callee.get());
  std::string func_name = ident->name;

  // 查找函数
  llvm::Function *callee = functions_[func_name];
  if (!callee) {
    // 尝试从模块中查找（可能是外部声明）
    callee = module_->getFunction(func_name);
  }

  if (!callee) {
    error("Unknown function: " + func_name, call->loc.line, call->loc.column);
    return nullptr;
  }

  // 检查参数数量
  if (callee->arg_size() != call->args.size()) {
    error("Incorrect number of arguments for function " + func_name,
          call->loc.line, call->loc.column);
    return nullptr;
  }

  // 生成参数值
  std::vector<llvm::Value *> args;
  for (auto &arg : call->args) {
    llvm::Value *arg_val = gen_expr(arg.get());
    if (!arg_val)
      return nullptr;
    args.push_back(arg_val);
  }

  // 生成函数调用
  // void 函数的调用不应该有名字
  if (callee->getReturnType()->isVoidTy()) {
    return builder_.CreateCall(callee, args);
  } else {
    return builder_.CreateCall(callee, args, "calltmp");
  }
}

} // namespace pecco
