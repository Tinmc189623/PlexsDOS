"""
Nexsteaduser — PlexsDOS
启动软盘镜像创建工具 (混合布局)
作者: Tinmc189623 | 团队: Nexlyh

创建启动盘 (disk1.img):
- 扇区 0: 引导扇区 (boot.bin)
- 扇区 1-N: 内核 (kernel.bin)
- 从 reserved_sectors 开始: FAT12 文件系统

用法: python tools/mkbootdisk.py <output.img> <boot.bin> <kernel.bin> [file1 file2 ...]
"""

import struct
import sys
import os

# 引导扇区中标记 "KSEC" 后面紧跟 sectors_left (2 字节 little-endian)
KSEC_MARKER = b'KSEC'


def patch_kernel_sectors(boot_data, kernel_sectors):
    """
    修补引导扇区中的 KERNEL_SECTORS 值

    在 boot_sector.S 数据段中搜索 "KSEC" 标记,
    标记后紧跟的 2 字节即为 sectors_left 变量。
    将其修补为实际内核扇区数。

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
    return bytes(boot)


# FAT12 参数 (1.44MB 软盘)
BYTES_PER_SECTOR = 512
SECTORS_PER_CLUSTER = 1
NUM_FATS = 2
ROOT_ENTRIES = 224
TOTAL_SECTORS = 2880
SECTORS_PER_FAT = 9
SECTORS_PER_TRACK = 18
NUM_HEADS = 2
MEDIA_DESCRIPTOR = 0xF0


def create_boot_sector(boot_bin, reserved_sectors):
    """创建 FAT12 引导扇区 (带 BPB)"""
    bs = bytearray(BYTES_PER_SECTOR)

    # 复制引导代码 (前 62 字节是代码, 后面是 BPB 和数据)
    # 但我们需要修改 BPB, 所以只复制代码部分
    if len(boot_bin) >= 3:
        bs[0] = boot_bin[0]  # JMP
        bs[1] = boot_bin[1]
        bs[2] = boot_bin[2]  # NOP

    # BPB 参数
    bs[3:11] = b'PLXSDOS '  # OEM 名称
    struct.pack_into('<H', bs, 11, BYTES_PER_SECTOR)     # 每扇区字节数
    struct.pack_into('<B', bs, 13, SECTORS_PER_CLUSTER)   # 每簇扇区数
    struct.pack_into('<H', bs, 14, reserved_sectors)      # 保留扇区数 (含引导+内核)
    struct.pack_into('<B', bs, 16, NUM_FATS)              # FAT 表数量
    struct.pack_into('<H', bs, 17, ROOT_ENTRIES)          # 根目录项数
    struct.pack_into('<H', bs, 19, TOTAL_SECTORS)         # 总扇区数
    struct.pack_into('<B', bs, 21, MEDIA_DESCRIPTOR)      # 介质描述符
    struct.pack_into('<H', bs, 22, SECTORS_PER_FAT)       # 每 FAT 扇区数
    struct.pack_into('<H', bs, 24, SECTORS_PER_TRACK)     # 每磁道扇区数
    struct.pack_into('<H', bs, 26, NUM_HEADS)             # 磁头数
    struct.pack_into('<I', bs, 28, 0)                     # 隐藏扇区数

    # 扩展引导记录
    bs[36] = 0x00                                         # 驱动器号 (软盘)
    bs[38] = 0x29                                         # 扩展引导签名
    struct.pack_into('<I', bs, 39, 0x12345678)            # 卷序列号
    bs[43:54] = b'PLEXSDOS   '                            # 卷标 (11 字节)
    bs[54:62] = b'FAT12   '                               # 文件系统类型

    # 引导签名
    struct.pack_into('<H', bs, 510, 0xAA55)

    # 复制原始引导代码 (从 boot.bin 的偏移 62 开始)
    if len(boot_bin) > 62:
        code_start = 62
        code_end = min(len(boot_bin), 510)
        bs[code_start:code_end] = boot_bin[code_start:code_end]

    return bs


def create_fat_table():
    """创建 FAT12 表"""
    fat = bytearray(SECTORS_PER_FAT * BYTES_PER_SECTOR)
    fat[0] = MEDIA_DESCRIPTOR
    fat[1] = 0xFF
    fat[2] = 0xFF
    return fat


def name_to_83(name):
    """将普通文件名转为 8.3 格式"""
    name83 = bytearray(11)
    for i in range(11):
        name83[i] = 0x20

    base = os.path.basename(name)
    parts = base.split('.', 1)
    fname = parts[0].upper()
    ext = parts[1].upper() if len(parts) > 1 else ''

    for i, c in enumerate(fname[:8]):
        name83[i] = ord(c)
    for i, c in enumerate(ext[:3]):
        name83[8+i] = ord(c)

    return bytes(name83)


def create_dir_entry(name, cluster, size, attr=0x20):
    """创建 32 字节目录项"""
    entry = bytearray(32)
    entry[0:11] = name_to_83(name)
    entry[11] = attr
    struct.pack_into('<H', entry, 26, cluster)
    struct.pack_into('<I', entry, 28, size)
    return entry


def get_fat_entry(fat, cluster):
    """读取 FAT12 条目"""
    offset = cluster + (cluster // 2)
    if cluster & 1:
        return ((fat[offset] >> 4) | (fat[offset + 1] << 4)) & 0xFFF
    else:
        return (fat[offset] | ((fat[offset + 1] & 0x0F) << 8)) & 0xFFF


def set_fat_entry(fat, cluster, value):
    """写入 FAT12 条目"""
    offset = cluster + (cluster // 2)
    if cluster & 1:
        fat[offset] = (fat[offset] & 0x0F) | ((value & 0x0F) << 4)
        fat[offset + 1] = (value >> 4) & 0xFF
    else:
        fat[offset] = value & 0xFF
        fat[offset + 1] = (fat[offset + 1] & 0xF0) | ((value >> 8) & 0x0F)


def allocate_cluster(fat, prev_cluster):
    """在 FAT 表中分配一个新簇"""
    for cluster in range(2, 4084):
        if get_fat_entry(fat, cluster) == 0:
            if prev_cluster > 0:
                set_fat_entry(fat, prev_cluster, cluster)
            return cluster
    return 0


def main():
    """创建启动盘镜像: 解析参数, 读取引导扇区和内核, 构建混合布局 FAT12 镜像"""
    if len(sys.argv) < 4:
        print("用法: python tools/mkbootdisk.py <output.img> <boot.bin> <kernel.bin> [files...]")
        sys.exit(1)

    output = sys.argv[1]
    boot_path = sys.argv[2]
    kernel_path = sys.argv[3]
    files = sys.argv[4:]

    # 读取引导扇区和内核
    with open(boot_path, 'rb') as f:
        boot_bin = f.read()
    with open(kernel_path, 'rb') as f:
        kernel_bin = f.read()

    # 计算保留扇区数 (1 引导 + N 内核)
    kernel_sectors = (len(kernel_bin) + BYTES_PER_SECTOR - 1) // BYTES_PER_SECTOR
    reserved_sectors = 1 + kernel_sectors

    print(f"Boot sector: {len(boot_bin)} bytes")
    print(f"Kernel: {len(kernel_bin)} bytes ({kernel_sectors} sectors)")
    print(f"Reserved sectors: {reserved_sectors}")

    # 创建 1.44MB 空白镜像
    image = bytearray(TOTAL_SECTORS * BYTES_PER_SECTOR)

    # 写入引导扇区 (带修改后的 BPB 和修补的 KERNEL_SECTORS)
    boot = create_boot_sector(boot_bin, reserved_sectors)
    boot = patch_kernel_sectors(boot, kernel_sectors)
    print(f"Patched KERNEL_SECTORS = {kernel_sectors}")
    image[0:512] = boot

    # 写入内核 (从扇区 1 开始)
    for i in range(kernel_sectors):
        offset = (1 + i) * BYTES_PER_SECTOR
        chunk = kernel_bin[i * BYTES_PER_SECTOR:(i + 1) * BYTES_PER_SECTOR]
        if len(chunk) < BYTES_PER_SECTOR:
            chunk = chunk + bytes(BYTES_PER_SECTOR - len(chunk))
        image[offset:offset + BYTES_PER_SECTOR] = chunk

    # 创建 FAT 表
    fat1 = create_fat_table()

    # 计算数据区起始扇区
    data_start = reserved_sectors + NUM_FATS * SECTORS_PER_FAT + \
                 (ROOT_ENTRIES * 32 + BYTES_PER_SECTOR - 1) // BYTES_PER_SECTOR

    # 添加文件到 FAT12 文件系统
    root_dir = bytearray(ROOT_ENTRIES * 32)
    dir_idx = 0

    for filepath in files:
        if not os.path.exists(filepath):
            print(f"警告: 文件不存在, 跳过: {filepath}")
            continue

        with open(filepath, 'rb') as f:
            file_data = f.read()

        fname = os.path.basename(filepath)
        file_size = len(file_data)

        clusters_needed = (file_size + BYTES_PER_SECTOR - 1) // BYTES_PER_SECTOR
        if clusters_needed == 0:
            clusters_needed = 1

        first_cluster = 0
        prev_cluster = 0
        cluster_list = []

        for _ in range(clusters_needed):
            c = allocate_cluster(fat1, prev_cluster)
            if c == 0:
                print(f"错误: 磁盘空间不足, 无法写入 {fname}")
                break
            if first_cluster == 0:
                first_cluster = c
            cluster_list.append(c)
            prev_cluster = c

        if first_cluster == 0:
            continue

        set_fat_entry(fat1, prev_cluster, 0xFFF)

        for i, cluster in enumerate(cluster_list):
            sector = data_start + (cluster - 2) * SECTORS_PER_CLUSTER
            offset = sector * BYTES_PER_SECTOR
            chunk = file_data[i * BYTES_PER_SECTOR:(i + 1) * BYTES_PER_SECTOR]
            if len(chunk) < BYTES_PER_SECTOR:
                chunk = chunk + bytes(BYTES_PER_SECTOR - len(chunk))
            image[offset:offset + BYTES_PER_SECTOR] = chunk

        if dir_idx < ROOT_ENTRIES:
            entry = create_dir_entry(fname, first_cluster, file_size)
            dir_idx_pos = dir_idx * 32
            root_dir[dir_idx_pos:dir_idx_pos + 32] = entry
            dir_idx += 1
            print(f"Added: {fname} ({file_size} bytes)")

    # 写入 FAT 表
    fat1_offset = reserved_sectors * BYTES_PER_SECTOR
    image[fat1_offset:fat1_offset + len(fat1)] = fat1

    fat2_offset = fat1_offset + SECTORS_PER_FAT * BYTES_PER_SECTOR
    image[fat2_offset:fat2_offset + len(fat1)] = fat1

    # 写入根目录
    root_offset = (reserved_sectors + NUM_FATS * SECTORS_PER_FAT) * BYTES_PER_SECTOR
    image[root_offset:root_offset + len(root_dir)] = root_dir

    # 写入镜像文件
    with open(output, 'wb') as f:
        f.write(image)

    print(f"\nBoot floppy created: {output} ({len(image)} bytes)")


if __name__ == '__main__':
    main()
