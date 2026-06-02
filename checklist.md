# PlexsDOS — 开发检查清单

## Nexsteaduser — PlexsDOS 开发检查清单

作者: Tinmc189623 | 团队: Nexlyh

---

## 一、构建系统

- [x] Makefile 创建完成
- [x] 链接器脚本 (linker.ld) 创建完成
- [x] 工具链 (gcc -m32 -march=i686, as --32, ld -m i386pe) 安装验证
- [x] `make clean && make all` 编译无错误
- [x] 生成 `build/plexsdos.img` (1,474,560 字节, FAT12)
- [x] 生成 `build/disk.img` (67,108,864 字节, FAT32)
- [x] Python 脚本使用 pyfatfs 外部库创建镜像
- [x] `make run` 启动 QEMU 无报错

---

## 二、引导扇区 (boot_sector.S)

- [ ] 引导扇区恰好 512 字节
- [ ] 最后两字节为引导签名 0x55, 0xAA
- [ ] BPB (BIOS 参数块) 正确填写
- [ ] 段寄存器 (DS, ES, SS) 初始化为 0
- [ ] 栈指针设置在 0x7C00
- [ ] BIOS INT 13h 磁盘读取成功
- [ ] 内核加载到 0x1000
- [ ] `ljmp` 跳转到内核入口
- [ ] QEMU 中成功引导到内核

---

## 三、内核入口 (kernel_entry.S)

- [ ] 使用 `.code32` 指令 (AT&T 语法, 32-bit 保护模式)
- [ ] 段寄存器正确初始化
- [ ] 栈指针设置在 0x90000
- [ ] `call kernel_main` 正确调用 C 函数
- [ ] 返回后执行 `cli; hlt`

---

## 四、屏幕驱动 (screen.c / screen.S)

- [ ] VGA 显存地址 0xB8000 正确映射
- [ ] 屏幕初始化为黑底白字 (属性 0x07)
- [ ] `screen_putchar()` 正确显示字符
- [ ] `screen_puts()` 正确显示字符串
- [ ] 换行符 (\n) 正确处理
- [ ] 回车符 (\r) 正确处理
- [ ] 退格符 (\b) 正确删除前一字符
- [ ] 制表符 (\t) 正确扩展为空格
- [ ] 屏幕滚动正常 (最后一行满时)
- [ ] 光标位置通过端口 0x3D4/0x3D5 正确更新
- [ ] `screen_clear()` 清屏并重置光标
- [ ] `screen_put_hex()` 十六进制输出正确

---

## 五、键盘驱动 (keyboard.c / keyboard.S)

- [ ] INT 0x09 中断处理程序正确注册
- [ ] 键盘缓冲区 (环形队列) 实现
- [ ] 扫描码到 ASCII 转换表完整
- [ ] 普通字母键输入正确
- [ ] 数字键输入正确
- [ ] Shift + 字母 大写输出正确
- [ ] Shift + 数字 符号输出正确
- [ ] Backspace 键正确处理
- [ ] Enter 键正确提交
- [ ] Caps Lock 切换正常
- [ ] `keyboard_getchar()` 阻塞等待正确
- [ ] `keyboard_available()` 状态检查正确
- [ ] `keyboard_read_line()` 行输入正确

---

## 六、中断处理框架 (interrupt.S / idt.c)

- [ ] IDT 初始化代码正确
- [ ] 256 个中断向量表项分配
- [ ] `idt_set_gate()` 正确设置中断门
- [ ] IRQ 重映射 (主 PIC: 0x20-0x27, 从 PIC: 0x28-0x2F)
- [ ] IRQ0 (定时器) 中断触发正常
- [ ] IRQ1 (键盘) 中断触发正常
- [ ] EOI (End of Interrupt) 正确发送
- [ ] 中断嵌套处理（如支持）
- [ ] `interrupt_register()` 注册/注销正常

---

## 七、Shell (shell.c)

### 7.1 基础框架

- [ ] Shell 提示符 "PLXSDOS> " 正确显示
- [ ] 命令行缓冲区正确管理
- [ ] 命令解析器正确分割命令和参数
- [ ] 未知命令显示错误信息

### 7.2 内置命令

