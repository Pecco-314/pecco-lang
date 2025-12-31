# The Pecco Language

一个基于 LLVM 的系统级语言。

## 快速开始

### 构建

```bash
cmake -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build
```

### 运行

```bash
# 词法分析
./build/src/plc sample.pl --lex

# 语法分析
./build/src/plc sample.pl --parse

# 默认编译（语义分析）
./build/src/plc sample.pl

# 输出解析后的 AST
./build/src/plc sample.pl --dump-ast

# 输出符号表
./build/src/plc sample.pl --dump-symbols
```

### 测试

```bash
ctest --test-dir build --output-on-failure
```

## 特性

- ✅ 词法分析（有限状态机实现）
- ✅ 语法分析（递归下降解析器）
- ✅ 运算符重载与自定义运算符
- ✅ 优先级与结合性支持
- ✅ 语义分析（符号表、运算符解析）
- 🚧 类型检查与类型推断
- 🚧 LLVM IR 代码生成
