# PlexsDOS — 技术规格

## Nexsteaduser — PlexsDOS 技术规格文档

作者: Tinmc189623 | 团队: Nexlyh

---

## 1. 系统概述

PlexsDOS 是一个面向 x86 32-bit 保护模式的类 DOS 操作系统，采用自研内核。系统支持从软盘、硬盘或 ISO 9660 光盘启动，提供命令行 Shell 环境和可选的 GUI 桌面环境，支持 FAT12/FAT32/ISO 9660 文件系统、ATA PIO/AHCI SATA/ATAPI CD-ROM/Floppy 多种存储介质，以及 .comx 格式外部程序加载与执行。

### 1.1 设计目标

- 从 BIOS 引导，内核运行在 x86 32-bit 保护模式下
- 提供类 DOS 的命令行交互体验
- 自研内核，采用模块化分层架构
- 支持 FAT12/FAT32/ISO 9660 文件系统读写
- 支持 ATA PIO / AHCI SATA / ATAPI CD-ROM / FDC 多种存储
- 支持加载和执行 .comx 格式外部程序 (自研 32-bit 可执行格式)
- DOS 兼容的 INT 21h 系统调用接口
- 处理器运行时优化 (i686 基线 → SSE2 → AVX)
- 抢占式多任务调度
- 分页内存管理 (4KB 页)
- GUI 桌面环境 (窗口管理器 + 小部件)
- 用户账户系统
- Display Manager (LightDM 移植) 集成
- C++ 面向对象中断管理器
- 使用 GCC + GAS (AT&T 语法) + Make 工具链构建

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
| 寻址空间 | 4GB (32-bit 平坦内存模型, 分页启用) |
| 段寄存器 | CS=0x08 (代码, Ring 0), CS=0x1B (代码, Ring 3), DS/ES/FS/GS/SS=0x10 (数据, Ring 0), SS=0x23 (数据, Ring 3) |

### 2.2 最低硬件要求

- CPU: Intel Pentium Pro (i686) 或兼容处理器 (支持 CMOV/FPU/CPUID)
- 内存: 64MB RAM (建议 128MB+)
- 存储: 1.44MB 3.5" 软盘 + ATA/ATAPI 硬盘 或 AHCI SATA 硬盘
- 显示: VGA 兼容显卡 (80x25 文本模式 / 图形模式)
- 输入: PS/2 键盘 + PS/2 鼠标
- PCI: PCI 总线 (用于 AHCI SATA)

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
地址范围                   用途
────────────────────────────────────────────────
0x00000000 - 0x000003FF   实模式中断向量表 (IVT, 引导阶段)
0x00000400 - 0x000004FF   BIOS 数据区 (BDA)
0x00000500 - 0x00000FFF   可用区域
0x00001000 - 0x00002FFF   引导阶段临时内核加载区
0x00003000 - 0x00007BFF   可用区域
0x00007C00 - 0x00007DFF   引导扇区 (512 字节, 引导阶段)
0x00007E00 - 0x0001FFFF   EBDA 前可用空间
0x00020000 - 0x0002FFFF   外部程序加载区 (.comx 默认)
0x00030000 - 0x001FFFFF   内核代码 + 数据 (约 1.8MB)
0x00200000 - 0x002FFFFF   内核 BSS 段 (清零后复用)
0x00300000 - 0x003FFFFF   页目录 + 页表 (4KB 页)
0x00400000 - 0x004FFFFF   内核栈 (1MB)
0x00500000 - 0x005FFFFF   外部程序加载区 (高地址)
0x00B80000                 VGA 文本模式显存 (80×25×2)
0x00E00000 - 0x00FFFFFF   GUI 帧缓冲区
```

### 3.1 内核段布局

| 段 | 起始地址 | 描述 |
|----|---------|------|
| .text | 0x30000 | 代码段 (含 .rdata, .rodata, .data) |
| .bss | 0x200000 | 未初始化数据段 (约 1MB) |
| 页目录 | 0x300000 | 4KB 页目录 |
| 页表 | 0x301000+ | 页表 (连续) |

### 3.2 内核链接地址

- 内核入口: `_start` 位于 `kernel/kernel_entry.S`
- `.text` 起始: `0x30000` (由 linker.ld 控制)
- `.bss` 起始: `0x200000` (由 linker.ld 指定绝对地址)
- `_end` 符号: 内核结束地址

---

## 4. 启动流程

### 4.1 软盘启动

1. BIOS 加载引导扇区到 0x7C00 (boot_sector.S, 16-bit 实模式)
2. 引导扇区初始化段寄存器和栈
3. 通过 BIOS INT 13h 从软盘读取内核到 0x1000
4. 设置 GDT (空 + 代码段 0x08 + 数据段 0x10)
5. 开启 CR0 PE 位, 切换到 32-bit 保护模式
6. 远跳转到内核入口 0x1000 (后跳转到 0x30000)

### 4.2 硬盘启动

1. BIOS 加载 MBR 到 0x7C00 (hd_mbr.S, 16-bit 实模式)
2. MBR 扫描分区表, 定位活动分区
3. MBR 加载 VBR 到 0x7C00 (hd_vbr.S, 16-bit 实模式)
4. VBR 读取 BPB, 通过 INT 13h 加载内核到 0x1000
5. VBR 设置 GDT, 切换到 32-bit 保护模式
6. 远跳转到内核入口

### 4.3 内核初始化流程

```
kernel_entry.S (_start)
  ├─ 设置数据段选择子 (0x10) 和栈指针 (0x400000)
  ├─ 保存 BIOS DL (启动驱动器号) 到 boot_drive
  ├─ 根据内核实际位置调整入口
  └─ 调用 kernel_main()

