"""
Nexsteaduser — PlexsDOS
VBR 二进制修复工具
作者: Tinmc189623 | 团队: Nexlyh

确保 VBR 二进制为 512 字节，引导签名 0xAA55 在字节 510-511。
"""

import sys

def main():
    path = sys.argv[1]
    with open(path, 'rb') as f:
        data = bytearray(f.read())
    data = data[:510]
    data += b'\x55\xAA'
    if len(data) < 512:
        data += bytes(512 - len(data))
    with open(path, 'wb') as f:
        f.write(data)
    print(f"[fix_vbr] {path}: {len(data)} bytes, signature at 510-511")

if __name__ == '__main__':
    main()
