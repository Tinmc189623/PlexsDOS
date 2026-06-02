/*
 * Nexsteaduser — PlexsDOS
 * 内核入口 (32-bit 保护模式)
 * 作者: Tinmc189623 | 团队: Nexlyh
 *
 * 引导扇区通过 ljmp 0x08:0x30000 跳转到此处 (已进入 32-bit 保护模式)。
 * 设置段选择子和栈, 初始化 COM1 串口用于调试输出,
 * 然后调用 C 语言 kernel_main()。
 * 汇编语法: GAS AT&T
 */

/* COM1 端口基址 */
.set COM1, 0x3F8

.code32
.section .text

.global _start
.global ___chkstk_ms
.global _boot_drive          /* 供 kernel_main 读取启动驱动器号 */

/*
 * _start — 内核入口点 (32-bit 保护模式)
 * GDT 代码段选择子 = 0x08, 数据段选择子 = 0x10
 *
 * 进入时 DL = 启动驱动器号 (由 BIOS 设置, 经 MBR/VBR 传递):
 *   0x00-0x7F: 软盘
 *   0x80-0xFF: 硬盘
 *
 * 注意: _start 必须是 .text 段的第一个符号,
 * 引导扇区的 ljmp $0x08, $0x30000 跳转到此地址。
 */
_start:
    /* 保存启动驱动器号 (DL, 由 BIOS 经 MBR/VBR 传递至此) */
    mov  %dl, _boot_drive

    /* 设置数据段选择子 */
    mov  $0x10, %ax         /* 数据段选择子 */
    mov  %ax, %ds
    mov  %ax, %es
    mov  %ax, %fs
    mov  %ax, %gs

    /* 设置栈 */
    mov  %ax, %ss
    mov  $0x90000, %esp     /* 栈顶设在 0x90000 */
    mov  %esp, %ebp

    /* 初始化 COM1 串口 (完整初始化序列) */
    call serial_init

    /* 清零 .bss 段 (ISO/CD-ROM 启动时 BIOS 会在该区域留下垃圾数据) */
    mov  $_bss_start, %edi
    mov  $_bss_end, %ecx
    sub  %edi, %ecx            /* ecx = _bss_end - _bss_start (字节数) */
    xor  %al, %al
    rep  stosb

    /* 串口输出: 内核已加载 */
    mov  $msg_kernel_entry, %esi
    call serial_puts

    /* 调用 C 内核主函数 */
    call _kernel_main

    /* kernel_main 不应返回 */
    mov  $msg_kernel_return, %esi
    call serial_puts
.halt:
    cli
    hlt
    jmp  .halt

/* 串口调试消息 */
msg_kernel_entry:  .asciz "[entry] kernel_entry.S: _start reached, calling kernel_main\n"
msg_kernel_return: .asciz "[entry] kernel_main returned!\n"

/*
 * serial_init — 初始化 COM1 串口 (9600 波特率, 8N1)
 * 用于内核最早期的调试输出, 在 VGA 初始化之前即可使用。
 */
serial_init:
    mov  $0x00, %al
    mov  $(COM1 + 1), %dx
    outb %al, %dx              /* 禁用中断 */

    mov  $0x80, %al
    mov  $(COM1 + 3), %dx
    outb %al, %dx              /* 启用 DLAB */

    mov  $0x0C, %al
    mov  $COM1, %dx
    outb %al, %dx              /* 波特率低字节 (divisor=12 → 9600 baud) */

    mov  $0x00, %al
    mov  $(COM1 + 1), %dx
    outb %al, %dx              /* 波特率高字节 */

    mov  $0x03, %al
    mov  $(COM1 + 3), %dx
    outb %al, %dx              /* 8 数据位, 无校验, 1 停止位 */

    mov  $0xC7, %al
    mov  $(COM1 + 2), %dx
    outb %al, %dx              /* 启用 FIFO, 清空, 14 字节阈值 */

    mov  $0x0B, %al
    mov  $(COM1 + 4), %dx
    outb %al, %dx              /* IRQ 启用, RTS/DSR */
    ret

/*
 * serial_putc — 通过 COM1 输出一个字符
 * %al = 要输出的字符
 */
serial_putc:
    push %dx
    push %cx
    mov  %al, %cl
.sp_wait:
    mov  $(COM1 + 5), %dx
    inb  %dx, %al
    test $0x20, %al            /* 检查传输缓冲区空 */
    jz   .sp_wait
    mov  %cl, %al
    mov  $COM1, %dx
    outb %al, %dx
    pop  %cx
    pop  %dx
    ret

/*
 * serial_puts — 通过 COM1 输出以 null 结尾的字符串
 * %esi = 字符串地址
 */
serial_puts:
    push %eax
.sp_loop:
    lodsb
    test %al, %al
    jz   .sp_done
    call serial_putc
    jmp  .sp_loop
.sp_done:
    pop  %eax
    ret

/*
 * ___chkstk_ms — GCC 栈探针桩 (裸机环境无需探测)
 * MinGW GCC 对大栈帧自动生成此调用, 内核中直接返回。
 */
___chkstk_ms:
    ret

/* 启动驱动器号 (由 _start 从 DL 保存, 供 kernel_main 读取) */
.section .data
.align 1
_boot_drive: .byte 0
