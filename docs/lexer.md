# 词法分析

将源代码转换为 token 流。

## Token 类型

- `Integer` / `Float` - 数字字面量
- `String` - 字符串字面量（支持转义序列）
- `Identifier` / `Keyword` - 标识符与关键字
- `Operator` / `Punctuation` - 操作符与标点
- `Comment` - 注释（`#` 开头）

每个 token 包含：`kind`、`lexeme`、`line`、`column`。

## 语言特性

### 数字

- 整数：`42`、`1234567890`
- 浮点数：`3.14`、`6.022e23`、`1e-3`

### 字符串

- 双引号包围：`"hello"`
- 转义序列：`\\`、`\"`、`\n`、`\t` 等
- 错误检测：未终止字符串、无效转义

### 标识符

- 规则：`[a-zA-Z_][a-zA-Z0-9_]*`
- 关键字：
  ```
  let func operator if else return while
  true false prefix infix postfix
  prec assoc_left assoc_right
  ```

### 操作符

- 字符集：`+-*/%=&|^!<>?.`
- 贪婪匹配：连续字符合并（如 `==`、`<=`、`&&`、`->`）

### 标点

单字符：`(){}[],;:#`
