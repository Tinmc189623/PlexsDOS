; Nexsteaduser — PlexsDOS
; 用户态 Shell (Ring 3)
; 作者: Tinmc189623 | 团队: Nexlyh
;
; 此程序运行在 Ring 3 用户态, 通过 INT 0x22 系统调用与内核交互。
; 宏内核架构: 用户程序在用户态运行, 通过系统调用请求内核服务。
; 编译为 flat binary, 加载地址 0x50000 (USER_LOAD_ADDR)。
; 语法: NASM Intel, 32-bit 保护模式。
; 所有代码和数据都在 .text 段, 方便 flat binary 提取。

; Copyright © 2026 Nexsteaduser. All Rights Reserved

bits 32
org 0x50000

; ====================== 系统调用常量 ======================
SYS_EXIT        equ 0x4C
SYS_READ_CHAR   equ 0x01
SYS_WRITE_CHAR  equ 0x02
SYS_WRITE_STR   equ 0x09
SYS_READ_STR    equ 0x0A
SYS_CLEAR       equ 0x50
SYS_SET_COLOR   equ 0x51
SYS_RESET_COLOR equ 0x52
SYS_SHELL_CMD   equ 0x5C

; ----------------------
; _start — 用户态 Shell 入口 (bin 格式, 直接从文件开头执行)
; ----------------------
_start:
    ; 设置 Ring 3 数据段寄存器 (DS=ES=FS=GS=0x40, Ring 3 数据段选择子)
    ; 注意: 内核在上下文切换时已经设置了正确的段选择子, 此处重新设置以确保安全
    mov ax, 0x40
    mov ds, ax
    mov es, ax
    mov fs, ax
    mov gs, ax

    ; 设置用户栈 (用户栈顶 0x81000, 向下增长)
    mov esp, 0x81000
    mov ebp, esp

    ; 清屏 (通过系统调用, 不能直接访问 VGA 显存)
    mov ah, SYS_CLEAR
    int 0x22

    ; 显示欢迎信息
    lea edx, [msg_welcome]
    mov ah, SYS_WRITE_STR
    int 0x22

    lea edx, [msg_banner]
    mov ah, SYS_WRITE_STR
    int 0x22

    lea edx, [msg_copyright]
    mov ah, SYS_WRITE_STR
    int 0x22

    lea edx, [msg_help]
    mov ah, SYS_WRITE_STR
    int 0x22

    lea edx, [msg_newline]
    mov ah, SYS_WRITE_STR
    int 0x22

; ----------------------
; 主命令循环
; ----------------------
.shell_loop:
    ; 设置提示符颜色 (绿色)
    mov dx, 0x000A         ; DH=背景(0), DL=前景(0x0A=绿色)
    mov ah, SYS_SET_COLOR
    int 0x22

    ; 显示提示符 "C:\> "
    lea edx, [prompt]
    mov ah, SYS_WRITE_STR
    int 0x22

    ; 重置颜色
    mov ah, SYS_RESET_COLOR
    int 0x22

    ; 读取命令行输入
    ; 缓冲区格式: buf[0]=max_len, buf[1]=actual_len, buf[2..]=string
    mov byte [cmd_buf], 126    ; 最大长度 126
    lea edx, [cmd_buf]
    mov ah, SYS_READ_STR
    int 0x22

    ; 获取实际长度
    movzx ecx, byte [cmd_buf + 1]
    cmp ecx, 0
    je .shell_loop             ; 空命令, 继续循环

    ; 在命令末尾添加 '$' 用于 SYS_SHELL_CMD (系统调用的字符串结束标记)
    lea edi, [cmd_buf + 2]
    add edi, ecx
    mov byte [edi], '$'        ; 字符串结束标记
    inc edi
    mov byte [edi], 0

    ; 检查是否是 EXIT/QUIT 命令 (退出用户态 Shell)
    lea esi, [cmd_buf + 2]
    call check_exit
    test eax, eax
    jnz .do_exit

    ; 通过系统调用让内核执行命令
    lea edx, [cmd_buf + 2]
    mov ah, SYS_SHELL_CMD
    int 0x22

    jmp .shell_loop

; ----------------------
; 退出程序
; ----------------------
.do_exit:
    lea edx, [msg_goodbye]
    mov ah, SYS_WRITE_STR
    int 0x22

    xor eax, eax
    mov ah, SYS_EXIT           ; AH=0x4C, AL=0 (返回码)
    int 0x22

; ----------------------
; check_exit — 检查是否是 EXIT/QUIT 命令
; 输入: ESI = 命令字符串
; 输出: EAX = 1 如果是退出命令, 0 否则
; ----------------------
check_exit:
    push esi
    push ebx

    ; 将命令转换为大写并比较
    mov eax, 0

    ; 检查 "EXIT"
    mov bl, [esi]
    call to_upper
    cmp bl, 'E'
    jne .check_quit
    mov bl, [esi + 1]
    call to_upper
    cmp bl, 'X'
    jne .check_quit
    mov bl, [esi + 2]
    call to_upper
    cmp bl, 'I'
    jne .check_quit
    mov bl, [esi + 3]
    call to_upper
    cmp bl, 'T'
    jne .check_quit
    ; 检查后面是否是结束符或空格
    mov bl, [esi + 4]
    cmp bl, 0
    je .is_exit
    cmp bl, ' '
    je .is_exit
    cmp bl, 13
    je .is_exit
    cmp bl, 10
    je .is_exit

.check_quit:
    ; 检查 "QUIT"
    mov bl, [esi]
    call to_upper
    cmp bl, 'Q'
    jne .not_exit
    mov bl, [esi + 1]
    call to_upper
    cmp bl, 'U'
    jne .not_exit
    mov bl, [esi + 2]
    call to_upper
    cmp bl, 'I'
    jne .not_exit
    mov bl, [esi + 3]
    call to_upper
    cmp bl, 'T'
    jne .not_exit
    mov bl, [esi + 4]
    cmp bl, 0
    je .is_exit
    cmp bl, ' '
    je .is_exit
    cmp bl, 13
    je .is_exit
    cmp bl, 10
    je .is_exit

.not_exit:
    mov eax, 0
    pop ebx
    pop esi
    ret

.is_exit:
    mov eax, 1
    pop ebx
    pop esi
    ret

; ----------------------
; to_upper — 将字符转换为大写
; 输入: BL = 字符
; 输出: BL = 大写字符
; ----------------------
to_upper:
    cmp bl, 'a'
    jb .done
    cmp bl, 'z'
    ja .done
    sub bl, 32
.done:
    ret

; ====================== 字符串数据 ======================
msg_welcome:
    db 'Nexsteaduser PlexsDOS v0.2', 10, '$'

msg_banner:
    db 'x86 32-bit Monolithic Kernel - User Mode Shell', 10, 10, '$'

msg_copyright:
    db 'Copyright (c) 2026 Nexsteaduser. All Rights Reserved.', 10, '$'

msg_help:
    db 'Type HELP for commands, EXIT/QUIT to return to kernel.', 10, '$'

msg_newline:
    db 10, '$'

prompt:
    db 'C:\> $'

msg_goodbye:
    db 10, 'User shell exited. Returning to kernel...', 10, '$'

; ====================== BSS 数据 (未初始化, 在二进制中用 0 填充) ======================
; 注意: flat binary 需要显式分配空间, 不能用 resb
; 命令输入缓冲区 (132 字节, 初始化为 0)
cmd_buf:
    times 132 db 0
