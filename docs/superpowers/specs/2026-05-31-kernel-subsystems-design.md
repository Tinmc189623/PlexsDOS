# Nexsteaduser PlexsDOS — 内核子系统扩展设计

作者: Tinmc189623 | 团队: Nexlyh
日期: 2026-05-31

## 概述

为 PlexsDOS 内核添加 5 个独立子系统，按依赖顺序分阶段实现：

1. **C23 迁移** — 全项目所有 C 代码统一升级到 C23
2. **红屏 (RSOD)** — 内核恐慌诊断屏幕
3. **C++ INT 子系统** — 用 C++ 重写中断/系统调用分发
4. **软盘马达改进 + MS-DOS 风格安装程序** — 多格式、检测、写入、状态机、换盘提示
5. **CD-ROM 驱动** — 完整 ATAPI + ISO 9660

## Phase 1: C99 → C23 迁移

### 目标
**全项目所有 C 代码统一使用 C23 标准**（不仅是新代码）。利用现代 C 特性。

### 改动
- `Makefile`: `-std=c99` → `-std=c23`
- `include/plexsdos/types.h`: 移除 `#define bool int`，使用 C23 原生 `bool`/`true`/`false`
- `include/plexsdos/types.h`: 移除 `#define NULL ((void *)0)`，使用 C23 `nullptr`
- 审计所有 `.c` 文件，修复 C23 不兼容模式
- 更新 `CLAUDE.md` 中的 C 标准说明

### C23 新特性利用
- `bool`/`true`/`false` 作为关键字（不再需要 `#define`）
- `nullptr` 替代 `NULL`
- `typeof` 运算符用于宏
- `constexpr` 用于编译期常量
- `static_assert` 用于编译期断言
- `auto` 类型推断（谨慎使用）

### 兼容性
- GCC 16.1 完全支持 `-std=c23`
- 当前代码已很规范，预计改动量极小

---

## Phase 2: 红屏 (Red Screen of Death)

### 目标
类似 Windows BSOD 的内核恐慌屏幕，用于不可恢复错误的诊断显示。

### 文件
- `include/plexsdos/panic.h` — 公共 API
- `kernel/arch/panic.c` — 实现

### API
```c
/* 触发内核恐慌，显示红屏诊断信息后死循环 */
_Noreturn void kernel_panic(const char *fmt, ...);
```

### CPU 异常处理程序
注册到 IDT 0x00-0x1F 的关键异常：
- `#DE` (0x00) 除零错误
- `#DB` (0x01) 调试异常
- `#BP` (0x03) 断点
- `#UD` (0x06) 无效操作码
- `#DF` (0x08) 双重错误
- `#TS` (0x0A) 无效 TSS
- `#NP` (0x0B) 段不存在
- `#SS` (0x0C) 栈段错误
- `#GP` (0x0D) 一般保护错误
- `#PF` (0x0E) 页错误
- `#MF` (0x10) 浮点错误
- `#AC` (0x11) 对齐检查
- `#MC` (0x12) 机器检查

### 显示布局
```
┌────────────────────────────────────────────────────┐
│              Nexsteaduser PlexsDOS                  │
│          *** KERNEL PANIC ***                       │
│                                                     │
│  Error: <error_type> at <address>                   │
│  Error Code: <code>                                 │
│                                                     │
│  Registers:                                         │
│    EAX=0x________  EBX=0x________  ECX=0x________  │
│    EDX=0x________  ESI=0x________  EDI=0x________  │
│    EBP=0x________  ESP=0x________  EIP=0x________  │
│    CS=0x__  DS=0x__  SS=0x__  EFLAGS=0x________   │
│    CR0=0x________  CR2=0x________  CR3=0x________  │
│                                                     │
│  Stack (8 words):                                   │
│    <地址>: <4 个 32 位值>                            │
│                                                     │
│  System halted. Press Ctrl+Alt+Del to reboot.       │
└────────────────────────────────────────────────────┘
```

