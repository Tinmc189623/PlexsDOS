# PlexsDOS — 实现计划

## Nexsteaduser — PlexsDOS 实现计划

作者: Tinmc189623 | 团队: Nexlyh

---

## 阶段总览

| 阶段 | 内容 | 预计工作量 | 依赖 | 状态 |
|------|------|-----------|------|------|
| 1 | 构建系统与项目骨架 | 1 天 | 无 | ✅ 完成 |
| 2 | 引导扇区 | 2 天 | 阶段 1 | ✅ 完成 |
| 3 | 内核入口与基础框架 | 2 天 | 阶段 2 | ✅ 完成 |
| 4 | 屏幕驱动 | 1 天 | 阶段 3 | ✅ 完成 |
| 5 | 键盘驱动 | 2 天 | 阶段 4 | ✅ 完成 |
| 6 | 中断处理框架 | 2 天 | 阶段 3 | ✅ 完成 |
| 7 | Shell 实现 | 3 天 | 阶段 4, 5 | ✅ 完成 |
| 8 | 文件系统 (FAT32) | 3 天 | 阶段 6 | ✅ 完成 |
| 9 | 外部程序加载 | 2 天 | 阶段 7, 8 | ✅ 完成 |
| 10 | 系统调用 (INT 21h) | 2 天 | 阶段 7 | ✅ 完成 |
| 11 | 集成测试与调试 | 2 天 | 阶段 9, 10 | ✅ 完成 |
| 12 | 通用处理器优化 (i686→SSE2→AVX) | 2 天 | 阶段 3 | ✅ 完成 |
| 13 | PCI 总线与 ATA DMA | 3 天 | 阶段 8 | ✅ 完成 |
| 14 | .comx 可执行格式 | 1 天 | 阶段 8, 9 | ✅ 完成 |
| 15 | Python 镜像构建脚本 (pyfatfs) | 1 天 | 阶段 8 | ✅ 完成 |

---

## 阶段 1: 构建系统与项目骨架

### 目标

建立项目目录结构、Makefile、链接器脚本，确保空项目可编译。

### 文件清单

```
PlexsDOS/
├── Makefile
├── linker.ld
├── boot/
│   └── boot_sector.S
├── kernel/
│   └── kernel_entry.S
├── include/
│   └── plexsdos/
│       ├── types.h
│       └── config.h
└── build/          (构建输出，gitignore)
```

### 关键实现

#### Makefile

```makefile
# Nexsteaduser — PlexsDOS Makefile
# 作者: Tinmc189623 | 团队: Nexlyh
# 自研内核 (32-bit 保护模式)

CC      = gcc
AS      = as
LD      = ld
OBJCOPY = objcopy

CFLAGS   = -m32 -ffreestanding -fno-builtin -nostdlib \
           -Wall -Wextra -std=c99 -Os -Iinclude
ASFLAGS  = --32 -Iinclude
LD_KERN  = -m i386pe -T linker.ld -nostdlib

BUILD_DIR = build

SRCS_S := $(wildcard boot/*.S) $(wildcard kernel/*.S) \
          $(wildcard kernel/arch/*.S) $(wildcard kernel/drivers/*.S)
SRCS_C := $(wildcard kernel/*.c) $(wildcard kernel/arch/*.c) \
          $(wildcard kernel/drivers/*.c) $(wildcard kernel/mm/*.c) \
          $(wildcard kernel/shell/*.c) $(wildcard kernel/fs/*.c) \
          $(wildcard lib/*.c)

OBJS := $(patsubst %.S,$(BUILD_DIR)/%.o,$(SRCS_S)) \
        $(patsubst %.c,$(BUILD_DIR)/%.o,$(SRCS_C))

BOOT_OBJ  = $(BUILD_DIR)/boot/boot_sector.o
KERN_OBJS = $(filter-out $(BOOT_OBJ),$(OBJS))

BOOT_BIN   = $(BUILD_DIR)/boot.bin
KERNEL_BIN = $(BUILD_DIR)/kernel.bin
FLOPPY_IMG = $(BUILD_DIR)/plexsdos.img

.PHONY: all clean run

all: $(FLOPPY_IMG)

$(BUILD_DIR)/%.o: %.S
	@mkdir -p $(dir $@)
	$(AS) $(ASFLAGS) $< -o $@

$(BUILD_DIR)/%.o: %.c
	@mkdir -p $(dir $@)
	$(CC) $(CFLAGS) -c $< -o $@

$(BUILD_DIR)/boot.elf: $(BOOT_OBJ)
	$(LD) -m i386pe -Ttext 0x7C00 -nostdlib $< -o $@

$(BOOT_BIN): $(BUILD_DIR)/boot.elf
	$(OBJCOPY) -O binary -j .text $< $@

$(KERNEL_BIN): $(KERN_OBJS)
	$(LD) $(LD_KERN) $^ -o $(BUILD_DIR)/kernel.exe
	$(OBJCOPY) -O binary -j .text -j .rdata -j .rodata -j .data $(BUILD_DIR)/kernel.exe $@

$(FLOPPY_IMG): $(BOOT_BIN) $(KERNEL_BIN)
	dd if=/dev/zero of=$@ bs=512 count=2880
	dd if=$(BOOT_BIN) of=$@ bs=512 count=1 conv=notrunc
	dd if=$(KERNEL_BIN) of=$@ bs=512 seek=1 conv=notrunc

run: $(FLOPPY_IMG)
	qemu-system-i386 -fda $< -m 16M

clean:
	rm -rf $(BUILD_DIR)
```

