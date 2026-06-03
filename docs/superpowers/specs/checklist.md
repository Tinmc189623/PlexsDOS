# PlexsDOS — 开发检查清单

## Nexsteaduser — PlexsDOS 开发检查清单

作者: Tinmc189623 | 团队: Nexlyh

---

## 一、构建系统

- [x] Makefile 创建完成
- [x] 链接器脚本 (linker.ld) 创建完成 (内核 0x30000, BSS 0x200000)
- [x] 工具链 (gcc -m32 -std=c23, g++ -std=c++23, as --32) 安装验证
- [x] `make clean && make all` 编译无错误
- [x] 生成 `build/plexsdos.img` (1,474,560 字节, FAT12)
- [x] 生成 `build/disk.img` (67,108,864 字节, FAT32)
- [x] 生成 `build/plexsdos.iso` (ISO 9660 El Torito)
- [x] 生成 `build/plexsdos.vmdk` (40GB, 预装系统)
- [x] 生成 5 张安装盘镜像
- [x] Python 脚本使用 pyfatfs 外部库创建镜像
- [x] `make run` 启动 QEMU 无报错

---

## 二、引导扇区 (boot_sector.S — 软盘)

- [x] 引导扇区恰好 512 字节
- [x] 最后两字节为引导签名 0x55, 0xAA
- [x] BPB (BIOS 参数块) 正确填写
- [x] 段寄存器 (DS, ES, SS) 初始化为 0
- [x] 栈指针设置在 0x7C00
- [x] BIOS INT 13h 磁盘读取成功
- [x] 内核加载到 0x1000 (引导阶段临时)
- [x] `ljmp` 跳转到内核入口
- [x] QEMU 中成功引导到内核

---

## 三、硬盘引导 (hd_mbr.S + hd_vbr.S)

- [x] MBR 512 字节, 0x55AA 结尾
- [x] MBR 分区表正确解析
- [x] MBR 加载 VBR 到 0x7C00
- [x] VBR FAT32 BPB 正确
- [x] VBR 从 FAT32 分区加载内核
- [x] VBR 切换到保护模式跳转内核
- [x] tools/fix_vbr.py 正确修正 BPB
- [x] 硬盘引导到 Shell 成功

---

## 四、内核入口 (kernel_entry.S)

- [x] 使用 `.code32` 指令 (AT&T 语法, 32-bit 保护模式)
- [x] 段寄存器正确初始化
- [x] 栈指针设置在 0x400000
- [x] boot_drive 从 DL 寄存器保存
- [x] `call kernel_main` 正确调用 C 函数
- [x] 返回后执行 `cli; hlt`

---

## 五、屏幕驱动 (screen.c)

- [x] VGA 显存地址 0xB8000 正确映射
- [x] 屏幕初始化为黑底白字 (属性 0x07)
- [x] `screen_putchar()` 正确显示字符
- [x] `screen_puts()` 正确显示字符串
- [x] 换行符 (\n) 正确处理
- [x] 回车符 (\r) 正确处理
- [x] 退格符 (\b) 正确删除前一字符
- [x] 制表符 (\t) 正确扩展为空格
- [x] 屏幕滚动正常
- [x] 光标位置通过端口 0x3D4/0x3D5 正确更新
- [x] `screen_clear()` 清屏并重置光标
- [x] `screen_put_hex()` 十六进制输出正确
- [x] `screen_set_color()` 前景/背景色设置正确

---

## 六、VGA 图形模式 (vga.c)

- [x] VGA 图形模式切换
- [x] 帧缓冲区访问

---

## 七、键盘驱动 (keyboard.c)

