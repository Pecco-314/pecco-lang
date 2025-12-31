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

- **模式选项**（互斥）：
  - `--lex`：运行词法分析器并输出 token 流
  - `--parse`：运行语法分析器并输出 AST

- **其他选项**：
  - `--help`：显示帮助信息
  - `--version`：显示版本信息

## 功能说明

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

输出 AST 的文本表示：

```bash
$ plc sample.pl --parse
AST:
Let(x = IntLiteral(42))
Func(add : i32
  Params: [a : i32, b : i32]
  Block
    Return(OperatorSeq(Identifier(a) + Identifier(b)))
)
```

**输出格式**：
- 树状结构，缩进表示层级
- 节点类型后跟括号内的详细信息
- 运算符序列显示为 `OperatorSeq(operand1 op1 operand2 op2 ...)`

**错误处理**：
- 语法错误会显示详细位置和建议
- 错误恢复机制允许报告多个错误
- 不会因一个错误而停止整个解析过程

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

## 文件要求

- **扩展名**：`.pl`（pecco language）
- **编码**：UTF-8（推荐）或 ASCII
- **行尾**：支持 Unix（LF）和 Windows（CRLF）

## 退出码

- `0`：成功（无错误）
- `1`：词法或语法错误
- `其他`：系统错误（文件不存在、权限问题等）

## 使用示例

### 基本使用

```bash
# 词法分析
plc sample.pl --lex

# 语法分析
plc sample.pl --parse
```

### 管道使用

```bash
# 从标准输入读取
echo "let x = 42;" | plc /dev/stdin --lex

# 重定向到文件
plc sample.pl --parse > ast.txt 2> errors.txt
```
