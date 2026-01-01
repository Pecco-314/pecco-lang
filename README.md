# Pecco Language

基于 LLVM 的系统级编程语言。

## 构建

```bash
cmake -B build
cmake --build build
ctest --test-dir build
```

## 文档

详细文档见 [docs/](docs/) 目录：

- [lexer.md](docs/lexer.md) - 词法分析
- [parser.md](docs/parser.md) - 语法分析
- [semantic.md](docs/semantic.md) - 语义分析
- [driver.md](docs/driver.md) - 编译驱动
