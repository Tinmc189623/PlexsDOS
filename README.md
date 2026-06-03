# Nexsteaduser — PlexsDOS

作者: Tinmc189623 | 团队: Nexlyh

---

PlexsDOS 是一个面向 x86 32-bit 保护模式的类 DOS 操作系统，采用自研内核。系统支持从软盘、硬盘或 ISO 9660 光盘启动，提供命令行 Shell 环境和可选的 GUI 桌面环境，支持 FAT12/FAT32/ISO 9660 文件系统、ATA PIO/AHCI SATA/ATAPI CD-ROM/FDC 多种存储介质，以及 .comx 格式外部程序加载与执行。

## 快速开始

### 前置条件

- MSYS2 环境 (Windows) 或 Linux
- GCC + GAS + LD (32-bit 目标, mingw32)
- QEMU (用于测试)
- Python 3 + pyfatfs (用于创建磁盘镜像)

### 构建

```bash
# 1. 克隆项目
git clone <repo-url> PlexsDOS
cd PlexsDOS

# 2. 完整构建 (软盘 + 硬盘 + ISO + 安装盘)
make clean && make

# 3. QEMU 测试 (软盘 + 硬盘)
make run
```

### QEMU 测试

```bash
# 完整测试 (软盘 + 硬盘)
make run

# 仅软盘
make run-floppy

# ISO 光盘启动 + 硬盘
make run-iso

# VMDK 虚拟硬盘启动
make run-vmdk
```

## 项目结构

```
PlexsDOS/
├── Makefile                  # 构建系统 (软盘/硬盘/ISO/VMDK/安装盘)
├── linker.ld                 # 链接器脚本 (内核 0x30000, BSS 0x200000)
├── boot/                     # 引导扇区
│   ├── boot_sector.S         # 软盘引导 (16-bit → 32-bit PM)
│   ├── hd_mbr.S              # 硬盘 MBR
│   └── hd_vbr.S              # 硬盘 VBR
├── kernel/                   # 内核
│   ├── kernel_entry.S        # 内核入口
│   ├── kernel_main.c         # 内核主函数
│   ├── hd_boot.S             # HDD 启动代码
│   ├── loader.c              # .comx 程序加载器
│   ├── installer.c           # 多盘安装程序
│   ├── arch/                 # 架构相关
│   │   ├── cpu.c             # CPU 检测 (CPUID, SSE/AVX)
│   │   ├── gdt.c             # GDT (Ring 0-3 + TSS)
│   │   ├── idt.c             # IDT + 8259A PIC
│   │   ├── interrupt.S       # 中断汇编入口
│   │   ├── interrupt_mgr.cpp # C++ 中断管理器
│   │   ├── panic.c           # 红屏 (RSOD)
│   │   └── syscall.c         # INT 21h 系统调用
│   ├── drivers/              # 设备驱动
│   │   ├── ahci.c            # AHCI SATA
│   │   ├── cdrom.c           # ATAPI CD-ROM + ISO 9660
│   │   ├── disk.c            # ATA PIO/DMA
│   │   ├── drive.c           # 驱动器抽象 (A:-E:)
│   │   ├── fdc.c             # FDC 软盘 (马达状态机)
│   │   ├── keyboard.c        # PS/2 键盘
│   │   ├── mouse.c           # PS/2 鼠标
│   │   ├── pci.c             # PCI 总线
│   │   ├── screen.c          # VGA 文本模式
│   │   ├── serial.c          # COM1 串口
│   │   └── vga.c             # VGA 图形模式
│   ├── fs/                   # 文件系统
│   │   ├── fat12.c           # FAT12
│   │   ├── fat32.c           # FAT32
│   │   └── fs.c              # 虚拟文件系统
│   ├── mm/                   # 内存管理
│   │   ├── paging.c          # 分页 (4KB)
│   │   └── pfa.c             # 物理帧分配器
│   ├── shell/                # Shell + 用户
│   │   ├── shell.c           # 命令行 Shell
│   │   └── users.c           # 用户账户
│   ├── sched/                # 调度器
│   │   └── scheduler.c       # 抢占式多任务
│   ├── gui/                  # GUI
│   │   ├── desktop.c         # 桌面环境
│   │   ├── wm.c              # 窗口管理器
│   │   └── widgets.c         # 小部件
│   ├── editor/               # 编辑器
│   │   └── editor.c          # 文本编辑器
│   ├── hal/                  # 硬件抽象层
│   │   ├── hal_init.cpp      # HAL 初始化
│   │   ├── hal_io.c          # HAL I/O
│   │   └── isa.c             # ISA 枚举
│   ├── dm/                   # 显示管理器
│   │   ├── plxdm_*.c         # PlexsDM
│   │   └── lightdm-core/     # LightDM 参考
│   └── shim/                 # 兼容层
├── include/                  # 头文件
│   ├── plexsdos/             # 内核头文件
│   ├── libc/                 # C 标准库头文件
│   ├── libcpp/               # C++ 头文件
│   ├── X11/                  # X11 协议头
│   └── sys/                  # POSIX 兼容头
├── lib/                      # 库函数
│   ├── fast_mem.c            # SSE2/AVX 分派
│   └── string.c              # 字符串库
├── programs/                 # 外部程序
│   ├── test_hello.S          # HELLO.COMX
│   └── pnp.S                 # PNP.COMX
├── tools/                    # 构建工具
│   ├── mkfloppy.py           # FAT12 软盘镜像
│   ├── mkbootdisk.py         # 可引导安装盘
│   ├── mkfat12.py            # FAT12 镜像
│   ├── mkfat32.py            # FAT32 镜像
│   ├── mkcomx.py             # .comx 打包
│   ├── mkiso.py              # ISO 9660 光盘
│   ├── mkvmdk.py             # VMDK 虚拟硬盘
│   └── fix_vbr.py            # VBR 修正
└── docs/                     # 文档
```

