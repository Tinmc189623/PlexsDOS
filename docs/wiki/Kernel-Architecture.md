# Plexs 内核架构

> **Nexsteaduser PlexsDOS** 采用 **宏内核（Monolithic Kernel）** 架构，所有核心子系统运行在单一内核地址空间（Ring 0）。

---

## 架构总览

Plexs 是一个面向 x86 32 位保护模式的宏内核，所有子系统（内存管理、进程调度、驱动程序、文件系统、系统调用接口、Shell、GUI）均静态链接进同一个 `kernel.bin` 二进制文件，在最高特权级（Ring 0）直接执行。子系统之间通过直接 C 函数调用交互，不使用 IPC 消息传递，无用户态/内核态边界分离。

```
┌─────────────────────────────────────────────────────────────────┐
│                        用户接口层                                │
│  Shell (命令行)  │  EDIT.COM (编辑器)  │  GUI (VBE 图形)        │
│  安装程序        │  用户账户系统       │  内置命令集             │
├─────────────────────────────────────────────────────────────────┤
│                      系统调用接口 (INT 0x22)                     │
│            interrupt_manager (C++ OOP 分发器)                    │
├─────────────────────────────────────────────────────────────────┤
│  文件系统层       │  进程调度        │   驱动框架                 │
│  FAT32           │  sched.c        │   keyboard (PS/2)         │
│  ISO 9660        │  协作/抢占调度   │   mouse    (PS/2)         │
│  drive manager   │  上下文切换      │   PCI bus enumerator      │
│  MBR 分区扫描    │                 │   AHCI SATA               │
│  CONFIG.SYS      │                 │   ATA PIO                 │
│                  │                 │   FDC (floppy)            │
│                  │                 │   CD-ROM (ATAPI)          │
│                  │                 │   ISA legacy devices      │
├─────────────────────────────────────────────────────────────────┤
│                     HAL 硬件抽象层                               │
│               hal_blkdev (块设备统一接口)                         │
├─────────────────────────────────────────────────────────────────┤
│                      内存管理                                    │
│         Paging (4KB 页面)  │  PFA (物理帧分配器)                  │
│         fast_memcpy/fast_memset (SSE2/AVX 优化)                  │
├─────────────────────────────────────────────────────────────────┤
│              CPU 与中断底层                                       │
│  GDT (Ring 0-3) │ IDT (256 vectors) │ 8259 PIC │ panic/异常处理  │
│  CPU vendor/feature detection   │ serial debug (COM1)           │
├─────────────────────────────────────────────────────────────────┤
│                     引导加载器 (NASM)                             │
│  MBR (bootsect) → Stage 1.5 (boot16, A20) → Stage 2 (boot32, PM)│
│  → 加载 kernel.bin 到 0x100000 → 跳转 kernel_main()             │
└─────────────────────────────────────────────────────────────────┘
```

---

## 宏内核特征

| 特征 | Plexs 宏内核实现 |
|------|-----------------|
| **地址空间** | 单一内核地址空间，所有子系统共享同一张页表 |
| **特权级** | 全部代码运行在 Ring 0，无 Ring 3 用户态 |
| **子系统交互** | 直接 C/C++ 函数调用，无 IPC、无消息队列、无 RPC |
| **驱动模型** | 驱动静态链接进内核，直接调用内核函数，无 UIO/IOMMU |
| **系统调用** | 软中断 `INT 0x22`，入口即内核函数，零特权级切换开销 |
| **内存隔离** | 进程之间无严格地址空间隔离（开发中，调度器框架已就绪） |
| **链接方式** | 全部静态链接，内核为单一 ELF 可执行文件 |
| **内核体积** | 紧凑（约数十 KB），适合嵌入式和教学场景 |

---

## 宏内核 vs 微内核对比

Plexs 选择宏内核架构而非微内核，原因：

