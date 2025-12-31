# Parser 语法分析器

语法分析器（Parser）负责将 token 流转换为抽象语法树（AST）。

## AST 节点类型

### 表达式节点

定义在 `include/ast.hpp`：

```cpp
enum class ExprKind {
  IntLiteral,     // 整数字面量
  FloatLiteral,   // 浮点数字面量
  StringLiteral,  // 字符串字面量
  BoolLiteral,    // 布尔字面量
  Identifier,     // 标识符
  OperatorSeq,    // 运算符序列
  Call,           // 函数调用
};
```

### 语句节点

```cpp
enum class StmtKind {
  Let,      // 变量声明
  Func,     // 函数定义
  If,       // if 语句
  Return,   // return 语句
  While,    // while 循环
  Expr,     // 表达式语句
  Block,    // 代码块
};
```

## 支持的语法结构

### 变量声明（Let Statement）

```
let <name> [: <type>] = <expr>;
```

- 可选类型注解
- 示例：
  ```
  let x = 42;
  let y : i32 = 100;
  let name : str = "hello";
  ```

### 函数定义（Func Statement）

```
func <name>([<param> [: <type>], ...]) [: <return_type>] {
  <statements>
}
```

- 参数可选类型注解
- 可选返回类型注解
- 示例：
  ```
  func add(a, b) { return a + b; }
  func multiply(x : i32, y : i32) : i32 { return x * y; }
  ```

### 条件语句（If Statement）

```
if <condition> {
  <statements>
} [else {
  <statements>
}]

if <condition> {
  <statements>
} else if <condition> {
  <statements>
} else {
  <statements>
}
```

### 循环语句（While Statement）

```
while <condition> {
  <statements>
}
```

### 返回语句（Return Statement）

```
return [<expr>];
```

- 表达式可选

### 代码块（Block Statement）

```
{
  <statements>
}
```

## 表达式解析

### 运算符序列

Parser 不进行运算符优先级解析，而是将表达式解析为平面序列。

```
a + b * c  ->  OperatorSeq([a, b, c], ["+", "*"])
```

这种设计提供了灵活性，允许：
- 自定义运算符优先级
- 动态运算符定义
- 在语义分析阶段处理优先级

### 主表达式（Primary Expression）

支持的主表达式类型：
- **字面量**：整数、浮点数、字符串、布尔值（`true`/`false`）
- **标识符**：变量名、函数名
- **函数调用**：`func(arg1, arg2, ...)`
- **括号表达式**：`(expr)`

### 表达式组合

表达式通过运算符连接：
```
<primary> (<operator> <primary>)*
```

运算符从 token 流中识别（`TokenKind::Operator`）。

## 注释处理

Parser 自动跳过注释 token：
- `peek()` 和 `advance()` 会自动忽略 `TokenKind::Comment`
- 注释可以出现在任何地方，不影响语法解析

## 错误处理

### 错误定位

错误报告包含精确的位置信息：

```cpp
struct ParseError {
  std::string message;   // 错误消息
  size_t line;          // 行号
  size_t column;        // 列号
  size_t end_column;    // 结束列号（用于范围高亮）
};
```

### 错误恢复

Parser 使用同步机制（synchronization）进行错误恢复：
- 遇到错误后跳到下一个语句边界
- 语句边界包括：`;`、`}`、关键字（`let`、`func`、`if`、`return`、`while`）
- 不会跳过块结束符 `}`，避免级联错误

### 特殊错误定位

对于"期望但缺失"的 token：
- 错误位置指向**前一个 token 的末尾**
- 示例：`let x = 42` 缺少分号，错误指向 `42` 之后
