# IR 代码生成

将类型检查后的 AST 转换为 LLVM IR。

## 架构

- 使用 LLVM C++ API 生成 IR
- 编译目标：x86_64 静态可执行文件
- 入口点：`__pecco_entry` 函数包含顶层语句

## 类型映射

- `i32` → LLVM `i32`
- `f64` → LLVM `double`
- `bool` → LLVM `i1`
- `string` → LLVM `ptr`
- `void` → LLVM `void`

## 变量存储

所有变量使用栈分配（`alloca`）：

- 声明时：分配空间并初始化
- 使用时：`load` 指令读取值
- 赋值时：`store` 指令写入值

## 操作符实现

### 算术操作符

- `+` `-` `*` `/` `%`：区分整数（`add`、`sub`、`mul`、`sdiv`、`srem`）和浮点数（`fadd`、`fsub`、`fmul`、`fdiv`）
- `**`（浮点）：调用 LLVM intrinsic `llvm.pow.f64`

### 赋值操作符

- `=`：左值检查 → 存储值
- `+=` `-=` `*=` `/=` `%=`：加载 → 计算 → 存储
- 赋值表达式返回赋值后的值

### 位运算操作符

- `&` `|` `^`：按位与、或、异或（`and`、`or`、`xor`）
- `<<` `>>`：左移、算术右移（`shl`、`ashr`）

### 比较操作符

- 整数：`icmp eq/ne/slt/sle/sgt/sge`
- 浮点数：`fcmp oeq/one/olt/ole/ogt/oge`

### 逻辑操作符

- `&&` `||`：短路求值使用 `phi` 指令
- `!`：按位取反（`xor i1 %val, true`）

### 一元操作符

- `-`（整数）：`sub i32 0, %val`
- `-`（浮点）：`fneg double %val`

## 函数

- 声明：加载 prelude 和用户定义的函数签名
- 调用：区分 void 和非 void 函数
- 参数传递：按值传递

## 控制流

### if-else

生成 then、else、merge 基本块，使用 `phi` 合并结果。

### while

生成 loop.cond、loop.body、loop.end 基本块：

- loop.cond：条件判断，`br i1` 跳转
- loop.body：循环体，末尾跳回 loop.cond
- loop.end：循环结束后继续

### return

提前返回需要跳转到函数出口块。

## 外部函数

从 prelude 加载的外部函数（如 `exit`）：

- 声明为 external 函数
- 调用时直接生成 `call` 指令

## 编译流程

1. 加载 prelude（stdlib/prelude.pec）
2. 收集所有函数签名（符号表）
3. 生成所有函数定义
4. 生成 `__pecco_entry` 包含顶层语句
5. 验证 module 并输出 IR 或目标文件

## 链接

使用 `cc` 链接器：

- 输入：`.o` 文件 + prelude.o
- 添加 main wrapper（调用 `__pecco_entry` 并 `exit`）
- 输出：可执行文件（默认 `-no-pie`）
