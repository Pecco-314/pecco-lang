# 语义分析

## 概述

语义分析器负责在语法分析之后对 AST 进行深入处理，包括符号表构建、运算符解析、类型检查等。

## 处理流程

### 1. 加载标准库

```cpp
analyzer.load_prelude("stdlib/prelude.pl");
```

**功能**：
- 解析 `stdlib/prelude.pl` 文件
- 收集内置函数和运算符声明
- 将它们加入符号表

**标准库内容**：
- 1 个内置函数：`write()`
- 19 个运算符族（支持多种类型重载）

### 2. 收集用户声明

```cpp
analyzer.collect_declarations(statements);
```

**功能**：
- 遍历用户代码的所有语句
- 收集函数定义和运算符声明
- 加入符号表，支持函数重载

**支持的声明**：
- 函数定义：`func name(...) : type { ... }`
- 函数声明：`func name(...) : type;`
- 运算符声明：`operator <pos> <op>(...) : type [prec N] [assoc <dir>];`

### 3. 运算符解析

```cpp
analyzer.resolve_operators(expression);
```

**功能**：
- 将平面的 `OperatorSeqExpr` 转换为树状结构
- 应用运算符优先级和结合性规则
- 生成 `BinaryExpr` 和 `UnaryExpr` 节点

## 符号表

### 数据结构

```cpp
class SymbolTable {
  // 函数表：函数名 -> 重载列表
  map<string, vector<FunctionSignature>> functions_;

  // 运算符表：(运算符, 位置) -> 重载列表
  OperatorTable operators_;
};
```

### 函数签名

```cpp
struct FunctionSignature {
  string name;                      // 函数名
  vector<string> param_types;       // 参数类型
  string return_type;               // 返回类型
  bool is_declaration_only;         // 是否仅声明
};
```

**示例**：
```
write(i32, string, i32) : i32 [declaration]
add(i32, i32) : i32
add(f64, f64) : f64  // 重载
```

### 运算符信息

```cpp
struct OperatorInfo {
  string op;                        // 运算符符号
  OpPosition position;              // 位置：prefix/infix/postfix
  int precedence;                   // 优先级（仅 infix）
  Associativity assoc;              // 结合性（仅 infix）
  FunctionSignature signature;      // 类型签名
};
```

**示例**：
```
infix +(i32, i32) : i32 [prec 70, left]
infix +(f64, f64) : f64 [prec 70, left]
prefix -(i32) : i32
```

### 查询接口

```cpp
// 查找函数（返回所有重载）
vector<FunctionSignature> find_functions(const string &name);

// 查找运算符（返回所有重载）
vector<OperatorInfo> find_operators(const string &op, OpPosition pos);

// 查找特定运算符的所有位置
vector<OperatorInfo> find_all_operators(const string &op);
```

## 运算符解析

### 优先级表

| 优先级 | 运算符 | 结合性 | 说明 |
|--------|--------|--------|------|
| 90 | `**` | 右 | 幂运算 |
| 80 | `*` `/` `%` | 左 | 乘除余 |
| 70 | `+` `-` | 左 | 加减 |
| 65 | `<<` `>>` | 左 | 位移 |
| 60 | `<` `>` `<=` `>=` | 无 | 关系比较 |
| 55 | `==` `!=` | 无 | 相等性 |
| 50 | `&` | 左 | 按位与 |
| 45 | `^` | 左 | 按位异或 |
| 40 | `\|` | 左 | 按位或 |
| 30 | `&&` | 左 | 逻辑与 |
| 20 | `\|\|` | 左 | 逻辑或 |

### 解析算法

使用**优先级攀登（Precedence Climbing）**算法：

```cpp
ExprPtr build_expr_tree(vector<ExprPtr> &operands,
                       vector<OpInfo> &operators) {
  if (operands.size() == 1) return operands[0];

  // 找到优先级最低的运算符
  int min_prec = INT_MAX;
  int split_idx = -1;

  for (int i = 0; i < operators.size(); ++i) {
    if (operators[i].precedence < min_prec ||
        (operators[i].precedence == min_prec &&
         operators[i].assoc == Associativity::Left)) {
      min_prec = operators[i].precedence;
      split_idx = i;  // 左结合：取最右的
    }
  }

  // 递归构建左右子树
  auto left = build_expr_tree(operands[0..split_idx]);
  auto right = build_expr_tree(operands[split_idx+1..end]);

  return BinaryExpr(operators[split_idx].op, left, right);
}
```

**关键点**：
- **左结合**：优先级相同时，取**最右边**的运算符作为根
- **右结合**：优先级相同时，取**最左边**的运算符作为根

### 解析示例

#### 示例 1：优先级

输入：`1 + 2 * 3`

```
OperatorSeqExpr:
  operands: [1, 2, 3]
  operators: [+, *]

优先级: * (80) > + (70)
选择 + 作为根（优先级最低）

结果:
  Binary(+,
    IntLiteral(1),
    Binary(*,
      IntLiteral(2),
      IntLiteral(3)))
```

#### 示例 2：左结合

输入：`10 - 5 - 2`

```
OperatorSeqExpr:
  operands: [10, 5, 2]
  operators: [-, -]

优先级相同 (70)，结合性为左
选择最右边的 - 作为根

结果:
  Binary(-,
    Binary(-,
      IntLiteral(10),
      IntLiteral(5)),
    IntLiteral(2))

计算顺序: (10 - 5) - 2 = 3
```

#### 示例 3：右结合

输入：`2 ** 3 ** 2`

```
OperatorSeqExpr:
  operands: [2, 3, 2]
  operators: [**, **]

优先级相同 (90)，结合性为右
选择最左边的 ** 作为根

结果:
  Binary(**,
    IntLiteral(2),
    Binary(**,
      IntLiteral(3),
      IntLiteral(2)))

计算顺序: 2 ** (3 ** 2) = 512
```

#### 示例 4：混合运算

输入：`1 + 2 * 3 - 4 / 2`

```
优先级:
  * (80), / (80)
  + (70), - (70)

步骤:
1. 先处理 * 和 /（优先级 80）
   2 * 3 → Binary(*, 2, 3)
   4 / 2 → Binary(/, 4, 2)

2. 处理 + 和 -（优先级 70，左结合）
   选择最右边的 - 作为根

结果:
  Binary(-,
    Binary(+,
      IntLiteral(1),
      Binary(*, 2, 3)),
    Binary(/, 4, 2))
```
