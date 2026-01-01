# 编译驱动

将源文件编译为可执行文件。

## 基本用法

```bash
plc <input.pec> [options]
```

## 命令行选项

### 模式选项

- `--lex` - 词法分析，输出 token 流
- `--parse` - 语法分析，输出平面 AST
- `--emit-llvm` - 生成 LLVM IR
- `--compile` - 编译为目标文件（.o）
- `--run` - 编译、链接并运行程序
- 默认 - 编译并链接，生成可执行文件

### 输出选项

- `--dump-ast` - 输出解析后的 AST（优先级树）
- `--dump-symbols` - 输出符号表
- `--hide-prelude` - 隐藏标准库符号（配合 `--dump-symbols`）
- `-o <file>` - 指定输出文件名

## 示例

```bash
# 词法分析
plc sample.pec --lex

# 语法分析
plc sample.pec --parse

# 查看符号表
plc sample.pec --dump-symbols

# 生成 LLVM IR
plc sample.pec --emit-llvm

# 编译为目标文件
plc sample.pec --compile -o lib.o

# 编译并链接（默认）
plc sample.pec
# 生成可执行文件 sample

# 指定输出文件名
plc sample.pec -o myapp
# 生成可执行文件 myapp

# 编译、链接并运行
plc sample.pec --run
# 运行后退出
```

## 编译流程

```
源文件 (.pec)
    ↓ 词法分析
Token 流
    ↓ 语法分析
平面 AST
    ↓ 符号表构建
层级符号表
    ↓ 操作符解析
优先级树 AST
    ↓ 代码生成
LLVM IR
    ↓ LLVM 编译
目标文件 (.o)
    ↓ 链接 (cc)
可执行文件
```

## 错误报告

错误信息包含：
- 精确的位置（文件名、行号、列号）
- 源代码上下文
- 错误位置标记（`^` 或 `~`）

示例：

```
plc: error: parse error at test.pec:2:12: Expected ';' after return statement
  2 |   return 42
    |            ^
```
