"""
Nexsteaduser — PlexsDOS
FAT12 磁盘镜像创建工具
作者: Tinmc189623 | 团队: Nexlyh

创建一个 FAT12 格式的 1.44MB 软盘镜像, 可嵌入测试程序。
用法: python tools/mkfat12.py [output.img] [file1 file2 ...]
"""

import struct
import sys
import os

# FAT12 参数 (1.44MB 软盘)
BYTES_PER_SECTOR = 512
SECTORS_PER_CLUSTER = 1
RESERVED_SECTORS = 1
NUM_FATS = 2
ROOT_ENTRIES = 224
TOTAL_SECTORS = 2880
SECTORS_PER_FAT = 9
SECTORS_PER_TRACK = 18
NUM_HEADS = 2
MEDIA_DESCRIPTOR = 0xF0

def create_boot_sector():
    """创建 FAT12 引导扇区 (带 BPB, 无实际引导代码)"""
    bs = bytearray(BYTES_PER_SECTOR)

    # 跳转指令 + NOP
    bs[0] = 0xEB
    bs[1] = 0x3C
    bs[2] = 0x90

    # OEM 名称
    bs[3:11] = b'PLXSDOS '

    # BPB 参数
    struct.pack_into('<H', bs, 11, BYTES_PER_SECTOR)     # 每扇区字节数
    struct.pack_into('<B', bs, 13, SECTORS_PER_CLUSTER)   # 每簇扇区数
    struct.pack_into('<H', bs, 14, RESERVED_SECTORS)      # 保留扇区数
    struct.pack_into('<B', bs, 16, NUM_FATS)              # FAT 表数量
    struct.pack_into('<H', bs, 17, ROOT_ENTRIES)          # 根目录项数
    struct.pack_into('<H', bs, 19, TOTAL_SECTORS)         # 总扇区数
    struct.pack_into('<B', bs, 21, MEDIA_DESCRIPTOR)      # 介质描述符
    struct.pack_into('<H', bs, 22, SECTORS_PER_FAT)       # 每 FAT 扇区数
    struct.pack_into('<H', bs, 24, SECTORS_PER_TRACK)     # 每磁道扇区数
    struct.pack_into('<H', bs, 26, NUM_HEADS)             # 磁头数
    struct.pack_into('<I', bs, 28, 0)                     # 隐藏扇区数

    # 扩展引导记录
    bs[36] = 0x80                                         # 驱动器号
    bs[38] = 0x29                                         # 扩展引导签名
    struct.pack_into('<I', bs, 39, 0x12345678)            # 卷序列号
    bs[43:54] = b'PLXSDOS    '                            # 卷标 (11 字节)
    bs[54:62] = b'FAT12   '                               # 文件系统类型

    # 引导代码 (简单打印消息)
    code = bytes([
        0xBE, 0x3E, 0x7C,       # lea 0x7C3E, %si (消息偏移)
        0xB4, 0x0E,              # mov $0x0E, %ah
        0xBB, 0x07, 0x00,        # mov $7, %bx
        0xAC,                    # lodsb
        0x84, 0xC0,              # test %al, %al
        0x74, 0x02,              # jz halt
        0xCD, 0x10,              # int $0x10
        0xEB, 0xF7,              # jmp loop
        0xFA,                    # halt: cli
        0xF4,                    # hlt
        0xEB, 0xFD,              # jmp halt
    ])
    bs[62:62+len(code)] = code

    # 引导代码后的消息
    msg = b'No system. Use -hda disk.img\r\n'
    msg_offset = 62 + len(code)
    bs[msg_offset:msg_offset+len(msg)] = msg

    # 引导签名
    struct.pack_into('<H', bs, 510, 0xAA55)

    return bs

def create_fat_table():
    """创建 FAT12 表 (仅保留介质描述符和结束标记)"""
    fat = bytearray(SECTORS_PER_FAT * BYTES_PER_SECTOR)

    # FAT[0] = 0xFF0 (介质描述符), FAT[1] = 0xFFF (结束标记)
    # FAT12 条目打包: 两个 12-bit 条目占 3 字节
    # 字节布局: [FAT[n]_lo] [FAT[n]_hi:FAT[n+1]_lo] [FAT[n+1]_hi]
    # FAT[0]: fat[0]=0xF0, fat[1]高4位=0x0F → 0xFF0
    # FAT[1]: fat[1]低4位=0x0F, fat[2]=0xFF → 0xFFF
    fat[0] = MEDIA_DESCRIPTOR   # 0xF0
    fat[1] = 0xFF               # 高4位=FAT[0]_hi(0x0F), 低4位=FAT[1]_lo(0x0F)
    fat[2] = 0xFF               # FAT[1]_hi

    return fat

