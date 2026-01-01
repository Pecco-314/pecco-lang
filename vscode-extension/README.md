# Pecco Language VSCode Extension

VSCode 语法高亮扩展，支持 `.pec` 文件。

## 快速安装

```bash
cd vscode-extension
./install.sh
```

脚本会自动检测你的 VSCode 类型（本地/远程/Insiders）并安装到正确的目录。

## 手动安装

```bash
# VSCode 本地
ln -s "$(pwd)" ~/.vscode/extensions/pecco-lang-0.1.0

# VSCode Insiders 本地
ln -s "$(pwd)" ~/.vscode-insiders/extensions/pecco-lang-0.1.0

# VSCode Remote
ln -s "$(pwd)" ~/.vscode-server/extensions/pecco-lang-0.1.0

# VSCode Insiders Remote
ln -s "$(pwd)" ~/.vscode-server-insiders/extensions/pecco-lang-0.1.0
```

安装后重启 VSCode 或按 `Ctrl+Shift+P` → "Reload Window"。

## 功能

- 语法高亮：关键字、类型、字符串、数字、注释
- 自动补全括号和引号
- 注释支持（`#`）
- 代码折叠
