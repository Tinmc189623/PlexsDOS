# PlexsDOS — 实现计划

## Nexsteaduser — PlexsDOS 实现计划

作者: Tinmc189623 | 团队: Nexlyh

---

## 阶段总览

| 阶段 | 内容 | 状态 |
|------|------|------|
| 1 | 构建系统与项目骨架 | ✅ 完成 |
| 2 | 引导扇区 (软盘) | ✅ 完成 |
| 3 | 内核入口与基础框架 | ✅ 完成 |
| 4 | 屏幕驱动 (VGA 文本模式) | ✅ 完成 |
| 5 | 键盘驱动 (PS/2, IRQ1) | ✅ 完成 |
| 6 | 中断处理框架 (IDT + 8259A PIC) | ✅ 完成 |
| 7 | Shell 实现 (命令解析/内置命令) | ✅ 完成 |
| 8 | 文件系统 (FAT12 + FAT32) | ✅ 完成 |
| 9 | .comx 程序加载 | ✅ 完成 |
| 10 | 系统调用 (INT 21h, DOS 兼容) | ✅ 完成 |
| 11 | 集成测试与调试 (QEMU + Bochs) | ✅ 完成 |
| 12 | 通用处理器优化 (i686→SSE2→AVX) | ✅ 完成 |
| 13 | PCI 总线与 ATA PIO/DMA | ✅ 完成 |
| 14 | .comx 可执行格式规范 | ✅ 完成 |
| 15 | Python 镜像构建脚本 (pyfatfs) | ✅ 完成 |
| 16 | C23 迁移 | ✅ 完成 |
| 17 | 红屏 (RSOD) 内核恐慌 | ✅ 完成 |
| 18 | C++ 中断管理器 | ✅ 完成 |
| 19 | FDC 软盘控制器 (马达状态机, 写入, 多格式) | ✅ 完成 |
| 20 | CD-ROM 驱动 (ATAPI + ISO 9660) | ✅ 完成 |
| 21 | 硬盘 MBR/VBR 引导 | ✅ 完成 |
| 22 | AHCI SATA 驱动 | ✅ 完成 |
| 23 | 分页内存管理 (4KB 页) | ✅ 完成 |
| 24 | 进程调度器 (抢占式多任务) | ✅ 完成 |
| 25 | GUI 系统 (桌面/WM/Widgets) | ✅ 完成 |
| 26 | 编辑器 | ✅ 完成 |
| 27 | 多盘安装系统 | ✅ 完成 |
| 28 | 驱动器抽象 (A:-E: 映射) | ✅ 完成 |
| 29 | 用户账户系统 | ✅ 完成 |
| 30 | 显示管理器 (PlexsDM / LightDM 移植) | ✅ 完成 |
| 31 | HAL (硬件抽象层 + 块设备框架) | ✅ 完成 |
| 32 | ISA 设备枚举 | ✅ 完成 |
| 33 | 鼠标驱动 (PS/2, IRQ12) | ✅ 完成 |
| 34 | 串口驱动 (COM1 调试输出) | ✅ 完成 |
| 35 | ISO 9660 可引导光盘 (El Torito) | ✅ 完成 |
| 36 | VMDK 虚拟硬盘 (40GB, 预装系统) | ✅ 完成 |
| 37 | VGA 图形模式驱动 | ✅ 完成 |

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

### 验证标准

- [x] `make clean && make` 编译无错误
- [x] 生成 `build/plexsdos.img` (1.44MB)
- [x] `make run` 启动 QEMU 并显示引导扇区输出

---

## 阶段 2: 引导扇区 (软盘)

### 目标

实现 512 字节引导扇区，加载内核到内存并跳转。

### 文件清单

```
boot/boot_sector.S       # 引导扇区主代码 (含 BPB、磁盘读取、GDT、PM 切换)
```

### 验证标准

- [x] 引导扇区 512 字节，以 0x55AA 结尾
- [x] BIOS 正确识别为可引导设备
- [x] 成功从软盘读取内核到 0x1000
- [x] 跳转到内核入口点执行
- [x] DL 寄存器保存的引导设备号正确传递

---

## 阶段 3: 内核入口与基础框架

### 目标

建立内核入口点，初始化运行环境，调用 C 语言内核主函数。

### 文件清单