### 实现细节
- 红底白字 (bg=0x04, fg=0x0F) 全屏填充
- 通过内联汇编读取 CR0/CR2/CR3 寄存器
- 栈回溯通过 EBP 链遍历（最多 10 帧）
- `kernel_panic` 使用可变参数格式化消息
- 最终 `cli; hlt; jmp .` 死循环
- 同时输出到串口 (serial) 便于调试
- 在 `kernel_main.c` 的 `idt_init()` 之后注册异常处理程序

---

## Phase 3: C++ INT 子系统

### 目标
用 C++ 重写中断管理和系统调用分发，提供面向对象的中断处理框架。

### Makefile 改动
```makefile
CXX      = /c/msys64/mingw32/bin/g++
CXXFLAGS = -m32 -march=i686 -ffreestanding -fno-builtin -nostdlib \
           -fno-exceptions -fno-rtti -fno-threadsafe-statics \
           -fno-stack-check -fno-stack-protector \
           -fno-asynchronous-unwind-tables \
           -Wall -Wextra -std=c++23 -Os -Iinclude
```

编译规则：
```makefile
build/%.o: %.cpp
	$(CXX) $(CXXFLAGS) -c $< -o $@
```

链接改动：
```makefile
LD_KERN = -m i386pe -T linker.ld -nostdlib -N -lstdc++
```

### 文件
- `include/plexsdos/interrupt.hpp` — C++ 中断管理器头文件
- `kernel/arch/interrupt.cpp` — C++ 中断管理器实现
- `kernel/arch/interrupt.S` — 保留汇编入口桩（调用 C++ 函数）

### 类设计
```cpp
// 中断处理程序抽象基类
class InterruptHandler {
public:
    virtual void handle(uint32_t vector, uint32_t error_code) = 0;
    virtual ~InterruptHandler() = default;
};

// 中断管理器（单例）
class InterruptManager {
private:
    static InterruptManager s_instance;
    InterruptHandler* m_handlers[256];

public:
    static InterruptManager& instance();

    void init();
    void registerHandler(uint8_t vector, InterruptHandler* handler);
    void unregisterHandler(uint8_t vector);

    // 由汇编桩调用
    extern "C" void dispatch(uint32_t vector, uint32_t error_code);
};

// 系统调用分发器
class SyscallDispatcher : public InterruptHandler {
public:
    void handle(uint32_t vector, uint32_t error_code) override;
    extern "C" uint32_t dispatch_syscall(uint32_t eax, uint32_t edx, uint32_t esi);
};
```

### C/C++ 互操作
- 汇编桩通过 `extern "C"` 调用 C++ 函数
- 现有 C 驱动通过 `extern "C"` 接口调用 C++ 中断管理器
- C++ 代码调用现有 C 函数（`pic_eoi`, `screen_puts` 等）
- 链接时添加 `-lstdc++`

### 迁移策略
- `interrupt.S` 中的汇编桩保持不变
- `idt.c` 中的 IDT 设置代码迁移到 C++（`InterruptManager::init()`）
- 现有的 `interrupt_register()` 函数改为调用 `InterruptManager::registerHandler()`
- 保持 C 兼容层：`extern "C" void interrupt_register(...)` 包装 C++ 调用

---

## Phase 4: 软盘马达改进

### 目标
参考 Linux/DOSBox/FreeBSD 实现，全面改进 FDC 马达控制。

### 参考实现
- Linux `drivers/block/floppy.c` — 定时器驱动的马达自动关闭
- DOSBox FDC 模拟 — 完整 NEC 765 命令集
- FreeBSD `sys/dev/fdc/fdc.c` — 多格式支持

### 改进 1: 马达状态机

```c
enum fdc_motor_state {
    MOTOR_OFF,      /* 马达关闭 */
    MOTOR_STARTING, /* 马达启动中 (等待旋转稳定) */
    MOTOR_ON,       /* 马达运行中 */
    MOTOR_STOPPING  /* 马达关闭中 (延迟关闭) */
};

#define MOTOR_SPINUP_MS     300   /* 旋转稳定时间 */
#define MOTOR_OFF_DELAY_MS  2000  /* 无操作后自动关闭 */
```