kernel_main()
  ├─ 1. 屏幕驱动 (VGA 文本模式)
  ├─ 2. CPU 特性检测 (CPUID, SSE/MMX/AVX)
  ├─ 3. GDT 初始化 (Ring 0-3 段 + TSS)
  ├─ 4. 分页初始化 (页目录/页表, 身份映射, CR0.PG)
  ├─ 5. IDT + PIC 初始化 (256 向量, 8259A 重映射)
  ├─ 6. CPU 异常处理注册 (红屏 RSOD)
  ├─ 7. PS/2 键盘驱动 (IRQ1)
  ├─ 8. PS/2 鼠标驱动
  ├─ 9. sti — 启用中断
  ├─ 10. PCI 总线枚举
  ├─ 11. AHCI SATA 检测与初始化
  ├─ 12. 驱动器子系统注册 (drive_init)
  ├─ 13. FDC 软盘控制器初始化
  ├─ 14. ATA/AHCI 磁盘 + FAT32 挂载
  ├─ 15. CD-ROM (ATAPI) + ISO 9660 挂载
  ├─ 16. C++ 中断管理器 + INT 21h 系统调用
  ├─ 17. HAL 块设备注册
  ├─ 18. ISA 传统设备枚举
  ├─ 19. 进程调度器初始化
  ├─ 20. 用户账户子系统初始化
  ├─ [安装模式: 软盘/CD 启动 → installer_run()]
  └─ [正常模式: 硬盘启动 → shell_main()]