```
kernel/kernel_entry.S      # 内核入口 (汇编)
kernel/kernel_main.c       # 内核主函数 (C)
kernel/arch/gdt.S          # 全局描述符表 (可选)
include/plexsdos/types.h   # 基础类型定义 (C23)
include/plexsdos/config.h  # 系统配置常量
```

### 验证标准

- [x] 内核从引导扇区接管执行
- [x] C 函数 kernel_main() 被正确调用
- [x] boot_drive 变量从 DL 寄存器正确保存
- [x] 栈指针设置在 0x400000

### 备注

- 当前内核入口: 0x30000 (由 linker.ld 指定)
- 引导阶段临时加载至 0x1000, 链接至 0x30000

---

## 阶段 4: 屏幕驱动

### 目标

实现 VGA 文本模式屏幕输出，支持基本字符显示、清屏、光标控制。

### 文件清单

```
kernel/drivers/screen.c        # 屏幕驱动实现
include/plexsdos/screen.h      # 屏幕驱动接口
```

### 关键 API

```c
void screen_init(void);
void screen_clear(void);
void screen_putchar(char c);
void screen_puts(const char *str);
void screen_put_hex(uint32_t val);
void screen_set_color(uint8_t fg, uint8_t bg);
void screen_scroll(void);
```

### 验证标准

- [x] 屏幕初始化为黑底白字
- [x] 字符串正确显示在屏幕上
- [x] 换行、退格、制表符正确处理
- [x] 屏幕滚动正常工作
- [x] 光标位置正确更新

---

## 阶段 5: 键盘驱动

### 目标

实现键盘输入，支持字符读取和命令行编辑。

### 文件清单

```
kernel/drivers/keyboard.c      # 键盘驱动实现
include/plexsdos/keyboard.h    # 键盘驱动接口
```

### 关键 API

```c
void keyboard_init(void);
char keyboard_getchar(void);
int keyboard_available(void);
char keyboard_read_line(char *buf, int max_len);
```

### 验证标准

- [x] 按键输入正确回显到屏幕
- [x] Shift 键组合正常工作
- [x] Caps Lock 切换正常
- [x] 退格键正确删除字符
- [x] Enter 键正确提交输入

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
void idt_init(void);
void idt_set_gate(int vector, uint16_t selector,
                  uint32_t offset, uint8_t flags);
void interrupt_register(int vector, void (*handler)(void));
```

### 验证标准

- [x] IDT 正确初始化 (256 向量)
- [x] IRQ 重映射 (主 PIC: 0x20-0x27, 从 PIC: 0x28-0x2F)
- [x] 键盘中断 (IRQ1) 正常触发
- [x] EOI 正确发送

---

## 阶段 7: Shell 实现

### 目标

实现命令行 Shell，支持内置命令和命令解析。

### 文件清单

```
kernel/shell/shell.c             # Shell 主逻辑
include/plexsdos/shell.h         # Shell 接口
```

### 内置命令

- [x] `help` — 显示帮助
- [x] `cls` — 清屏
- [x] `ver` — 版本信息
- [x] `echo` — 文本回显
- [x] `mem` — 内存信息
- [x] `reboot` — 重启
- [x] `ls` — 文件列表
- [x] `type` — 文件查看
- [x] `run` — 程序加载执行
- [x] `users` — 用户管理

### 验证标准

- [x] Shell 提示符正确显示
- [x] 命令行输入正确解析
- [x] 所有内置命令功能正常
- [x] 未知命令显示错误信息

---

## 阶段 8: 文件系统

### 目标

实现 FAT12/FAT32 文件系统读取支持。

### 文件清单

```
kernel/fs/fat12.c                # FAT12 实现 (软盘)
kernel/fs/fat32.c                # FAT32 实现 (硬盘)
include/plexsdos/fat12.h         # FAT12 接口
include/plexsdos/fat32.h         # FAT32 接口
tools/mkfat32.py                 # FAT32 镜像创建工具
tools/mkfat12.py                 # FAT12 镜像创建工具
```

### 关键 API

```c
bool fat32_init(void);
void fat32_list_root(void);
struct fat32_dir_entry *fat32_find_file(const char *name);
uint32_t fat32_load_file(struct fat32_dir_entry *entry, uint32_t load_addr);
```

### 验证标准

- [x] 正确读取 FAT32/FAT12 BPB 参数
- [x] 根目录列表正确显示
- [x] 文件内容正确读取
- [x] `ls` 和 `type` 命令正常工作

---

## 阶段 9: .comx 程序加载

### 目标

支持加载和执行 .comx 格式外部程序。

### 文件清单

```
kernel/loader.c                  # .comx 程序加载器
include/plexsdos/loader.h        # 加载器接口
include/plexsdos/comx.h          # .comx 格式定义
```

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
kernel/arch/syscall.c            # 系统调用分发
kernel/shim/int21.c              # INT 21h 额外处理
include/plexsdos/syscall.h       # 系统调用定义
```

