# Driver 命令行工具

## 概述

`plc`是 pecco-lang 的命令行编译器驱动程序。

## 命令行接口

### 基本用法

```bash
plc <input-file> [options]
```

### 命令行选项

- **位置参数**：
  - `<input-file>`：要处理的源文件路径（`.pl` 文件）
  - **必需**：必须提供输入文件

- **模式选项**：
  - `--lex`：仅运行词法分析器并输出 token 流
  - `--parse`：仅运行语法分析器并输出 AST
  - 默认：执行完整编译

- **输出选项**（可与默认模式组合）：
  - `--dump-ast`：输出语义分析后的 AST
  - `--dump-symbols`：输出符号表

- **其他选项**：
  - `--help`：显示帮助信息
  - `--version`：显示版本信息

## 功能说明

### 默认编译模式

不指定任何模式选项时，执行完整的语义分析：

```bash
$ plc sample.pl
Compilation successful (semantic analysis complete)
```

**处理流程**：
1. 词法分析：源代码 → token 流
2. 语法分析：token 流 → 平面 AST
3. 加载标准库：`stdlib/prelude.pl`
4. 收集声明：构建符号表
5. 运算符解析：平面表达式 → 树状结构
6. 报告成功或错误

**配合输出选项**：

```bash
# 输出解析后的 AST
$ plc sample.pl --dump-ast
Resolved AST:
Let(x = Binary(+, IntLiteral(1), Binary(*, IntLiteral(2), IntLiteral(3))))

# 输出符号表
$ plc sample.pl --dump-symbols

Symbol Table:

Functions:
  write(i32, string, i32) : i32 [declaration]

Operators:
  infix +(i32, i32) : i32 [prec 70, left]
  infix +(f64, f64) : f64 [prec 70, left]
  infix *(i32, i32) : i32 [prec 80, left]
  ...

# 同时输出 AST 和符号表
$ plc sample.pl --dump-ast --dump-symbols
```

### 词法分析模式 (`--lex`)

输出所有 token 的详细信息：

```bash
$ plc sample.pl --lex
[Keyword] 'let' (line 1, col 1)
[Identifier] 'x' (line 1, col 5)
[Operator] '=' (line 1, col 7)
[Integer] '42' (line 1, col 9)
[Punctuation] ';' (line 1, col 11)
[EndOfFile]  (line 2, col 1)
```

**输出格式**：`[类型] '内容' (line 行号, col 列号)`

**错误处理**：
- 词法错误会立即报告
- 包含错误位置和源代码上下文
- 精确定位错误字符（如字符串中的无效转义）

### 语法分析模式 (`--parse`)

输出语法分析后 AST 的文本表示：

```bash
$ plc sample.pl --parse
AST:
Let(x = IntLiteral(42))
Let(result = OperatorSeq(IntLiteral(1) + IntLiteral(2) * IntLiteral(3)))
Func(add : i32
  Params: [a : i32, b : i32]
  Block
    Return(OperatorSeq(Identifier(a) + Identifier(b)))
)
```

**输出格式**：
- 树状结构，缩进表示层级
- 节点类型后跟括号内的详细信息
- **运算符序列**：显示为 `OperatorSeq(operand1 op1 operand2 op2 ...)`
  - 这是解析器直接输出的平面结构
  - 不包含优先级和结合性信息
  - 与语义分析后的 Binary/Unary 树状结构不同

**错误处理**：
- 语法错误会显示详细位置和建议
- 错误恢复机制允许报告多个错误
- 不会因一个错误而停止整个解析过程

### AST 输出 (`--dump-ast`)

输出语义分析后的 AST：

```bash
$ plc sample.pl --dump-ast
Resolved AST:
Let(x = IntLiteral(42))
Let(result = Binary(+, IntLiteral(1), Binary(*, IntLiteral(2), IntLiteral(3))))
Func(add : i32
  Params: [a : i32, b : i32]
  Block
    Return(Binary(+, Identifier(a), Identifier(b)))
)
```

**与 `--parse` 的区别**：
- `--parse`：`OperatorSeq(1 + 2 * 3)` - 平面序列
- `--dump-ast`：`Binary(+, 1, Binary(*, 2, 3))` - 根据优先级构建的树

**运算符解析**：
- 自动应用优先级规则
- 正确处理左结合性和右结合性
- 示例：
  - `1 + 2 * 3` → `Binary(+, 1, Binary(*, 2, 3))` （乘法优先）
  - `10 - 5 - 2` → `Binary(-, Binary(-, 10, 5), 2)` （左结合）
  - `2 ** 3 ** 2` → `Binary(**, 2, Binary(**, 3, 2))` （右结合）