- [ ] `help` — 显示所有可用命令
- [ ] `cls` — 清屏并重置光标
- [ ] `ver` — 显示 "PlexsDOS version 0.1"
- [ ] `echo <text>` — 回显参数文本
- [ ] `mem` — 显示内存使用信息
- [ ] `time` — 显示当前时间 (32-bit PM 下不支持 BIOS INT 1Ah, 暂不实现)
- [ ] `date` — 显示当前日期 (32-bit PM 下不支持 BIOS INT 1Ah, 暂不实现)
- [ ] `reboot` — 通过键盘控制器复位 (0x64)
- [ ] `dir` — 列出根目录文件 (依赖文件系统)
- [ ] `type <file>` — 显示文件内容 (依赖文件系统)

### 7.3 命令行编辑

- [ ] 退格删除最后一个字符
- [ ] 命令行最大长度限制检查
- [ ] 空命令 (直接回车) 不执行操作

---

## 八、文件系统 — FAT32 (fat32.c)

### 8.1 初始化

- [ ] BPB 参数正确解析 (FAT32 特有字段)
- [ ] FAT 表加载到内存
- [ ] 根目录簇链正确遍历
- [ ] 驱动器号正确保存

### 8.2 文件操作

- [ ] 文件名匹配 (8.3 格式)
- [ ] FAT32 表簇链正确遍历 (32-bit 条目)
- [ ] 文件内容正确读取到缓冲区
- [ ] 文件大小正确报告
- [ ] 文件结束 (EOF) 正确检测

### 8.3 目录操作

- [ ] 根目录簇链遍历正确
- [ ] 文件名/扩展名正确显示
- [ ] 文件大小正确显示
- [ ] 删除条目 (0xE5) 正确跳过
- [ ] 长文件名条目正确跳过

---

## 八-b、CPU 检测与通用处理器优化 (cpu.c)

### 8b.1 CPUID 检测

- [x] CPUID 指令正确执行
- [x] CPU 厂商字符串正确获取
- [x] CPU 品牌字符串正确获取
- [x] 特性标志位正确解析 (leaf 1 EDX/ECX, leaf 7 EBX, extended 0x80000001)

### 8b.2 SIMD 启用

- [x] CR4.OSFXSR (bit 9) 正确设置 (SSE)
- [x] CR4.OSXMMEXCPT (bit 10) 正确设置 (SSE)
- [x] CR4.OSXSAVE (bit 18) 正确设置 (AVX)
- [x] XSETBV 启用 XMM+YMM 状态 (AVX)
- [x] FPU 初始化 (FINIT) 正确执行
- [x] SSE/SSE2/AVX 特性检测正确
- [x] 运行时分派: 基线 → SSE2 → AVX

### 8b.3 快速内存操作 (fast_mem.c)

- [x] fast_memcpy: 运行时分派 (rep movsd / MOVDQA / VMOVDQA)
- [x] fast_memset: 运行时分派 (rep stosd / MOVDQA / VMOVDQA)
- [x] fast_memcmp: 运行时分派 (4-byte / PCMPEQB)

---

## 八-c、PCI 总线 (pci.c)

### 8c.1 PCI 配置空间访问

- [ ] I/O 端口 0xCF8/0xCFC 访问正确
- [ ] 配置地址格式正确 (bus/slot/func/offset)
- [ ] 读取 Vendor ID 正确
- [ ] 读取 Class Code 正确
- [ ] 读取 BAR 寄存器正确

### 8c.2 设备扫描

- [ ] 扫描所有总线 (0-255) 和设备 (0-31)
- [ ] IDE 控制器检测 (Class 0x01, Subclass 0x01)
- [ ] 设备信息正确保存

### 8c.3 Bus Master 启用

- [ ] PCI Command 寄存器 Bus Master 位设置
- [ ] DMA 传输支持确认

---

## 八-d、ATA DMA 磁盘驱动 (disk.c)

### 8d.1 PIO 模式

- [ ] ATA 端口 0x1F0-0x1F7 访问正确
- [ ] IDENTIFY DEVICE 命令执行正确
- [ ] READ SECTORS (0x20) PIO 读取正确

### 8d.2 DMA 模式