```

---

## 5. 内核架构

PlexsDOS 自研内核采用分层模块化架构：

### 5.1 架构层次

```
┌──────────────────────────────────────────┐
│  Shell | GUI Desktop/WM/Widgets | Editor │  用户层
├──────────────────────────────────────────┤
│   用户账户系统 | 进程调度器 | 程序加载器   │  系统服务层
├──────────────────────────────────────────┤
│      系统调用接口 (INT 21h, C++ OOP)      │  系统调用层
├──────────────────────────────────────────┤
│  FAT12 | FAT32 | ISO 9660 | 虚拟文件系统  │  文件系统层
├──────────────────────────────────────────┤
│  HAL (硬件抽象层) | ISA 枚举 | 块设备框架 │  抽象层
├──────────────────────────────────────────┤
│ AHCI | ATA PIO | ATAPI | FDC | CD-ROM   │  存储驱动层
├──────────────────────────────────────────┤
│   屏幕 | VGA | 键盘 | 鼠标 | 串口         │  设备驱动层
├──────────────────────────────────────────┤
│   分页管理 | 物理帧分配器 | 堆内存管理     │  内存管理层
├──────────────────────────────────────────┤
│   C++ 中断管理器 | IDT | GDT | PIC | TSS │  硬件抽象层
├──────────────────────────────────────────┤
│    CPU 检测 | SSE/MMX/AVX 运行时分派      │  优化层
├──────────────────────────────────────────┤
│           x86 32-bit 保护模式硬件          │  硬件层
└──────────────────────────────────────────┘
```

### 5.2 核心子系统

| 子系统 | 模块 | 功能 |
|--------|------|------|
| 中断管理 | kernel/arch/interrupt.S, idt.c, interrupt_mgr.cpp | IDT 管理, 8259A PIC, C++ OOP 分发 |
| 系统调用 | kernel/arch/syscall.c, shim/int21.c | INT 21h 调用, C++ 分发器 |
| GDT | kernel/arch/gdt.c, gdt_load.S | Ring 0-3 段, TSS |
| CPU 检测 | kernel/arch/cpu.c | CPUID 全特性检测, SSE/AVX 启用 |
| 分页 | kernel/mm/paging.c, pfa.c | 4KB 页, 身份映射, 物理帧分配 |
| 红屏 (RSOD) | kernel/arch/panic.c | 内核恐慌, CPU 异常诊断 |
| 屏幕驱动 | kernel/drivers/screen.c | VGA 文本模式, 0xB8000 |
| VGA 驱动 | kernel/drivers/vga.c | VGA 图形模式切换 |
| 键盘驱动 | kernel/drivers/keyboard.c | PS/2 扫描码, IRQ1, 环形缓冲区 |
| 鼠标驱动 | kernel/drivers/mouse.c | PS/2 鼠标, IRQ12 |
| 串口驱动 | kernel/drivers/serial.c | COM1 调试输出 |
| PCI 总线 | kernel/drivers/pci.c | PCI 配置空间, AHCI/ATA 扫描 |
| AHCI SATA | kernel/drivers/ahci.c | AHCI HBA, NCQ, PRDT |
| ATA 磁盘 | kernel/drivers/disk.c | ATA PIO + DMA, PCI Bus Master |
| FDC 软盘 | kernel/drivers/fdc.c | NEC 765, 马达状态机, 多格式, 写入 |
| CD-ROM | kernel/drivers/cdrom.c | ATAPI PACKET, ISO 9660 |
| 驱动器抽象 | kernel/drivers/drive.c | 驱动器字母 A:-E: 管理 |
| FAT12 | kernel/fs/fat12.c | 软盘 FAT12 文件系统 |
| FAT32 | kernel/fs/fat32.c | 硬盘 FAT32 文件系统 |
| ISO 9660 | kernel/fs/fs.c + cdrom.c | 光盘文件系统 |
| 程序加载器 | kernel/loader.c | .comx 格式解析, 校验, 执行 |
| Shell | kernel/shell/shell.c | 命令解析, 内置命令, 程序加载 |
| 用户系统 | kernel/shell/users.c | 多用户账户管理 |
| 调度器 | kernel/sched/scheduler.c | 抢占式多任务, 时间片 |
| HAL | kernel/hal/hal_init.cpp, hal_io.c, isa.c | 硬件抽象, ISA 枚举, 块设备框架 |
| GUI 桌面 | kernel/gui/desktop.c | 桌面环境 |
| 窗口管理器 | kernel/gui/wm.c | 窗口创建/管理/事件分发 |
| 小部件 | kernel/gui/widgets.c | 按钮, 标签, 输入框 |
| 编辑器 | kernel/editor/editor.c | 文本编辑器 |
| 安装程序 | kernel/installer.c | MS-DOS 风格多盘安装 |
| 快速内存 | lib/fast_mem.c | 基线/SSE2/AVX 运行时内存操作 |
| 库函数 | lib/string.c | strlen, memset, memcpy, strcmp |
| C++ 运行时 | libcpp/*.hpp | 基础 C++ 头文件 (algorithm, array, type_traits 等) |
| Shim 层 | kernel/shim/dfan.c, onebus.c, syslog.c, usrgn.c, x12.c, int21.c | 兼容适配层 |

### 5.3 Display Manager 集成

PlexsDOS 包含 LightDM 显示管理器的移植版 (PlexsDM):

- `kernel/dm/plxdm_*.c` — PlexsDM 本地实现, 适配 PlexsDOS 内核接口
- `kernel/dm/lightdm-core/` — LightDM 原始代码 (参考/兼容)
- 功能: 显示管理, 会话管理, 用户选择, 自动登录, XDMCP

---

## 6. 引导加载器规格

### 6.1 软盘引导扇区 (512 字节)

- 文件: boot/boot_sector.S
- 模式: 16-bit 实模式 (BIOS 要求)
- BPB: FAT12 格式参数块
- 功能: 加载内核 → 设置 GDT → 开启保护模式 → 跳转内核

### 6.2 硬盘 MBR (512 字节)

- 文件: boot/hd_mbr.S
- 加载地址: 0x0600
- 功能: 分区表扫描, 活动分区定位, VBR 加载

### 6.3 硬盘 VBR (512 字节)

- 文件: boot/hd_vbr.S
- 加载地址: 0x7C00
- 功能: FAT32 BPB 参数解析, 内核加载, GDT 设置, 保护模式切换
- 后处理: tools/fix_vbr.py 修正 BPB 字段

### 6.4 引导参数

- 引导设备号通过 DL 寄存器传递, 保存到 `boot_drive`
- 内核加载地址: 0x1000 (引导阶段临时), 最终地址 0x30000
- 内核最大扇区数: 根据内核实际大小动态
- DL < 0x80: 从软盘/CD 启动 (进入安装模式)
- DL >= 0x80: 从硬盘启动 (正常启动 Shell)

---

## 7. 内核接口

### 7.1 中断向量分配

| 向量 | 用途 | 类型 |
|------|------|------|
| 0x00 - 0x1F | CPU 异常 (panic 处理) | 硬件 |
| 0x20 | 默认/未注册中断处理 | 硬件 |
| 0x21 | 键盘中断 (IRQ1) | 硬件 |
| 0x22 | INT 21h 系统调用 | 软件 |

### 7.2 系统调用接口 (INT 21h, 向量 0x22)

DOS 兼容的 INT 21h 子功能:

| AH | 功能 | 参数 | 返回值 |
|----|------|------|--------|
| 0x01 | 读字符并回显 | - | AL=字符 |
| 0x02 | 写字符 | DL=字符 | - |
| 0x09 | 写字符串 | EDX=字符串地址 ('$' 结尾) | - |
| 0x0A | 读字符串到缓冲区 | EDX=缓冲区地址 | 缓冲区填入 |
| 0x4C | 程序终止 | AL=返回码 | - |

### 7.3 C++ 中断管理器

面向对象的中断处理框架:

```cpp
class InterruptHandler {
public:
    virtual void handle(uint32_t vector, uint32_t error_code) = 0;
};

