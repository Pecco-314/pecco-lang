# 语义分析

语义分析分为两个独立阶段：

## 阶段 1：声明收集（DeclarationCollector）

从 AST 构建符号表。

**处理流程**：

1. 加载标准库：`load_prelude("stdlib/prelude.pec", symbol_table)`
2. 收集用户声明：`collect(statements, symbol_table)`

**收集内容**：

- 函数定义/声明
- 操作符定义/声明（包括优先级和结合性）

**符号表内容**：

```
函数：name(param_types) : return_type
操作符：pos op(param_types) : return_type [prec N] [assoc dir]
```

示例：

```
Functions:
  add(i32, i32) : i32
  write(i32, string, i32) : i32 [declaration]

Operators:
  infix +(i32, i32) : i32 [prec 70, left]
  infix +(f64, f64) : f64 [prec 70, left]
  prefix -(i32) : i32
```

## 阶段 2：操作符解析（OperatorResolver）

将 `OperatorSeq` 转换为树形结构。

**输入**：
- AST（包含 `OperatorSeq` 节点）
- 符号表（只读）

**输出**：
- 解析后的 AST（`OperatorSeq` → `Binary` / `Unary`）
- 错误列表（如果有）

**解析算法**：

1. 前缀/后缀折叠：从左到右贪婪匹配
2. 中缀树构建：使用优先级攀登算法
   - 找到优先级最低的操作符作为根
   - 左结合：相同优先级取最右边
   - 右结合：相同优先级取最左边

**错误检测**：

- 操作符未定义
- 同一优先级混合结合性（如 `a +< b +>` 其中 `+<` 左结合、`+>` 右结合）

**示例**：

输入：`1 + 2 * 3`（`OperatorSeq`）

```
operators: [+, *]
优先级: +(70), *(80)
选择 + 作为根（优先级最低）

输出:
  Binary(+,
    IntLiteral(1),
    Binary(*, IntLiteral(2), IntLiteral(3)))
```

输入：`10 - 5 - 2`（`OperatorSeq`）

```
operators: [-, -]
优先级相同(70)，左结合 → 选最右边的 - 作为根

输出:
  Binary(-,
    Binary(-, IntLiteral(10), IntLiteral(5)),
    IntLiteral(2))
```

输入：`2 ** 3 ** 2`（`OperatorSeq`）

```
operators: [**, **]
优先级相同(90)，右结合 → 选最左边的 ** 作为根

输出:
  Binary(**,
    IntLiteral(2),
    Binary(**, IntLiteral(3), IntLiteral(2)))
```
