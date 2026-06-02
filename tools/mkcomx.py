"""
Nexsteaduser — PlexsDOS
.comx 可执行文件打包工具
作者: Tinmc189623 | 团队: Nexlyh

将 flat binary 转换为 .comx 格式 (添加 32 字节头部)。

.comx 头部结构 (32 字节):
  0x00  4  魔数 "CPX\x00" (0x43505800)
  0x04  1  版本 (0x01)
  0x05  1  标志位 (bit0=SSE, bit1=SSE2, bit2=MMX)
  0x06  2  保留
  0x08  4  入口点偏移
  0x0C  4  代码大小
  0x10  4  BSS 大小
  0x14  4  加载地址
  0x18  4  校验和
  0x1C  4  保留

用法: python tools/mkcomx.py <input.bin> <output.comx> [options]
  --entry=OFFSET    入口点偏移 (默认 0)
  --bss=SIZE        BSS 段大小 (默认 0)
  --load-addr=ADDR  加载地址 (默认 0x50000)
  --flags=FLAGS     标志位 (默认 0)
"""

import struct
import sys


# .comx 常量
COMX_MAGIC = 0x43505800  # "CPX\x00"
COMX_VERSION = 0x01
COMX_HEADER_SIZE = 32


def compute_checksum(data):
    """
    计算代码段校验和 (与内核 loader_checksum 一致)

    参数:
        data: 代码数据
    返回:
        32 位校验和
    """
    checksum = 0
    for b in data:
        checksum = ((checksum << 3) | (checksum >> 29)) ^ b
        checksum &= 0xFFFFFFFF
    return checksum


def create_comx(input_path, output_path, entry=0, bss_size=0,
                load_addr=0x50000, flags=0):
    """
    创建 .comx 文件

    参数:
        input_path: 输入 flat binary 路径
        output_path: 输出 .comx 文件路径
        entry: 入口点偏移
        bss_size: BSS 段大小
        load_addr: 加载地址
        flags: 标志位
    """
    with open(input_path, 'rb') as f:
        code_data = f.read()

    code_size = len(code_data)
    if code_size == 0:
        print("Error: input file is empty")
        sys.exit(1)

    if code_size > 256 * 1024:
        print(f"Error: code too large ({code_size} bytes, max 256KB)")
        sys.exit(1)

    # 计算校验和
    checksum = compute_checksum(code_data)

    # 构建头部 (32 字节)
    header = struct.pack('<I B B H I I I I I I',
        COMX_MAGIC,         # 0x00: 魔数
        COMX_VERSION,       # 0x04: 版本
        flags,              # 0x05: 标志位
        0,                  # 0x06: 保留
        entry,              # 0x08: 入口点偏移
        code_size,          # 0x0C: 代码大小
        bss_size,           # 0x10: BSS 大小
        load_addr,          # 0x14: 加载地址
        checksum,           # 0x18: 校验和
        0,                  # 0x1C: 保留
    )

    # 写入 .comx 文件
    with open(output_path, 'wb') as f:
        f.write(header)
        f.write(code_data)

    print(f"Created: {output_path}")
    print(f"  Magic:    0x{COMX_MAGIC:08X}")
    print(f"  Version:  {COMX_VERSION}")
    print(f"  Flags:    0x{flags:02X}")
    print(f"  Entry:    0x{entry:X}")
    print(f"  Code:     {code_size} bytes")
    print(f"  BSS:      {bss_size} bytes")
    print(f"  Load:     0x{load_addr:X}")
    print(f"  Checksum: 0x{checksum:08X}")


def main():
    if len(sys.argv) < 3:
        print("Usage: python tools/mkcomx.py <input.bin> <output.comx> [options]")
        print("Options:")
        print("  --entry=OFFSET    Entry point offset (default: 0)")
        print("  --bss=SIZE        BSS size (default: 0)")
        print("  --load-addr=ADDR  Load address (default: 0x50000)")
        print("  --flags=FLAGS     Flags (default: 0)")
        sys.exit(1)

    input_path = sys.argv[1]
    output_path = sys.argv[2]

    # 解析选项
    entry = 0
    bss_size = 0
    load_addr = 0x50000
    flags = 0

    for arg in sys.argv[3:]:
        if arg.startswith('--entry='):
            entry = int(arg.split('=', 1)[1], 0)
        elif arg.startswith('--bss='):
            bss_size = int(arg.split('=', 1)[1], 0)
        elif arg.startswith('--load-addr='):
            load_addr = int(arg.split('=', 1)[1], 0)
        elif arg.startswith('--flags='):
            flags = int(arg.split('=', 1)[1], 0)
        else:
            print(f"Unknown option: {arg}")
            sys.exit(1)

    create_comx(input_path, output_path, entry, bss_size, load_addr, flags)


if __name__ == '__main__':
    main()
