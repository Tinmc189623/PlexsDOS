; Nexsteaduser — PlexsDOS
; 硬盘主引导记录 (MBR)
; 作者: Tinmc189623 | 团队: Nexlyh

; 标准 MBR 引导代码 (自重定位):
; 1. 将自身从 0x7C00 复制到 0x0600
; 2. 在 0x0600 继续执行
; 3. 扫描分区表找到活动分区
; 4. 读取分区起始 LBA, 使用 INT 13h 扩展读取 (AH=42h) 加载 VBR 到 0x7C00
; 5. 跳转到 VBR

; 使用 INT 13h 扩展读取 (LBA 模式), 无需 CHS 转换。
; 语法: NASM Intel, 16-bit 实模式。

; Copyright © 2026 Nexsteaduser. All Rights Reserved

bits 16
section .text
global _start

; 硬盘几何参数
HEADS equ 16
SPT   equ 63

_start:
    ; 设置段寄存器
    cli
    xor ax, ax
    mov ds, ax
    mov es, ax
    mov ss, ax
    mov sp, 0x7C00
    sti

    ; 保存启动驱动器号 — 此时代码仍在 0x7C00, 但目标地址在 0x600
    ; NASM: 编译时计算 boot_drive - _start 的段内偏移, 加上 0x600 得到绝对地址
    ; DS 已被清零, 所以 [abs_addr] = [ds:abs_addr] 等价于 [cs:abs_addr]
    mov byte [0x600 + (boot_drive - _start)], dl

    ; 将自身从 0x7C00 复制到 0x0600
    cld
    mov si, 0x7C00
    mov di, 0x0600
    mov cx, 256          ; 512 字节 = 256 字
    rep movsw

    ; 跳转到 0x0600 处的代码
    jmp 0x0000:relocated

relocated:
    ; 现在运行在 0x0600, 0x7C00 可用
    mov dl, [cs:boot_drive]

    ; 扫描分区表 (0x07BE = 0x0600 + 0x1BE) 查找活动分区
    mov si, 0x07BE
    mov cx, 4

scan_loop:
    cmp byte [si], 0x80
    jz found_active
    add si, 16
    dec cx
    jnz scan_loop

    ; 没有活动分区
    mov si, err_no_part
    call print_string
    jmp halt

found_active:
    ; 读取分区起始 LBA (字节 8-11)
    mov eax, [si + 0x08]       ; EAX = LBA
    mov [cs:part_lba], eax

    ; 重置磁盘控制器
    mov dl, [cs:boot_drive]
    xor ah, ah
    int 0x13

    ; 使用 INT 13h 扩展读取 (AH=42h) 加载 VBR
    ; 构建 DAP (Disk Address Packet) 在栈上
    ; DAP: size(1) + reserved(1) + count(2) + addr(4) + LBA(8) = 16 字节
    xor ax, ax
    mov es, ax               ; ES = 0x0000
    mov bx, 0x7C00           ; BX = 0x7C00 (目标地址)

    push dword 0                  ; LBA 高 32 位 = 0
    mov eax, [cs:part_lba]
    push dword eax                ; LBA 低 32 位
    push word es                  ; 缓冲区段
    push word bx                  ; 缓冲区偏移
    push word 1                   ; 扇区数 = 1
    push word 0x0010              ; DAP 大小 = 16 字节

    mov ah, 0x42
    mov dl, [cs:boot_drive]
    mov si, sp               ; SI = DAP 指针
    int 0x13

    add sp, 16               ; 清理栈上 DAP
    jc read_error

    ; 跳转到 VBR
    mov dl, [cs:boot_drive]
    jmp 0x0000:0x7C00

read_error:
    mov si, err_read
    call print_string

halt:
    cli
    hlt
    jmp halt

print_string:
    pusha
    mov ah, 0x0E
.loop:
    lodsb
    test al, al
    jz .done
    int 0x10
    jmp .loop
.done:
    popa
    ret

boot_drive: db 0
part_lba:   dd 0
err_no_part: db "No active partition", 13, 10, 0
err_read:    db "Disk read error", 13, 10, 0

; 填充到 446 字节 (代码 + 数据部分占前 446 字节)
times 446 - ($ - $$) db 0

; 分区表 (4 × 16 字节) — 由安装程序填充
times 64 db 0

; 引导签名
dw 0xAA55