1. **性能优先**：操作系统开发初期，文件系统、驱动、Shell 之间频繁交互，宏内核的直接函数调用比 IPC 快几个数量级。
2. **实现简洁**：单地址空间模型简单，无需设计复杂的 IPC 协议、消息序列化、权限传递机制。
3. **教学价值**：宏内核架构清晰，从引导到 Shell 一条链路，方便学习操作系统各层交互。
4. **传统 DOS 兼容精神**：项目名为 Plexs"DOS"，秉承单体内核、直接硬件访问的传统。

> 注：Plexs 宏内核在保持传统宏内核性能优势的同时，引入了 HAL（硬件抽象层）和 C++ 面向对象中断管理器，在内部实现了一定程度的模块化和接口抽象，为未来的扩展性留有余地。

---

## 核心模块详解

### 1. 引导系统（boot/）

使用 NASM 汇编编写，三阶段引导：

| 阶段 | 文件 | 模式 | 职责 |
|------|------|------|------|
| Stage 1 | `boot/bootsect.asm` | 16-bit 实模式 | MBR 引导扇区（512 字节），加载 Stage 1.5 |
| Stage 1.5 | `boot/boot16.asm` | 16-bit 实模式 | 启用 A20 地址线，检测内存，加载 Stage 2 |
| Stage 2 | `boot/boot32.asm` | 32-bit 保护模式 | 设置临时 GDT/页表，启用分页，加载 kernel.bin 到 0x100000 |

引导加载器最终跳转到 `kernel_main()`（定义在 `kernel/kernel_main.c`）。

### 2. CPU 与中断底层

- **GDT** (`kernel/gdt.c`): 定义 Ring 0 代码/数据段、Ring 3 代码/数据段、TSS 段描述符。
- **IDT** (`kernel/idt.c`): 256 个中断向量，包含 CPU 异常（0-31）和 IRQ（32-47）。
- **8259 PIC** (`kernel/idt.c`): 主从两片 8259A 可编程中断控制器，重映射到 IRQ 32-47。
- **Panic** (`kernel/panic.c`): 异常处理，寄存器转储，红屏死机（BSOD 风格）。
- **CPU 检测** (`kernel/cpu.c`): 通过 CPUID 指令获取厂商字符串和特性位。
- **串口调试** (`kernel/serial.c`): COM1 (0x3F8) 115200 8N1，实时内核日志输出。

### 3. 内存管理

- **分页** (`kernel/paging.c`): 4KB 页面大小，二级页表（x86 32-bit 标准），内核空间映射。
- **物理帧分配器** (`kernel/paging.c`): 位图/栈式物理页管理，支持分配和释放。
- **Fast Memory Ops** (`kernel/string.c`): 根据 CPU 能力选择 SSE2/AVX/REP MOVSD 优化路径，`fast_memcpy`/`fast_memset` 在 `fast_mem_init()` 后生效。

### 4. 驱动框架

驱动使用 C++ 编写（部分基础驱动用 C），全部静态链接：

| 驱动 | 语言 | 总线 | 说明 |
|------|------|------|------|
| keyboard | C | PS/2 | 扫描码 Set 1 → ASCII，Shift/Caps/NumLock 处理 |
| mouse | C | PS/2 | 3 字节字节流解析，光标事件 |
| PCI | C | PCI Config Space | 总线枚举，Vendor ID/Device ID 识别，BAR 读取 |
| AHCI | C | PCI SATA | GHC/PI 检测，HBA 端口初始化，PIO 读/写 |
| ATA | C | IDE PIO | LBA28 寻址，主/从盘检测，单块/多块读写 |
| FDC | C | ISA | NEC 765 兼容软盘控制器，DMA 传输 |
| CD-ROM | C | ATAPI | SCSI 命令包，READ TOC/READ(10) |
| VGA TTY | C | MMIO | 80×25 文本模式，颜色属性，光标控制 |
| ISA PnP | C | ISA | 传统 ISA 设备枚举（硬盘启动时启用） |

### 5. HAL 硬件抽象层

`kernel/hal/` 目录提供块设备统一抽象接口 `hal_blkdev_ops`：

```c
struct hal_blkdev_ops {
    bool (*read)(uint32_t lba, uint8_t count, void *buf);
    bool (*write)(uint32_t lba, uint8_t count, const void *buf);
};
```

