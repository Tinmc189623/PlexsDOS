/*
 * Nexsteaduser — PlexsDOS
 * 引导扇区 (MBR Boot Sector)
 * 作者: Tinmc189623 | 团队: Nexlyh
 *
 * BIOS 引导阶段 (16-bit 实模式) → 切换到 32-bit 保护模式 → 跳转到内核。
 * 注意: .code16 仅用于 BIOS 引导阶段, 内核运行在 32-bit 保护模式。
 * 汇编语法: GAS AT&T
 */

.code16
.section .text

.global _start

/* ===== 引导扇区入口 ===== */
_start:
    jmp boot_init           /* 跳过 BPB, 进入引导代码 */
    nop

/* ===== BIOS 参数块 (BPB) - FAT12 格式 ===== */
oem_name:        .ascii "PLXSDOS "
bytes_per_sec:   .word  512
sec_per_clust:   .byte  1
reserved_secs:   .word  1
num_fats:        .byte  2
root_entries:    .word  224
total_secs:      .word  2880
media_desc:      .byte  0xF0
fat_size:        .word  9
sec_per_track:   .word  18
num_heads:       .word  2
hidden_secs:     .long  0
total_secs_l:    .long  0
drive_num:       .byte  0
reserved_bpb:    .byte  0
boot_sig:        .byte  0x29
vol_serial:      .long  0x12345678
vol_label:       .ascii "PLXSDOS    "
fs_type:         .ascii "FAT12   "

/* ===== 引导初始化 ===== */
boot_init:
    cli
    xor  %ax, %ax
    mov  %ax, %ds
    mov  %ax, %es
    mov  %ax, %ss
    mov  $0x7C00, %sp
    sti

    mov  %dl, (boot_drive)

    mov  $msg_loading, %si
    call print_string

/* ===== 加载内核到 0x30000 (逐扇区读取) ===== */
load_kernel:
    mov  $0x3000, %ax       /* ES = 0x3000, 物理地址 = ES*16 = 0x30000 */
    mov  %ax, %es
    mov  $0x0000, %bx       /* BX 从 0 开始, 每 128 扇区通过 ES 推进避免溢出 */
    movw $KERNEL_SECTORS, (sectors_left)   /* mkfloppy 修补 KSEC 后的立即数 */
    movb $0x00, (cur_cyl)
    movb $0x00, (cur_head)
    movb $0x02, (cur_sec)

/* 逐扇区读取: 每次 INT 13h 只读 1 个扇区, 避免多磁道问题 */
load_loop:
    cmpw $0, (sectors_left)
    je   load_done

    /* INT 13h AH=02: 读取 1 个扇区 */
    mov  $0x02, %ah
    mov  $0x01, %al
    movb (cur_cyl), %ch
    movb (cur_sec), %cl
    movb (cur_head), %dh
    movb (boot_drive), %dl
    int  $0x13
    jc   disk_error

    /* 更新: 缓冲区指针 +512 */
    add  $512, %bx
    jnc  1f                      /* 无进位, 跳过段调整 */

    /* BX 溢出: ES += 0x1000 (前进 64KB), BX 已自动回绕 */
    mov  %es, %ax
    add  $0x1000, %ax
    mov  %ax, %es
1:
    decw (sectors_left)

    /* 推进 CHS: cur_sec++ */
    incb (cur_sec)
    movb (cur_sec), %al
    cmpb $19, %al               /* SPT+1 = 19 (扇区号 1-18) */
    jb   load_loop              /* 未越界, 继续 */

    /* 越界: 重置扇区号, 推进磁头 */
    movb $1, (cur_sec)
    incb (cur_head)
    cmpb $2, (cur_head)
    jb   load_loop
    /* 磁头越界: 推进柱面 */
    movb $0, (cur_head)
    incb (cur_cyl)
    jmp  load_loop

load_done:
    mov  $msg_ok, %si
    call print_string

/* ===== 切换到 32-bit 保护模式 ===== */
switch_to_pm:
    cli                     /* 关中断 */

    /*
     * 手动编码保护模式切换指令:
     * GAS 在 16-bit 模式下无法正确生成 0x66 操作数大小前缀,
     * 必须手动编码以确保 32-bit 操作数。
     */

    /* lgdt: 加载 GDT (32-bit 基址) */
    .byte 0x66, 0x0F, 0x01, 0x16    /* 0x66 lgdt m16:32 */
    .word gdt_descriptor             /* GDT 描述符偏移 */

    /* 读取 CR0 */
    .byte 0x0F, 0x20, 0xC0          /* mov %cr0, %eax */

    /* 设置 PE 位 (CR0 bit 0) */
    .byte 0x66, 0x83, 0xC8, 0x01    /* or $0x01, %eax (32-bit) */

    /* 写回 CR0, 启用保护模式 */
    .byte 0x0F, 0x22, 0xC0          /* mov %eax, %cr0 */

    /* 远跳转: 进入 32-bit 保护模式, 跳转到内核 */
    .byte 0x66, 0xEA                 /* ljmp with 32-bit offset */
    .long pm_stub                    /* EIP = pm_stub (32-bit 保护模式桩) */
    .word 0x0008                     /* CS = 0x08 (代码段选择子) */