#### linker.ld

```ld
/* Nexsteaduser — PlexsDOS 链接器脚本 (32-bit 保护模式) */
/* 作者: Tinmc189623 | 团队: Nexlyh */

ENTRY(_start)

SECTIONS
{
    . = 0x1000;

    .text : SUBALIGN(4) {
        *(.text)
    }

    .rdata : SUBALIGN(4) {
        *(.rdata)
        *(.rdata*)
    }

    .rodata : SUBALIGN(4) {
        *(.rodata*)
    }

    .data : SUBALIGN(4) {
        *(.data)
        *(.data*)
    }

    .bss : SUBALIGN(4) {
        *(.bss)
        *(COMMON)
    }

    _end = .;

    /DISCARD/ : {
        *(.reloc)
    }
}
```

### 验证标准

- [ ] `make clean && make` 编译无错误
- [ ] 生成 `build/plexsdos.img` (1.44MB)
- [ ] `make run` 启动 QEMU 并显示引导扇区输出

---

## 阶段 2: 引导扇区

### 目标

实现 512 字节引导扇区，加载内核到内存并跳转。

### 文件清单

```
boot/boot_sector.S       # 引导扇区主代码 (含 BPB、磁盘读取、GDT、PM 切换)
```

### 关键实现

引导扇区职责：
1. 设置段寄存器和栈指针
2. 检测引导驱动器号 (DL)
3. 使用 BIOS INT 13h 读取内核扇区到 0x1000 (16-bit 实模式, 仅引导阶段)
4. 设置 GDT 并切换到 32-bit 保护模式
5. 远跳转到内核入口 0x1000

### 验证标准

- [ ] 引导扇区 512 字节，以 0x55AA 结尾
- [ ] BIOS 正确识别为可引导设备
- [ ] 成功从软盘读取内核到 0x1000
- [ ] 跳转到内核入口点执行

---

## 阶段 3: 内核入口与基础框架

### 目标

建立内核入口点，初始化运行环境，调用 C 语言内核主函数。

### 文件清单

```
kernel/kernel_entry.S      # 内核入口 (汇编)
kernel/kernel_main.c       # 内核主函数 (C)
kernel/arch/gdt.S          # 全局描述符表 (可选)
include/plexsdos/types.h   # 基础类型定义
include/plexsdos/config.h  # 系统配置常量
```

### 关键实现

#### kernel/kernel_entry.S

```gas
# kernel/kernel_entry.S — PlexsDOS 内核入口 (32-bit 保护模式)
# Nexsteaduser — PlexsDOS
# 作者: Tinmc189623 | 团队: Nexlyh

.code32
.section .text

.global _start
_start:
    /* 设置数据段选择子 (GDT 数据段 0x10) */
    mov  $0x10, %ax
    mov  %ax, %ds
    mov  %ax, %es
    mov  %ax, %fs
    mov  %ax, %gs
    mov  %ax, %ss

    /* 设置栈 */
    mov  $0x90000, %esp
    mov  %esp, %ebp

    /* 调用 C 内核主函数 */
    call _kernel_main

    /* kernel_main 不应返回，如返回则停机 */
    cli
    hlt
```

#### include/plexsdos/types.h