FDC0/FDC1（软盘 A:/B:）和 ATA/AHCI（硬盘）统一注册到 HAL 层，上层文件系统无需关心底层是 PIO 还是 DMA 还是 AHCI。当 AHCI 初始化成功时，HAL 自动覆盖默认的 `disk_read_sectors` 实现为 AHCI 路径。

### 6. 文件系统

| 文件系统 | 实现文件 | 说明 |
|----------|----------|------|
| FAT32 | `kernel/fs/fat32.c` | VBR 解析、FAT 表遍历、簇链读取、短文件名（8.3）和长文件名（LFN）、目录遍历 |
| ISO 9660 | `kernel/fs/iso9660.c` | 主卷描述符、目录记录、路径表、Rock Ridge 兼容 |
| Drive Manager | `kernel/fs/drive.c` | 驱动器号管理（A:-Z:），按类型/盘符注册和查找 |
| MBR Scanner | `kernel_main.c` 内联 | 扫描 MBR 分区表，自动识别并挂载 FAT12/16/32 分区到 E: 之后 |

### 7. 进程调度

- `kernel/sched.c` 提供调度器框架。
- 当前支持协作式调度，抢占式调度基础代码就绪。
- 进程控制块（PCB）结构保存 CPU 上下文、栈指针、状态等。
- 调度器初始化后可在未来支持 Ring 3 用户进程。

### 8. 系统调用接口

- 系统调用通过软中断 `INT 0x22` 触发（非 DOS 的 INT 21h，避免混淆）。
- `kernel/interrupt.hpp` 和 `kernel/interrupt.cpp` 使用 C++ 面向对象实现中断管理器。
- 中断处理器通过注册机制绑定 ISR 到对应的 IRQ/异常向量。
- 系统调用号通过 EAX 传递，参数通过 EBX/ECX/EDX 传递（x86 32-bit 约定）。

### 9. 用户接口

- **Shell** (`kernel/shell.c`): 命令行解释器，支持 28+ 内置命令、命令行编辑、历史记录。
- **EDIT.COM** (`kernel/edit.c`): 全屏可视文本编辑器，支持光标移动、文本插入/删除、文件保存。
- **GUI** (`kernel/gui/`): VBE VESA 图形模式框架，窗口管理基础。
- **Installer** (`kernel/installer.c`): 安装程序，分区选择、文件复制、引导扇区写入。
- **Users** (`kernel/users.c`): 用户账户管理，登录/注销、密码设置、权限控制。

### 10. 配置系统

- `kernel/config_sys.c` 实现 CONFIG.SYS 解析。
- CONFIG.SYS 文件在编译时通过 `objcopy` 嵌入到内核二进制的 `binary_programs_CONFIG_SYS_start/end` 区域。
- 启动早期使用 `fast_memcpy` 复制到栈缓冲区后解析（因为 fast_mem_init 在这之前完成）。

---

## 内存布局

```
0x00000000 ─┬─  NULL 保护区 (4KB)
            │
0x00001000 ─┤  内核栈 / 临时数据
            │
0x00100000 ─┤  kernel.bin 加载基址 (1MB)
            │  .text 代码段
            │  .rodata 只读数据
            │  .data 已初始化数据
            │  .bss 未初始化数据
            │
0x00200000 ─┤  内核堆（动态分配区域）
            │
0x00400000 ─┤  页目录 / 页表区域
            │
0x10000000 ─┤  用户空间（未来 Ring 3 进程）
            │
0xFFFFFFFF ─┘  虚拟地址空间顶端
```

物理内存前 1MB（`0x00000-0xFFFFF`）为 BIOS/硬件保留区域，内核加载到 1MB 以上。

---

## 启动序列

