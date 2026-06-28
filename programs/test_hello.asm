; Nexsteaduser — PlexsDOS
; 测试程序: 使用 INT 21h 系统调用
; 作者: Tinmc189623 | 团队: Nexlyh

; 通过 INT 21h (向量 0x22) 系统调用输出消息。
; 演示 AH=0x02 (写字符) 和 AH=0x09 (写字符串) 功能。
; 此程序为 flat binary 格式, 入口在文件开头。
; 由 Shell 的 run 命令加载到 0x50000 (USER_LOAD_ADDR) 并执行。
; 语法: NASM Intel, 32-bit 保护模式。

; Copyright © 2026 Nexsteaduser. All Rights Reserved

bits 32
section .text
global _start

_start:
    ; VGA 调试标记: 屏幕第 2 行显示 'U3' (Ring 3 user code started)
    mov eax, 0xB8000
    mov word [eax + 160], 0x2F55   ; 'U' 绿底白字, 第 2 行
    mov word [eax + 162], 0x2F33   ; '3'

    ; 保存调用者的栈帧
    push ebp
    mov ebp, esp

    ; INT 21h AH=0x09: 写字符串 (DS:EDX = 字符串地址, '$' 结尾)
    lea edx, [msg_hello]
    mov ah, 0x09
    int 0x22                       ; INT 21h (向量 0x22)

    ; INT 21h AH=0x02: 逐字符写 "OK!"
    mov dl, 'O'
    mov ah, 0x02
    int 0x22

    mov dl, 'K'
    mov ah, 0x02
    int 0x22

    mov dl, '!'
    mov ah, 0x02
    int 0x22

    mov dl, 10                      ; '\n'
    mov ah, 0x02
    int 0x22

    ; 通过 INT 0x22 AH=0x4C 退出程序 (DOS 兼容)
    xor eax, eax
    mov ah, 0x4C
    int 0x22

; ----------------------------------------------------------------------------
; 只读数据 (NASM 默认没有 .rodata, 显式声明以便与 GAS 行为一致)
; ----------------------------------------------------------------------------
section .rodata
; 以 '$' 结尾的字符串 (DOS 格式)
msg_hello: db "Hello from Nexsteaduser PlexsDOS via INT 21h!", 10, '$'
