# 语法分析

将 token 流转换为抽象语法树（AST）。

## 语法结构

### 变量声明

```
let <name> [: <type>] = <expr>;
```

### 函数

```
func <name>([<params>]) [: <type>] { <body> }  // 定义
func <name>([<params>]) [: <type>];            // 声明
```

### 操作符

```
operator <pos> <op>(<params>) : <type> [prec N] [assoc <dir>] [= <expr>];
operator <pos> <op>(<params>) : <type> [prec N] [assoc <dir>] { <body> }
```

- `<pos>`：`prefix` / `infix` / `postfix`
- `prec`：优先级（整数，仅 infix）
- `assoc`：结合性（`left` / `right` / `none`，仅 infix）
- 可以是声明（`;` 结尾）或定义（`= <expr>` 或 `{ <body> }`）

示例：

```
operator infix +(a: i32, b: i32): i32 prec 70 assoc left;
operator prefix -(x: i32): i32 = 0 - x;
```

### 控制流

```
if <cond> { <body> } [else { <body> }]
while <cond> { <body> }
return [<expr>];
```

## 表达式解析

Parser 将表达式解析为**表达式序列**（`OperatorSeq`），不处理优先级：

```
a + b * c  →  OperatorSeq(a + b * c)
-x++ + 3   →  OperatorSeq(- x ++ + 3)
```

操作符与操作数的顺序完整保留，由语义分析阶段根据优先级和结合性构建树形结构。

## 错误处理

- 精确的位置信息（行号、列号、范围）
- 错误恢复：跳到下一个语句边界（`;`、`}`、关键字）
- "期望但缺失"的错误位置指向前一个 token 的末尾