class InterruptManager {
    static InterruptManager& instance();
    void registerHandler(uint8_t vector, InterruptHandler* handler);
    void unregisterHandler(uint8_t vector);
};

class SyscallDispatcher : public InterruptHandler {
    void handle(uint32_t vector, uint32_t error_code) override;
};
```

### 7.4 内核恐慌 (RSOD)

```c
_Noreturn void kernel_panic(const char *fmt, ...);
```

- 红底白字全屏显示
- 显示: 错误类型, 地址, 错误码, 寄存器状态, 栈回溯
- 同时输出到串口
- CPU 异常 0x00-0x1F 自动触发

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
| `ls` | 列出磁盘文件 | `ls [path]` |
| `type` | 显示文件内容 | `type <filename>` |
| `run` | 加载并执行程序 | `run <filename>` |
| `echo` | 回显文本 | `echo <text>` |
| `ver` | 显示版本 | `ver` |
| `reboot` | 重启系统 | `reboot` |
| `mem` | 显示内存信息 | `mem` |
| `users` | 用户管理 | `users [list/add/del]` |
| `edit` | 文本编辑器 | `edit <filename>` |

### 8.3 外部程序加载

- 程序格式: .comx (自研 32-bit 可执行格式, 32 字节头部)
- 加载地址: 头部 load_addr 字段指定
- 校验和验证: 32-bit 滚动校验和
- CPU 特性检查: 头部 flags 字段
- 入口点: 头部 entry_offset 字段
- BSS 段: 加载后自动清零
- 程序通过 INT 21h 系统调用服务
- 程序通过 `ret` 返回 Shell

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
- `tools/mkfat32.py` — 使用 pyfatfs 创建 FAT32 镜像
- `tools/mkfat12.py` — 创建 FAT12 软盘镜像 (安装盘)
- `tools/mkfloppy.py` — 创建 1.44MB FAT12 软盘镜像 (引导盘)
- `tools/mkbootdisk.py` — 创建可引导安装盘
- `tools/mkiso.py` — 创建 ISO 9660 可引导光盘 (El Torito)
- `tools/mkvmdk.py` — 创建 VMDK 虚拟硬盘 (40GB, 预装系统)
- `tools/fix_vbr.py` — 修正 VBR BPB 字段

---

## 10. 文件系统

### 10.1 存储介质布局

| 介质 | 文件系统 | 大小 | 用途 |
|------|---------|------|------|
| 软盘 (A:) | FAT12 | 1.44MB | 引导 + 内核 + 安装文件 |
| 硬盘 (C:) | FAT32 | 64MB+ | 系统分区: .comx 程序, 用户文件 |
| 光盘 (D:) | ISO 9660 | 可变 | 安装光盘 / 数据光盘 |

### 10.2 FAT12 支持 (软盘)

- 介质: 软盘控制器 (FDC), DMA 通道 2
- 每扇区: 512 字节
- 每簇: 1 扇区
- FAT 数: 2 (主 + 备份)
- 根目录: 固定位置 (14 扇区, 224 条目)

### 10.3 FAT32 支持 (硬盘)

- 介质: ATA/AHCI (IO_BASE 0x1F0 或 AHCI BAR)
- 每扇区: 512 字节
- 每簇: 8 扇区 (4KB)
- FAT 数: 2 (主 + 备份)
- 根目录起始簇: 2
- FAT 条目: 32-bit (使用低 28 位)

### 10.4 ISO 9660 支持 (光盘)

- 介质: ATAPI CD-ROM (2048 字节/扇区)
- 级别: 1 (基本格式, 无 Rock Ridge / Joliet)
- PVD: 扇区 16
- 目录: 分层路径遍历
- Shell 命令: `CDIR`, `CCAT`, `CDMOUNT`

### 10.5 驱动器抽象

统一的驱动器字母映射系统:

| 字母 | 类型 | 描述 |
|------|------|------|
| A: | 软盘 | FDC 驱动器 0 |
| B: | 软盘 | FDC 驱动器 1 |
| C: | 硬盘 | 主 ATA/AHCI 分区 |
| D: | 光盘 | ATAPI CD-ROM |
| E:+ | 硬盘 | 额外 MBR 分区 |

---

## 11. 存储驱动

### 11.1 FDC 软盘控制器

- NEC 765 兼容 FDC, I/O 端口 0x3F0-0x3F7
- 马达状态机: OFF → STARTING → ON → STOPPING
- 多格式支持: 1.44MB (18 SPT), 1.2MB (15 SPT), 720KB (9 SPT)
- 写入支持: WRITE DATA 命令, 写保护检测
- 磁盘更换检测: CHANGE LINE 信号
- 旋转稳定延迟: 300ms, 马达关闭延迟: 2000ms

### 11.2 ATA PIO/DMA

- I/O 端口: 0x1F0-0x1F7 (Primary), 0x170-0x177 (Secondary)
- PIO 模式: READ SECTORS (0x20), WRITE SECTORS (0x30)
- DMA 模式: READ DMA (0xC8), WRITE DMA (0xCA)
- PCI Bus Master IDE, PRDT 表
- LBA28 寻址

### 11.3 AHCI SATA

- PCI BAR5 (ABAR), HBA 内存映射寄存器
- 端口枚举, 设备签名检测
- Command List + Command Table + PRDT
- NCQ (Native Command Queuing) 支持
- 读写: `ahci_read_sectors()`, `ahci_write_sectors()`

### 11.4 ATAPI CD-ROM

- ATA PACKET 命令 (0xA0)
- 12 字节 CDB 命令集 (TEST_UNIT_READY, READ_10, READ_CAPACITY, INQUIRY)
- 2048 字节扇区大小
- 自动检测 ATAPI 设备

---

## 12. 内存管理

### 12.1 分页

- 页大小: 4KB
- 页目录: 1024 条目 (4KB), 地址 0x300000
- 页表: 可变数量, 地址 0x301000+
- 身份映射: 虚拟地址 = 物理地址
- CR0.PG 启用分页
- 物理帧分配器 (PFA): 位图管理

### 12.2 物理帧分配

- `pfa_init()` — 从可用内存区域初始化
- `pfa_alloc()` — 分配一页 (4KB)
- `pfa_free()` — 释放一页

---

## 13. 进程调度器

### 13.1 调度模型

- 抢占式多任务
- 时间片轮转 (Round-Robin)
- PIT (8253) 定时器中断驱动
- 任务控制块 (TCB) 结构

### 13.2 API

```c
void sched_init(void);
int  sched_create_task(void (*entry)(void*), void *arg, uint32_t flags);
void sched_yield(void);
void sched_exit(int code);
```

---

## 14. GUI 系统

### 14.1 桌面环境

- 全屏桌面背景
- 任务栏/面板
- 开始菜单

### 14.2 窗口管理器

- 窗口创建/销毁/移动
- Z-order 管理
- 鼠标事件分发
- 窗口标题栏 + 边框

### 14.3 小部件

- 按钮 (Button)
- 标签 (Label)
- 输入框 (Text Input)
- 画布 (Canvas)

---

## 15. 构建规格

### 15.1 工具链

| 工具 | 用途 |
|------|------|
| gcc (mingw32) | C 编译器 (-m32 -march=i686 -std=c23 -ffreestanding) |
| g++ (mingw32) | C++ 编译器 (-m32 -march=i686 -std=c++23 -ffreestanding, -fno-exceptions -fno-rtti) |
| as | GAS 汇编器 (--32, AT&T 语法) |
| ld | 链接器 (-m i386pe -T linker.ld) |
| objcopy | 二进制提取 (-O binary) |
| make | 构建系统 |
| Python | 镜像创建工具 (pyfatfs) |
| qemu-system-i386 | 测试模拟器 |

### 15.2 编译标志

```makefile
CFLAGS   = -m32 -march=i686 -ffreestanding -fno-builtin -nostdlib \
           -fno-stack-check -fno-stack-protector \
           -fno-asynchronous-unwind-tables \
           -Wall -Wextra -std=c23 -Os -Iinclude \
           -DMINIMAL_KERNEL