- [x] 键盘中断处理程序正确注册 (IRQ1)
- [x] 键盘缓冲区 (环形队列) 实现
- [x] 扫描码到 ASCII 转换表完整
- [x] 普通字母键输入正确
- [x] 数字键输入正确
- [x] Shift + 字母 大写输出正确
- [x] Shift + 数字 符号输出正确
- [x] Backspace 键正确处理
- [x] Enter 键正确提交
- [x] Caps Lock 切换正常
- [x] `keyboard_getchar()` 阻塞等待正确
- [x] `keyboard_available()` 状态检查正确
- [x] `keyboard_read_line()` 行输入正确

---

## 八、鼠标驱动 (mouse.c)

- [x] PS/2 鼠标初始化 (IRQ12)
- [x] 鼠标事件处理
- [x] GUI 集成

---

## 九、串口驱动 (serial.c)

- [x] COM1 初始化 (115200 baud)
- [x] `serial_puts()` 字符串输出
- [x] `serial_putchar()` 字符输出
- [x] 内核启动过程串口日志

---

## 十、中断处理框架

### 10.1 IDT + PIC

- [x] IDT 初始化代码正确 (256 向量)
- [x] `idt_set_gate()` 正确设置中断门
- [x] IRQ 重映射 (主 PIC: 0x20-0x27, 从 PIC: 0x28-0x2F)
- [x] IRQ1 (键盘) 中断触发正常
- [x] EOI (End of Interrupt) 正确发送
- [x] 中断嵌套处理

### 10.2 C++ 中断管理器

- [x] C++ 编译和链接通过
- [x] `InterruptManager` 单例正确
- [x] `registerHandler()` / `unregisterHandler()` 正确
- [x] 汇编桩正确调用 C++ dispatch
- [x] C/C++ 互操作正常
- [x] `SyscallDispatcher` 系统调用分发正确

---

## 十一、CPU 异常处理与红屏 (panic.c)

- [x] `kernel_panic()` 函数实现
- [x] 红底白字全屏填充 (bg=0x04, fg=0x0F)
- [x] 显示错误类型和地址
- [x] 显示寄存器状态 (EAX, EBX, ECX, EDX, ESI, EDI, EBP, ESP, EIP)
- [x] 显示 CR0/CR2/CR3 寄存器
- [x] 栈回溯 (EBP 链, 最多 10 帧)
- [x] 串口同步输出
- [x] 可变参数格式化消息
- [x] `cli; hlt` 死循环
- [x] CPU 异常 (0x00-0x1F) 注册

---

## 十二、Shell (shell.c)

### 12.1 基础框架

- [x] Shell 提示符 "PLXSDOS> " 正确显示
- [x] 命令行缓冲区正确管理
- [x] 命令解析器正确分割命令和参数
- [x] 未知命令显示错误信息

### 12.2 内置命令

- [x] `help` — 显示所有可用命令
- [x] `cls` — 清屏并重置光标
- [x] `ver` — 显示版本信息
- [x] `echo <text>` — 回显参数文本
- [x] `mem` — 显示内存使用信息
- [x] `reboot` — 通过键盘控制器复位
- [x] `ls` — 列出根目录文件
- [x] `type <file>` — 显示文件内容
- [x] `run <file>` — 加载并执行 .comx 程序
- [x] `users` — 用户管理

### 12.3 命令行编辑

- [x] 退格删除最后一个字符
- [x] 命令行最大长度限制检查
- [x] 空命令 (直接回车) 不执行操作

---

## 十三、文件系统

### 13.1 FAT12 (软盘)

- [x] BPB 参数正确解析
- [x] FAT 表正确解析 (12-bit 条目)
- [x] 根目录固定位置读取
- [x] 文件内容正确读取

### 13.2 FAT32 (硬盘)

- [x] BPB 参数正确解析 (FAT32 特有字段)
- [x] FAT 表加载到内存
- [x] 根目录簇链正确遍历
- [x] 文件名匹配 (8.3 格式)
- [x] FAT32 表簇链正确遍历 (32-bit, 低 28 位)
- [x] 文件内容正确读取到缓冲区
- [x] 文件大小正确报告
- [x] EOF 正确检测
- [x] 删除条目 (0xE5) 正确跳过
- [x] 长文件名条目正确跳过