/* ===== 32-bit 保护模式桩 (内嵌在引导扇区中) ===== */
.code32
pm_stub:
    /* 设置数据段选择子 */
    mov  $0x10, %ax
    mov  %ax, %ds
    mov  %ax, %es
    mov  %ax, %ss
    mov  $0x90000, %esp

    /* 跳转到内核 */
    ljmp $0x08, $0x30000

.code16

/* ===== 磁盘错误 ===== */
disk_error:
    mov  $msg_disk_err, %si
    call print_string
    mov  %ah, %al
    call print_hex_byte
    mov  $msg_newline, %si
    call print_string
    cli
    hlt

/* ===== 辅助函数 (实模式) ===== */
print_string:
    pusha
    mov  $0x0E, %ah
    mov  $0x0007, %bx
.ps_loop:
    lodsb
    cmp  $0, %al
    je   .ps_done
    int  $0x10
    jmp  .ps_loop
.ps_done:
    popa
    ret

/* serial_putc — 将 AL 中的字符发送到 COM1 (0x3F8) */
serial_putc:
    push %dx
    push %ax
    mov  $0x3F8, %dx
    add  $5, %dx           /* 0x3FD: 线状态寄存器 */
.sc_wait:
    inb  %dx, %al
    test $0x20, %al        /* 检查 THRE (发送保持寄存器空) */
    jz   .sc_wait
    pop  %ax
    sub  $5, %dx           /* 0x3F8: 数据寄存器 */
    outb %al, %dx
    pop  %dx
    ret

print_hex_byte:
    pusha
    mov  %al, %cl
    shr  $4, %al
    and  $0x0F, %al
    call .phex_nibble
    mov  %cl, %al
    and  $0x0F, %al
    call .phex_nibble
    popa
    ret

.phex_nibble:
    cmp  $10, %al
    jl   .phex_digit
    add  $0x37, %al
    jmp  .phex_out
.phex_digit:
    add  $0x30, %al
.phex_out:
    mov  $0x0E, %ah
    int  $0x10
    ret

/* ===== GDT (全局描述符表) ===== */
gdt_start:
    /* 空描述符 (必须) */
    .quad 0x0000000000000000

    /* 代码段: base=0, limit=4GB, 32-bit, 可执行/可读 */
gdt_code:
    .word 0xFFFF            /* Limit 0-15 */
    .word 0x0000            /* Base 0-15 */
    .byte 0x00              /* Base 16-23 */
    .byte 0x9A              /* Access: Present, Ring 0, Code, Readable */
    .byte 0xCF              /* Flags+Limit 16-19: 4KB granularity, 32-bit */
    .byte 0x00              /* Base 24-31 */

    /* 数据段: base=0, limit=4GB, 32-bit, 可读/可写 */
gdt_data:
    .word 0xFFFF
    .word 0x0000
    .byte 0x00
    .byte 0x92              /* Access: Present, Ring 0, Data, Writable */
    .byte 0xCF
    .byte 0x00

gdt_end:

gdt_descriptor:
    .word gdt_end - gdt_start - 1   /* GDT 大小 - 1 */
    .long gdt_start                  /* GDT 线性地址 */

/* ===== 数据 ===== */
.set KERNEL_SECTORS, 47

boot_drive:    .byte 0
cur_cyl:       .byte 0
cur_head:      .byte 0
cur_sec:       .byte 2

/* 标记: 构建工具通过搜索 "KSEC" 找到此位置并修补 sectors_left 值 */
ksec_marker:   .ascii "KSEC"
sectors_left:  .word KERNEL_SECTORS
msg_loading:   .asciz "PlexsDOS loading...\r\n"
msg_ok:        .asciz "OK\r\n"
msg_disk_err:  .asciz "Disk err: 0x"
msg_newline:   .asciz "\r\n"

/* ===== 引导签名 ===== */
.fill 510 - (. - _start), 1, 0
.word 0xAA55
