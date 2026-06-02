#!/bin/sh
#
# Nexsteaduser — PlexsDOS
# QEMU 测试脚本
# 作者: Tinmc189623 | 团队: Nexlyh
#
# 使用 QEMU 测试 PlexsDOS 自研内核。
# 用法: ./tools/test_qemu.sh
#

set -e

SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
PROJECT_DIR="$(cd "${SCRIPT_DIR}/.." && pwd)"
BUILD_DIR="${PROJECT_DIR}/build"

FLOPPY="${BUILD_DIR}/plexsdos.img"
DISK="${BUILD_DIR}/disk.img"

# 检查镜像文件
if [ ! -f "${FLOPPY}" ]; then
    echo "错误: 软盘镜像不存在: ${FLOPPY}"
    echo "请先运行: make"
    exit 1
fi

# 检查 QEMU
if ! command -v qemu-system-i386 >/dev/null 2>&1; then
    echo "错误: QEMU 未安装。"
    echo "安装方法:"
    echo "  Windows: choco install qemu"
    echo "  macOS:   brew install qemu"
    echo "  Linux:   sudo apt install qemu-system-x86"
    exit 1
fi

echo "============================================"
echo " Nexsteaduser PlexsDOS — QEMU Test"
echo " Author: Tinmc189623 | Team: Nexlyh"
echo "============================================"
echo ""
echo "软盘: ${FLOPPY}"
echo "硬盘: ${DISK}"
echo "按 Ctrl+A, X 退出 QEMU"
echo ""

# QEMU 参数
QEMU_ARGS="
    -fda ${FLOPPY}
    -boot a
    -m 16M
    -display gtk
    -vga std
    -no-reboot
"

# 如果有硬盘镜像则添加
if [ -f "${DISK}" ]; then
    QEMU_ARGS="${QEMU_ARGS} -drive file=${DISK},format=raw,if=ide"
fi

# 启动 QEMU
qemu-system-i386 ${QEMU_ARGS}
