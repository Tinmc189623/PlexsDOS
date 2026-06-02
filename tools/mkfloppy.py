"""
Nexsteaduser — PlexsDOS
软盘镜像创建工具
作者: Tinmc189623 | 团队: Nexlyh

创建 1.44MB 软盘镜像, 将引导扇区和内核写入指定扇区。
软盘不使用文件系统 — 引导扇区在扇区 0, 内核从扇区 1 开始。

用法: python tools/mkfloppy.py <output.img> <boot.bin> <kernel.bin>
"""

import struct
import sys


# 引导扇区中标记 "KSEC" 后面紧跟 sectors_left (2 字节 little-endian)
# boot_sector.S 中: ksec_marker .ascii "KSEC" / sectors_left .word KERNEL_SECTORS
KSEC_MARKER = b'KSEC'


def patch_kernel_sectors(boot_data, kernel_sectors):
    """
    修补引导扇区中的 KERNEL_SECTORS 值

    修补两处:
    1. sectors_left 变量 (KSEC 标记后 4 字节处)
    2. movw $KERNEL_SECTORS, (sectors_left) 指令中的立即数
       搜索 C7 06 <sectors_left_addr_lo> <sectors_left_addr_hi> 模式,
       修补其后的 2 字节立即数。

    参数:
        boot_data: 引导扇区二进制数据 (bytes)
        kernel_sectors: 实际内核扇区数
    返回:
        修补后的引导扇区数据 (bytes)
    """
    boot = bytearray(boot_data)
    marker_pos = boot.find(KSEC_MARKER)
    if marker_pos < 0:
        print("警告: 引导扇区中未找到 KSEC 标记, 跳过修补")
        return bytes(boot)

    sectors_left_offset = marker_pos + len(KSEC_MARKER)
    struct.pack_into('<H', boot, sectors_left_offset, kernel_sectors)

    # 修补 movw $KERNEL_SECTORS, (sectors_left) 指令中的立即数
    # 指令格式: C7 06 <addr_lo> <addr_hi> <imm_lo> <imm_hi>
    # sectors_left 在内存中的地址 = 0x7C00 + sectors_left_offset
    sl_addr = 0x7C00 + sectors_left_offset
    sl_addr_bytes = struct.pack('<H', sl_addr)
    # 搜索 C7 06 <sectors_left_addr> 模式
    pattern = bytes([0xC7, 0x06]) + sl_addr_bytes
    instr_pos = boot.find(pattern)
    if instr_pos >= 0:
        imm_offset = instr_pos + len(pattern)
        struct.pack_into('<H', boot, imm_offset, kernel_sectors)
        print(f"Patched movw immediate at offset 0x{imm_offset:X} = {kernel_sectors}")
    else:
        print("警告: 未找到 movw $KERNEL_SECTORS 指令, 跳过指令修补")

    return bytes(boot)


# 1.44MB 软盘参数
FLOPPY_SIZE = 1474560  # 2880 * 512
SECTOR_SIZE = 512


def create_floppy_image(output_path, boot_path, kernel_path):
    """
    创建可引导的软盘镜像

    参数:
        output_path: 输出镜像文件路径
        boot_path: 引导扇区二进制文件路径 (512 字节)
        kernel_path: 内核二进制文件路径
    """
    # 读取引导扇区
    with open(boot_path, 'rb') as f:
        boot_data = f.read()

    if len(boot_data) != SECTOR_SIZE:
        print(f"Error: boot sector must be exactly {SECTOR_SIZE} bytes, got {len(boot_data)}")
        sys.exit(1)

    # 验证引导签名
    if boot_data[510] != 0x55 or boot_data[511] != 0xAA:
        print("Error: boot sector missing 0x55AA signature")
        sys.exit(1)

    # 读取内核
    with open(kernel_path, 'rb') as f:
        kernel_data = f.read()

    kernel_sectors = (len(kernel_data) + SECTOR_SIZE - 1) // SECTOR_SIZE
    print(f"Boot sector: {len(boot_data)} bytes")
    print(f"Kernel: {len(kernel_data)} bytes ({kernel_sectors} sectors)")

    # 修补引导扇区中的 KERNEL_SECTORS
    boot_data = patch_kernel_sectors(boot_data, kernel_sectors)
    print(f"Patched KERNEL_SECTORS = {kernel_sectors}")

    if kernel_sectors > 2879:
        print(f"Error: kernel too large ({kernel_sectors} sectors, max 2879)")
        sys.exit(1)

    # 创建空白软盘镜像
    image = bytearray(FLOPPY_SIZE)

    # 写入引导扇区 (扇区 0)
    image[0:SECTOR_SIZE] = boot_data

    # 写入内核 (从扇区 1 开始)
    kernel_offset = SECTOR_SIZE
    image[kernel_offset:kernel_offset + len(kernel_data)] = kernel_data

    # 写入镜像文件
    with open(output_path, 'wb') as f:
        f.write(image)

    print(f"Floppy image created: {output_path} ({FLOPPY_SIZE} bytes)")


def main():
    if len(sys.argv) != 4:
        print("Usage: python tools/mkfloppy.py <output.img> <boot.bin> <kernel.bin>")
        sys.exit(1)

    create_floppy_image(sys.argv[1], sys.argv[2], sys.argv[3])


if __name__ == '__main__':
    main()