```c
/*
 * Nexsteaduser — PlexsDOS
 * 基础类型定义
 * 作者: Tinmc189623 | 团队: Nexlyh
 */

#ifndef _PLXSDOS_TYPES_H
#define _PLXSDOS_TYPES_H

typedef unsigned char      uint8_t;
typedef unsigned short     uint16_t;
typedef unsigned long      uint32_t;
typedef signed char        int8_t;
typedef signed short       int16_t;
typedef signed long        int32_t;
typedef unsigned int       size_t;

#define NULL ((void *)0)

#endif /* _PLXSDOS_TYPES_H */
```

### 验证标准

- [ ] 内核成功从引导扇区接管执行
- [ ] C 函数 kernel_main() 被正确调用
- [ ] 基础类型定义可用

---

## 阶段 4: 屏幕驱动

### 目标

实现 VGA 文本模式屏幕输出，支持基本字符显示、清屏、光标控制。

### 文件清单

```
kernel/drivers/screen.c        # 屏幕驱动实现
kernel/drivers/screen.S        # 底层端口操作 (AT&T 语法)
include/plexsdos/screen.h      # 屏幕驱动接口
```

### 关键 API

```c
void screen_init(void);                    // 初始化屏幕
void screen_clear(void);                   // 清屏
void screen_putchar(char c);               // 输出单个字符
void screen_puts(const char *str);         // 输出字符串
void screen_put_hex(uint32_t val);         // 输出十六进制数
void screen_set_color(uint8_t fg, uint8_t bg); // 设置前景/背景色
void screen_scroll(void);                  // 滚动屏幕
```

### 实现要点

- 直接写 VGA 显存 0xB8000
- 每个字符占 2 字节 (ASCII + 属性)
- 80x25 文本模式
- 光标定位通过端口 0x3D4/0x3D5 控制

### 验证标准

- [ ] 屏幕初始化为黑底白字
- [ ] 字符串正确显示在屏幕上
- [ ] 换行、退格、制表符正确处理
- [ ] 屏幕滚动正常工作
- [ ] 光标位置正确更新

---

## 阶段 5: 键盘驱动

### 目标

实现键盘输入，支持字符读取和命令行编辑。

### 文件清单

```
kernel/drivers/keyboard.c      # 键盘驱动实现
kernel/drivers/keyboard.S      # INT 9h 中断处理 (AT&T 语法)
include/plexsdos/keyboard.h    # 键盘驱动接口
```

### 关键 API

```c
void keyboard_init(void);           // 初始化键盘中断
char keyboard_getchar(void);        // 阻塞读取一个字符
int keyboard_available(void);       // 检查是否有按键
char keyboard_read_line(char *buf, int max_len); // 读取一行输入
```

### 实现要点

- 注册 INT 0x09 中断处理程序
- 维护键盘缓冲区 (环形队列)
- 支持 Shift、Caps Lock、Backspace、Enter
- 扫描码到 ASCII 转换表

### 验证标准

- [ ] 按键输入正确回显到屏幕
- [ ] Shift 键组合正常工作
- [ ] 退格键正确删除字符
- [ ] Enter 键正确提交输入

---

## 阶段 6: 中断处理框架

### 目标

建立统一的中断处理框架。

### 文件清单

```
kernel/arch/interrupt.S          # 中断处理入口 (AT&T 语法)
kernel/arch/idt.c                # 中断描述符表管理
include/plexsdos/interrupt.h     # 中断处理接口
```

### 关键 API

```c
void idt_init(void);                              // 初始化 IDT
void idt_set_gate(int vector, uint16_t selector,
                  uint32_t offset, uint8_t flags); // 设置中断门
void interrupt_register(int vector,
                        void (*handler)(void));     // 注册处理程序
```

### 设计要点

- 统一的中断分发机制
- 中断处理程序注册/注销接口
- 中断嵌套管理（保护模式下支持）

### 验证标准

- [ ] IDT 正确初始化
- [ ] 定时器中断 (IRQ0) 正常触发
- [ ] 键盘中断 (IRQ1) 正常触发
- [ ] 中断处理程序正确分发

---

## 阶段 7: Shell 实现

### 目标

实现命令行 Shell，支持内置命令和命令解析。

### 文件清单

```
kernel/shell/shell.c             # Shell 主逻辑
kernel/shell/builtin.c           # 内置命令实现
kernel/shell/parser.c            # 命令解析器
include/plexsdos/shell.h         # Shell 接口
```

