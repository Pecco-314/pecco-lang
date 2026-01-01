#!/bin/bash
# VSCode 扩展安装脚本

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
EXT_NAME="pecco-lang-0.1.0"

# 检测 VSCode 类型（优先检测远程，因为远程也可能有本地目录）
if [ -d "$HOME/.vscode-server-insiders" ]; then
    EXT_DIR="$HOME/.vscode-server-insiders/extensions"
    echo "检测到 VSCode Insiders Remote"
elif [ -d "$HOME/.vscode-server" ]; then
    EXT_DIR="$HOME/.vscode-server/extensions"
    echo "检测到 VSCode Remote"
elif [ -d "$HOME/.vscode-insiders" ] || command -v code-insiders &> /dev/null; then
    EXT_DIR="$HOME/.vscode-insiders/extensions"
    echo "检测到 VSCode Insiders 本地"
else
    EXT_DIR="$HOME/.vscode/extensions"
    echo "检测到 VSCode 本地"
fi

# 创建扩展目录
mkdir -p "$EXT_DIR"

# 删除旧的符号链接（如果存在）
[ -L "$EXT_DIR/$EXT_NAME" ] && rm "$EXT_DIR/$EXT_NAME"

# 创建新的符号链接
ln -sf "$SCRIPT_DIR" "$EXT_DIR/$EXT_NAME"

echo "✓ Pecco 语法高亮扩展已安装到: $EXT_DIR/$EXT_NAME"
echo ""
echo "请重启 VSCode 或按 Ctrl+Shift+P 输入 'Reload Window' 使扩展生效"
