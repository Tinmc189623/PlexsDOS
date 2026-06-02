"""
Nexsteaduser — PlexsDOS
40GB VMDK 虚拟硬盘镜像构建工具
作者: Tinmc189623 | 团队: Nexlyh

离线构建 40GB 原始磁盘镜像, 写入 MBR + FAT32 文件系统 + 内核 + 程序文件,
然后通过 qemu-img 转换为 VMDK 格式。

磁盘布局:
  扇区 0:          MBR (分区表: FAT32 LBA, type=0x0C, start=2048)
  扇区 2048:       FAT32 分区起始
    +0:            VBR (引导扇区)
    +1:            FSINFO 扇区
    +6:            备份 VBR
    +32 ~ +32+N:   FAT 表 1
    +32+N ~ +32+2N: FAT 表 2
    +32+2N ~:      数据区 (簇 2 起始)
      簇 2:        根目录
      簇 3+:       文件数据

用法:
  python tools/mkvmdk.py build/plexsdos.vmdk \\
      --mbr build/hd_mbr.bin --vbr build/hd_vbr.bin \\
      --kernel build/kernel.bin \\
      --file build/programs/HELLO.COMX --file programs/README.TXT

依赖: qemu-img (QEMU 附带)
"""

import argparse
import os
import struct
import subprocess
import sys


# ==================== 常量 ====================

SECTOR_SIZE = 512

# 分区参数
PART_START_LBA = 2048           # 分区起始 LBA (1MB 偏移)

# FAT32 参数
FAT32_SECS_PER_CLUST = 8       # 每簇 8 扇区 = 4KB
FAT32_NUM_FATS = 2             # 2 个 FAT 表
FAT32_RESERVED_SECS = 32       # 保留扇区

# qemu-img 路径 (Windows 常见位置)
QEMU_IMG_PATHS = [
    r"C:\Program Files\qemu\qemu-img.exe",
    r"C:\Program Files (x86)\qemu\qemu-img.exe",
    "qemu-img",
]


# ==================== 工具函数 ====================

def name_to_83(name: str) -> bytes:
    """
    将文件名转为 8.3 格式 (11 字节, 空格填充)

    参数:
        name: 原始文件名
    返回:
        11 字节的 8.3 格式文件名
    """
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
        name83[8 + i] = ord(c)

    return bytes(name83)


def parse_size(size_str: str) -> int:
    """
    解析大小字符串 (如 '40G', '64M', '1024K')

    参数:
        size_str: 大小字符串
    返回:
        字节数
    """
    size_str = size_str.strip().upper()
    multipliers = {'G': 1024**3, 'M': 1024**2, 'K': 1024, 'B': 1}

    for suffix, mult in multipliers.items():
        if size_str.endswith(suffix):
            return int(float(size_str[:-1]) * mult)

    return int(size_str)


def find_qemu_img() -> str:
    """
    查找 qemu-img 可执行文件

    按优先级搜索常见安装路径。

    返回:
        qemu-img 可执行文件路径, 未找到则退出
    """
    for path in QEMU_IMG_PATHS:
        if os.path.exists(path):
            return path
        try:
            result = subprocess.run(
                [path, "--version"],
                capture_output=True, timeout=5
            )
            if result.returncode == 0:
                return path
        except (FileNotFoundError, subprocess.TimeoutExpired):
            continue

    print("错误: 未找到 qemu-img")
    print("请安装 QEMU: https://www.qemu.org/download/")
    sys.exit(1)


def convert_to_vmdk(raw_path: str, vmdk_path: str) -> None:
    """
    将原始磁盘镜像转换为 VMDK 格式

    使用 qemu-img convert 将 raw 镜像转换为 monolithicSparse VMDK。

    参数:
        raw_path: 原始镜像文件路径
        vmdk_path: 输出 VMDK 文件路径
    """
    qemu_img = find_qemu_img()

    cmd = [
        qemu_img, "convert",
        "-f", "raw",
        "-O", "vmdk",
        "-o", "subformat=monolithicSparse",
        raw_path,
        vmdk_path,
    ]

    print(f"  转换为 VMDK...")
    print(f"  命令: {' '.join(cmd)}")

    result = subprocess.run(cmd, capture_output=True, text=True, timeout=300)

    if result.returncode != 0:
        print(f"  qemu-img 错误:\n{result.stderr}")
        sys.exit(1)

    if result.stderr:
        for line in result.stderr.strip().split('\n'):
            if line.strip():
                print(f"  qemu-img: {line.strip()}")


# ==================== 主函数 ====================

