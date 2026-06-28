; Nexsteaduser — PlexsDOS
; 内核入口 (32-bit 保护模式)
; 作者: Tinmc189623 | 团队: Nexlyh

; 引导扇区通过 jmp 0x08:0x30000 跳转到此处 (已进入 32-bit 保护模式)。
; 设置段选择子和栈, 初始化 COM1 串口用于调试输出,
; 然后调用 C 语言 kernel_main()。
; 语法: NASM Intel, 32-bit 保护模式。

; Copyright © 2026 Nexsteaduser. All Rights Reserved

; ----------------------------------------------------------------------------
; COM1 端口基址
; ----------------------------------------------------------------------------
COM1 equ 0x3F8

bits 32
section .text

global _start
global ___chkstk_ms
global _boot_drive          ; 供 kernel_main 读取启动驱动器号

; 外部符号 — 由链接器解析 (C 函数 / linker.ld 定义的段边界)
extern _bss_start
extern _bss_end
extern _kernel_main

; ----------------------------------------------------------------------------
; _start — 内核入口点 (32-bit 保护模式)
; GDT 代码段选择子 = 0x08, 数据段选择子 = 0x10

; 进入时 DL = 启动驱动器号 (由 BIOS 设置, 经 MBR/VBR 传递):
; 0x00-0x7F: 软盘
; 0x80-0xFF: 硬盘

; 注意: _start 必须是 .text 段的第一个符号,
; 引导扇区的 jmp 0x08, 0x30000 跳转到此地址。
; ----------------------------------------------------------------------------
_start:
    ; 保存启动驱动器号 (DL, 由 BIOS 经 MBR/VBR 传递至此)
    mov [_boot_drive], dl

    ; 设置数据段选择子
    mov ax, 0x10          ; 数据段选择子
    mov ds, ax
    mov es, ax
    mov fs, ax
    mov gs, ax

    ; 设置栈
    mov ss, ax
    mov esp, 0x90000      ; 栈顶设在 0x90000
    mov ebp, esp

    ; 初始化 COM1 串口 (完整初始化序列)
    call serial_init

    ; 清零 .bss 段 (ISO/CD-ROM 启动时 BIOS 会在该区域留下垃圾数据)
    cld                        ; 确保方向标志为 0, stosb 正向递增 EDI
    mov edi, _bss_start
    mov ecx, _bss_end
    sub ecx, edi               ; ecx = _bss_end - _bss_start (字节数)
    xor al, al
    rep stosb

    ; 串口输出: 内核已加载
    mov esi, msg_kernel_entry
    call serial_puts

    ; 调用 C 内核主函数
    call _kernel_main

    ; kernel_main 不应返回
    mov esi, msg_kernel_return
    call serial_puts
.halt:
    cli
    hlt
    jmp .halt

; 串口调试消息 (NASM 的 db 不自动加 0, 必须显式追加)
msg_kernel_entry:  db "[entry] kernel_entry.asm: _start reached, calling kernel_main", 10, 0
msg_kernel_return: db "[entry] kernel_main returned!", 10, 0

; ----------------------------------------------------------------------------
; serial_init — 初始化 COM1 串口 (9600 波特率, 8N1)
; 用于内核最早期的调试输出, 在 VGA 初始化之前即可使用。
; ----------------------------------------------------------------------------
serial_init:
    mov al, 0x00
    mov dx, COM1 + 1
    out dx, al              ; 禁用中断

    mov al, 0x80
    mov dx, COM1 + 3
    out dx, al              ; 启用 DLAB

    mov al, 0x0C
    mov dx, COM1
    out dx, al              ; 波特率低字节 (divisor=12 → 9600 baud)

    mov al, 0x00
    mov dx, COM1 + 1
    out dx, al              ; 波特率高字节

    mov al, 0x03
    mov dx, COM1 + 3
    out dx, al              ; 8 数据位, 无校验, 1 停止位

    mov al, 0xC7
    mov dx, COM1 + 2
    out dx, al              ; 启用 FIFO, 清空, 14 字节阈值

    mov al, 0x0B
    mov dx, COM1 + 4
    out dx, al              ; IRQ 启用, RTS/DSR
    ret

; ----------------------------------------------------------------------------
; serial_putc — 通过 COM1 输出一个字符
; al = 要输出的字符
; ----------------------------------------------------------------------------
serial_putc:
    push edx
    push ecx
    mov cl, al
.sp_wait:
    mov dx, COM1 + 5
    in al, dx
    test al, 0x20           ; 检查传输缓冲区空
    jz .sp_wait
    mov al, cl
    mov dx, COM1
    out dx, al
    pop ecx
    pop edx
    ret

; ----------------------------------------------------------------------------
; serial_puts — 通过 COM1 输出以 null 结尾的字符串
; esi = 字符串地址
; ----------------------------------------------------------------------------
serial_puts:
    push eax
.sp_loop:
    lodsb
    test al, al
    jz .sp_done
    call serial_putc
    jmp .sp_loop
.sp_done:
    pop eax
    ret

; ----------------------------------------------------------------------------
; ___chkstk_ms — GCC 栈探针桩 (裸机环境无需探测)
; MinGW GCC 对大栈帧自动生成此调用, 内核中直接返回。
; ----------------------------------------------------------------------------
___chkstk_ms:
    ret

; ----------------------------------------------------------------------------
; 启动驱动器号 (由 _start 从 DL 保存, 供 kernel_main 读取)
; ----------------------------------------------------------------------------
section .data
align 1
_boot_drive: db 0
