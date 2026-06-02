"""
Nexsteaduser — PlexsDOS
FAT32 硬盘镜像创建工具 (基于 pyfatfs)
作者: Tinmc189623 | 团队: Nexlyh

使用 pyfatfs 库创建 FAT32 格式的 64MB 硬盘镜像。
用法: python tools/mkfat32.py <output.img> [file1 file2 ...]
"""

import sys
import os
import tempfile

from pyfatfs.PyFat import PyFat
from pyfatfs.PyFatFS import PyFatFS


# FAT32 镜像大小: 64MB
DISK_SIZE = 64 * 1024 * 1024

# 卷标
VOLUME_LABEL = "PLXSDOS"


def create_fat32_image(output_path, files):
    """
    创建 FAT32 硬盘镜像

    参数:
        output_path: 输出镜像文件路径
        files: 要写入镜像的文件路径列表
    """
    # 创建空白镜像文件
    with open(output_path, 'wb') as f:
        f.write(b'\x00' * DISK_SIZE)

    # 使用 pyfatfs 创建 FAT32 文件系统
    pf = PyFat()
    pf.mkfs(output_path, PyFat.FAT_TYPE_FAT32,
             size=DISK_SIZE, label=VOLUME_LABEL)
    pf.close()

    # 打开文件系统并写入文件
    fs = PyFatFS(output_path)

    for filepath in files:
        if not os.path.exists(filepath):
            print(f"Warning: file not found, skipping: {filepath}")
            continue

        # 读取文件内容
        with open(filepath, 'rb') as f:
            data = f.read()

        # 生成 8.3 格式文件名
        basename = os.path.basename(filepath)
        name83 = to_83_name(basename)

        # 写入文件
        fs.writebytes(name83, data)
        print(f"Added: {name83} ({len(data)} bytes)")

    fs.close()

    print(f"\nFAT32 image created: {output_path} ({DISK_SIZE} bytes)")


def to_83_name(name):
    """
    将文件名转换为 8.3 格式

    参数:
        name: 原始文件名
    返回:
        8.3 格式文件名 (大写)
    """
    base = os.path.basename(name)
    parts = base.split('.', 1)
    fname = parts[0].upper()[:8]
    ext = parts[1].upper()[:3] if len(parts) > 1 else ''
    if ext:
        return f"{fname}.{ext}"
    return fname


def main():
    if len(sys.argv) < 2:
        print("Usage: python tools/mkfat32.py <output.img> [file1 file2 ...]")
        sys.exit(1)

    output = sys.argv[1]
    files = sys.argv[2:]
    create_fat32_image(output, files)


if __name__ == '__main__':
    main()
