# PlexsDOS — 项目指令

## 项目概述

PlexsDOS 是一个面向 x86 32-bit 保护模式的类 DOS 操作系统，采用自研内核。使用 GCC + GAS (AT&T 语法) + Make 工具链构建。

## 品牌规范

- 品牌名：Nexsteaduser
- 项目作者：Tinmc189623
- 团队：Nexlyh
- 联系邮箱：admin@nexsteaduser.com

## 代码规范

### 汇编 (GAS AT&T 语法)

- 所有汇编必须使用 GAS（GNU Assembler）并采用 AT&T 语法
- 操作数顺序：`source, destination`
- 寄存器前缀 `%`，立即数前缀 `$`
- 内存操作数格式：`offset(%base, %index, scale)`
- 指令后缀标明操作数大小：`movb`, `movw`, `movl`
- 文件扩展名：`.S`（需要预处理）或 `.s`（纯汇编）

### C 代码

- 标准：C23
- 使用 `-ffreestanding -fno-builtin -nostdlib` 编译选项
- 所有函数必须添加函数级注释
- 禁止使用占位符代码，所有代码必须真实可运行
- 代码中涉及品牌时必须使用完整名称「Nexsteaduser」

### C++ 代码

- 标准：C++23
- 使用 `-fno-exceptions -fno-rtti -fno-threadsafe-statics` 编译选项
- 中断管理器使用面向对象设计 (InterruptManager / InterruptHandler)
- 与 C 代码通过 `extern "C"` 互操作

### 文件头注释

```c
/*
 * Nexsteaduser — PlexsDOS
 * 文件描述
 * 作者: Tinmc189623
 * 团队: Nexlyh
 */
```

## 构建系统

- 使用 GNU Make 作为构建系统
- 工具链：GCC (mingw32) + GAS + LD (32-bit 目标, `-m32`)
- C 标准：C23 (`-std=c23`)
- C++ 标准：C++23 (`-std=c++23`, `-fno-exceptions -fno-rtti`)
- 链接器脚本：内核 0x30000, BSS 0x200000
- 构建产物：可引导软盘镜像 (`.img`), ISO 9660 光盘 (`.iso`), VMDK 虚拟硬盘 (`.vmdk`)

## 项目结构

```
PlexsDOS/
├── boot/          # 引导扇区 (boot_sector.S, hd_mbr.S, hd_vbr.S)
├── kernel/        # 内核代码
│   ├── arch/      # CPU/GDT/IDT/中断/红屏/系统调用
│   ├── drivers/   # 设备驱动 (AHCI/ATA/FDC/CD-ROM/键盘/鼠标/屏幕/串口/PCI)
│   ├── fs/        # 文件系统 (FAT12/FAT32/ISO 9660)
│   ├── mm/        # 内存管理 (分页/物理帧分配)
│   ├── shell/     # 命令行 Shell + 用户管理
│   ├── sched/     # 进程调度器
│   ├── gui/       # 桌面/窗口管理器/小部件
│   ├── editor/    # 文本编辑器
│   ├── hal/       # 硬件抽象层 (块设备/ISA)
│   ├── dm/        # 显示管理器 (PlexsDM / LightDM 移植)
│   └── shim/      # 兼容适配层
├── include/       # 头文件 (plexsdos/libc/libcpp/X11/sys)
├── lib/           # 库函数 (fast_mem, string)
├── libcpp/        # C++ 头文件 (algorithm, array, type_traits)
├── programs/      # 外部程序 (test_hello.S, pnp.S)
├── tools/         # 构建工具 (Python: 镜像创建/打包)
├── docs/          # 文档
├── Makefile       # 主构建脚本
├── linker.ld      # 链接器脚本
└── requirements.txt  # Python 依赖
```

## 程序接口
- int 0x21 (DOS 兼容系统调用, 向量 0x22)
- C++ InterruptManager / InterruptHandler OOP 框架

## 测试与验证

- QEMU 测试：
  - `make run` — 软盘 + 硬盘
  - `make run-floppy` — 仅软盘
  - `make run-iso` — 光盘 + 硬盘
  - `make run-vmdk` — 虚拟硬盘启动
- 使用 Bochs 进行精确硬件模拟调试
- 每个模块完成后必须在模拟器中验证功能

## 禁止事项

- 禁止模拟任何代码
- 禁止使用占位符
- 禁止任何「Nexstead」表述
- 禁止拆分「Nexsteaduser」品牌名
- 禁止删除项目作者「Tinmc189623」任何字符