### 实现的子功能

- [x] AH=0x01: 读字符并回显
- [x] AH=0x02: 写字符
- [x] AH=0x09: 写字符串 ($ 结尾)
- [x] AH=0x0A: 读字符串到缓冲区
- [x] AH=0x4C: 程序终止

### 验证标准

- [x] INT 21h 正确分发到对应处理函数
- [x] 外部程序可调用所有已实现的子功能
- [x] 程序终止正确返回 Shell

---

## 阶段 12: 通用处理器优化 (i686 → SSE2 → AVX)

### 目标

实现全系列 x86 处理器运行时优化。

### 文件清单

```
kernel/arch/cpu.c                # CPU 全特性检测
include/plexsdos/cpu.h           # CPU 接口定义
lib/fast_mem.c                   # 运行时分派内存操作
```

### 验证标准

- [x] CPUID 正确检测全系列 CPU 特性
- [x] SSE/AVX 正确启用 (CR4 + XSETBV)
- [x] fast_memcpy/memset/memcmp 运行时分派到最优路径
- [x] `-march=i686` 基线编译无 SIMD 指令冲突

---

## 阶段 13: PCI 总线与 ATA PIO/DMA

### 目标

实现 PCI 总线扫描和 ATA 磁盘驱动。

### 文件清单

```
kernel/drivers/pci.c             # PCI 总线扫描
include/plexsdos/pci.h           # PCI 接口定义
kernel/drivers/disk.c            # ATA PIO + DMA 驱动
include/plexsdos/disk.h          # 磁盘接口定义
```

### 验证标准

- [x] PCI 总线正确扫描
- [x] IDE 控制器正确识别
- [x] PIO 模式读写正常
- [x] DMA 传输正常工作

---

## 阶段 14: .comx 可执行格式定义

### 目标

定义 .comx 自研 32-bit 可执行格式规范。

### 文件清单

```
include/plexsdos/comx.h          # .comx 格式定义 (32 字节头部)
tools/mkcomx.py                  # .comx 打包工具
programs/test_hello.S            # 测试程序
programs/pnp.S                   # PnP 管理器
```

### 验证标准

- [x] .comx 头部结构定义完整
- [x] mkcomx.py 正确生成 .comx 文件
- [x] 内核 loader 正确解析和验证 .comx
- [x] HELLO.COMX / PNP.COMX 在 QEMU 中正确执行

---

## 阶段 15: Python 镜像构建脚本 (pyfatfs)

### 目标

使用 Python + pyfatfs 外部库构建 FAT12/FAT32 镜像。

### 文件清单

```
tools/mkfloppy.py                # FAT12 软盘镜像 (1.44MB)
tools/mkfat32.py                 # FAT32 硬盘镜像 (64MB)
tools/mkfat12.py                 # FAT12 安装盘镜像
tools/mkbootdisk.py              # 可引导安装盘
tools/mkiso.py                   # ISO 9660 可引导光盘
tools/mkvmdk.py                  # VMDK 虚拟硬盘
tools/fix_vbr.py                 # VBR BPB 修正
requirements.txt                 # Python 依赖 (pyfatfs>=1.1.0)
```

### 验证标准

- [x] pyfatfs 正确安装和导入
- [x] 所有镜像类型正确生成
- [x] ISO El Torito 可引导光盘工作
- [x] VMDK 虚拟硬盘可启动

---

## 阶段 16: C23 迁移

### 目标

全项目 C 代码从 C99 升级到 C23 标准。

### 改动

- `Makefile`: `-std=c99` → `-std=c23`
- `types.h`: 使用 C23 原生 `bool`/`true`/`false`/`nullptr`
- 所有 `.c` 文件适配 C23 语法

### 验证标准

- [x] `make clean && make` 编译通过
- [x] `nullptr` 替代 `NULL`
- [x] `bool`/`true`/`false` 作为关键字

---

## 阶段 17: 红屏 (RSOD) 内核恐慌