### 内置命令实现顺序

1. `help` — 显示帮助
2. `cls` — 清屏
3. `ver` — 版本信息
4. `echo` — 文本回显
5. `mem` — 内存信息
6. `time` / `date` — 时间日期
7. `reboot` — 重启
8. `dir` — 文件列表 (依赖阶段 8)
9. `type` — 文件查看 (依赖阶段 8)

### 验证标准

- [ ] Shell 提示符正确显示
- [ ] 命令行输入正确解析
- [ ] 所有内置命令功能正常
- [ ] 未知命令显示错误信息
- [ ] 命令行编辑（退格）正常

---

## 阶段 8: 文件系统 (FAT32)

### 目标

实现 FAT32 文件系统读取支持。

### 文件清单

```
kernel/fs/fat32.c                # FAT32 实现
include/plexsdos/fat32.h         # FAT32 内部结构
tools/mkfat32.py                 # FAT32 镜像创建工具
```

### 关键 API

```c
bool fat32_init(void);                              // 初始化 FAT32
void fat32_list_root(void);                         // 列出根目录
struct fat32_dir_entry *fat32_find_file(const char *name); // 查找文件
uint32_t fat32_load_file(struct fat32_dir_entry *entry,    // 加载文件
                         uint32_t load_addr);
```

### 实现要点

- 通过 ATA 磁盘驱动读取扇区 (支持 PIO 和 DMA 模式)
- 解析 FAT32 BPB 获取文件系统参数 (32-bit FAT 条目)
- 遍历 FAT 表获取簇链 (28-bit 有效位)
- 根目录在数据区 (簇 2), 按簇链遍历

### 验证标准

- [ ] 正确读取 FAT32 BPB 参数
- [ ] 根目录列表正确显示
- [ ] 文件内容正确读取
- [ ] `ls` 和 `type` 命令正常工作

---

## 阶段 12: 通用处理器优化 (i686 → SSE2 → AVX)

### 目标

实现全系列 x86 处理器运行时优化，不限于奔腾3。

### 文件清单

```
kernel/arch/cpu.c                # CPU 全特性检测与 SIMD 启用
include/plexsdos/cpu.h           # CPU 接口定义 (SSE~AVX2, 3DNow!)
lib/fast_mem.c                   # 运行时分派内存操作
```

### 关键功能

- CPUID 全特性检测: leaf 1 EDX/ECX, leaf 7 EBX, extended 0x80000001
- SSE 启用: CR4.OSFXSR + CR4.OSXMMEXCPT
- AVX 启用: CR4.OSXSAVE + XSETBV (XMM+YMM 状态)
- 编译器基线: `-march=i686` (通用 32-bit)
- 函数级 SIMD 启用: `__attribute__((target("sse2")))` / `__attribute__((target("avx")))`
- 运行时分派: 基线 (rep movsd) → SSE2 (MOVDQA 128-bit) → AVX (VMOVDQA 256-bit)

### 验证标准

- [x] CPUID 正确检测全系列 CPU 特性
- [x] SSE/AVX 正确启用 (CR4 + XSETBV)
- [x] fast_memcpy 运行时分派到最优路径
- [x] fast_memset 运行时分派到最优路径
- [x] fast_memcmp 运行时分派到最优路径
- [x] `-march=i686` 基线编译无 SIMD 指令冲突

---

## 阶段 13: PCI 总线与 ATA DMA

### 目标

实现 PCI 总线扫描和 ATA DMA 传输支持。

### 文件清单

```
kernel/drivers/pci.c             # PCI 总线扫描
include/plexsdos/pci.h           # PCI 接口定义
kernel/drivers/disk.c            # ATA PIO + DMA 驱动
include/plexsdos/disk.h          # 磁盘接口定义
```

### 关键功能

- PCI 配置空间访问 (I/O 端口 0xCF8/0xCFC)
- IDE 控制器扫描 (Class 0x01, Subclass 0x01)
- Bus Master 启用 (PCI Command 寄存器)
- PRDT (Physical Region Descriptor Table) 设置
- ATA DMA READ (0xC8) 命令执行
- DMA 传输完成检测

### 验证标准

- [ ] PCI 总线正确扫描
- [ ] IDE 控制器正确识别
- [ ] Bus Master 正确启用
- [ ] DMA 传输正常工作
- [ ] PIO 回退模式正常工作