### 13.3 ISO 9660 (光盘)

- [x] PVD 扇区 (16) 正确读取
- [x] "CD001" 标识符验证
- [x] 目录路径遍历
- [x] 文件内容读取
- [x] Shell 集成 (CDIR/CCAT/CDMOUNT)

---

## 十四、CPU 检测与通用处理器优化 (cpu.c)

### 14.1 CPUID 检测

- [x] CPUID 指令正确执行
- [x] CPU 厂商字符串正确获取
- [x] CPU 品牌字符串正确获取
- [x] 特性标志位正确解析 (leaf 1 EDX/ECX, leaf 7 EBX, extended 0x80000001)

### 14.2 SIMD 启用

- [x] CR4.OSFXSR (bit 9) 正确设置 (SSE)
- [x] CR4.OSXMMEXCPT (bit 10) 正确设置 (SSE)
- [x] CR4.OSXSAVE (bit 18) 正确设置 (AVX)
- [x] XSETBV 启用 XMM+YMM 状态 (AVX)
- [x] SSE/SSE2/AVX 特性检测正确

### 14.3 快速内存操作 (fast_mem.c)

- [x] fast_memcpy: 基线 / SSE2 / AVX 运行时分派
- [x] fast_memset: 基线 / SSE2 / AVX 运行时分派
- [x] fast_memcmp: 运行时分派

---

## 十五、GDT (gdt.c + gdt_load.S)

- [x] Ring 0 代码段 (CS=0x08)
- [x] Ring 0 数据段 (DS/ES/FS/GS/SS=0x10)
- [x] Ring 3 代码段 (CS=0x1B)
- [x] Ring 3 数据段 (SS=0x23)
- [x] TSS 段 (任务状态段)
- [x] LGDT 指令正确加载

---

## 十六、分页内存管理 (paging.c + pfa.c)

- [x] 页目录创建 (1024 条目, 4KB, 地址 0x300000)
- [x] 页表创建 (4KB 页)
- [x] 身份映射 (虚拟地址 = 物理地址)
- [x] CR0.PG 启用分页
- [x] 物理帧分配器 (位图) 初始化
- [x] `pfa_alloc()` / `pfa_free()` 正常工作

---

## 十七、PCI 总线 (pci.c)

- [x] I/O 端口 0xCF8/0xCFC 访问正确
- [x] 配置地址格式正确 (bus/slot/func/offset)
- [x] AHCI/IDE 控制器正确检测
- [x] BAR 寄存器正确读取

---

## 十八、存储驱动

### 18.1 ATA PIO/DMA (disk.c)

- [x] ATA 端口 0x1F0-0x1F7 访问正确
- [x] IDENTIFY DEVICE 命令执行正确
- [x] READ SECTORS (0x20) PIO 读取正确
- [x] WRITE SECTORS (0x30) PIO 写入正确
- [x] READ DMA (0xC8) / WRITE DMA (0xCA) 正确
- [x] PCI Bus Master IDE 寄存器访问正确
- [x] PRDT 设置正确
- [x] 错误处理正确

### 18.2 AHCI SATA (ahci.c)

- [x] PCI BAR5 (ABAR) 正确映射
- [x] HBA 端口枚举正确
- [x] AHCI 设备签名检测正确
- [x] Command List 设置正确
- [x] Command Table + PRDT 正确
- [x] `ahci_read_sectors()` 读取正确
- [x] `ahci_write_sectors()` 写入正确
- [x] NCQ 支持

### 18.3 FDC 软盘控制器 (fdc.c)

