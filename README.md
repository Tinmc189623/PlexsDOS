# Nexsteaduser — PlexsDOS

作者: Tinmc189623 | 团队: Nexlyh

---

PlexsDOS 是一个面向 x86 32-bit 保护模式的类 DOS 操作系统，采用自研内核。

## 快速开始

### 前置条件

- MSYS2 环境 (Windows) 或 Linux
- GCC + GAS + LD (32-bit 目标)
- QEMU (用于测试)
- Python (用于创建 FAT12 磁盘镜像)

### 构建

```bash
# 1. 克隆项目
git clone <repo-url> PlexsDOS
cd PlexsDOS

# 2. 构建 (使用 MSYS2 或 Linux)
make clean && make

# 3. 测试
make run
```

### QEMU 测试

```bash
# 完整测试 (软盘 + 硬盘)
make run

# 仅软盘
make run-floppy
```

## 项目结构

```
PlexsDOS/
├── Makefile                  # 构建系统
├── linker.ld                 # 链接器脚本
├── boot/
│   └── boot_sector.S         # 引导扇区 (16-bit 实模式 → 32-bit PM)
├── kernel/
│   ├── kernel_entry.S        # 内核入口 (32-bit PM)
│   ├── kernel_main.c         # 内核主函数
│   ├── arch/
│   │   ├── interrupt.S       # 中断处理入口
│   │   ├── idt.c             # IDT 管理 + 8259A PIC
│   │   └── syscall.c         # INT 21h 系统调用
│   ├── drivers/
│   │   ├── screen.c          # VGA 文本模式驱动
│   │   ├── keyboard.c        # PS/2 键盘驱动
│   │   └── disk.c            # ATA PIO 磁盘驱动
│   ├── fs/
│   │   └── fat12.c           # FAT12 文件系统
│   └── shell/
│       └── shell.c           # 命令行 Shell
├── include/
│   └── plexsdos/             # 头文件
├── lib/
│   └── string.c              # 字符串库
├── programs/
│   ├── test_hello.S          # 测试程序
│   └── README.TXT            # 测试文本
├── tools/
│   ├── mkfat12.py            # FAT12 镜像创建工具
│   └── test_qemu.sh          # QEMU 测试脚本
├── docs/                     # 文档
├── spec.md                   # 技术规格
├── plan.md                   # 实现计划
├── checklist.md              # 开发检查清单
└── legacy/                   # 旧代码 (参考)
```

## 内核特性

自研 32-bit 保护模式内核，包含：

- **引导**: BIOS → 16-bit 实模式 → GDT → 32-bit 保护模式
- **中断**: IDT + 8259A PIC, IRQ1 键盘中断
- **屏幕**: VGA 文本模式 (80×25, 直写 0xB8000)
- **键盘**: PS/2 扫描码驱动, 环形缓冲区
- **磁盘**: ATA PIO 模式 (端口 0x1F0-0x1F7)
- **文件系统**: FAT12 (根目录读取, 文件加载)
- **Shell**: help/cls/ver/echo/ls/type/run/reboot
- **系统调用**: INT 21h (向量 0x22, DOS 兼容)

## Shell 命令

| 命令 | 功能 |
|------|------|
| `help` | 显示帮助 |
| `cls` | 清屏 |
| `ver` | 版本信息 |
| `echo <text>` | 输出文本 |
| `ls` | 列出磁盘文件 |
| `type <file>` | 显示文件内容 |
| `run <file>` | 加载并执行程序 |
| `reboot` | 重启系统 |

## 文档

- [技术规格](spec.md)
- [实现计划](plan.md)
- [开发检查清单](checklist.md)

---

*Nexsteaduser*
*作者: Tinmc189623 | 团队: Nexlyh*