---

## 阶段 9: .comx 程序加载

### 目标

支持加载和执行 .comx 格式外部程序 (自研 32-bit 可执行格式)。

### 文件清单

```
kernel/loader.c                  # .comx 程序加载器
include/plexsdos/loader.h        # 加载器接口
include/plexsdos/comx.h          # .comx 格式定义
tools/mkcomx.py                  # .comx 打包工具
```

### 实现要点

- .comx 格式: 32 字节头部 (魔数、版本、标志、入口、代码大小、BSS、加载地址、校验和)
- 从 FAT32 硬盘读取 .comx 文件
- 验证头部魔数 (0x43505800)、版本、校验和
- 检查 CPU 特性需求 (SSE/SSE2/MMX flags)
- 复制代码到加载地址 (默认 0x20000)，清零 BSS
- 跳转到入口点执行

### 验证标准

- [x] 加载 .comx 程序到正确地址
- [x] 程序正确执行并返回 Shell
- [x] 校验和验证正确工作
- [x] CPU 特性需求检查正确
- [x] 无效文件显示错误信息

---

## 阶段 10: 系统调用 (INT 21h)

### 目标

实现 DOS 兼容的 INT 21h 系统调用接口。

### 文件清单

```
kernel/syscall.S                 # INT 21h 处理程序 (AT&T 语法)
kernel/syscall.c                 # 系统调用分发
include/plexsdos/syscall.h       # 系统调用定义
```

### 实现的子功能

按优先级排序：
1. AH=0x4C: 程序终止
2. AH=0x02: 写字符
3. AH=0x09: 写字符串
4. AH=0x01: 读字符
5. AH=0x0A: 读字符串
6. AH=0x25/0x35: 中断向量操作

### 验证标准

- [ ] INT 21h 正确分发到对应处理函数
- [ ] 外部程序可调用所有已实现的子功能
- [ ] 程序终止正确返回 Shell

---

## 阶段 14: .comx 可执行格式

### 目标

发明 .comx 自研 32-bit 可执行格式，替代原始 flat binary。

### 文件清单

```
include/plexsdos/comx.h          # .comx 格式定义 (32 字节头部)
kernel/loader.c                  # .comx 解析与加载器
tools/mkcomx.py                  # .comx 打包工具
programs/test_hello.S            # 测试程序 (.comx 输出)
```

### 验证标准

- [x] .comx 头部结构定义完整
- [x] mkcomx.py 正确生成 .comx 文件
- [x] 内核 loader 正确解析和验证 .comx
- [x] HELLO.COMX 在 QEMU 中正确执行

---

## 阶段 15: Python 镜像构建脚本 (pyfatfs)

### 目标

使用 Python + pyfatfs 外部库构建 FAT12/FAT32 镜像，替代手写 dd 命令。

### 文件清单

```
tools/mkfloppy.py                # FAT12 软盘镜像创建 (1.44MB)
tools/mkfat32.py                 # FAT32 硬盘镜像创建 (64MB, 使用 pyfatfs)
requirements.txt                 # Python 依赖 (pyfatfs>=1.1.0)
```

### 验证标准

- [x] pyfatfs 正确安装和导入
- [x] mkfat32.py 创建 64MB FAT32 镜像
- [x] mkfloppy.py 创建 1.44MB FAT12 软盘镜像
- [x] 文件正确写入 FAT32 镜像
- [x] 内核 FAT32 驱动正确读取镜像中的文件

---

## 阶段 11: 集成测试与调试

### 目标

全面测试系统功能，修复 bug，完善文档。

### 测试矩阵

| 测试项 | 方法 | 预期结果 |
|--------|------|---------|
| 冷启动 | QEMU 从软盘引导 | Shell 提示符出现 |
| 热重启 | `reboot` 命令 | 系统重新引导 |
| 字符输入 | 键盘输入测试 | 正确回显 |
| 屏幕输出 | 多行文本 | 正确显示、滚动 |
| 内存显示 | `mem` 命令 | 正确显示内存状态 |
| 文件列表 | `dir` 命令 | 正确列出根目录文件 |
| 文件查看 | `type FILE.TXT` | 正确显示文件内容 |
| 程序执行 | 运行测试程序 | 程序正常运行并返回 |

---

*Nexsteaduser — PlexsDOS*
*作者: Tinmc189623 | 团队: Nexlyh*
