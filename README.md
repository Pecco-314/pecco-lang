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
```

### 测试

```bash
ctest --test-dir build --output-on-failure
```
