# Driver 命令行工具

## 概述

`plc`是 pecco-lang 的命令行编译器驱动程序。

## 命令行接口

### 基本用法

```bash
plc <input-file> [options]
```

### 命令行选项

- **位置参数**：
  - `<input-file>`：要处理的源文件路径（`.pl` 文件）
  - **必需**：必须提供输入文件

- **标志选项**：
  - `--lex`：运行词法分析器并输出 token 流
  - `--help`：显示帮助信息
  - `--version`：显示版本信息（继承自 LLVM）
