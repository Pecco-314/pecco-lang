# 语义分析

语义分析分为 3 个阶段：

## 阶段 1：符号表构建（SymbolTableBuilder）

递归遍历 AST，构建层级化符号表。

**处理流程**：

1. 加载标准库：`load_prelude("stdlib/prelude.pec")` - 标记为 `Prelude`
2. 收集用户声明：递归处理所有语句 - 标记为 `User`

**收集内容**：

- 全局：函数定义/声明、操作符定义/声明
- 作用域：函数参数、局部变量、嵌套块

**符号表结构**：

层级化存储：
- 全局符号表：函数、操作符
- 作用域树：每个函数/块维护自己的变量表

每个符号包含：
- 名称、类型
- 源码位置（行、列）
- 来源标记（`User` / `Prelude`）

**验证规则**：

- 同一作用域内不允许重复定义变量
- 不同作用域允许变量遮蔽（shadowing）
- 嵌套函数定义报错（闭包未实现）
- 函数/操作符参数缺少类型标注报错（泛型未实现）

**示例输出**：

```
Global Functions:
  factorial(i32) : i32
  write(...) : i32 [declaration] [prelude]

Operators:
  infix +(i32, i32) : i32 [prec 70] [prelude]
  prefix <+>(i32) : i32

Scope Hierarchy:
Scope [global]:
  Scope [function factorial]:
    Variables:
      n : i32 (line 4)
    Scope [block #0 at line 4]:
      Variables:
        result (line 5)
      Scope [block #1 at line 6]:
        Variables:
          i (line 7)
```

## 阶段 2：操作符解析（OperatorResolver）

将 `OperatorSeq` 转换为树形结构。

**输入**：
- AST
- 符号表

**输出**：
- 解析后的 AST
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

## 阶段 3：类型检查（TypeChecker）

推导表达式类型并检查类型兼容性。

**处理流程**：

1. 递归遍历 AST
2. 推导每个表达式的类型
3. 检查类型约束

**类型推导规则**：

字面量类型：
- `IntLiteral` → `i32`
- `FloatLiteral` → `f64`
- `StringLiteral` → `string`
- `BoolLiteral` → `bool`

变量类型：
- 维护作用域栈跟踪变量类型
- 从内到外查找变量定义
- 支持跨作用域类型传播

操作符类型：
- 查找符号表中匹配的操作符重载
- 返回操作符的返回类型

函数调用类型：
- 查找函数签名
- 返回函数的返回类型

**类型检查规则**：

- Let 语句：声明类型必须与初始化类型匹配
- If/While：条件表达式必须是 `bool` 类型
- 变量传播：支持多层嵌套作用域的类型传播

**作用域管理**：

- 每个函数创建新作用域，记录参数类型
- 每个 Block 创建嵌套作用域
- 变量类型在作用域内传播

**错误示例**：

```pec
func test() : i32 {
  let x = 3.14;
  let y : i32 = x;  # 错误：类型不匹配 (f64 → i32)
  return y;
}
```

错误信息：
```
type error at test.pec:3:17: Type mismatch: variable 'y' declared as 'i32' but initialized with 'f64'
  3 |   let y : i32 = x;
    |                 ^
```

所有类型检查都在编译时完成，无运行时开销。
