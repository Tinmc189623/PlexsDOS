# Nexsteaduser — PlexsDOS Makefile
# 作者: Tinmc189623 | 团队: Nexlyh
#
# 自研内核 (32-bit 保护模式, 通用处理器优化)
# 引导扇区 (16-bit 实模式) → GDT → 32-bit PM → 内核
# 软盘: FAT12, 1.44MB (引导 + 内核)
# 硬盘: FAT32, 64MB (数据: .comx 程序, 用户文件)
#
# 安装盘:
#   plexsdos-x86_32-0.1-beta-install_disks_01.img — 启动盘 (引导 + 内核 + 安装程序)
#   plexsdos-x86_32-0.1-beta-install_disks_02.img — 安装盘 2 (内核核心文件)
#   plexsdos-x86_32-0.1-beta-install_disks_03.img — 安装盘 3 (程序文件)
#   plexsdos-x86_32-0.1-beta-install_disks_04.img — 安装盘 4 (驱动/库)
#   plexsdos-x86_32-0.1-beta-install_disks_05.img — 安装盘 5 (文档/示例)

CC      = gcc
CXX     = g++
AS      = as
LD      = ld
OBJCOPY = objcopy
PYTHON  = /c/Users/Tindo/AppData/Local/Python/bin/python.exe

# i686 基线: CMOV, FPU, TSC — 所有 32-bit x86 处理器通用
# 不锁定任何特定 SIMD 扩展 — 运行时检测并分派
CFLAGS   = -m32 -march=i686 \
           -ffreestanding -fno-builtin -nostdlib \
           -fno-stack-check -fno-stack-protector \
           -fno-asynchronous-unwind-tables \
           -Wall -Wextra -std=c23 -O2 -Iinclude \
           -DMINIMAL_KERNEL \
           -MMD -MP

# C++ 编译: 无异常、无 RTTI、无线程安全静态、freestanding
CXXFLAGS = -m32 -march=i686 \
           -ffreestanding -fno-builtin -nostdlib \
           -fno-exceptions -fno-rtti -fno-threadsafe-statics \
           -fno-stack-check -fno-stack-protector \
           -fno-asynchronous-unwind-tables \
           -Wall -Wextra -std=c++23 -O2 -Iinclude \
           -MMD -MP

ASFLAGS  = --32 -Iinclude
LD_KERN  = -m i386pe -T linker.ld --image-base 0x0 -nostdlib -N --section-alignment 0x200

export TEMP = C:\Users\Tindo\tmp
export TMP  = C:\Users\Tindo\tmp
export PATH := /c/msys64/mingw32/bin:$(PATH)

# 自动依赖文件 (.d), 与目标 .o 文件一一对应
DEPFILES = $(OBJS:.o=.d)
-include $(DEPFILES)

BUILD_DIR  = build