- [ ] Bus Master IDE 寄存器访问正确
- [ ] PRDT (Physical Region Descriptor Table) 设置正确
- [ ] READ DMA (0xC8) 命令执行正确
- [ ] DMA 传输完成检测正确
- [ ] 错误处理正确

---

## 九、.comx 程序加载器 (loader.c)

- [x] .comx 头部魔数验证 (0x43505800)
- [x] .comx 版本号验证 (0x01)
- [x] 代码大小验证 (不超过 32KB)
- [x] CPU 特性需求检查 (SSE/SSE2/MMX flags)
- [x] 校验和验证 (32-bit 滚动校验和)
- [x] 代码复制到加载地址 (默认 0x20000)
- [x] BSS 段清零
- [x] 跳转到入口点执行
- [x] 程序终止后正确返回 Shell
- [x] 无效文件显示错误信息

### 9.1 .comx 构建工具 (mkcomx.py)

- [x] 读取 flat binary 输入
- [x] 生成 32 字节 .comx 头部
- [x] 计算校验和 (与内核 loader_checksum 一致)
- [x] 支持 --entry, --bss, --load-addr, --flags 参数

---

## 十、系统调用 — INT 21h (syscall.S)

- [ ] INT 21h 中断向量正确设置
- [ ] AH=0x01: 读字符并回显
- [ ] AH=0x02: 写字符 (DL=字符)
- [ ] AH=0x09: 写字符串 (DS:DX=地址)
- [ ] AH=0x0A: 读字符串到缓冲区
- [ ] AH=0x25: 设置中断向量
- [ ] AH=0x35: 获取中断向量
- [ ] AH=0x4C: 程序终止，AL=返回码
- [ ] 未知功能号正确处理

---

## 十一、汇编代码规范检查

所有 .S 文件必须通过以下检查：

- [ ] 使用 GAS AT&T 语法 (非 Intel 语法)
- [ ] 操作数顺序：`source, destination`
- [ ] 寄存器带 `%` 前缀
- [ ] 立即数带 `$` 前缀
- [ ] 指令后缀正确：`movb`, `movw`, `movl`
- [ ] 文件头包含 Nexsteaduser 品牌声明
- [ ] 文件头包含作者 Tinmc189623 和团队 Nexlyh
- [ ] 函数级注释完整
- [ ] 无占位符代码
- [ ] 无模拟代码

---

## 十二、C 代码规范检查

所有 .c 和 .h 文件必须通过以下检查：

- [ ] 使用 C99 标准
- [ ] 包含 `plexsdos/types.h` 或等效类型定义
- [ ] 函数级注释完整
- [ ] 文件头包含 Nexsteaduser 品牌声明
- [ ] 文件头包含作者 Tinmc189623 和团队 Nexlyh
- [ ] 无占位符代码
- [ ] 无模拟代码
- [ ] 品牌名「Nexsteaduser」完整使用，未被拆分
- [ ] 无任何「Nexstead」表述

---

## 十三、集成测试

### 13.1 QEMU 测试

- [ ] 冷启动引导成功
- [ ] Shell 提示符显示
- [ ] 键盘输入响应
- [ ] 内置命令全部测试通过
- [ ] 文件读取测试通过
- [ ] 外部程序加载测试通过
- [ ] 程序终止返回 Shell
- [ ] 热重启测试通过

### 13.2 边界测试

- [ ] 空命令输入
- [ ] 超长命令输入 (溢出检查)
- [ ] 不存在的文件名
- [ ] 空软盘引导
- [ ] 损坏的 FAT 表

---

## 十四、文档完整性

- [ ] CLAUDE.md — 项目指令完整
- [ ] spec.md — 技术规格完整
- [ ] plan.md — 实现计划完整
- [ ] checklist.md — 本检查清单完整
- [ ] 源码注释覆盖率 > 80%

---

## 十五、发布前检查

- [ ] 所有阶段完成
- [ ] 所有测试通过
- [ ] QEMU 全功能验证
- [ ] Bochs 兼容性测试 (可选)
- [ ] 物理硬件测试 (可选)
- [ ] 构建产物大小合理 (< 1.44MB)
- [ ] 版本号更新
- [ ] 最终镜像可引导

---

*Nexsteaduser — PlexsDOS*
*作者: Tinmc189623 | 团队: Nexlyh*
