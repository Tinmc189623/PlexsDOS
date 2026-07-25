# Nexsteaduser PlexsDOS

> Nexsteaduser 自研 x86 32 位保护模式宏内核操作系统，使用 C / C++ / NASM 从零构建。

[![License: GPL v2](https://img.shields.io/badge/License-GPL%20v2-blue.svg)](LICENSE)
[![Platform: x86 32-bit](https://img.shields.io/badge/Platform-x86%2032--bit-orange.svg)]()
[![Kernel: Monolithic](https://img.shields.io/badge/Kernel-Monolithic-green.svg)]()
[![Build: GCC + NASM + CMake](https://img.shields.io/badge/Build-GCC%20%2B%20NASM%20%2B%20CMake-lightgrey.svg)]()

---

## 项目简介

**PlexsDOS** 是由 Nexsteaduser 团队（Nexlyh）开发的一个面向 x86 32 位保护模式的**宏内核（Monolithic Kernel）**操作系统。从引导扇区（MBR/VBR）、内核本体、驱动程序、文件系统、Shell 命令行、图形子系统到内置编辑器，全部在内核态（Ring 0）直接运行，无用户态/内核态分离、无 IPC 消息传递、无用户态服务器进程，是一个紧凑、纯粹、单地址空间的传统宏内核实现。

内核命名为 **Plexs**，操作系统命名为 **PlexsDOS**（DOS 仅为命名传统，非 MS-DOS 兼容）。

- **作者**: Tinmc189623
- **团队**: Nexlyh
- **联系**: admin@nexsteaduser.com
- **品牌**: Nexsteaduser

---

## 内核架构

```
┌────────────────────────────────────────────────────────────┐
│                   Plexs 宏内核 (Ring 0)                     │
│  单地址空间 · 所有子系统直接链接进内核二进制                  │
├──────────┬──────────┬──────────┬──────────┬─────────────────┤
│  引导加载 │  内核核心 │  驱动框架 │  文件系统 │   用户接口      │
│  (NASM)  │   (C)    │  (C++)   │   (C)    │    (C/C++)      │
├──────────┼──────────┼──────────┼──────────┼─────────────────┤
│ MBR/VBR  │ GDT/IDT  │ PS/2     │ FAT32    │ Shell 命令行    │
│ Boot S16 │ 分页/中断 │ Keyboard │ ISO 9660 │ EDIT.COM 编辑器 │
│ Boot S32 │ 调度器    │ Mouse    │ DriveMgr │ GUI 图形子系统  │
│ Entry ASM│ Syscall  │ PCI/AHCI │ VFS 抽象  │ 用户/权限管理   │
│ A20/PM   │ HAL 抽象  │ ATA/FDC  │ 路径解析  │ 内置命令集      │
│ Paging   │ String/Mem│ ISA/CMOS │ PartScan │ 用户程序加载    │
│ Serial   │ 异常/恐慌 │ VGA TTY  │ 引导配置  │ 安装程序        │
└──────────┴──────────┴──────────┴──────────┴─────────────────┘
```

### 宏内核特征

- **单地址空间**: 所有代码运行在同一个内核地址空间（Ring 0），无用户态/内核态切换开销。
- **直接函数调用**: 子系统之间通过直接 C 函数调用交互，无 IPC、消息队列或 RPC 机制。
- **静态链接**: 所有驱动、文件系统、Shell、GUI、编辑器均编译链接进 `kernel.bin`，无动态加载（后续可扩展）。
- **高性能**: 系统调用通过软中断 `INT 0x22` 实现，入口即内核函数，零上下文切换开销。
- **紧凑内核**: 内核二进制体积可控，适合学习操作系统原理及嵌入式场景。

---

## 功能特性

### 核心功能

| 子系统 | 状态 | 说明 |
|--------|------|------|
| 引导加载器 (Stage 1/1.5/2) | ✅ 完成 | NASM 编写，MBR → 32 位保护模式切换 |
| GDT/IDT | ✅ 完成 | Ring 0-3 段描述符、256 个中断向量 |
| 分页内存管理 | ✅ 完成 | 4KB 页面、内核页目录 |
| 物理内存管理 | ✅ 完成 | 物理页帧分配器 |
| 异常处理 (Panic) | ✅ 完成 | 寄存器转储 + 红屏死机 |
| PS/2 键盘驱动 | ✅ 完成 | 扫描码 → ASCII 转换、Shift/Caps 处理 |
| PS/2 鼠标驱动 | ✅ 完成 | 字节流解析、光标移动 |
| PCI 总线枚举 | ✅ 完成 | 配置空间扫描、设备识别 |
| AHCI SATA 驱动 | ✅ 完成 | GHC 检测、端口 HBA 初始化、PIO 读写 |
| ATA PIO 驱动 | ✅ 完成 | LBA28 28-bit 寻址、主/从盘检测 |
| 软盘控制器 (FDC) | ✅ 完成 | 720KB/1.44MB、DMA 传输 |
| ISO 9660 (CD-ROM) | ✅ 完成 | 主卷描述符、目录遍历 |
| FAT32 文件系统 | ✅ 完成 | 短文件名/长文件名、簇链读取、目录遍历 |
| MBR 分区扫描 | ✅ 完成 | 4 个主分区、类型识别、自动挂载 |
| 驱动器号管理 | ✅ 完成 | A:/B: 软盘、C: 硬盘、D: 光驱、E:+ 扩展分区 |
| 进程调度器 | ✅ 完成 | 协作式/抢占式调度框架 |
| 用户账户系统 | ✅ 完成 | 用户创建、登录、权限控制 |
| 命令行 Shell | ✅ 完成 | 28+ 内置命令、命令历史、路径补全 |
| EDIT.COM 编辑器 | ✅ 完成 | 全屏文本编辑器、光标移动、保存/退出 |
| 图形子系统 (GUI) | ✅ 完成 | VESA VBE 模式、窗口管理 |
| 安装程序 | ✅ 完成 | 分区选择、文件复制、引导记录写入 |
| HAL 抽象层 | ✅ 完成 | 块设备统一接口，FDC/ATA/AHCI 透明切换 |
| 串口调试输出 | ✅ 完成 | COM1 115200 8N1，实时内核日志 |
| 配置系统 (CONFIG.SYS) | ✅ 完成 | 编译时嵌入、启动时解析 |

### 内置命令

Shell 支持以下命令：
`HELP`, `CLS`, `ECHO`, `VER`, `DATE`, `TIME`, `DIR`, `CD`, `TYPE`,
`EDIT`, `CLEAR`, `SHUTDOWN`, `REBOOT`, `WHOAMI`, `ADDUSER`, `DELUSER`,
`PASSWD`, `LOGIN`, `LOGOUT`, `USERS`, `DRIVES`, `MOUNT`, `MEM`,
`PCI`, `CPUINFO`, `COLOR`, `TITLE`, `PROMPT`, `PAUSE`, `EXIT`

---

## 技术栈

| 层次 | 语言 | 工具链 | 说明 |
|------|------|--------|------|
| 引导加载器 | NASM 汇编 | `nasm` | MBR 512B、16 位实模式 → 32 位保护模式 |
| 内核本体 | C (C99) | `i686-elf-gcc` / `gcc` | 核心功能、内存管理、调度器、文件系统 |
| 驱动程序 | C++ | `i686-elf-g++` / `g++` | 设备驱动、中断管理器、面向对象抽象 |
| 构建系统 | CMake + GNU Make | `cmake`, `make` | 跨平台构建、依赖管理 |
| 镜像打包 | Python 3 / shell | `dd`, `mtools`, `mkfs.fat` | 生成可启动 floppy/hd 镜像 |
| 调试 | QEMU + GDB | `qemu-system-i386` | 模拟器运行、串口日志、远程调试 |

### 编译器/链接器选项

- **架构**: `-m32 -march=i586` (兼容 Pentium 及以上)
- **优化**: `-O2 -ffreestanding -fno-stack-protector`
- **警告**: `-Wall -Wextra`
- **标准**: C 使用 `-std=gnu99`，C++ 使用 `-std=gnu++17`
- **链接**: 自定义 linker script，入口 `_start`，起始地址 `0x100000` (1MB)

---

## 构建与运行

### 前置依赖

**Linux (Ubuntu/Debian)**:
```bash
sudo apt install gcc g++ make cmake nasm qemu-system-x86
# 可选：交叉编译工具链
sudo apt install gcc-i686-linux-gnu g++-i686-linux-gnu
```

**Windows (MSYS2 MinGW32)**:
```bash
pacman -S mingw-w64-i686-gcc mingw-w64-i686-cmake mingw-w64-i686-nasm qemu-system-i386 make
```

**macOS**:
```bash
brew install i686-elf-gcc nasm qemu cmake
```

### 构建步骤

```bash
# 1. 克隆仓库
git clone https://gitcode.com/2503_94276351/PlexsDOS.git
cd PlexsDOS

# 2. 创建构建目录
mkdir build && cd build

# 3. 配置 CMake
cmake .. -DCMAKE_BUILD_TYPE=Release
# 或 Debug 版本（含更多调试信息、跳过安装程序直接进 Shell）
cmake .. -DCMAKE_BUILD_TYPE=Debug

# 4. 编译
make -j$(nproc)

# 5. 生成产物位置
# build/kernel.bin          — 内核 ELF 二进制
# build/PlexsDOS.img        — 1.44MB 软盘镜像 (可直接引导)
# build/PlexsDOS-hd.img     — 硬盘镜像 (含 MBR + 分区)
```

### 运行

```bash
# QEMU 运行软盘镜像
qemu-system-i386 -fda build/PlexsDOS.img -boot a

# QEMU 运行硬盘镜像
qemu-system-i386 -hda build/PlexsDOS-hd.img -boot c

# 启用串口日志（推荐开发调试）
qemu-system-i386 -fda build/PlexsDOS.img -boot a \
    -serial stdio -display sdl

# 启用 GDB 远程调试
qemu-system-i386 -fda build/PlexsDOS.img -boot a -s -S
# 另开终端：
# i686-elf-gdb build/kernel.bin -ex "target remote :1234"
```

### 常见构建选项

| CMake 选项 | 默认值 | 说明 |
|------------|--------|------|
| `CMAKE_BUILD_TYPE` | `Release` | `Release`/`Debug`/`MinSizeRel` |
| `ENABLE_SERIAL_DEBUG` | `ON` | 启用 COM1 串口调试输出 |
| `MINIMAL_KERNEL` | `OFF` | 精简内核（跳过部分驱动，体积更小） |
| `DEBUG_SKIP_INSTALLER` | `OFF` | Debug 模式下跳过安装程序直接进 Shell |

---

## 仓库地址

| 平台 | 地址 |
|------|------|
| **GitCode** (主仓库) | https://gitcode.com/2503_94276351/PlexsDOS |
| **Gitee** (镜像) | https://gitee.com/nexsteaduser/plexs-dos |

```bash
# GitCode 克隆
git clone https://gitcode.com/2503_94276351/PlexsDOS.git

# Gitee 克隆
git clone https://gitee.com/nexsteaduser/plexs-dos.git
```

---

## 目录结构

```
PlexsDOS/
├── boot/                   # 引导加载器 (NASM)
│   ├── bootsect.asm        # Stage 1: MBR 引导扇区 (512 字节)
│   ├── boot16.asm          # Stage 1.5: 16 位实模式 → 保护模式切换
│   └── boot32.asm          # Stage 2: 32 位保护模式、A20、加载内核
├── kernel/                 # Plexs 宏内核 (C/C++)
│   ├── kernel_main.c       # 内核主入口、初始化序列
│   ├── gdt.c/gdt.h         # 全局描述符表
│   ├── idt.c/idt.h         # 中断描述符表 + 8259 PIC
│   ├── paging.c/paging.h   # 分页内存管理
│   ├── interrupt.hpp/cpp   # C++ 中断管理器 + 系统调用分发
│   ├── panic.c/panic.h     # 内核异常/恐慌处理
│   ├── keyboard.c/keyboard.h  # PS/2 键盘驱动
│   ├── mouse.c/mouse.h     # PS/2 鼠标驱动
│   ├── pci.c/pci.h         # PCI 总线枚举
│   ├── ahci.c/ahci.h       # AHCI SATA 控制器驱动
│   ├── ata.c/ata.h         # ATA PIO 磁盘驱动
│   ├── fdc.c/fdc.h         # 软盘控制器驱动
│   ├── cdrom.c/cdrom.h     # ATAPI CD-ROM 驱动
│   ├── disk.c/disk.h       # 磁盘 I/O 抽象层
│   ├── hal/                # 硬件抽象层
│   ├── fs/                 # 文件系统 (FAT32, ISO 9660)
│   │   ├── fat32.c/fat32.h
│   │   ├── iso9660.c/iso9660.h
│   │   └── drive.c/drive.h # 驱动器号管理
│   ├── shell.c/shell.h     # 命令行 Shell
│   ├── edit.c/edit.h       # EDIT.COM 全屏编辑器
│   ├── gui/                # 图形子系统 (VBE)
│   ├── sched.c/sched.h     # 进程调度器
│   ├── users.c/users.h     # 用户账户管理
│   ├── installer.c/installer.h  # 系统安装程序
│   ├── vga.c/vga.h         # VGA 文本模式显示
│   ├── serial.c/serial.h   # 串口调试输出
│   ├── cpu.c/cpu.h         # CPU 初始化与特性检测
│   ├── string.c/string.h   # 优化版内存/字符串操作
│   ├── config_sys.c/config_sys.h  # CONFIG.SYS 解析
│   └── programs/           # 内置二进制资源 (CONFIG.SYS 等)
├── lib/                    # 内核通用库
├── include/                # 公共头文件
├── drivers/                # C++ 外部驱动
├── cmake/                  # CMake 模块
├── CMakeLists.txt          # 顶层 CMake 配置
├── linker.ld               # 内核链接脚本 (起始地址 0x100000)
├── LICENSE                 # GPL v2 许可证
└── README.md               # 本文件
```

---

## 启动流程

1. **BIOS POST** → 加载 MBR 到 `0x7C00` → 执行 Stage 1 (`bootsect.asm`)
2. **Stage 1** → 加载 Stage 1.5 (`boot16.asm`) → 启用 A20 地址线
3. **Stage 1.5** → 加载 Stage 2 (`boot32.asm`) → 切换到 32 位保护模式
4. **Stage 2** → 设置临时页表 → 加载 `kernel.bin` 到 `0x100000` → 跳转 `kernel_main()`
5. **kernel_main()** 初始化序列：
   - VGA 文本模式、串口调试
   - CPU 初始化 + vendor string 检测 + fast_mem 优化路径
   - CONFIG.SYS 解析
   - GDT 加载 (Ring 0-3) → IDT 加载 (256 vectors) → 分页启用
   - 异常处理器安装
   - PS/2 键盘/鼠标初始化
   - `sti` 启用中断
   - PCI 总线枚举 → AHCI/ATA 磁盘探测 → FDC 软盘探测 → CD-ROM 探测
   - FAT32 / ISO 9660 文件系统挂载 → MBR 分区扫描
   - HAL 块设备层注册
   - ISA 传统设备枚举（硬盘启动时）
   - 调度器初始化 → 用户子系统初始化
   - 根据启动介质判断：软盘/CD → 安装程序；硬盘 → 直接进入 Shell
6. **Shell** 等待用户输入命令

---

## 许可证

本项目采用 **GNU General Public License v2.0** 开源许可证。详见 [LICENSE](LICENSE) 文件。

```
Nexsteaduser PlexsDOS — x86 32-bit Monolithic Kernel Operating System
Copyright (C) 2025 Tinmc189623 (Nexlyh Team, Nexsteaduser Brand)

This program is free software; you can redistribute it and/or modify
it under the terms of the GNU General Public License as published by
the Free Software Foundation; either version 2 of the License, or
(at your option) any later version.
```

---

## 致谢

- 感谢 [OSDev Wiki](https://wiki.osdev.org) 提供的大量底层开发资料
- 感谢所有为本项目提供反馈和建议的社区成员
- 感谢 QEMU 项目提供的优秀模拟器

---

**Nexsteaduser** · **Nexlyh** · **Tinmc189623**
*admin@nexsteaduser.com*