### 符号表输出 (`--dump-symbols`)

输出收集到的所有函数和运算符签名：

**函数输出格式**：
```
Functions:
  <函数名>(<参数类型列表>) : <返回类型> [declaration]
```

**运算符输出格式**：
```
Operators:
  <位置> <运算符>(<参数类型列表>) : <返回类型> [prec <优先级>, <结合性>]
```

- **位置**：`prefix`（前缀）、`infix`（中缀）、`postfix`（后缀）
- **优先级**：数值越大优先级越高
- **结合性**：`left`（左结合）、`right`（右结合）、`none`（不可结合）

## 错误报告

### 错误信息格式

```
plc: error: <错误类型> at <文件名>:<行号>:<列号>: <错误消息>
  <行号> | <源代码行>
       | <错误位置标记>
```

### 错误位置标记

- **单字符错误**：使用 `^` 标记
- **范围错误**：使用 `~` 标记范围，`^` 标记精确位置
- **插入点错误**（如缺少分号）：`^` 指向应该插入的位置

### 示例

#### 词法错误：无效转义序列

```bash
$ plc test.pl --lex
plc: error: lexer error at test.pl:1:11: Invalid string escape
  1 | let str = "bad\qescape";
    |           ~~~~^~~~~~~~~
```

#### 语法错误：缺少分号

```bash
$ plc test.pl --parse
plc: error: parse error at test.pl:1:11: Expected ';' after let statement
  1 | let x = 42
    |           ^
```

#### 语法错误：错误恢复示例

```bash
$ plc test.pl --parse
plc: error: parse error at test.pl:2:11: Expected ';' after let statement
  2 | let x = 42
    |           ^
plc: error: parse error at test.pl:4:17: Expected ';' after return statement
  4 |     return a + b
    |                 ^
```

注意：即使第一个语句有错误，parser 也会继续解析后续代码。

#### 语义错误：找不到运算符

```bash
$ plc test.pl --dump-ast
plc: semantic error at test.pl:2:10: No matching operator found for '++' with position prefix
  2 | let x = ++y;
    |          ^
```

## 标准库

编译器会自动加载标准库 `stdlib/prelude.pl`，其中包含：

### 内置函数

- `write(fd: i32, buf: string, count: i32) : i32` - 写入文件描述符

### 内置运算符

**算术运算符**（优先级 70-90）：
- `+`, `-`, `*`, `/`, `%`：基本算术（i32, f64）
- `**`：幂运算（右结合，优先级 90）

**位运算符**（优先级 50-65）：
- `&`, `|`, `^`：按位与、或、异或（优先级 50）
- `<<`, `>>`：位移（优先级 65）

**逻辑运算符**（优先级 20-30）：
- `&&`：逻辑与（优先级 30）
- `||`：逻辑或（优先级 20）
- `!`：逻辑非（前缀）

**比较运算符**（优先级 55-60，不可结合）：
- `==`, `!=`：相等性（优先级 55）
- `<`, `>`, `<=`, `>=`：关系比较（优先级 60）

**一元运算符**：
- `-`：取负（前缀）
- `!`：逻辑非（前缀）

## 文件要求

- **扩展名**：`.pl`（pecco language）
- **编码**：UTF-8（推荐）或 ASCII
- **行尾**：支持 Unix（LF）和 Windows（CRLF）

## 退出码

- `0`：成功（无错误）
- `1`：编译错误（词法、语法或语义错误）
- `其他`：系统错误（文件不存在、权限问题等）

## 使用示例

### 基本使用

```bash
# 词法分析
plc sample.pl --lex

# 语法分析
plc sample.pl --parse

# 默认编译（语义分析）
plc sample.pl

# 输出解析后的树状 AST
plc sample.pl --dump-ast

# 输出符号表
plc sample.pl --dump-symbols

# 同时输出 AST 和符号表
plc sample.pl --dump-ast --dump-symbols
```

### 开发调试

```bash
# 比较平面 AST 和树状 AST
plc test.pl --parse > parse.txt
plc test.pl --dump-ast > semantic.txt
diff -u parse.txt semantic.txt

# 检查运算符优先级是否正确
echo "let x = 1 + 2 * 3;" | plc /dev/stdin --dump-ast

# 检查符号表中的运算符定义
plc sample.pl --dump-symbols | grep "infix +"
```

### 管道使用

```bash
# 从标准输入读取
echo "let x = 42;" | plc /dev/stdin --lex

# 重定向到文件
plc sample.pl --parse > ast.txt 2> errors.txt
```