- 马达开启后启动内部计时器
- 每次 FDC 操作重置计时器
- 计时器到期后自动关闭马达
- 支持双驱动器独立状态管理

### 改进 2: 软盘存在检测

```c
/* READ ID 命令检测 */
static bool fdc_detect_disk(uint8_t drive);

/* CHANGE LINE 信号 (DIR bit 7) */
static bool fdc_disk_changed(uint8_t drive);
```

- `fdc_init()` 时检测驱动器中是否有磁盘
- 无磁盘时跳过 recalibrate，避免超时等待

### 改进 3: 写入支持

```c
bool fdc_write_sectors(uint8_t drive, uint8_t head, uint8_t cylinder,
                       uint8_t sector, uint8_t count, const void *buf);

/* 写保护检测 (DIR bit 6) */
static bool fdc_write_protected(uint8_t drive);
```

- DMA 编程改为 `DMA_MODE_WRITE` 方向
- WRITE DATA 命令实现
- 写入前检测写保护状态

### 改进 4: 多格式支持

```c
enum fdc_media_type {
    FDC_MEDIA_1440K,  /* 80 cyl, 2 heads, 18 SPT */
    FDC_MEDIA_1200K,  /* 80 cyl, 2 heads, 15 SPT */
    FDC_MEDIA_720K    /* 80 cyl, 2 heads, 9 SPT  */
};
```

- 通过读取 FAT12 BPB 媒体描述符字节检测格式
- SPECIFY 命令根据格式调整步进速率
- `fdc_read_sectors` / `fdc_write_sectors` 使用正确的 SPT

### 改进 5: MS-DOS 风格安装程序

参考 MS-DOS 6.22 安装流程，改进 PlexsDOS 安装程序的换盘交互。

**MS-DOS 安装流程特征：**
1. 清晰的磁盘编号标签："Please insert Setup Disk X in Drive A:"
2. "and press ENTER when ready." 等待用户确认
3. 错误磁盘检测：插入错误盘时报错并重新提示
4. 每个文件复制时显示文件名和大小
5. 复制进度指示

**PlexsDOS 安装流程改进：**

```
Nexsteaduser PlexsDOS Installer
Version 0.1
Author: Tinmc189623 | Team: Nexlyh

Checking for hard disk...
Hard disk detected.
WARNING: This will ERASE all data on the hard disk!
Continue? (Y/N): Y

Starting installation...

[install] MBR written
[install] FAT32 filesystem created

--- Copy files from installation floppies ---

Please insert Setup Disk 2 into Drive A:
and press ENTER when ready.

Reading floppy...
  Copying: KERNEL.BIN (23996 bytes)
  Copying: FAT12.DRV (4096 bytes)
[install] 2 file(s) copied from Disk 2

Please insert Setup Disk 3 into Drive A:
and press ENTER when ready.

Reading floppy...
  Copying: HELLO.COMX (80 bytes)
[install] 1 file(s) copied from Disk 3

...

========================================
  Installation complete!
  Remove all floppies and reboot.
========================================
```

**错误处理：**
- 读取失败："Error reading floppy. Please check the disk and try again."
- 错误磁盘："This does not appear to be Setup Disk X. Please insert the correct disk."
- 写保护："Disk is write-protected. Please use an unprotected disk."

**实现改动（`kernel/installer.c`）：**
- `inst_prompt_disk(disk_num)` — 显示 MS-DOS 风格的换盘提示
- `inst_verify_disk(disk_num, drive)` — 通过读取引导扇区验证磁盘
- `inst_copy_floppy` 重试循环：最多 3 次重试
- 每个文件复制时显示文件名和大小
- 复制完成后显示文件计数

---

## Phase 5: CD-ROM 驱动 (ATAPI + ISO 9660)

### 目标
完整的 CD-ROM 驱动，支持读取 ISO 9660 文件系统。

### 文件
- `include/plexsdos/cdrom.h` — 公共 API
- `kernel/drivers/cdrom.c` — ATAPI + ISO 9660 实现

### ATAPI 硬件层

ATAPI 设备使用 ATA primary channel (0x1F0-0x1F7)，但命令协议不同：
- 发送 `0xA0` (PACKET) 命令
- 12 字节 CDB (Command Descriptor Block)
- 2048 字节扇区大小