CXXFLAGS = -m32 -march=i686 -ffreestanding -fno-builtin -nostdlib \
           -fno-exceptions -fno-rtti -fno-threadsafe-statics \
           -fno-stack-check -fno-stack-protector \
           -fno-asynchronous-unwind-tables \
           -Wall -Wextra -std=c++23 -Os -Iinclude

ASFLAGS  = --32 -Iinclude
LD_KERN  = -m i386pe -T linker.ld --image-base 0x0 -nostdlib \
           -N --section-alignment 0x200
```

### 15.3 构建产物

| 文件 | 大小 | 说明 |
|------|------|------|
| build/boot.bin | 512B | 软盘引导扇区 |
| build/hd_mbr.bin | 512B | 硬盘 MBR |
| build/hd_vbr.bin | 512B | 硬盘 VBR |
| build/kernel.bin | ~25KB | 32-bit 内核 (含所有子系统) |
| build/plexsdos.img | 1.44MB | 可引导软盘镜像 (FAT12) |
| build/disk.img | 64MB | FAT32 硬盘镜像 |
| build/plexsdos.iso | ~50MB | ISO 9660 可引导光盘 (El Torito) |
| build/plexsdos.vmdk | 40GB | VMDK 虚拟硬盘 (预装系统) |
| build/programs/HELLO.COMX | ~80B | 测试程序 |
| build/programs/PNP.COMX | ~256B | PnP 管理器 |

### 15.4 安装盘

| 镜像 | 内容 |
|------|------|
| `*_install_disks_01.img` | 启动盘 (引导 + 内核 + 安装程序) |
| `*_install_disks_02.img` | 安装盘 2 (内核核心文件) |
| `*_install_disks_03.img` | 安装盘 3 (程序文件) |
| `*_install_disks_04.img` | 安装盘 4 (驱动/库) |
| `*_install_disks_05.img` | 安装盘 5 (文档/示例) |

---

## 16. 安装系统

MS-DOS 风格的多盘安装系统:

- 文件: `kernel/installer.c`, `include/plexsdos/installer.h`
- 从软盘启动时自动进入安装模式
- 提示用户按顺序插入安装盘
- 文件复制显示: 文件名 + 大小
- 错误重试: 最多 3 次
- 磁盘验证: 读取引导扇区校验
- 安装目标: 硬盘 (写入 MBR, 创建 FAT32, 复制文件)
- 磁盘检测: `MINIMAL_KERNEL` 定义可跳过安装

---

## 17. HAL (硬件抽象层)

### 17.1 块设备框架

统一的块设备操作接口:

```c
struct hal_blkdev_ops {
    bool (*read)(uint32_t lba, uint8_t count, void *buf);
    bool (*write)(uint32_t lba, uint8_t count, const void *buf);
};

