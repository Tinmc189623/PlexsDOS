# 文件系统

PlexsDOS 支持 FAT12、FAT32 和 ISO 9660 文件系统。

## FAT12

- 用于 1.44 MB 软盘镜像
- 12-bit 文件分配表
- 支持 8.3 文件名格式
- 在 `boot_sector.S` 中实现最小化读取

## FAT32

- 用于硬盘镜像 (64 MB)
- 32-bit 文件分配表
- 支持长文件名
- 通过 `kernel/fs/fat32.c` 实现

## ISO 9660

- 用于 CD-ROM 光盘
- 兼容 Joliet 扩展
- 通过 ATAPI CD-ROM 驱动访问

## 驱动器映射

| 驱动器 | 介质 |
|--------|------|
| A: | 软盘 (FAT12) |
| C: | 硬盘 (FAT32) |
| D: | 光盘 (ISO 9660) |