```
BIOS POST
  ↓
MBR (0x7C00, 16-bit real mode)
  ↓  加载 Stage 1.5
boot16.asm — 启用 A20，检测内存映射
  ↓  切换到 32-bit 保护模式
boot32.asm — 设置临时页表，启用分页
  ↓  加载 kernel.bin 到 0x100000
kernel_main()
  ├─ vga_dbg 早期调试标记
  ├─ screen_init (VGA 文本模式)
  ├─ 启动横幅打印
  ├─ cpu_init + fast_mem_init
  ├─ config_sys_init + CONFIG.SYS 解析
  ├─ gdt_init
  ├─ paging_init
  ├─ idt_init (含 8259 PIC 重映射)
  ├─ panic_init (异常处理器)
  ├─ keyboard_init
  ├─ mouse_init
  ├─ sti (启用中断)
  ├─ pci_init
  ├─ ahci_init (失败则 fallback ATA)
  ├─ drive_init
  ├─ fdc_init (A:/B: 软盘)
  ├─ disk_init / AHCI override
  │   ├─ fat32_init (C: 盘)
  │   └─ MBR 分区扫描 (E:+ 扩展分区)
  ├─ cdrom_init (D: 光驱)
  │   └─ iso9660_mount
  ├─ interrupt_manager_init (INT 0x22)
  ├─ hal_blkdev_register (FDC/ATA/AHCI 注册)
  ├─ isa_init (硬盘启动时)
  ├─ sched_init
  ├─ users_init
  ├─ System Ready 横幅
  └─ 判断启动介质:
       ├─ 软盘/CD → installer_run() 安装程序
       └─ 硬盘    → shell_main() Shell 命令行
```

---

## 中断向量分配

| 向量 | 用途 |
|------|------|
| 0-19 | CPU 异常（Divide Error, Double Fault, Page Fault, GPF 等） |
| 20-31 | Intel 保留 |
| 32 (0x20) | PIT 系统定时器（IRQ0） |
| 33 (0x21) | PS/2 键盘（IRQ1） |
| 34 (0x22) | **系统调用（软中断）** |
| 35 (0x23) | COM2/COM4 串口（IRQ3） |
| 36 (0x24) | COM1/COM3 串口（IRQ4） |
| 37 (0x25) | LPT2 （IRQ5） |
| 38 (0x26) | 软盘控制器（IRQ6） |
| 39 (0x27) | LPT1 / SPURIOUS（IRQ7） |
| 40 (0x28) | CMOS RTC（IRQ8） |
| 44 (0x2C) | PS/2 鼠标（IRQ12） |
| 46 (0x2E) | 主 ATA 硬盘（IRQ14） |
| 47 (0x2F) | 从 ATA 硬盘（IRQ15） |

---

## 开发规范

- **内核 C 代码**: 使用 `-std=gnu99`，`-ffreestanding`，`-fno-stack-protector`，`-m32 -march=i586`
- **驱动 C++ 代码**: 使用 `-std=gnu++17`，禁用 RTTI 和异常（`-fno-rtti -fno-exceptions`）
- **引导汇编**: 使用 `nasm -f elf32`
- **链接脚本**: `linker.ld`，入口 `_start`，基址 `0x100000`
- **函数级注释**: 所有全局/静态函数需有中文注释，说明用途、参数、返回值
- **错误处理**: 驱动初始化失败使用 `boot_skip()` 输出黄色 `[--]` 并继续，非致命错误不 panic
- **调试输出**: 早期调试使用 `vga_dbg(pos, ch)` 在 VGA 屏幕最底行输出字符标记；详细日志走 COM1 串口

---

## 未来方向

虽然当前为宏内核架构，但代码已经具备一定的模块化基础：

1. **HAL 抽象**: 通过 `hal_blkdev_ops` 将块设备与文件系统解耦，未来可平滑加入新存储控制器。
2. **C++ 中断管理器**: 面向对象的 ISR 注册机制，方便扩展新的中断处理器。
3. **调度器框架**: sched.c 已具备进程上下文管理基础，未来可扩展为 Ring 3 用户进程。
4. **系统调用向量**: INT 0x22 已建立，可逐步增加系统调用号，支持用户态程序。

如需进一步向混合内核或微内核方向演进（将驱动/文件系统移至用户态），以上模块化为未来演进留出了空间。

---

**Nexsteaduser PlexsDOS** · Plexs Monolithic Kernel Architecture
**Nexlyh Team** · Tinmc189623