### 目标

实现类似 Windows BSOD 的内核恐慌诊断屏幕。

### 文件清单

```
include/plexsdos/panic.h         # 红屏 API
kernel/arch/panic.c              # 红屏实现
```

### API

```c
_Noreturn void kernel_panic(const char *fmt, ...);
```

### 验证标准

- [x] CPU 异常 (0x00-0x1F) 正确触发红屏
- [x] 红底白字全屏显示
- [x] 显示寄存器状态 (EAX, EBX, ECX, EDX, ESI, EDI, EBP, ESP, EIP)
- [x] 显示错误代码和地址
- [x] 栈回溯 (EBP 链)
- [x] 串口同步输出

---

## 阶段 18: C++ 中断管理器

### 目标

用 C++ 面向对象框架重写中断管理和系统调用分发。

### 文件清单

```
include/plexsdos/interrupt.hpp   # C++ 中断管理器头文件
kernel/arch/interrupt_mgr.cpp    # C++ 中断管理器实现
kernel/arch/interrupt.S          # 汇编入口桩
```

### 类设计

```cpp
class InterruptHandler { virtual void handle(uint32_t, uint32_t) = 0; };
class InterruptManager { static InterruptManager& instance(); ... };
class SyscallDispatcher : public InterruptHandler { ... };
```

### 验证标准

- [x] C++ 编译和链接通过 (-fno-exceptions -fno-rtti)
- [x] 汇编桩正确调用 C++ dispatch 函数
- [x] C/C++ 互操作正常
- [x] 系统调用分发正常

---

## 阶段 19: FDC 软盘控制器

### 目标

全面改进 FDC 马达控制, 支持写入和多格式。

### 文件清单

```
kernel/drivers/fdc.c            # FDC 驱动 (NEC 765)
include/plexsdos/fdc.h          # FDC 接口
```

### 特性

- [x] 马达状态机 (OFF → STARTING → ON → STOPPING)
- [x] 多格式: 1.44MB / 1.2MB / 720KB
- [x] WRITE DATA 命令写入
- [x] 写保护检测
- [x] 磁盘更换检测
- [x] 旋转稳定延迟 300ms, 关闭延迟 2000ms

---

## 阶段 20: CD-ROM 驱动 (ATAPI + ISO 9660)

### 目标

完整的 CD-ROM 驱动, 支持读取 ISO 9660 文件系统。

### 文件清单

```
include/plexsdos/cdrom.h         # CD-ROM API
kernel/drivers/cdrom.c           # ATAPI + ISO 9660 实现
```

### 特性

- [x] ATAPI PACKET 命令 (0xA0)
- [x] 12 字节 CDB 命令集
- [x] 2048 字节/扇区
- [x] ISO 9660 PVD 解析
- [x] 目录路径遍历
- [x] Shell 命令: CDIR, CCAT, CDMOUNT

---

## 阶段 21: 硬盘 MBR/VBR 引导

### 目标

实现从硬盘启动, 支持 MBR + VBR 引导链。

### 文件清单

```
boot/hd_mbr.S                   # 硬盘 MBR (分区表扫描)
boot/hd_vbr.S                   # 硬盘 VBR (FAT32 引导)
kernel/hd_boot.S                # HDD 启动代码 (嵌入)
tools/fix_vbr.py                # VBR BPB 修正
```

### 特性

- [x] MBR 分区表解析
- [x] VBR FAT32 BPB 参数读取
- [x] 内核从 FAT32 分区加载
- [x] VBR BPB 字段自动修正

---

## 阶段 22: AHCI SATA 驱动

### 目标

实现 AHCI SATA 控制器驱动, 支持高速磁盘访问。

### 文件清单

```
kernel/drivers/ahci.c           # AHCI SATA 驱动
include/plexsdos/ahci.h         # AHCI 接口
```

### 特性

- [x] PCI BAR5 (ABAR) 映射
- [x] HBA 端口枚举
- [x] Command List + PRDT
- [x] NCQ 支持
- [x] 读写扇区操作

---

## 阶段 23: 分页内存管理

### 目标

实现 4KB 页分页和物理帧分配器。

### 文件清单

```
kernel/mm/paging.c              # 分页管理
kernel/mm/pfa.c                 # 物理帧分配器
include/plexsdos/paging.h       # 分页接口
```

### 特性

