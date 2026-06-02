# PlexsDOS — 技术规格

## Nexsteaduser — PlexsDOS 技术规格文档

作者: Tinmc189623 | 团队: Nexlyh

---

## 1. 系统概述

PlexsDOS 是一个面向 x86 32-bit 保护模式的类 DOS 操作系统，采用自研内核。系统以软盘为启动介质，提供基本的命令行 Shell 环境，支持 FAT32 文件系统、ATA DMA 传输、通用处理器运行时优化 (i686 至 AVX2)，以及 .comx 格式外部程序加载与执行。

### 1.1 设计目标

- 从 BIOS 引导，内核运行在 x86 32-bit 保护模式下
- 提供类 DOS 的命令行交互体验
- 自研内核，采用模块化分层架构
- 支持 FAT32 文件系统读写
- 支持 ATA DMA 传输 (PCI Bus Master IDE)
- 通用处理器运行时优化 (i686 基线 → SSE2 → AVX 运行时分派)
- 支持加载和执行 .comx 格式外部程序 (自研 32-bit 可执行格式)
- DOS 兼容的 INT 21h 系统调用接口
- 使用 GCC + GAS (AT&T 语法) + Make 工具链

### 1.2 品牌信息

- 品牌：Nexsteaduser
- 作者：Tinmc189623
- 团队：Nexlyh

---

## 2. 硬件平台

### 2.1 目标架构

| 属性 | 值 |
|------|-----|
| CPU | x86 (i386 兼容或更高) |
| 运行模式 | 32-bit 保护模式 (Protected Mode) |
| 字长 | 32-bit |
| 寻址空间 | 4GB (32-bit 平坦内存模型) |
| 段寄存器 | CS=0x08 (代码), DS/ES/FS/GS/SS=0x10 (数据) |

### 2.2 最低硬件要求

- CPU: Intel Pentium Pro (i686) 或兼容处理器 (支持 CMOV/FPU)
- 内存: 64MB RAM
- 存储: 1.44MB 3.5" 软盘 + ATA 硬盘 (64MB, FAT32)
- 显示: VGA 兼容显卡 (80x25 文本模式)
- 输入: PS/2 键盘
- PCI: PCI 总线 (用于 ATA DMA)

### 2.3 处理器优化支持

系统通过 CPUID 运行时检测, 自动选择最优代码路径:

| 优化级别 | 最低 CPU | 内存操作宽度 | 指令集 |
|----------|---------|-------------|--------|
| 基线 | i486 | 32-bit | REP MOVSD/STOSD |
| SSE2 | Pentium 4 | 128-bit | MOVDQA/MOVDQU |
| AVX | Sandy Bridge | 256-bit | VMOVDQA |

支持检测的 CPU 特性: FPU, TSC, CMOV, MMX, SSE, SSE2, SSE3, SSSE3, SSE4.1, SSE4.2, POPCNT, AVX, F16C, FMA, BMI1, AVX2, BMI2, ERMSB, 3DNow!, 3DNow!+, SSE4a, ABM

---

## 3. 内存布局

```
地址范围              用途
────────────────────────────────────────
0x00000 - 0x003FF    实模式中断向量表 (IVT, 引导阶段)
0x00400 - 0x004FF    BIOS 数据区 (BDA)
0x00500 - 0x00FFF    可用区域
0x01000 - 0x1FFFF    内核代码 + 数据 (约 128KB)
0x20000 - 0x2FFFF    外部程序加载区
0x7C000 - 0x7DFFF    引导扇区 (512 字节, 引导阶段)
0x80000 - 0x8FFFF    内核栈 (64KB)
0xB8000              VGA 文本模式显存 (80×25×2)
```

---

## 4. 启动流程

### 4.1 阶段一：BIOS → 引导扇区 (boot_sector.S, 16-bit 实模式)

1. BIOS 加载引导扇区到 0x7C00
2. 引导扇区初始化段寄存器和栈
3. 通过 BIOS INT 13h 从软盘读取内核到 0x1000
4. 设置 GDT (全局描述符表)
5. 开启 CR0 PE 位, 切换到 32-bit 保护模式
6. 远跳转到内核入口 0x1000

### 4.2 阶段二：内核初始化 (kernel_entry.S → kernel_main.c, 32-bit 保护模式)

1. 设置数据段选择子 (0x10) 和栈指针 (0x90000)
2. 调用 kernel_main() C 函数
3. 初始化串口 (COM1, 用于调试输出)
4. 初始化屏幕驱动 (VGA 文本模式)
5. 初始化 CPU 特性 (奔腾3 SSE/MMX, CPUID)
6. 初始化键盘驱动 (PS/2, IRQ1)
7. 初始化 PCI 总线 (扫描 IDE 控制器, 启用 Bus Master)
8. 初始化 ATA 磁盘驱动 (PIO + DMA 模式)
9. 初始化 FAT32 文件系统
10. 初始化 INT 21h 系统调用
11. 启动 Shell

---

## 5. 内核架构

PlexsDOS 自研内核采用分层架构：

### 5.1 架构层次

