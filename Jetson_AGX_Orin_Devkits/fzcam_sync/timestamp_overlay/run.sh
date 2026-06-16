#!/bin/bash
# 一键启动 timestamp_overlay：停止 daemon → 确保模块 → 运行程序

set -e

SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
MODULE_PATH="/home/nvidia/fzcam_sync/build/fzcam_sync.ko"

echo "=== timestamp_overlay 启动脚本 ==="

# 1. 停止 fzcam_sync daemon（避免 ioctl 冲突）
echo "[1/3] 停止 fzcam-sync daemon..."
sudo systemctl stop fzcam-sync 2>/dev/null && echo "  daemon 已停止" || echo "  daemon 未运行或已停止"

# 2. 确保 fzcam_sync 内核模块已加载
echo "[2/3] 检查 fzcam_sync 内核模块..."
if ! lsmod | grep -q fzcam_sync; then
    if [ -f "$MODULE_PATH" ]; then
        sudo insmod "$MODULE_PATH"
        echo "  模块已加载"
    else
        echo "  [ERROR] 找不到 $MODULE_PATH，请先编译内核模块 (cd /home/nvidia/fzcam_sync && make)"
        exit 1
    fi
else
    echo "  模块已加载"
fi

# 3. 编译并运行
echo "[3/3] 运行 timestamp_overlay..."
cd "$SCRIPT_DIR"
make -s 2>/dev/null || make

echo ""
sudo ./timestamp_overlay "$@"

echo ""
echo "=== 已退出 ==="