- [x] 页目录 + 页表 (4KB 页)
- [x] 身份映射 (虚拟=物理)
- [x] CR0.PG 启用
- [x] 物理帧分配器 (位图)
- [x] 页目录/页表在 0x300000

---

## 阶段 24: 进程调度器

### 目标

实现抢占式多任务调度。

### 文件清单

```
kernel/sched/scheduler.c         # 调度器实现
include/plexsdos/scheduler.h     # 调度器接口
```

### 特性

- [x] 抢占式多任务 (PIT 定时器驱动)
- [x] 时间片轮转
- [x] 任务控制块 (TCB)
- [x] 任务创建/退出/让出

---

## 阶段 25: GUI 系统

### 目标

实现桌面环境、窗口管理器和小部件系统。

### 文件清单

```
kernel/gui/desktop.c            # 桌面环境
kernel/gui/wm.c                 # 窗口管理器
kernel/gui/widgets.c            # GUI 小部件
include/plexsdos/desktop.h      # 桌面接口
include/plexsdos/wm.h           # 窗口接口
include/plexsdos/widgets.h      # 小部件接口
include/plexsdos/graphics.h     # 图形接口
```

### 特性

- [x] 桌面背景 + 任务栏
- [x] 窗口创建/销毁/移动
- [x] Z-order 管理
- [x] 鼠标事件分发
- [x] 按钮/标签/输入框

---

## 阶段 26: 编辑器

### 目标

实现内置文本编辑器。

### 文件清单

```
kernel/editor/editor.c           # 编辑器
include/plexsdos/editor.h        # 编辑器接口
```

### 特性

- [x] 文本文件编辑
- [x] 光标移动
- [x] 保存文件

---

## 阶段 27: 多盘安装系统

### 目标

实现 MS-DOS 风格的多盘安装程序。

### 文件清单

```
kernel/installer.c               # 安装程序
include/plexsdos/installer.h     # 安装程序接口
```

### 特性

- [x] MS-DOS 风格换盘提示
- [x] 文件复制显示 (文件名 + 大小)
- [x] 错误重试 (最多 3 次)
- [x] 磁盘验证
- [x] 写入 MBR + 创建分区 + 复制文件

---

## 阶段 28: 驱动器抽象

### 目标

实现统一的驱动器字母映射系统。

### 文件清单

```
kernel/drivers/drive.c          # 驱动器抽象
include/plexsdos/drive.h        # 驱动器接口
```

### 映射

| 字母 | 类型 |
|------|------|
| A: | 软盘 0 |
| B: | 软盘 1 |
| C: | 硬盘 (FAT32) |
| D: | CD-ROM |
| E:+ | 额外分区 |

---

## 阶段 29: 用户账户系统

### 目标

实现多用户账户管理。

### 文件清单

```
kernel/shell/users.c            # 用户管理
include/plexsdos/users.h        # 用户接口
```

### 特性

- [x] 用户创建/删除/查询
- [x] Shell 集成 (users 命令)

---

## 阶段 30: 显示管理器 (PlexsDM)

### 目标

移植 LightDM 为 PlexsDM 显示管理器。

### 文件清单

```
kernel/dm/plxdm_*.c             # PlexsDM 实现
kernel/dm/lightdm-core/         # LightDM 参考代码
include/plexsdos/*              # 相关头文件
```

### 特性

- [x] 显示管理
- [x] 会话管理
- [x] 用户选择
- [x] 自动登录
- [x] XDMCP 支持

---

## 阶段 31-37: 其他基础设施

### 31. HAL (硬件抽象层)

- [x] 块设备框架 (FDC/ATA 注册)
- [x] I/O 操作抽象

### 32. ISA 设备枚举

- [x] ISA 传统设备扫描
- [x] IRQ 冲突检测

### 33. 鼠标驱动

- [x] PS/2 鼠标 (IRQ12)
- [x] 鼠标事件

### 34. 串口驱动

- [x] COM1 初始化
- [x] 调试输出

### 35. ISO 9660 可引导光盘

- [x] El Torito 2.88MB 仿真
- [x] 所有文件在单张光盘

### 36. VMDK 虚拟硬盘

- [x] 40GB 虚拟磁盘
- [x] 预装 MBR + VBR + 内核 + 程序

### 37. VGA 图形模式

- [x] 图形模式切换

---

*Nexsteaduser — PlexsDOS*
*作者: Tinmc189623 | 团队: Nexlyh*