## 内核特性

自研 32-bit 保护模式内核，包含:

- **引导**: 软盘/硬盘 MBR+VBR/ISO El Torito/VMDK
- **中断**: IDT + 8259A PIC + C++ 面向对象管理器
- **GDT**: Ring 0-3 代码/数据段 + TSS
- **分页**: 4KB 页, 身份映射, 物理帧分配器
- **红屏 (RSOD)**: 内核恐慌, CPU 异常诊断, 寄存器/栈回溯
- **屏幕**: VGA 文本模式 (80×25, 0xB8000) + 图形模式
- **键盘**: PS/2 扫描码驱动, 环形缓冲区, Shift/Caps
- **鼠标**: PS/2 鼠标, GUI 集成
- **串口**: COM1 调试输出 (115200 baud)
- **存储**: ATA PIO/DMA, AHCI SATA, FDC, ATAPI CD-ROM
- **文件系统**: FAT12, FAT32, ISO 9660
- **驱动器**: 统一的 A:-E: 字母映射
- **Shell**: help/cls/ver/echo/ls/type/run/reboot/mem/users
- **系统调用**: INT 21h (DOS 兼容, 6 个子功能)
- **程序加载**: .comx 格式 (32 字节头部, 校验和, CPU 检查)
- **调度器**: 抢占式多任务, 时间片轮转
- **GUI**: 桌面环境, 窗口管理器, 小部件
- **编辑器**: 内置文本编辑器
- **安装程序**: MS-DOS 风格多盘安装
- **用户系统**: 多用户账户管理
- **HAL**: 硬件抽象层, 块设备框架, ISA 枚举
- **显示管理器**: PlexsDM (LightDM 移植)
- **处理器优化**: CPUID 检测, SSE/AVX 运行时分派

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
| `mem` | 显示内存信息 |
| `users` | 用户管理 |
| `edit <file>` | 文本编辑器 |
| `CDIR` | 列出光盘根目录 |
| `CCAT <file>` | 显示光盘文件 |
| `CDMOUNT` | 挂载光盘 |

## 构建目标

| 目标 | 说明 |
|------|------|
| `make` | 完整构建 (软盘+硬盘+ISO+安装盘) |
| `make disk` | 仅 FAT32 硬盘镜像 |
| `make install-disks` | 5 张安装软盘 |
| `make iso` | ISO 9660 可引导光盘 |
| `make vmdk` | 40GB VMDK 虚拟硬盘 |
| `make run` | QEMU 测试 (软盘+硬盘) |
| `make run-floppy` | QEMU 测试 (仅软盘) |
| `make run-iso` | QEMU 测试 (光盘+硬盘) |
| `make run-vmdk` | QEMU 测试 (VMDK) |
| `make clean` | 清理构建产物 |

## 文档

- [技术规格](docs/superpowers/specs/spec.md)
- [实现计划](docs/superpowers/specs/plan.md)
- [开发检查清单](docs/superpowers/specs/checklist.md)

---

*Nexsteaduser — PlexsDOS*
*作者: Tinmc189623 | 团队: Nexlyh*