void hal_blkdev_register(int type, int unit, struct hal_blkdev_ops *ops);
```

注册的设备: FDC 0 (A:), FDC 1 (B:), ATA/AHCI (C:)

### 17.2 ISA 设备枚举

- `isa_init()` — 扫描传统 ISA 设备
- I/O 端口范围检测
- IRQ 冲突检测

---

## 18. 用户账户系统

### 18.1 功能

- 多用户管理
- 用户名 + 用户 ID
- Shell 命令: `users list/add/del`

### 18.2 API

```c
void users_init(void);
int  user_create(const char *name);
int  user_delete(int uid);
int  user_find(const char *name);
```

---

## 19. HDD 启动规格

### 19.1 MBR 布局

| 偏移 | 大小 | 字段 |
|------|------|------|
| 0x000 | 446 | 引导代码 (hd_mbr.S) |
| 0x1BE | 64 | 分区表 (4×16 字节) |
| 0x1FE | 2 | 引导签名 (0x55AA) |

### 19.2 VBR 参数

- FAT32 BPB 完整填充
- 通过 `tools/fix_vbr.py` 自动修正
- 内核从 FAT32 分区数据区读取

---

## 20. 测试规格

### 20.1 测试环境

| 模式 | 命令 | 说明 |
|------|------|------|
| 软盘 + 硬盘 | `make run` | 标准测试 |
| 仅软盘 | `make run-floppy` | 无硬盘测试 |
| 光盘 | `make run-iso` | ISO 引导测试 |
| VMDK | `make run-vmdk` | 虚拟硬盘启动 |
| 辅助 | Bochs | 精确调试 (可选) |

### 20.2 测试矩阵

| 测试项 | 验证方式 |
|--------|---------|
| 引导 | QEMU 启动到 Shell 提示符 |
| CPU | CPUID 全特性检测, 串口输出 |
| 分页 | CR0.PG 启用, 页表建立 |
| GDT | Ring 0-3 段选择子 |
| 异常 | RSOD 红屏显示 |
| PCI | AHCI/ATA 控制器扫描 |
| AHCI | SATA 设备识别, 读写测试 |
| FDC | 马达控制, 软盘读写 |
| CD-ROM | ATAPI 识别, ISO 9660 挂载 |
| FAT32 | 文件列表, 读取 |
| Shell | 全部内置命令测试 |
| .comx | 程序加载, 执行, 返回 |
| 系统调用 | INT 21h 子功能 |
| 调度器 | 多任务切换 |
| GUI | 桌面/窗口/小部件 |
| 安装程序 | 多盘安装流程 |
| VMDK | 虚拟硬盘启动 |

---

## 21. 项目目录结构

```
PlexsDOS/
├── Makefile                  # 构建系统 (软盘/硬盘/ISO/VMDK/安装盘)
├── linker.ld                 # 链接器脚本 (内核 0x30000, BSS 0x200000)
├── .gitignore
├── requirements.txt          # Python 依赖 (pyfatfs)
├── AGENTS.md                 # 开发规范
├── CLAUDE.md                 # 项目指令
├── README.md                 # 项目简介
├── boot/
│   ├── boot_sector.S         # 软盘引导扇区 (16-bit → 32-bit PM)
│   ├── hd_mbr.S              # 硬盘 MBR (分区表扫描)
│   └── hd_vbr.S              # 硬盘 VBR (FAT32 引导)
├── kernel/
│   ├── kernel_entry.S        # 内核入口 (32-bit PM)
│   ├── kernel_main.c         # 内核主函数
│   ├── hd_boot.S             # HDD 启动代码 (嵌入 MBR/VBR)
│   ├── loader.c              # .comx 程序加载器
│   ├── installer.c           # MS-DOS 风格安装程序
│   ├── arch/
│   │   ├── cpu.c             # CPU 特性检测 (CPUID)
│   │   ├── gdt.c             # GDT 初始化 (Ring 0-3 + TSS)
│   │   ├── gdt_load.S        # GDT 加载 (LGDT)
│   │   ├── idt.c             # IDT 管理 + 8259A PIC
│   │   ├── interrupt.S       # 中断处理入口 (汇编)
│   │   ├── interrupt_mgr.cpp # C++ 中断管理器
│   │   ├── panic.c           # 红屏 (RSOD) 内核恐慌
│   │   └── syscall.c         # INT 21h 系统调用
│   ├── drivers/
│   │   ├── ahci.c            # AHCI SATA 驱动
│   │   ├── cdrom.c           # ATAPI CD-ROM + ISO 9660
│   │   ├── disk.c            # ATA PIO/DMA 磁盘驱动
│   │   ├── drive.c           # 驱动器抽象 (A:-E:)
│   │   ├── fdc.c             # FDC 软盘控制器 (马达状态机)
│   │   ├── keyboard.c        # PS/2 键盘驱动
│   │   ├── mouse.c           # PS/2 鼠标驱动
│   │   ├── pci.c             # PCI 总线枚举
│   │   ├── screen.c          # VGA 文本模式
│   │   ├── serial.c          # COM1 串口调试
│   │   └── vga.c             # VGA 图形模式
│   ├── fs/
│   │   ├── fat12.c           # FAT12 文件系统
│   │   ├── fat32.c           # FAT32 文件系统
│   │   └── fs.c              # 虚拟文件系统抽象
│   ├── mm/
│   │   ├── paging.c          # 分页管理 (4KB 页)
│   │   └── pfa.c             # 物理帧分配器
│   ├── shell/
│   │   ├── shell.c           # 命令行 Shell
│   │   └── users.c           # 用户账户管理
│   ├── sched/
│   │   └── scheduler.c       # 进程调度器 (抢占式)
│   ├── gui/
│   │   ├── desktop.c         # 桌面环境
│   │   ├── wm.c              # 窗口管理器
│   │   └── widgets.c         # GUI 小部件
│   ├── editor/
│   │   └── editor.c          # 文本编辑器
│   ├── hal/
│   │   ├── hal_init.cpp      # HAL 初始化
│   │   ├── hal_io.c          # HAL I/O 操作
│   │   └── isa.c             # ISA 设备枚举
│   ├── dm/
│   │   ├── plxdm_*.c         # PlexsDM 显示管理器 (LightDM 移植)
│   │   └── lightdm-core/     # LightDM 原始参考代码
│   ├── shim/
│   │   ├── dfan.c            # DFAN 兼容层
│   │   ├── int21.c           # INT 21h 额外处理
│   │   ├── onebus.c          # OneBus 总线抽象
│   │   ├── syslog.c          # Syslog 兼容接口
│   │   ├── usrgn.c           # 用户登录管理器
│   │   └── x12.c             # X12 协议适配
│   └── debug/
│       └── debug.c           # 调试工具
├── include/
│   ├── plexsdos/             # 内核头文件
│   ├── libc/                 # 标准 C 库头文件
│   ├── libcpp/               # C++ 标准库头文件
│   ├── X11/                  # X11 协议头文件 (兼容)
│   ├── xcb/                  # XCB 头文件
│   └── sys/                  # POSIX 兼容头文件
├── lib/
│   ├── fast_mem.c            # 快速内存操作 (SSE2/AVX 分派)
│   └── string.c              # 字符串库
├── libcpp/                   # C++ 头文件 (algorithm, array 等)
├── programs/
│   ├── test_hello.S          # 测试程序 (HELLO.COMX)
│   ├── pnp.S                 # PnP 管理器 (PNP.COMX)
│   └── README.TXT            # 测试文本文件
├── tools/
│   ├── mkfloppy.py           # FAT12 软盘镜像创建
│   ├── mkbootdisk.py         # 可引导安装盘创建
│   ├── mkfat12.py            # FAT12 镜像创建
│   ├── mkfat32.py            # FAT32 硬盘镜像
│   ├── mkcomx.py             # .comx 打包工具
│   ├── mkiso.py              # ISO 9660 可引导光盘
│   ├── mkvmdk.py             # VMDK 虚拟硬盘
│   └── fix_vbr.py            # VBR BPB 修正
├── docs/
│   └── superpowers/specs/    # 设计文档
├── Python/                   # Python 运行时 (本地)
└── temp/                     # 临时文件
```

---

## 22. Make 构建目标

| 目标 | 说明 |
|------|------|
| `all` | 完整构建 (软盘+硬盘+ISO+安装盘) |
| `disk` | 仅构建 FAT32 硬盘镜像 |
| `install-disks` | 构建 5 张安装软盘 |
| `iso` | 构建 ISO 9660 可引导光盘 |
| `vmdk` | 构建 VMDK 虚拟硬盘 |
| `run` | QEMU 测试 (软盘+硬盘) |
| `run-floppy` | QEMU 测试 (仅软盘) |
| `run-iso` | QEMU 测试 (光盘+硬盘) |
| `run-iso-only` | QEMU 测试 (仅光盘) |
| `run-vmdk` | QEMU 测试 (VMDK 启动) |
| `clean` | 清理构建产物 |

---

*Nexsteaduser — PlexsDOS*
*作者: Tinmc189623 | 团队: Nexlyh*