def main() -> None:
    """
    VMDK 构建工具入口

    解析命令行参数, 构建原始磁盘镜像, 转换为 VMDK 格式。
    """
    parser = argparse.ArgumentParser(
        description='Nexsteaduser PlexsDOS — 40GB VMDK 虚拟硬盘构建工具',
        formatter_class=argparse.RawDescriptionHelpFormatter,
        epilog='示例:\n'
               '  python tools/mkvmdk.py build/plexsdos.vmdk \\\n'
               '      --mbr build/hd_mbr.bin --vbr build/hd_vbr.bin \\\n'
               '      --kernel build/kernel.bin \\\n'
               '      --file build/programs/HELLO.COMX --file programs/README.TXT\n'
    )

    parser.add_argument('output', help='输出 VMDK 文件路径')
    parser.add_argument('--mbr', required=True, help='MBR 二进制文件路径')
    parser.add_argument('--vbr', required=True, help='VBR 二进制文件路径')
    parser.add_argument('--kernel', required=True, help='内核二进制文件路径')
    parser.add_argument('--file', action='append', default=[],
                        help='要添加的文件 (可重复使用)')
    parser.add_argument('--size', default='40G',
                        help='磁盘大小 (默认: 40G)')
    parser.add_argument('--volume-label', default='PLXSDOS',
                        help='卷标 (默认: PLXSDOS)')
    parser.add_argument('--keep-raw', action='store_true',
                        help='保留原始镜像文件')

    args = parser.parse_args()

    print("Nexsteaduser PlexsDOS — VMDK 构建工具")
    print("=" * 50)

    # 验证输入文件
    for path in [args.mbr, args.vbr, args.kernel]:
        if not os.path.exists(path):
            print(f"错误: 文件不存在: {path}")
            sys.exit(1)

    for path in args.file:
        if not os.path.exists(path):
            print(f"警告: 文件不存在, 跳过: {path}")

    # 解析磁盘大小
    disk_size = parse_size(args.size)
    disk_sectors = disk_size // SECTOR_SIZE
    part_sectors = disk_sectors - PART_START_LBA

    # 计算 FAT32 参数
    # FAT 扇区数 = ceil(total_clusters * 4 / 512)
    # total_clusters ≈ part_sectors / secs_per_clust
    total_data_clusters = part_sectors // FAT32_SECS_PER_CLUST
    fat_sectors = (total_data_clusters * 4 + SECTOR_SIZE - 1) // SECTOR_SIZE

    # 精确计算数据区
    data_area_sectors = part_sectors - FAT32_RESERVED_SECS - FAT32_NUM_FATS * fat_sectors
    total_clusters = data_area_sectors // FAT32_SECS_PER_CLUST

    print(f"\n磁盘参数:")
    print(f"  总大小: {disk_size:,} 字节 ({disk_size / 1024 / 1024 / 1024:.1f} GB)")
    print(f"  总扇区: {disk_sectors:,}")
    print(f"  分区起始: LBA {PART_START_LBA}")
    print(f"  分区扇区: {part_sectors:,}")
    print(f"  FAT 扇区: {fat_sectors:,}")
    print(f"  每簇扇区: {FAT32_SECS_PER_CLUST}")
    print(f"  总簇数: {total_clusters:,}")
    print(f"  卷标: {args.volume_label}")

    # 计算数据区起始 LBA
    data_lba = PART_START_LBA + FAT32_RESERVED_SECS + FAT32_NUM_FATS * fat_sectors

    # 准备文件列表
    all_files = []

    with open(args.kernel, 'rb') as kf:
        kernel_data = kf.read()
    all_files.append(('KERNEL.BIN', kernel_data))
    print(f"\n  KERNEL.BIN: {len(kernel_data):,} 字节")

    for filepath in args.file:
        if not os.path.exists(filepath):
            continue
        with open(filepath, 'rb') as f:
            file_data = f.read()
        basename = os.path.basename(filepath).upper()
        all_files.append((basename, file_data))
        print(f"  {basename}: {len(file_data):,} 字节")

    # 创建原始镜像 (稀疏文件)
    raw_path = args.output
    if raw_path.endswith('.vmdk'):
        raw_path = raw_path[:-5] + '.raw'

    print(f"\n[1/3] 创建原始镜像: {raw_path}")
    with open(raw_path, 'wb') as f:
        f.seek(disk_size - 1)
        f.write(b'\x00')

    # 写入磁盘结构
    print(f"\n[2/3] 写入磁盘结构...")

    with open(raw_path, 'r+b') as f:
        # === MBR (扇区 0) ===
        print("  写入 MBR...")
        mbr = bytearray(SECTOR_SIZE)

        with open(args.mbr, 'rb') as mf:
            mbr_code = mf.read()
        code_size = min(len(mbr_code), 446)
        mbr[:code_size] = mbr_code[:code_size]

        # 分区表项 (条目 1, 偏移 0x1BE)
        # CHS 几何参数: 63 扇区/磁道, 16 磁头
        # LBA → CHS: temp = LBA / 63, C = temp / 16, H = temp % 16, S = LBA % 63 + 1
        def lba_to_chs(lba, max_heads=255):
            """
            LBA 转 CHS, 自动封顶到 CHS 上限 (1023/254/63)

            参数:
                lba: 逻辑块地址
                max_heads: 最大磁头数 (默认 255)
            返回:
                (c, h, s) 封顶后的 CHS 值
            """
            secs_per_track = 63
            temp = lba // secs_per_track
            c = temp // max_heads
            h = temp % max_heads
            s = lba % secs_per_track + 1
            # CHS 上限: 1024 柱面/255 磁头/63 扇区
            if c > 1023:
                c = 1023
                h = 254
                s = 63
            return c, h, s

        start_c, start_h, start_s = lba_to_chs(PART_START_LBA)
        end_lba = PART_START_LBA + part_sectors - 1
        end_c, end_h, end_s = lba_to_chs(end_lba)

        part = bytearray(16)
        part[0] = 0x80               # 活动分区
        part[1] = start_h            # 起始磁头
        part[2] = start_s | ((start_c >> 8) << 6)  # 起始扇区 | 柱面高 2 位
        part[3] = start_c & 0xFF     # 起始柱面低 8 位
        part[4] = 0x0C               # 分区类型: FAT32 LBA
        part[5] = end_h              # 结束磁头
        part[6] = end_s | ((end_c >> 8) << 6)  # 结束扇区 | 柱面高 2 位
        part[7] = end_c & 0xFF       # 结束柱面低 8 位
        struct.pack_into('<I', part, 8, PART_START_LBA)
        struct.pack_into('<I', part, 12, part_sectors)
        mbr[0x1BE:0x1BE + 16] = part
        mbr[510] = 0x55
        mbr[511] = 0xAA

        f.seek(0)
        f.write(mbr)

        # === VBR (分区扇区 0) ===
        print("  写入 VBR...")
        with open(args.vbr, 'rb') as vf:
            vbr_code = vf.read()

        vbr = bytearray(SECTOR_SIZE)
        code_size = min(len(vbr_code), SECTOR_SIZE)
        vbr[:code_size] = vbr_code[:code_size]

        # 更新 BPB 动态参数
        struct.pack_into('<I', vbr, 32, part_sectors)   # bpb_total_secs32
        struct.pack_into('<I', vbr, 36, fat_sectors)    # bpb_fat_size32
        struct.pack_into('<I', vbr, 28, PART_START_LBA) # bpb_hidden_secs
        vbr[510] = 0x55
        vbr[511] = 0xAA

        f.seek(PART_START_LBA * SECTOR_SIZE)
        f.write(vbr)

        # 备份 VBR (分区扇区 6)
        f.seek((PART_START_LBA + 6) * SECTOR_SIZE)
        f.write(vbr)

        # === FSINFO (分区扇区 1) ===
        print("  写入 FSINFO...")
        fsinfo = bytearray(SECTOR_SIZE)
        fsinfo[0] = 0x52
        fsinfo[1] = 0x52
        fsinfo[2] = 0x61
        fsinfo[3] = 0x41
        fsinfo[484] = 0x72
        fsinfo[485] = 0x72
        fsinfo[486] = 0x41
        fsinfo[487] = 0x61
        struct.pack_into('<I', fsinfo, 488, total_clusters - 1)
        struct.pack_into('<I', fsinfo, 492, 3)
        fsinfo[510] = 0x55
        fsinfo[511] = 0xAA

        f.seek((PART_START_LBA + 1) * SECTOR_SIZE)
        f.write(fsinfo)

        # === FAT 表 (第一个扇区) ===
        print("  写入 FAT 表...")
        fat_sec = bytearray(SECTOR_SIZE)
        # FAT[0] = 0x0FFFFFF8 (媒体类型)
        fat_sec[0] = 0xF8
        fat_sec[1] = 0xFF
        fat_sec[2] = 0xFF
        fat_sec[3] = 0x0F
        # FAT[1] = 0x0FFFFFFF (结束标记)
        fat_sec[4] = 0xFF
        fat_sec[5] = 0xFF
        fat_sec[6] = 0xFF
        fat_sec[7] = 0x0F
        # FAT[2] = 0x0FFFFFFF (根目录结束标记)
        fat_sec[8] = 0xFF
        fat_sec[9] = 0xFF
        fat_sec[10] = 0xFF
        fat_sec[11] = 0x0F

        fat1_offset = (PART_START_LBA + FAT32_RESERVED_SECS) * SECTOR_SIZE
        fat2_offset = (PART_START_LBA + FAT32_RESERVED_SECS +
                       fat_sectors) * SECTOR_SIZE

        f.seek(fat1_offset)
        f.write(fat_sec)
        f.seek(fat2_offset)
        f.write(fat_sec)

        # === 根目录 (簇 2) ===
        print("  写入根目录...")
        label_entry = bytearray(32)
        label = args.volume_label.ljust(11)[:11]
        for i in range(11):
            label_entry[i] = ord(label[i])
        label_entry[11] = 0x08  # 卷标属性

        f.seek(data_lba * SECTOR_SIZE)
        f.write(label_entry)

        # === 写入文件 ===
        print(f"\n[3/3] 写入文件...")
        cluster_size = FAT32_SECS_PER_CLUST * SECTOR_SIZE
        next_cluster = 3  # 簇 2 是根目录
        dir_index = 1     # 跳过卷标目录项 (索引 0)

        for filename, file_data in all_files:
            if not file_data:
                continue

            num_clusters = (len(file_data) + cluster_size - 1) // cluster_size
            first_cluster = next_cluster

            # 分配簇链并写入数据
            for i in range(num_clusters):
                cluster = next_cluster
                next_cluster += 1

                # 写入 FAT 条目
                fat_entry_pos = fat1_offset + cluster * 4
                if i < num_clusters - 1:
                    fat_entry = struct.pack('<I', cluster + 1)
                else:
                    fat_entry = struct.pack('<I', 0x0FFFFFFF)
                f.seek(fat_entry_pos)
                f.write(fat_entry)

                # 写入文件数据
                data_pos = (data_lba + (cluster - 2) *
                            FAT32_SECS_PER_CLUST) * SECTOR_SIZE
                chunk_start = i * cluster_size
                chunk_end = min(chunk_start + cluster_size, len(file_data))
                chunk = file_data[chunk_start:chunk_end]

                if len(chunk) < cluster_size:
                    chunk = chunk + bytes(cluster_size - len(chunk))

                f.seek(data_pos)
                f.write(chunk)

            # 创建目录项
            dir_entry = bytearray(32)
            dir_entry[0:11] = name_to_83(filename)
            dir_entry[11] = 0x20  # 存档属性
            struct.pack_into('<H', dir_entry, 20,
                             (first_cluster >> 16) & 0xFFFF)
            struct.pack_into('<H', dir_entry, 26,
                             first_cluster & 0xFFFF)
            struct.pack_into('<I', dir_entry, 28, len(file_data))

            # 写入根目录
            dir_pos = data_lba * SECTOR_SIZE + dir_index * 32
            f.seek(dir_pos)
            f.write(dir_entry)

            dir_index += 1
            print(f"  {filename}: 簇 {first_cluster}-{next_cluster - 1}, "
                  f"{len(file_data):,} 字节")

    # 转换为 VMDK
    print(f"\n{'=' * 50}")
    print(f"转换为 VMDK...")
    vmdk_path = args.output
    if not vmdk_path.endswith('.vmdk'):
        vmdk_path += '.vmdk'

    convert_to_vmdk(raw_path, vmdk_path)

    # 清理原始镜像
    if not args.keep_raw:
        os.unlink(raw_path)
        print(f"  已删除原始镜像: {raw_path}")
    else:
        print(f"  保留原始镜像: {raw_path}")

    # 验证输出
    vmdk_size = os.path.getsize(vmdk_path)
    print(f"\n{'=' * 50}")
    print(f"VMDK 镜像已创建: {vmdk_path}")
    print(f"  大小: {vmdk_size:,} 字节 ({vmdk_size / 1024 / 1024:.1f} MB)")
    print(f"  磁盘容量: {disk_size / 1024 / 1024 / 1024:.1f} GB")
    print(f"  文件系统: FAT32")
    print(f"  文件数: {len(all_files)}")
    print(f"\n测试命令:")
    print(f"  qemu-system-i386 -hda {vmdk_path} -boot c -m 64M -serial stdio")


if __name__ == '__main__':
    main()