```
┌─────────────────────────────────┐
│         Shell (命令行)           │  用户层
├─────────────────────────────────┤
│     系统调用接口 (INT 21h)       │  系统调用层
├─────────────────────────────────┤
│  FAT32 文件系统 | 程序加载器      │  内核服务层
├─────────────────────────────────┤
│ 屏幕 | 键盘 | ATA DMA | PCI     │  驱动层
├─────────────────────────────────┤
│ CPU检测 | SSE/MMX | 快速内存     │  优化层
├─────────────────────────────────┤
│      IDT | GDT | 8259A PIC      │  硬件抽象层
├─────────────────────────────────┤
│      x86 32-bit 保护模式硬件     │  硬件层
└─────────────────────────────────┘
```

### 5.2 核心子系统

| 子系统 | 模块 | 功能 |
|--------|------|------|
| 中断处理 | kernel/arch/interrupt.S, idt.c | IDT 管理, 8259A PIC, 中断分发 |
| 系统调用 | kernel/arch/syscall.c | INT 21h (向量 0x22), DOS 兼容 |
| CPU 检测 | kernel/arch/cpu.c | CPUID 全特性检测, SSE/AVX 启用, 运行时分派 |
| 屏幕驱动 | kernel/drivers/screen.c | VGA 文本模式, 直写 0xB8000 |
| 键盘驱动 | kernel/drivers/keyboard.c | PS/2 扫描码, IRQ1, 环形缓冲区 |
| PCI 总线 | kernel/drivers/pci.c | PCI 配置空间访问, IDE 控制器扫描 |
| 磁盘驱动 | kernel/drivers/disk.c | ATA PIO + DMA, PCI Bus Master |
| 文件系统 | kernel/fs/fat32.c | FAT32 根目录读取, 簇链遍历 |
| 程序加载器 | kernel/loader.c | .comx 格式解析, 校验, BSS 清零 |
| Shell | kernel/shell/shell.c | 命令解析, 内置命令, .comx 程序加载 |
| 快速内存 | lib/fast_mem.c | 运行时分派: 基线/SSE2/AVX 内存操作 |
| 库函数 | lib/string.c | strlen, memset, memcpy, strcmp 等 |

---

## 6. 引导加载器规格

### 6.1 引导扇区 (512 字节)

- 文件: boot/boot_sector.S
- 模式: 16-bit 实模式 (BIOS 要求)
- BPB: FAT12 格式参数块
- 功能: 加载内核 → 设置 GDT → 开启保护模式 → 跳转内核

### 6.2 引导参数

- 引导设备号通过 DL 寄存器传递
- 内核加载地址：0x1000
- 内核最大扇区数：40 (20KB)
- GDT: 空描述符 + 代码段 (0x08) + 数据段 (0x10)

---

## 7. 内核接口

### 7.1 中断向量分配

| 向量 | 用途 | 类型 |
|------|------|------|
| 0x00 - 0x1F | CPU 异常 (保留) | 硬件 |
| 0x20 | 默认中断处理 | 硬件 IRQ0-7 |
| 0x21 | 键盘中断 (IRQ1) | 硬件 |
| 0x22 | INT 21h 系统调用 | 软件 |

### 7.2 系统调用接口 (INT 21h, 向量 0x22)

DOS 兼容的 INT 21h 子功能：

| AH | 功能 | 参数 | 返回值 |
|----|------|------|--------|
| 0x01 | 读字符并回显 | - | AL=字符 |
| 0x02 | 写字符 | DL=字符 | - |
| 0x09 | 写字符串 | EDX=字符串地址 ('$' 结尾) | - |
| 0x0A | 读字符串到缓冲区 | EDX=缓冲区地址 | 缓冲区填入 |
| 0x4C | 程序终止 | AL=返回码 | - |

---

## 8. Shell 规格

### 8.1 命令行格式

```
PLXSDOS> command [args...]
```

### 8.2 内置命令

| 命令 | 功能 | 语法 |
|------|------|------|
| `help` | 显示帮助信息 | `help` |
| `cls` | 清屏 | `cls` |
| `ls` | 列出磁盘文件 | `ls` |
| `type` | 显示文件内容 | `type <filename>` |
| `run` | 加载并执行程序 | `run <filename>` |
| `echo` | 回显文本 | `echo <text>` |
| `ver` | 显示版本 | `ver` |
| `reboot` | 重启系统 | `reboot` |

### 8.3 外部程序加载

- 程序格式：.comx (自研 32-bit 可执行格式, 32 字节头部)
- 加载地址：0x20000 (可在头部中指定)
- 入口点：头部 entry_offset 字段
- 校验和验证: 32-bit 滚动校验和
- CPU 特性检查: 头部 flags 字段声明 SSE/SSE2/MMX 需求
- BSS 段: 加载后自动清零
- 程序通过 INT 0x22 (INT 21h) 调用系统服务
- 程序通过 `ret` 指令返回 Shell

---

## 9. .comx 可执行格式

### 9.1 格式定义

.comx 是 PlexsDOS (Nexsteaduser) 自研的 32-bit 保护模式可执行文件格式。