```c
/* ATAPI CDB 命令 */
#define ATAPI_CMD_TEST_UNIT_READY  0x00
#define ATAPI_CMD_INQUIRY          0x12
#define ATAPI_CMD_READ_CAPACITY    0x25
#define ATAPI_CMD_READ_10          0x28
#define ATAPI_CMD_READ_TOC         0x43

/* 检测 ATAPI 设备 */
static bool cdrom_detect(void);

/* 发送 PACKET 命令 */
static bool cdrom_packet(uint8_t *cdb, uint8_t cdb_len,
                         void *buf, uint16_t buf_len, bool read);

/* 读取 2048 字节扇区 */
bool cdrom_read_sector(uint32_t lba, void *buf);
```

### ISO 9660 文件系统层

```c
/* Primary Volume Descriptor (sector 16) */
struct iso9660_pvd {
    uint8_t  type;               /* 1 = PVD */
    char     id[5];              /* "CD001" */
    uint8_t  version;
    char     system_id[32];
    char     volume_id[32];
    uint32_t volume_space_size_le;
    uint16_t logical_block_size_le;
    /* ... 其他字段 */
};

/* 目录记录 */
struct iso9660_dir_record {
    uint8_t  length;
    uint8_t  ext_attr_length;
    uint32_t extent_location_le;
    uint32_t extent_size_le;
    uint8_t  date[7];
    uint8_t  file_flags;         /* bit 2 = 目录 */
    uint8_t  file_unit_size;
    uint8_t  interleave_gap;
    uint16_t volume_seq_num_le;
    uint8_t  name_len;
    char     name[];
};
```

### 公共 API
```c
bool cdrom_init(void);
bool cdrom_read_file(const char *path, void *buf, uint32_t buf_size,
                     uint32_t *bytes_read);
bool cdrom_list_root(void);
```

### Shell 集成
- `CDIR` — 列出光盘根目录
- `CCAT <file>` — 读取并显示光盘文件内容
- `CDMOUNT` — 检测并挂载光盘

### 路径解析
- 支持 `/` 分隔的路径（如 `/DIR/FILE.TXT`）
- 逐级遍历目录直到找到目标文件
- 仅支持基本 ISO 9660（不支持 Rock Ridge / Joliet）

---

## 文件清单

### 新增文件
| 文件 | 阶段 | 描述 |
|------|------|------|
| `include/plexsdos/panic.h` | 2 | 红屏 API |
| `kernel/arch/panic.c` | 2 | 红屏实现 |
| `include/plexsdos/interrupt.hpp` | 3 | C++ 中断管理器头文件 |
| `kernel/arch/interrupt.cpp` | 3 | C++ 中断管理器实现 |
| `include/plexsdos/cdrom.h` | 5 | CD-ROM API |
| `kernel/drivers/cdrom.c` | 5 | ATAPI + ISO 9660 实现 |

### 修改文件
| 文件 | 阶段 | 改动 |
|------|------|------|
| `Makefile` | 1,3 | C23 标准, C++ 支持 |
| `include/plexsdos/types.h` | 1 | C23 原生 bool/nullptr |
| `kernel/arch/idt.c` | 3 | 迁移到 C++ |
| `kernel/arch/interrupt.S` | 3 | 汇编桩调用 C++ |
| `kernel/drivers/fdc.c` | 4 | 马达状态机, 写入, 多格式 |
| `include/plexsdos/fdc.h` | 4 | 新增 API 声明 |
| `kernel/kernel_main.c` | 2,3,5 | 注册异常处理, 初始化 CD-ROM |
| `kernel/shell/shell.c` | 5 | CDIR/CCAT/CDMOUNT 命令 |
| `kernel/installer.c` | 4 | MS-DOS 风格换盘提示 |
| `CLAUDE.md` | 1 | 更新 C 标准说明 |

---

## 构建验证

每个阶段完成后必须：
1. `make clean && make` 编译通过
2. QEMU 启动验证基本功能
3. 串口日志确认初始化成功