- [x] NEC 765 寄存器访问正确 (0x3F0-0x3F7)
- [x] SPECIFY 命令设置正确
- [x] RECALIBRATE 命令正确
- [x] READ DATA 命令正确
- [x] WRITE DATA 命令正确
- [x] 马达状态机 (OFF / STARTING / ON / STOPPING)
- [x] 旋转稳定延迟 (300ms)
- [x] 马达自动关闭 (2000ms 无操作超时)
- [x] 写保护检测 (DIR bit 6)
- [x] 磁盘更换检测 (CHANGE LINE)
- [x] 多格式: 1.44MB / 1.2MB / 720KB

### 18.4 CD-ROM ATAPI (cdrom.c)

- [x] ATAPI 设备检测
- [x] PACKET 命令发送 (0xA0)
- [x] TEST_UNIT_READY 命令
- [x] READ_CAPACITY 命令
- [x] READ_10 命令 (2048 字节/扇区)
- [x] ISO 9660 PVD 解析
- [x] 路径遍历读取文件

### 18.5 驱动器抽象 (drive.c)

- [x] 驱动器字母注册 (A:-E:)
- [x] 驱动器类型映射 (Floppy/HDD/CDROM)
- [x] MBR 分区扫描

---

## 十九、.comx 程序加载器 (loader.c)

- [x] .comx 头部魔数验证 (0x43505800)
- [x] .comx 版本号验证 (0x01)
- [x] 代码大小验证 (不超过 32KB)
- [x] CPU 特性需求检查 (SSE/SSE2/MMX flags)
- [x] 校验和验证 (32-bit 滚动校验和)
- [x] 代码复制到加载地址
- [x] BSS 段清零
- [x] 跳转到入口点执行
- [x] 程序终止后正确返回 Shell
- [x] 无效文件显示错误信息

### 19.1 .comx 构建工具 (mkcomx.py)

- [x] 读取 flat binary 输入
- [x] 生成 32 字节 .comx 头部
- [x] 计算校验和 (与内核 loader_checksum 一致)
- [x] 支持 --entry, --bss, --load-addr, --flags 参数

---

## 二十、系统调用 — INT 21h

- [x] INT 21h 中断向量正确设置
- [x] AH=0x01: 读字符并回显
- [x] AH=0x02: 写字符 (DL=字符)
- [x] AH=0x09: 写字符串 (EDX=地址, '$' 结尾)
- [x] AH=0x0A: 读字符串到缓冲区 (EDX=缓冲区地址)
- [x] AH=0x4C: 程序终止 (AL=返回码)
- [x] 未知功能号正确处理

---

## 二十一、进程调度器 (scheduler.c)

- [x] PIT 定时器初始化
- [x] 任务控制块 (TCB) 实现
- [x] `sched_init()` 调度器初始化
- [x] `sched_create_task()` 任务创建
- [x] `sched_yield()` 任务让出 CPU
- [x] `sched_exit()` 任务退出
- [x] 时间片轮转调度
- [x] 中断安全上下文切换

---

## 二十二、GUI 系统

### 22.1 桌面 (desktop.c)

- [x] 桌面背景渲染
- [x] 任务栏/面板
- [x] 开始菜单

### 22.2 窗口管理器 (wm.c)

- [x] 窗口创建/销毁
- [x] 窗口移动
- [x] Z-order 管理
- [x] 鼠标事件分发
- [x] 窗口标题栏 + 边框

### 22.3 小部件 (widgets.c)

- [x] 按钮 (Button)
- [x] 标签 (Label)
- [x] 输入框 (Text Input)
- [x] 画布 (Canvas)

---

## 二十三、编辑器 (editor.c)

- [x] 文本文件打开
- [x] 光标移动 (方向键)
- [x] 文本插入/删除
- [x] 文件保存

---

## 二十四、安装系统 (installer.c)

- [x] MS-DOS 风格换盘提示
- [x] 磁盘编号标签显示
- [x] 用户确认等待 (press ENTER)
- [x] 错误磁盘检测
- [x] 文件复制显示 (文件名 + 大小)
- [x] 错误重试 (最多 3 次)
- [x] 写入 MBR
- [x] FAT32 文件系统创建
- [x] 从安装盘复制文件
- [x] 安装完成提示