| 偏移 | 大小 | 字段 | 说明 |
|------|------|------|------|
| 0x00 | 4 | magic | 魔数 "CPX\x00" (0x43505800) |
| 0x04 | 1 | version | 格式版本 (0x01) |
| 0x05 | 1 | flags | CPU 需求标志 (bit0=SSE, bit1=SSE2, bit2=MMX) |
| 0x06 | 2 | reserved0 | 保留 |
| 0x08 | 4 | entry_offset | 入口点偏移 (相对于代码起始) |
| 0x0C | 4 | code_size | 代码 + 只读数据大小 |
| 0x10 | 4 | bss_size | BSS 段大小 (加载后清零) |
| 0x14 | 4 | load_addr | 建议加载地址 (默认 0x20000) |
| 0x18 | 4 | checksum | 代码部分 32-bit 滚动校验和 |
| 0x1C | 4 | reserved1 | 保留 |

### 9.2 构建工具

- `tools/mkcomx.py` — 将 flat binary 打包为 .comx 格式
- `tools/mkfat32.py` — 使用 pyfatfs 创建 FAT32 镜像并写入文件
- `tools/mkfloppy.py` — 创建 1.44MB FAT12 软盘镜像

---

## 10. 文件系统

### 10.1 磁盘布局

| 介质 | 文件系统 | 大小 | 用途 |
|------|---------|------|------|
| 软盘 (fda) | FAT12 | 1.44MB | 引导扇区 + 内核 (原始扇区) |
| 硬盘 (hda) | FAT32 | 64MB | 数据盘: .comx 程序, 用户文件 |

### 10.2 FAT32 支持 (硬盘)

- 介质：ATA 硬盘 (IO_BASE 0x1F0)
- 每扇区：512 字节
- 每簇：8 扇区 (4KB)
- FAT 数：2（主 + 备份）
- 根目录起始簇：2
- FAT 条目：32-bit (使用低 28 位)

### 10.3 文件操作 API

```c
bool fat32_init(void);                              // 初始化文件系统
void fat32_list_root(void);                         // 列出根目录
struct fat32_dir_entry *fat32_find_file(const char *name); // 查找文件
uint32_t fat32_load_file(struct fat32_dir_entry *entry,    // 加载文件
                         uint32_t load_addr);
```

---

## 11. 构建规格

### 11.1 工具链

| 工具 | 用途 |
|------|------|
| gcc | C 编译器 (-m32 -march=i686 -ffreestanding) |
| as | GAS 汇编器 (--32, AT&T 语法) |
| ld | 链接器 (-m i386pe -T linker.ld) |
| objcopy | 二进制提取 (-O binary) |
| make | 构建系统 |
| qemu-system-i386 | 测试模拟器 (-m 64M) |
| python + pyfatfs | FAT12/FAT32 镜像创建 (pyfatfs 库) |

### 11.2 编译标志

```makefile
CFLAGS   = -m32 -march=i686 -ffreestanding -fno-builtin -nostdlib -fno-stack-check -fno-stack-protector -Wall -Wextra -std=c99 -Os -Iinclude
ASFLAGS  = --32 -Iinclude
LD_KERN  = -m i386pe -T linker.ld -nostdlib
```

注: `-march=i686` 为通用基线 (CMOV/FPU), 不锁定任何 SIMD 扩展。
SIMD 指令通过 `__attribute__((target("sse2")))` / `__attribute__((target("avx")))` 在函数级别启用,
运行时根据 CPUID 检测结果分派到最优代码路径。

### 11.3 构建产物

| 文件 | 大小 | 说明 |
|------|------|------|
| build/boot.bin | 512B | 引导扇区 |
| build/kernel.bin | ~19KB | 32-bit 内核 (含 SIMD/DMA/FAT32/Loader) |
| build/plexsdos.img | 1.44MB | 可引导软盘镜像 (FAT12) |
| build/disk.img | 64MB | FAT32 硬盘镜像 (.comx 程序 + 用户文件) |
| build/programs/HELLO.COMX | ~80B | 测试 .comx 程序 |

---

## 12. 测试规格

### 12.1 测试环境

- 主要：QEMU (`make run`)
- 辅助：Bochs（精确时序调试）

### 12.2 测试用例

| 阶段 | 测试项 | 验证方式 |
|------|--------|---------|
| 引导 | 软盘引导成功 | QEMU 启动到 Shell 提示符 |
| CPU | CPUID 全特性检测 | 串口输出 CPU 信息 (SSE/AVX 等) |
| PCI | IDE 控制器扫描 | 串口输出 PCI 状态 |
| 屏幕 | 文本输出正确 | 屏幕显示验证 |
| 键盘 | 按键输入响应 | 交互输入测试 |
| 磁盘 | ATA DMA 传输 | disk OK 串口输出 |
| Shell | 内置命令执行 | 逐命令测试 |
| 文件 | FAT32 文件读取 | ls/type 命令 |
| 程序 | 外部程序加载执行 | run 命令 |
| 系统调用 | INT 21h 功能 | 程序内调用测试 |
| 重启 | 系统重启 | reboot 命令 |

---

*Nexsteaduser — PlexsDOS*
*作者: Tinmc189623 | 团队: Nexlyh*
