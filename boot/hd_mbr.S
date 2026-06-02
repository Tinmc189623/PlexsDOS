/*
 * Nexsteaduser — PlexsDOS
 * 硬盘主引导记录 (MBR)
 * 作者: Tinmc189623 | 团队: Nexlyh
 *
 * 标准 MBR 引导代码 (自重定位):
 * 1. 将自身从 0x7C00 复制到 0x0600
 * 2. 在 0x0600 继续执行
 * 3. 扫描分区表找到活动分区
 * 4. 读取分区起始 LBA, 使用 INT 13h 扩展读取 (AH=42h) 加载 VBR 到 0x7C00
 * 5. 跳转到 VBR
 *
 * 使用 INT 13h 扩展读取 (LBA 模式), 无需 CHS 转换
 *
 * 汇编语法: GAS AT&T
 */

.code16
.section .text
.global _start

/* 硬盘几何参数 */
.set HEADS,     16
.set SPT,       63

_start:
    /* 设置段寄存器 */
    cli
    xor  %ax, %ax
    mov  %ax, %ds
    mov  %ax, %es
    mov  %ax, %ss
    mov  $0x7C00, %sp
    sti

    /* 保存启动驱动器号 */
    mov  %dl, %cs:boot_drive - _start + 0x600

    /* 将自身从 0x7C00 复制到 0x0600 */
    cld
    mov  $0x7C00, %si
    mov  $0x0600, %di
    mov  $256, %cx        /* 512 字节 = 256 字 */
    rep  movsw

    /* 跳转到 0x0600 处的代码 */
    ljmp $0x0000, $relocated

relocated:
    /* 现在运行在 0x0600, 0x7C00 可用 */
    mov  %cs:boot_drive, %dl

    /* 扫描分区表 (0x07BE = 0x0600 + 0x1BE) 查找活动分区 */
    mov  $0x07BE, %si
    mov  $4, %cx

scan_loop:
    cmpb $0x80, (%si)
    jz   found_active
    add  $16, %si
    dec  %cx
    jnz  scan_loop

    /* 没有活动分区 */
    mov  $err_no_part, %si
    call print_string
    jmp  halt

found_active:
    /* 读取分区起始 LBA (字节 8-11) */
    mov  0x08(%si), %eax        /* EAX = LBA */
    mov  %eax, %cs:part_lba

    /* 重置磁盘控制器 */
    mov  %cs:boot_drive, %dl
    xor  %ah, %ah
    int  $0x13

    /* 使用 INT 13h 扩展读取 (AH=42h) 加载 VBR */
    /* 构建 DAP (Disk Address Packet) 在栈上 */
    /* DAP: size(1) + reserved(1) + count(2) + addr(4) + LBA(8) = 16 字节 */
    xor  %ax, %ax
    mov  %ax, %es               /* ES = 0x0000 */
    mov  $0x7C00, %bx           /* BX = 0x7C00 (目标地址) */

    pushl $0                    /* LBA 高 32 位 = 0 */
    mov  %cs:part_lba, %eax
    pushl %eax                  /* LBA 低 32 位 */
    pushw %es                   /* 缓冲区段 */
    pushw %bx                   /* 缓冲区偏移 */
    pushw $1                    /* 扇区数 = 1 */
    pushw $0x0010               /* DAP 大小 = 16 字节 */

    mov  $0x42, %ah
    mov  %cs:boot_drive, %dl
    mov  %sp, %si               /* SI = DAP 指针 */
    int  $0x13

    add  $16, %sp               /* 清理栈上 DAP */
    jc   read_error

    /* 跳转到 VBR */
    mov  %cs:boot_drive, %dl
    ljmp $0x0000, $0x7C00

read_error:
    mov  $err_read, %si
    call print_string

halt:
    cli
    hlt
    jmp  halt

print_string:
    pusha
    mov  $0x0E, %ah
.loop:
    lodsb
    test %al, %al
    jz   .done
    int  $0x10
    jmp  .loop
.done:
    popa
    ret

boot_drive: .byte 0
part_lba:   .long 0
err_no_part: .asciz "No active partition\r\n"
err_read:    .asciz "Disk read error\r\n"

/* 填充到 446 字节 */
.fill 446 - (. - _start), 1, 0

/* 分区表 (4 × 16 字节) — 由安装程序填充 */
.fill 64, 1, 0

/* 引导签名 */
.word 0xAA55