---

## 二十五、用户账户系统 (users.c)

- [x] `users_init()` 初始化
- [x] `user_create()` 创建用户
- [x] `user_delete()` 删除用户
- [x] `user_find()` 查找用户
- [x] Shell 集成 (users 命令)

---

## 二十六、HAL 硬件抽象层

### 26.1 块设备框架

- [x] `hal_blkdev_ops` 结构定义
- [x] `hal_blkdev_register()` 设备注册
- [x] FDC 块设备注册
- [x] ATA 块设备注册

### 26.2 ISA 枚举

- [x] `isa_init()` ISA 设备扫描
- [x] I/O 端口范围检测

---

## 二十七、汇编代码规范检查

所有 .S 文件必须通过以下检查:

- [x] 使用 GAS AT&T 语法 (非 Intel 语法)
- [x] 操作数顺序: `source, destination`
- [x] 寄存器带 `%` 前缀
- [x] 立即数带 `$` 前缀
- [x] 指令后缀正确: `movb`, `movw`, `movl`
- [x] 文件头包含 Nexsteaduser 品牌声明
- [x] 文件头包含作者 Tinmc189623 和团队 Nexlyh
- [x] 函数级注释完整
- [x] 无占位符代码
- [x] 无模拟代码

---

## 二十八、C/C++ 代码规范检查

所有 .c .cpp .h .hpp 文件必须通过以下检查:

- [x] C 使用 C23 标准, C++ 使用 C++23 标准
- [x] 包含 `plexsdos/types.h` 或等效类型定义
- [x] 函数级注释完整
- [x] 文件头包含 Nexsteaduser 品牌声明
- [x] 文件头包含作者 Tinmc189623 和团队 Nexlyh
- [x] 无占位符代码
- [x] 无模拟代码
- [x] 品牌名「Nexsteaduser」完整使用, 未被拆分
- [x] 无任何「Nexstead」表述

---

## 二十九、集成测试

### 29.1 QEMU 测试

- [x] 软盘引导成功到 Shell
- [x] 硬盘 MBR/VBR 引导成功
- [x] ISO 光盘 El Torito 引导成功
- [x] VMDK 虚拟硬盘启动成功
- [x] Shell 提示符显示
- [x] 键盘输入响应
- [x] 内置命令全部测试通过
- [x] 文件读取测试通过
- [x] 外部程序加载测试通过
- [x] 程序终止返回 Shell
- [x] 热重启测试通过
- [x] 多盘安装流程测试通过
- [x] AHCI SATA 读写测试
- [x] CD-ROM ATAPI 识别
- [x] GUI 桌面环境显示
- [x] 鼠标移动事件

### 29.2 边界测试

- [x] 空命令输入
- [x] 超长命令输入 (溢出检查)
- [x] 不存在的文件名
- [x] 空软盘引导
- [x] 损坏的 FAT 表
- [x] 无 AHCI 控制器时优雅回退
- [x] 无磁盘时正常启动

---

## 三十、文档完整性

- [x] CLAUDE.md — 项目指令完整
- [x] spec.md — 技术规格完整
- [x] plan.md — 实现计划完整
- [x] checklist.md — 本检查清单完整
- [x] AGENTS.md — 开发规范完整
- [x] README.md — 项目介绍完整
- [x] 源码注释覆盖率 > 80%

---

## 三十一、发布前检查

- [ ] 所有阶段完成 (37/37 完成)
- [ ] 所有集成测试通过
- [x] QEMU 全功能验证
- [ ] Bochs 兼容性测试 (可选)
- [ ] 物理硬件测试 (可选)
- [x] 构建产物大小合理
- [x] 版本号更新
- [x] 最终镜像可引导

---

*Nexsteaduser — PlexsDOS*
*作者: Tinmc189623 | 团队: Nexlyh*
