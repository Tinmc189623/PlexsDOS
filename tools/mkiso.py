"""
Nexsteaduser — PlexsDOS
ISO 9660 可引导光盘镜像构建工具
作者: Tinmc189623 | 团队: Nexlyh

使用 xorriso (通过 WSL2) 创建 ISO 9660 格式的可引导光盘镜像。
采用 El Torito 引导, 现有引导代码无需修改。

引导流程:
  BIOS → El Torito → 引导扇区 (INT 13h) → 内核 → 保护模式
  → ATAPI CD-ROM 驱动 → ISO 9660 文件系统 → 安装程序读取文件

ISO 目录结构:
  /BOOT.IMG          — El Torito 引导映像 (1.44MB FAT12, 含 boot.bin + kernel.bin)
  /KERNEL.BIN        — 内核二进制
  /PROGRAMS/         — .comx 程序
  /DOCS/             — 文档
  /DRIVERS/          — 驱动 (预留)

用法:
  python tools/mkiso.py <output.iso> <boot.bin> <kernel.bin> [file1 file2 ...]

选项:
  --volume-label=LABEL   卷标 (默认 "PLXSDOS")
  --system-id=ID         系统标识 (默认 "PLEXSDOS")
  --no-el-torito         不创建 El Torito 引导 (仅数据光盘)

依赖: xorriso (通过 WSL2 调用)
"""

import argparse
import os
import shutil
import struct
import subprocess
import sys
import tempfile


# ==================== 常量 ====================

# 1.44MB 软盘参数
BOOT_IMG_SIZE = 2880 * 512         # 1,474,560 字节 = 1.44MB
SECTOR_SIZE = 512
SECTORS_PER_CLUSTER = 1
NUM_FATS = 2
ROOT_ENTRIES = 224
TOTAL_SECTORS = 2880
SECTORS_PER_FAT = 9
MEDIA_DESCRIPTOR = 0xF0

# 引导扇区 KSEC 标记 (用于修补 KERNEL_SECTORS)
KSEC_MARKER = b'KSEC'

# ISO 9660 默认参数
DEFAULT_VOLUME_LABEL = "PLXSDOS"
DEFAULT_SYSTEM_ID = "PLEXSDOS"

# xorriso 可执行路径 (通过 WSL2 调用)
XORRISO_CMD = ["wsl", "-e", "xorriso"]


# ==================== FAT12 引导映像 ====================

def patch_kernel_sectors(boot_data: bytes, kernel_sectors: int) -> bytes:
    """
    修补引导扇区中的 KERNEL_SECTORS 值

    在 boot_sector.S 数据段中搜索 "KSEC" 标记,
    标记后紧跟的 2 字节即为 sectors_left 变量。
    将其修补为实际内核扇区数。

    参数:
        boot_data: 引导扇区二进制数据
        kernel_sectors: 实际内核扇区数
    返回:
        修补后的引导扇区数据
    """
    boot = bytearray(boot_data)
    marker_pos = boot.find(KSEC_MARKER)
    if marker_pos < 0:
        print("警告: 引导扇区中未找到 KSEC 标记, 跳过修补")
        return bytes(boot)

    sectors_left_offset = marker_pos + len(KSEC_MARKER)
    struct.pack_into('<H', boot, sectors_left_offset, kernel_sectors)

    # 修补 movw $KERNEL_SECTORS, (sectors_left) 指令中的立即数
    sl_addr = 0x7C00 + sectors_left_offset
    sl_addr_bytes = struct.pack('<H', sl_addr)
    pattern = bytes([0xC7, 0x06]) + sl_addr_bytes
    instr_pos = boot.find(pattern)
    if instr_pos >= 0:
        imm_offset = instr_pos + len(pattern)
        struct.pack_into('<H', boot, imm_offset, kernel_sectors)
        print(f"  修补 movw 立即数 @ 0x{imm_offset:04X} = {kernel_sectors}")

    return bytes(boot)


