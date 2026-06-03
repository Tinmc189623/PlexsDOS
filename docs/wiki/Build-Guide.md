# 构建指南

## 前置条件

- MSYS2 环境 (Windows) 或 Linux
- GCC + GAS + LD (32-bit 目标, mingw32)
- QEMU (用于测试)
- Python 3 + pyfatfs (用于创建磁盘镜像)

## 构建命令

```bash
# 完整构建 (软盘 + 硬盘 + ISO + 安装盘)
make clean && make

# 仅 FAT32 硬盘镜像
make disk

# ISO 9660 可引导光盘
make iso

# 40GB VMDK 虚拟硬盘
make vmdk
```

## QEMU 测试

```bash
# 软盘 + 硬盘
make run

# 仅软盘
make run-floppy

# 光盘 + 硬盘
make run-iso

# VMDK 虚拟硬盘
make run-vmdk
```

## 构建产物

| 文件 | 说明 |
|------|------|
| `build/plexsdos.img` | 可引导 FAT12 软盘镜像 (1.5 MB) |
| `build/disk.img` | FAT32 硬盘镜像 (64 MB) |
| `build/plexsdos.iso` | ISO 9660 可引导光盘 (1.8 MB) |
| `build/plexsdos-*install_disks_*.img` | 5 张安装软盘 (各 1.5 MB) |