SRCS_S := $(wildcard boot/*.S) \
          $(wildcard kernel/*.S) \
          $(wildcard kernel/arch/*.S) \
          $(wildcard kernel/drivers/*.S)
SRCS_C := $(wildcard kernel/*.c) \
          $(filter-out kernel/arch/syscall.c,$(wildcard kernel/arch/*.c)) \
          $(filter-out kernel/debug/%.c,$(wildcard kernel/debug/*.c)) \
          $(wildcard kernel/drivers/*.c) \
          $(filter-out kernel/editor/%.c,$(wildcard kernel/editor/*.c)) \
          $(filter-out kernel/gui/%.c,$(wildcard kernel/gui/*.c)) \
          $(wildcard kernel/hal/*.c) \
          $(wildcard kernel/mm/*.c) \
          $(wildcard kernel/shell/*.c) \
          $(wildcard kernel/fs/*.c) \
          $(wildcard lib/*.c) \
          $(wildcard kernel/shim/*.c) \
          $(wildcard kernel/dm/*.c) \
          $(wildcard kernel/sched/*.c) \
          $(wildcard kernel/security/*.c)

SRCS_CXX := $(wildcard kernel/*.cpp) \
            $(wildcard kernel/arch/*.cpp) \
            $(wildcard kernel/drivers/*.cpp) \
            $(wildcard kernel/hal/*.cpp) \
            $(wildcard kernel/shell/*.cpp) \
            $(wildcard lib/*.cpp)

OBJS := $(patsubst %.S,$(BUILD_DIR)/%.o,$(SRCS_S)) \
        $(patsubst %.c,$(BUILD_DIR)/%.o,$(SRCS_C)) \
        $(patsubst %.cpp,$(BUILD_DIR)/%.o,$(SRCS_CXX))

BOOT_OBJ  = $(BUILD_DIR)/boot/boot_sector.o
KERN_OBJS = $(filter-out $(BUILD_DIR)/boot/%,$(OBJS)) $(CONFIG_SYS_O)

BOOT_BIN   = $(BUILD_DIR)/boot.bin
KERNEL_BIN = $(BUILD_DIR)/kernel.bin
FLOPPY_IMG = $(BUILD_DIR)/plexsdos.img

# FAT32 硬盘镜像 (64MB, 用于测试文件系统)
DISK_IMG   = $(BUILD_DIR)/disk.img

# 硬盘引导扇区
HD_MBR_BIN = $(BUILD_DIR)/hd_mbr.bin
HD_VBR_BIN = $(BUILD_DIR)/hd_vbr.bin

# 测试程序
PROG_HELLO_SRC = programs/test_hello.S
PROG_HELLO_O   = $(BUILD_DIR)/programs/test_hello.o
PROG_HELLO_EXE = $(BUILD_DIR)/programs/test_hello.exe
PROG_HELLO_BIN = $(BUILD_DIR)/programs/HELLO.BIN
PROG_HELLO_COMX = $(BUILD_DIR)/programs/HELLO.COMX

# CONFIG.SYS 启动配置文件 (嵌入内核二进制)
CONFIG_SYS_SRC = programs/CONFIG.SYS
CONFIG_SYS_O   = $(BUILD_DIR)/programs/config_sys_embed.o

# PnP 管理器
PROG_PNP_SRC = programs/pnp.S
PROG_PNP_O   = $(BUILD_DIR)/programs/pnp.o
PROG_PNP_EXE = $(BUILD_DIR)/programs/pnp.exe
PROG_PNP_BIN = $(BUILD_DIR)/programs/PNP.BIN
PROG_PNP_COMX = $(BUILD_DIR)/programs/PNP.COMX

# 安装软盘镜像 (3 张: 启动盘 + 系统盘 + 程序盘)
INSTALL_BASE = plexsdos-x86_32-0.1-beta-install_disks
DISK1_IMG = $(BUILD_DIR)/$(INSTALL_BASE)_01.img
DISK2_IMG = $(BUILD_DIR)/$(INSTALL_BASE)_02.img
DISK3_IMG = $(BUILD_DIR)/$(INSTALL_BASE)_03.img

# ISO 9660 可引导光盘镜像 (El Torito 2.88MB 软盘仿真)
ISO_IMG = $(BUILD_DIR)/plexsdos.iso

# 40GB VMDK 虚拟硬盘镜像 (预装完整操作系统)
VMDK_IMG = $(BUILD_DIR)/plexsdos.vmdk

.PHONY: all clean run run-floppy run-iso disk install-disks iso vmdk

all: $(FLOPPY_IMG) $(DISK_IMG) $(ISO_IMG) $(DISK1_IMG) $(DISK2_IMG) $(DISK3_IMG)

# 通用编译规则 (GAS 汇编 / C / C++), 自动生成 .d 依赖文件
# 支持 make -jN 并行构建
$(BUILD_DIR)/%.o: %.S
	@mkdir -p $(dir $@)
	$(AS) $(ASFLAGS) $< -o $@

$(BUILD_DIR)/%.o: %.s
	@mkdir -p $(dir $@)
	$(AS) $(ASFLAGS) $< -o $@

$(BUILD_DIR)/%.o: %.c
	@mkdir -p $(dir $@)
	$(CC) $(CFLAGS) -c $< -o $@

$(BUILD_DIR)/%.o: %.cpp
	@mkdir -p $(dir $@)
	$(CXX) $(CXXFLAGS) -c $< -o $@

# 将 CONFIG.SYS 文本文件转为 .o (通过 objcopy 嵌入为二进制符号)
$(BUILD_DIR)/programs/config_sys_embed.o: programs/CONFIG.SYS
	@mkdir -p $(dir $@)
	$(OBJCOPY) -I binary -O pe-i386 -B i386 $< $@

# hd_boot.S 使用 .incbin 嵌入 MBR/VBR 二进制, 必须先编译 MBR/VBR
$(BUILD_DIR)/kernel/hd_boot.o: kernel/hd_boot.S $(HD_MBR_BIN) $(HD_VBR_BIN)
	@mkdir -p $(dir $@)
	$(AS) $(ASFLAGS) $< -o $@

$(BUILD_DIR)/boot.elf: $(BOOT_OBJ)
	$(LD) -m i386pe -Ttext 0x7C00 --image-base 0x0 -nostdlib $< -o $@

$(BOOT_BIN): $(BUILD_DIR)/boot.elf
	$(OBJCOPY) -O binary -j .text $< $@

# 编译硬盘 MBR 为 flat binary
$(HD_MBR_BIN): boot/hd_mbr.S
	@mkdir -p $(dir $@)
	$(AS) --32 $< -o $(BUILD_DIR)/hd_mbr.o
	$(LD) -m i386pe -Ttext 0x600 --image-base 0x0 -nostdlib $(BUILD_DIR)/hd_mbr.o -o $(BUILD_DIR)/hd_mbr.elf
	$(OBJCOPY) -O binary -j .text $(BUILD_DIR)/hd_mbr.elf $@

# 编译硬盘 VBR 为 flat binary
$(HD_VBR_BIN): boot/hd_vbr.S
	@mkdir -p $(dir $@)
	$(AS) --32 $< -o $(BUILD_DIR)/hd_vbr.o
	$(LD) -m i386pe -Ttext 0x7C00 --image-base 0x0 -nostdlib $(BUILD_DIR)/hd_vbr.o -o $(BUILD_DIR)/hd_vbr.elf
	$(OBJCOPY) -O binary -j .text $(BUILD_DIR)/hd_vbr.elf $@
	$(PYTHON) tools/fix_vbr.py $@

# 内核链接 + strip 去符号 (MBR/VBR 通过 hd_boot.S 的 .incbin 嵌入)
$(KERNEL_BIN): $(KERN_OBJS) $(HD_MBR_BIN) $(HD_VBR_BIN)
	$(LD) $(LD_KERN) $(KERN_OBJS) -o $(BUILD_DIR)/kernel.exe
	$(OBJCOPY) -O binary -j .text -j .rdata -j .rodata -j .data $(BUILD_DIR)/kernel.exe $@

# 创建软盘镜像 (FAT12, 1.44MB)
$(FLOPPY_IMG): $(BOOT_BIN) $(KERNEL_BIN)
	$(PYTHON) tools/mkfloppy.py $@ $(BOOT_BIN) $(KERNEL_BIN)

# 编译测试程序 (需链接以正确解析绝对地址)
$(PROG_HELLO_O): $(PROG_HELLO_SRC)
	@mkdir -p $(dir $@)
	$(AS) $(ASFLAGS) $< -o $@

$(PROG_HELLO_EXE): $(PROG_HELLO_O)
	$(LD) -m i386pe -Ttext 0x50000 --image-base 0x0 -nostdlib --section-alignment 0x200 $< -o $@

$(PROG_HELLO_BIN): $(PROG_HELLO_EXE)
	$(OBJCOPY) -O binary -j .text $< $@

# 打包为 .comx 格式
$(PROG_HELLO_COMX): $(PROG_HELLO_BIN)
	$(PYTHON) tools/mkcomx.py $< $@ --load-addr=0x50000

# 编译 PnP 管理器 (需链接以正确解析 .bss/.rodata 绝对地址)
$(PROG_PNP_O): $(PROG_PNP_SRC)
	@mkdir -p $(dir $@)
	$(AS) $(ASFLAGS) $< -o $@

$(PROG_PNP_EXE): $(PROG_PNP_O)
	$(LD) -m i386pe -Ttext 0x50000 --image-base 0x0 -nostdlib --section-alignment 0x200 $< -o $@

$(PROG_PNP_BIN): $(PROG_PNP_EXE)
	$(OBJCOPY) -O binary -j .text $< $@

# 打包为 .comx 格式
$(PROG_PNP_COMX): $(PROG_PNP_BIN)
	$(PYTHON) tools/mkcomx.py $< $@ --load-addr=0x50000

# 创建 FAT32 硬盘镜像 (64MB)
$(DISK_IMG): $(PROG_HELLO_COMX) $(PROG_PNP_COMX) programs/README.TXT
	$(PYTHON) tools/mkfat32.py $@ $(PROG_HELLO_COMX) $(PROG_PNP_COMX) programs/README.TXT

# ===== 安装软盘构建 =====

# 1 号盘: 启动盘 (引导扇区 + 内核)
$(DISK1_IMG): $(BOOT_BIN) $(KERNEL_BIN)
	$(PYTHON) tools/mkbootdisk.py $@ $(BOOT_BIN) $(KERNEL_BIN)

# 2 号盘: 内核核心文件
$(DISK2_IMG): $(KERNEL_BIN)
	$(PYTHON) tools/mkfat12.py $@ $(KERNEL_BIN)

# 3 号盘: 程序 + 文档
$(DISK3_IMG): $(PROG_HELLO_COMX) $(PROG_PNP_COMX) programs/README.TXT
	$(PYTHON) tools/mkfat12.py $@ $(PROG_HELLO_COMX) $(PROG_PNP_COMX) programs/README.TXT

# 仅构建磁盘镜像
disk: $(DISK_IMG)

# 仅构建安装盘
install-disks: $(DISK1_IMG) $(DISK2_IMG) $(DISK3_IMG)

# ISO 9660 可引导光盘镜像 (El Torito 2.88MB 软盘仿真)
# 替代 5 张安装软盘, 所有文件统一放在 ISO 中
iso: $(ISO_IMG)

$(ISO_IMG): $(DISK1_IMG) $(PROG_HELLO_COMX) $(PROG_PNP_COMX) programs/README.TXT
	$(PYTHON) tools/mkiso.py $@ --boot-img $(DISK1_IMG) \
		$(PROG_HELLO_COMX) $(PROG_PNP_COMX) programs/README.TXT

# 40GB VMDK 虚拟硬盘镜像 (预装完整操作系统)
vmdk: $(VMDK_IMG)

$(VMDK_IMG): $(HD_MBR_BIN) $(HD_VBR_BIN) $(KERNEL_BIN) $(PROG_HELLO_COMX) $(PROG_PNP_COMX) programs/README.TXT
	$(PYTHON) tools/mkvmdk.py $@ \
		--mbr $(HD_MBR_BIN) --vbr $(HD_VBR_BIN) \
		--kernel $(KERNEL_BIN) \
		--file $(PROG_HELLO_COMX) --file $(PROG_PNP_COMX) --file programs/README.TXT \
		--size 40G

# QEMU 测试 (软盘启动 + ATA 硬盘)
run: $(FLOPPY_IMG) $(DISK_IMG)
	qemu-system-i386 -fda $(FLOPPY_IMG) -hda $(DISK_IMG) -boot a -m 64M

# VMDK 启动测试
run-vmdk: $(VMDK_IMG)
	qemu-system-i386 -hda $(VMDK_IMG) -boot c -m 64M -serial stdio

# 仅软盘启动 (无硬盘)
run-floppy: $(FLOPPY_IMG)
	qemu-system-i386 -fda $(FLOPPY_IMG) -boot a -m 64M

# ISO 光盘启动 + ATA 硬盘
run-iso: $(ISO_IMG) $(DISK_IMG)
	qemu-system-i386 -cdrom $(ISO_IMG) -hda $(DISK_IMG) -boot d -m 64M

# 仅 ISO 光盘启动 (无硬盘)
run-iso-only: $(ISO_IMG)
	qemu-system-i386 -cdrom $(ISO_IMG) -boot d -m 64M

clean:
	rm -rf $(BUILD_DIR)
	@echo "Cleaned build directory and dependency files."