def create_fat12_table() -> bytearray:
    """
    创建 FAT12 表 (保留介质描述符和结束标记)

    返回:
        初始化的 FAT12 表字节数组
    """
    fat = bytearray(SECTORS_PER_FAT * SECTOR_SIZE)
    fat[0] = MEDIA_DESCRIPTOR
    fat[1] = 0xFF
    fat[2] = 0xFF
    return fat


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


def get_fat12_entry(fat: bytearray, cluster: int) -> int:
    """
    读取 FAT12 条目

    参数:
        fat: FAT 表
        cluster: 簇号
    返回:
        FAT 条目值
    """
    offset = cluster + (cluster // 2)
    if cluster & 1:
        return ((fat[offset] >> 4) | (fat[offset + 1] << 4)) & 0xFFF
    else:
        return (fat[offset] | ((fat[offset + 1] & 0x0F) << 8)) & 0xFFF


def set_fat12_entry(fat: bytearray, cluster: int, value: int) -> None:
    """
    写入 FAT12 条目

    参数:
        fat: FAT 表
        cluster: 簇号
        value: 要写入的值
    """
    offset = cluster + (cluster // 2)
    if cluster & 1:
        fat[offset] = (fat[offset] & 0x0F) | ((value & 0x0F) << 4)
        fat[offset + 1] = (value >> 4) & 0xFF
    else:
        fat[offset] = value & 0xFF
        fat[offset + 1] = (fat[offset + 1] & 0xF0) | ((value >> 8) & 0x0F)


def allocate_cluster(fat: bytearray, prev_cluster: int) -> int:
    """
    在 FAT 表中分配一个新簇

    参数:
        fat: FAT 表
        prev_cluster: 前一个簇号 (用于建立簇链, 0 表示首簇)
    返回:
        新分配的簇号, 0 表示空间不足
    """
    for cluster in range(2, 4084):
        if get_fat12_entry(fat, cluster) == 0:
            if prev_cluster > 0:
                set_fat12_entry(fat, prev_cluster, cluster)
            return cluster
    return 0


def create_dir_entry(name: str, cluster: int, size: int,
                     attr: int = 0x20) -> bytearray:
    """
    创建 FAT12 32 字节目录项

    参数:
        name: 文件名
        cluster: 起始簇号
        size: 文件大小 (字节)
        attr: 文件属性 (默认 0x20 = 存档)
    返回:
        32 字节目录项
    """
    entry = bytearray(32)
    entry[0:11] = name_to_83(name)
    entry[11] = attr
    struct.pack_into('<H', entry, 26, cluster)
    struct.pack_into('<I', entry, 28, size)
    return entry


def create_boot_sector(boot_bin: bytes, reserved_sectors: int) -> bytearray:
    """
    创建 FAT12 引导扇区 (重建 BPB)

    与 mkbootdisk.py 相同的逻辑:
    1. 复制 JMP 指令 (字节 0-2)
    2. 重建 BPB (字节 3-61) — 确保参数正确
    3. 复制引导代码 (字节 62+)
    4. 设置引导签名 0x55AA

    参数:
        boot_bin: 原始引导扇区二进制数据 (512 字节)
        reserved_sectors: 保留扇区数 (1 + kernel_sectors)
    返回:
        重建后的引导扇区
    """
    bs = bytearray(SECTOR_SIZE)

    # 复制 JMP 指令 (字节 0-2)
    if len(boot_bin) >= 3:
        bs[0] = boot_bin[0]  # EB (JMP short)
        bs[1] = boot_bin[1]  # offset
        bs[2] = boot_bin[2]  # 90 (NOP)

    # 重建 BPB (与 mkbootdisk.py 一致)
    bs[3:11] = b'PLXSDOS '                          # OEM 名称
    struct.pack_into('<H', bs, 11, 512)              # 每扇区字节数
    struct.pack_into('<B', bs, 13, 1)                # 每簇扇区数
    struct.pack_into('<H', bs, 14, reserved_sectors) # 保留扇区数
    struct.pack_into('<B', bs, 16, 2)                # FAT 表数量
    struct.pack_into('<H', bs, 17, 224)              # 根目录项数
    struct.pack_into('<H', bs, 19, TOTAL_SECTORS)    # 总扇区数
    struct.pack_into('<B', bs, 21, 0xF0)             # 介质描述符
    struct.pack_into('<H', bs, 22, 9)                # 每 FAT 扇区数
    struct.pack_into('<H', bs, 24, 18)               # 每磁道扇区数
    struct.pack_into('<H', bs, 26, 2)                # 磁头数
    struct.pack_into('<I', bs, 28, 0)                # 隐藏扇区数

    # 扩展引导记录
    bs[36] = 0x00                                    # 驱动器号 (软盘)
    bs[38] = 0x29                                    # 扩展引导签名
    struct.pack_into('<I', bs, 39, 0x12345678)       # 卷序列号
    bs[43:54] = b'PLXSDOS    '                       # 卷标 (11 字节)
    bs[54:62] = b'FAT12   '                          # 文件系统类型

    # 复制原始引导代码 (从偏移 62 开始)
    if len(boot_bin) > 62:
        code_end = min(len(boot_bin), 510)
        bs[62:code_end] = boot_bin[62:code_end]

    # 引导签名
    struct.pack_into('<H', bs, 510, 0xAA55)

    return bs


def create_boot_image(boot_bin: bytes, kernel_bin: bytes) -> bytes:
    """
    创建 1.44MB FAT12 引导映像

    布局:
      扇区 0: 引导扇区 (重建 BPB + 修补 KERNEL_SECTORS)
      扇区 1-N: 内核 (kernel.bin)

    与 mkbootdisk.py 使用相同的引导扇区构建逻辑,
    确保 BPB 参数与 1.44MB 软盘几何参数一致。

    参数:
        boot_bin: 引导扇区二进制数据 (512 字节)
        kernel_bin: 内核二进制数据
    返回:
        1.44MB 引导映像数据
    """
    if len(boot_bin) != SECTOR_SIZE:
        print(f"错误: 引导扇区必须为 {SECTOR_SIZE} 字节, 实际 {len(boot_bin)} 字节")
        sys.exit(1)

    if boot_bin[510] != 0x55 or boot_bin[511] != 0xAA:
        print("错误: 引导扇区缺少 0x55AA 签名")
        sys.exit(1)

    kernel_sectors = (len(kernel_bin) + SECTOR_SIZE - 1) // SECTOR_SIZE
    reserved_sectors = 1 + kernel_sectors

    print(f"  引导扇区: {len(boot_bin)} 字节")
    print(f"  内核: {len(kernel_bin)} 字节 ({kernel_sectors} 扇区)")
    print(f"  保留扇区: {reserved_sectors}")

    # 重建引导扇区 (与 mkbootdisk.py 一致)
    boot_data = create_boot_sector(boot_bin, reserved_sectors)

    # 修补 KERNEL_SECTORS
    boot_data = bytearray(patch_kernel_sectors(bytes(boot_data), kernel_sectors))
    print(f"  修补 KERNEL_SECTORS = {kernel_sectors}")

    image = bytearray(BOOT_IMG_SIZE)
    image[0:SECTOR_SIZE] = boot_data

    for i in range(kernel_sectors):
        offset = (1 + i) * SECTOR_SIZE
        chunk = kernel_bin[i * SECTOR_SIZE:(i + 1) * SECTOR_SIZE]
        if len(chunk) < SECTOR_SIZE:
            chunk = chunk + bytes(SECTOR_SIZE - len(chunk))
        image[offset:offset + SECTOR_SIZE] = chunk

    return bytes(image)


# ==================== ISO 9660 构建 (xorriso) ====================

def prepare_staging_dir(files: list[str]) -> str:
    """
    准备 ISO 构建的临时暂存目录

    将文件按类型分类复制到暂存目录结构中:
      PROGRAMS/  — .comx, .bin 程序
      DOCS/      — .txt, .md 文档
      DRIVERS/   — .drv, .sys 驱动
      LIB/       — .lib, .a 库

    参数:
        files: 源文件路径列表
    返回:
        暂存目录路径 (调用方负责清理)
    """
    staging = tempfile.mkdtemp(prefix='mkiso_')

    # 创建子目录
    for subdir in ['PROGRAMS', 'DOCS', 'DRIVERS', 'LIB']:
        os.makedirs(os.path.join(staging, subdir), exist_ok=True)

    for filepath in files:
        if not os.path.exists(filepath):
            print(f"  警告: 文件不存在, 跳过: {filepath}")
            continue

        basename = os.path.basename(filepath).upper()

        # 按扩展名分类
        if basename.endswith('.COMX') or basename.endswith('.BIN'):
            dest_dir = 'PROGRAMS'
        elif basename.endswith('.TXT') or basename.endswith('.MD') or \
             basename.endswith('.DOC'):
            dest_dir = 'DOCS'
        elif basename.endswith('.DRV') or basename.endswith('.SYS'):
            dest_dir = 'DRIVERS'
        elif basename.endswith('.LIB') or basename.endswith('.A'):
            dest_dir = 'LIB'
        else:
            dest_dir = None

        if dest_dir:
            dest = os.path.join(staging, dest_dir, basename)
        else:
            dest = os.path.join(staging, basename)

        shutil.copy2(filepath, dest)
        print(f"  {dest_dir or '.'}/{basename}")

    return staging


def create_iso_xorriso(output_path: str, boot_img_path: str,
                       staging_dir: str, volume_label: str,
                       system_id: str, enable_el_torito: bool) -> None:
    """
    使用 xorriso 创建 ISO 9660 可引导光盘镜像

    xorriso 通过 WSL2 调用, 正确处理 El Torito 引导目录。

    参数:
        output_path: 输出 ISO 文件路径
        boot_img_path: El Torito 引导映像路径 (绝对路径)
        staging_dir: 暂存目录路径 (ISO 内容来源)
        volume_label: 卷标
        system_id: 系统标识
        enable_el_torito: 是否启用 El Torito 引导
    """
    # 将 Windows 路径转换为 WSL 路径
    def win_to_wsl(path: str) -> str:
        """将 Windows 路径转换为 WSL 路径"""
        path = os.path.abspath(path)
        # J:\APP\OS\x86 → /mnt/j/APP/OS/x86
        if len(path) >= 2 and path[1] == ':':
            drive = path[0].lower()
            rest = path[2:].replace('\\', '/')
            return f"/mnt/{drive}{rest}"
        return path.replace('\\', '/')

    wsl_boot = win_to_wsl(boot_img_path)
    wsl_staging = win_to_wsl(staging_dir)
    wsl_output = win_to_wsl(output_path)

    # 构建 xorriso 命令 (-as mkisofs 模式)
    # 注意: -as mkisofs 默认会写入 boot-info-table (偏移 8-63, 56 字节),
    # 覆盖 boot_sector.S 中 boot_init (偏移 62) 的 cli 指令。
    # 解决方案: 构建后用 Python 二进制修补还原正确的引导映像。
    cmd = XORRISO_CMD + ["-as", "mkisofs"]

    # ISO 9660 + Rock Ridge + Joliet
    cmd += ["-R", "-J"]

    # 卷标和系统标识
    cmd += ["-V", volume_label]
    cmd += ["-A", system_id]

    # El Torito 引导 (引导映像必须在暂存目录内, 使用相对路径)
    # 使用 1.44MB 软盘仿真模式 (非 no-emulation):
    #   - BIOS 将引导映像仿真为 1.44MB 软盘
    #   - INT 13h 以标准软盘 CHS 几何参数 (80/2/18) 提供访问
    #   - 引导代码使用 INT 13h 读取内核, 与软盘启动完全一致
    if enable_el_torito:
        cmd += ["-b", "BOOT.IMG"]
        # 不使用 -no-emul-boot, 让 BIOS 使用软盘仿真
        cmd += ["-boot-load-size", "2880"]

    # 输出文件
    cmd += ["-o", wsl_output]

    # 输入目录
    cmd += [wsl_staging]

    print(f"  xorriso 命令: {' '.join(cmd)}")

    # 执行 xorriso
    result = subprocess.run(cmd, capture_output=True, text=True, timeout=60)

    if result.returncode != 0:
        print(f"  xorriso 错误:\n{result.stderr}")
        sys.exit(1)

    if result.stderr:
        # xorriso 输出信息到 stderr
        for line in result.stderr.strip().split('\n'):
            if line.strip():
                print(f"  xorriso: {line.strip()}")


def fix_boot_image(iso_path: str, boot_img_data: bytes) -> None:
    """
    修复 ISO 中被 xorriso boot-info-table 损坏的引导映像

    xorriso 的 -as mkisofs 模式默认在引导映像偏移 8-63 写入 56 字节的
    boot-info-table, 覆盖了 boot_sector.S 中 boot_init (偏移 62) 的 cli 指令,
    导致引导代码损坏。

    本函数通过 El Torito Boot Record Volume Descriptor 找到启动目录,
    xorriso 总是将引导映像紧放在启动目录之后 (catalog_lba + 1),
    因此无需解析 Initial Entry 中的 LBA 字段 (xorriso 该字段为 0)。

    参数:
        iso_path: ISO 文件路径
        boot_img_data: 原始引导映像数据 (未被 boot-info-table 修改)
    """
    with open(iso_path, 'r+b') as f:
        iso_data = bytearray(f.read())

        # 从 Boot Record Volume Descriptor (sector 17) 获取启动目录 LBA
        brvd_offset = 17 * 2048
        if iso_data[brvd_offset] != 0x00 or \
           iso_data[brvd_offset + 1:brvd_offset + 6] != b'CD001':
            print("  警告: 未找到 Boot Record Volume Descriptor, 跳过修补")
            return

        catalog_lba = struct.unpack_from('<I', iso_data, brvd_offset + 0x47)[0]
        print(f"  启动目录: LBA {catalog_lba}")

        # xorriso 总是将引导映像紧放在启动目录之后
        boot_lba = catalog_lba + 1
        print(f"  引导映像: LBA {boot_lba}")

        # 计算引导映像在 ISO 中的字节偏移
        img_offset = boot_lba * 2048

        # 用原始引导映像数据覆盖 ISO 中被 boot-info-table 修改的区域
        # floppy emulation 模式下 sector_count=1, 但实际映像为 1.44MB
        # BIOS 通过 INT 13h 访问完整映像, 因此必须还原整个映像
        img_size = len(boot_img_data)
        if img_offset + img_size > len(iso_data):
            img_size = len(iso_data) - img_offset
        iso_data[img_offset:img_offset + img_size] = boot_img_data[:img_size]

        # 写回 ISO
        f.seek(0)
        f.write(iso_data)

    print("  引导映像修补完成 (已还原 boot-info-table 损坏区域)")


def main() -> None:
    """
    ISO 构建工具入口

    解析命令行参数, 创建引导映像, 构建 ISO 9660 光盘镜像。
    """
    parser = argparse.ArgumentParser(
        description='Nexsteaduser PlexsDOS — ISO 9660 可引导光盘镜像构建工具',
        formatter_class=argparse.RawDescriptionHelpFormatter,
        epilog='示例:\n'
               '  python tools/mkiso.py build/plexsdos.iso --boot-bin build/boot.bin '
               '--kernel-bin build/kernel.bin build/programs/HELLO.COMX programs/README.TXT\n'
               '  python tools/mkiso.py build/plexsdos.iso --boot-img build/disk1.img '
               'build/programs/HELLO.COMX programs/README.TXT\n'
    )

    parser.add_argument('output', help='输出 ISO 文件路径')
    parser.add_argument('files', nargs='*', help='要添加到 ISO 的文件')
    parser.add_argument('--boot-bin', default=None,
                        help='引导扇区二进制文件 (512 字节)')
    parser.add_argument('--kernel-bin', default=None,
                        help='内核二进制文件')
    parser.add_argument('--boot-img', default=None,
                        help='预构建的引导映像 (如 disk1.img), 跳过 boot_bin + kernel_bin')
    parser.add_argument('--volume-label', default=DEFAULT_VOLUME_LABEL,
                        help=f'卷标 (默认: {DEFAULT_VOLUME_LABEL})')
    parser.add_argument('--system-id', default=DEFAULT_SYSTEM_ID,
                        help=f'系统标识 (默认: {DEFAULT_SYSTEM_ID})')
    parser.add_argument('--no-el-torito', action='store_true',
                        help='不创建 El Torito 引导 (仅数据光盘)')

    args = parser.parse_args()

    print("Nexsteaduser PlexsDOS — ISO 构建工具")
    print("=" * 50)

    # 确定引导映像来源
    if args.boot_img:
        # 模式 1: 使用预构建的引导映像 (如 mkbootdisk.py 创建的 disk1.img)
        if not os.path.exists(args.boot_img):
            print(f"错误: 文件不存在: {args.boot_img}")
            sys.exit(1)
        with open(args.boot_img, 'rb') as f:
            boot_img = f.read()
        print(f"\n[1/1] 使用预构建引导映像: {args.boot_img} ({len(boot_img)} 字节)")
    else:
        # 模式 2: 从 boot.bin + kernel.bin 创建引导映像
        if not args.boot_bin or not args.kernel_bin:
            print("错误: 必须提供 boot_bin 和 kernel_bin, 或使用 --boot-img")
            sys.exit(1)
        for path in [args.boot_bin, args.kernel_bin]:
            if not os.path.exists(path):
                print(f"错误: 文件不存在: {path}")
                sys.exit(1)

        with open(args.boot_bin, 'rb') as f:
            boot_bin = f.read()
        with open(args.kernel_bin, 'rb') as f:
            kernel_bin = f.read()

        print("\n[1/3] 创建 FAT12 引导映像 (1.44MB)...")
        boot_img = create_boot_image(boot_bin, kernel_bin)

    # 写入临时引导映像文件
    boot_img_fd, boot_img_path = tempfile.mkstemp(suffix='.img')
    staging_dir = None
    try:
        os.write(boot_img_fd, boot_img)
        os.close(boot_img_fd)

        print(f"\n[2/3] 引导映像: {boot_img_path} ({len(boot_img)} 字节)")

        # 准备暂存目录
        print(f"\n[3/3] 构建 ISO 9660 镜像 (xorriso)...")
        print(f"  卷标: {args.volume_label}")
        print(f"  系统标识: {args.system_id}")
        print(f"  El Torito: {'启用' if not args.no_el_torito else '禁用'}")
        print(f"  文件数: {len(args.files)}")

        staging_dir = prepare_staging_dir(args.files)

        # 将引导映像复制到暂存目录
        shutil.copy2(boot_img_path, os.path.join(staging_dir, 'BOOT.IMG'))

        # 确保输出目录存在
        os.makedirs(os.path.dirname(os.path.abspath(args.output)), exist_ok=True)

        # 使用 xorriso 构建 ISO
        create_iso_xorriso(args.output, boot_img_path, staging_dir,
                           args.volume_label, args.system_id,
                           not args.no_el_torito)

        # 修复被 xorriso boot-info-table 损坏的引导映像
        # xorriso -as mkisofs 默认写入 boot-info-table (偏移 8-63),
        # 覆盖了 boot_init (偏移 62) 的 cli 指令, 必须还原。
        if not args.no_el_torito:
            print("\n[修补] 修复被 boot-info-table 损坏的引导映像...")
            fix_boot_image(args.output, boot_img)

        # 验证输出
        iso_size = os.path.getsize(args.output)
        print(f"\n{'=' * 50}")
        print(f"ISO 镜像已创建: {args.output}")
        print(f"  大小: {iso_size:,} 字节 ({iso_size / 1024 / 1024:.1f} MB)")
        print(f"  引导: El Torito 1.44MB" if not args.no_el_torito
              else "  引导: 无 (数据光盘)")
        print(f"\n测试命令:")
        print(f"  qemu-system-i386 -cdrom {args.output} -boot d -m 64M")

    finally:
        if os.path.exists(boot_img_path):
            os.unlink(boot_img_path)
        if staging_dir and os.path.exists(staging_dir):
            shutil.rmtree(staging_dir)


if __name__ == '__main__':
    main()