def name_to_83(name):
    """将普通文件名转为 8.3 格式 (11 字节)"""
    name83 = bytearray(11)
    for i in range(11):
        name83[i] = 0x20  # 空格填充

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
    struct.pack_into('<H', entry, 26, cluster)   # 起始簇号
    struct.pack_into('<I', entry, 28, size)       # 文件大小
    return entry

def allocate_cluster(fat, prev_cluster):
    """在 FAT 表中分配一个新簇, 返回簇号"""
    for cluster in range(2, 4084):
        if get_fat_entry(fat, cluster) == 0:
            if prev_cluster > 0:
                set_fat_entry(fat, prev_cluster, cluster)
            return cluster
    return 0

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

def main():
    if len(sys.argv) < 2:
        print("用法: python tools/mkfat12.py <output.img> [file1 file2 ...]")
        sys.exit(1)

    output = sys.argv[1]
    files = sys.argv[2:]

    # 创建 1.44MB 空白镜像
    image = bytearray(TOTAL_SECTORS * BYTES_PER_SECTOR)

    # 写入引导扇区
    boot = create_boot_sector()
    image[0:512] = boot

    # 创建 FAT 表
    fat1 = create_fat_table()
    fat2 = create_fat_table()  # FAT2 是 FAT1 的副本

    # 添加文件到镜像
    root_dir = bytearray(ROOT_ENTRIES * 32)
    dir_idx = 0
    next_data_sector = 0  # 数据区起始扇区偏移

    for filepath in files:
        if not os.path.exists(filepath):
            print(f"警告: 文件不存在, 跳过: {filepath}")
            continue

        with open(filepath, 'rb') as f:
            file_data = f.read()

        fname = os.path.basename(filepath)
        file_size = len(file_data)

        # 计算需要的簇数
        clusters_needed = (file_size + BYTES_PER_SECTOR - 1) // BYTES_PER_SECTOR
        if clusters_needed == 0:
            clusters_needed = 1

        # 分配簇链
        first_cluster = 0
        prev_cluster = 0
        cluster_list = []

        for i in range(clusters_needed):
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

        # 标记最后一个簇为链尾
        set_fat_entry(fat1, prev_cluster, 0xFFF)

        # 写入文件数据到数据区
        data_start = (RESERVED_SECTORS + NUM_FATS * SECTORS_PER_FAT +
                      (ROOT_ENTRIES * 32 + BYTES_PER_SECTOR - 1) // BYTES_PER_SECTOR)

        for i, cluster in enumerate(cluster_list):
            sector = data_start + (cluster - 2) * SECTORS_PER_CLUSTER
            offset = sector * BYTES_PER_SECTOR
            chunk = file_data[i * BYTES_PER_SECTOR:(i + 1) * BYTES_PER_SECTOR]
            # 补齐到 512 字节
            if len(chunk) < BYTES_PER_SECTOR:
                chunk = chunk + bytes(BYTES_PER_SECTOR - len(chunk))
            image[offset:offset + BYTES_PER_SECTOR] = chunk

        # 创建目录项
        if dir_idx < ROOT_ENTRIES:
            entry = create_dir_entry(fname, first_cluster, file_size)
            dir_idx_pos = dir_idx * 32
            root_dir[dir_idx_pos:dir_idx_pos + 32] = entry
            dir_idx += 1
            print(f"已添加: {fname} ({file_size} 字节, 簇 {first_cluster})")

    # 写入 FAT 表
    fat1_offset = RESERVED_SECTORS * BYTES_PER_SECTOR
    image[fat1_offset:fat1_offset + len(fat1)] = fat1

    fat2_offset = fat1_offset + SECTORS_PER_FAT * BYTES_PER_SECTOR
    image[fat2_offset:fat2_offset + len(fat2)] = fat2

    # FAT2 = FAT1 副本
    image[fat2_offset:fat2_offset + len(fat1)] = fat1

    # 写入根目录
    root_offset = (RESERVED_SECTORS + NUM_FATS * SECTORS_PER_FAT) * BYTES_PER_SECTOR
    image[root_offset:root_offset + len(root_dir)] = root_dir

    # 写入镜像文件
    with open(output, 'wb') as f:
        f.write(image)

    print(f"\nFAT12 镜像已创建: {output} ({len(image)} 字节)")

if __name__ == '__main__':
    main()
