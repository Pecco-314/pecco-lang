# Pecco Language

基于 LLVM 的系统级编程语言。

## 快速开始

```bash
# 构建
cmake -B build
cmake --build build -j$(nproc)

# 运行测试
ctest --test-dir build

# 编译并运行程序
./build/src/plc --run hello.pec
```

## 语言特性

- **静态类型系统**：`i32`, `f64`, `bool`, `string`, `void`
- **函数定义**：支持递归、多参数、返回值
- **控制流**：`if`/`else`, `while` 循环
- **标准库**：包含一些基础的函数和操作符
  - **函数**：
    - 基础I/O：`write`
    - 其他：`exit`
  - **操作符**：
    - 算术：`+`, `-`, `*`, `/`, `%`, `**`
    - 赋值：`=`, `+=`, `-=`, `*=`, `/=`, `%=`
    - 位运算：`&`, `|`, `^`, `<<`, `>>`
    - 比较：`==`, `!=`, `<`, `<=`, `>`, `>=`
    - 逻辑：`&&`, `||`, `!`
- **自定义操作符**：支持自定义优先级和结合性

## 示例代码

```pecco
# Quick Power
operator infix **(a: i32, n: i32) : i32 prec 90 assoc_right {
    let ans = 1;
    while n != 0 {
        if n % 2 == 1 {
            ans *= a;
        }
        a *= a;
        n /= 2;
    }
    return ans;
}

exit(3 ** 4);
```

## VSCode 扩展

支持 `.pec` 文件的语法高亮：

```bash
cd vscode-extension
./install.sh
```

重启 VSCode 即可使用。

## 文档

- [lexer.md](docs/lexer.md) - 词法分析
- [parser.md](docs/parser.md) - 语法分析
- [semantic.md](docs/semantic.md) - 语义分析
- [codegen.md](docs/codegen.md) - IR 代码生成
- [driver.md](docs/driver.md) - 编译驱动
