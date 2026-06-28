; Nexsteaduser — PlexsDOS
; 引导扇区 (MBR Boot Sector)
; 作者: Tinmc189623 | 团队: Nexlyh

; BIOS 引导阶段 (16-bit 实模式) → 切换到 32-bit 保护模式 → 跳转到内核。
; 注意: 前半段为 16-bit 实模式 (BIOS 引导阶段), 后半段为 32-bit 保护模式桩。
; 语法: NASM Intel。

; 构建工具约定:
; - KSEC 标记后 2 字节为 sectors_left, 修补为实际内核扇区数 (mkfloppy/mkbootdisk)
; - C7 06 <addr> <imm> 模式中的立即数也会被 mkfloppy 修补

; Copyright © 2026 Nexsteaduser. All Rights Reserved

bits 16
section .text

global _start

; ===== 引导扇区入口 =====
_start:
    jmp boot_init           ; 跳过 BPB, 进入引导代码
    nop

; ===== BIOS 参数块 (BPB) - FAT12 格式 =====
oem_name:        db "PLXSDOS "
bytes_per_sec:   dw  512
sec_per_clust:   db  1
reserved_secs:   dw  1
num_fats:        db  2
root_entries:    dw  224
total_secs:      dw  2880
media_desc:      db  0xF0
fat_size:        dw  9
sec_per_track:   dw  18
num_heads:       dw  2
hidden_secs:     dd  0
total_secs_l:    dd  0
drive_num:       db  0
reserved_bpb:    db  0
boot_sig:        db  0x29
vol_serial:      dd  0x12345678
vol_label:       db "PLXSDOS    "
fs_type:         db "FAT12   "

; ===== 引导初始化 =====
boot_init:
    cli
    xor ax, ax
    mov ds, ax
    mov es, ax
    mov ss, ax
    mov sp, 0x7C00
    sti

    mov byte [boot_drive], dl

    mov si, msg_loading
    call print_string

; ===== 加载内核到 0x30000 (整磁道读取) =====
load_kernel:
    mov ax, 0x3000       ; ES = 0x3000, 物理地址 = ES*16 = 0x30000
    mov es, ax
    mov bx, 0x0000       ; BX 从 0 开始, 每 64KB 通过 ES 推进避免溢出
    mov word [sectors_left], KERNEL_SECTORS   ; mkfloppy 修补 KSEC 后的立即数
    mov byte [cur_cyl], 0x00
    mov byte [cur_head], 0x00
    mov byte [cur_sec], 0x02

; 整磁道读取: 每次 INT 13h 读一磁道剩余扇区 (≤ 18)。
; perf: 相比逐扇区读取, INT 13h 调用次数从 N 降到 N/17 左右,
; 每次读节省 ~17ms (磁头切换 + 寻道开销)。

; 每磁道扇区数限制 = min(sectors_left, 19 - cur_sec):
; - 第一磁道 cur_sec=2, 故首读最多 17 扇区
; - 后续磁道 cur_sec=1, 故读最多 18 扇区

load_track:
    ; 若 sectors_left == 0, 加载完成
    mov ax, [sectors_left]
    cmp ax, 0
    je load_done

    ; 计算本磁道最大可读扇区数 = 19 - cur_sec
    mov dl, 19
    sub dl, [cur_sec]       ; DL = 19 - cur_sec
    xor dh, dh              ; DX = max_sectors (16-bit zero-extended)

    ; 实际读 = min(剩余, max)
    cmp ax, dx
    jbe .lt_count_ok         ; AX <= DX: 用 AX
    mov ax, dx              ; 否则: 用 DX
.lt_count_ok:
    mov [track_count], ax   ; track_count = 本次读扇区数

    ; ---- 64KB 跨段保护 (QEMU SeaBIOS DMA 边界检查) ----
    ; 计算本段剩余可读扇区数 = (0x10000 - BX) / 512
    ; 用 32-bit EAX/EBX 避免 16-bit 装不下 0x10000 的问题
    ; 16-bit 模式下 NASM 接受 eax/ebx, 自动加 0x66 前缀
    mov eax, 0x10000
    sub eax, ebx
    shr eax, 9              ; EAX = 本段剩余扇区数 (0-128)
    cmp [track_count], ax
    jbe .lt_no_seg_wrap
    mov [track_count], ax
.lt_no_seg_wrap:

    ; INT 13h AH=02: 读取 track_count 个扇区
    mov ah, 0x02
    mov al, [track_count]   ; AL = 扇区数
    mov ch, [cur_cyl]
    mov cl, [cur_sec]
    mov dh, [cur_head]
    mov dl, [boot_drive]
    int 0x13
    jc disk_error

    ; 更新缓冲区指针 (track_count × 512 字节)
    mov ax, [track_count]
    shl ax, 9               ; AX = 扇区数 × 512
    add bx, ax
    jnc .lt_no_wrap

    ; BX 溢出: ES += 0x1000 (前进 64KB), BX 已自动回绕
    mov ax, es
    add ax, 0x1000
    mov es, ax
.lt_no_wrap:
    mov ax, [track_count]
    sub [sectors_left], ax

    ; 推进 CHS: 重置 cur_sec=1, 推进 head, 越界则推进 cyl
    mov byte [cur_sec], 1
    inc byte [cur_head]
    cmp byte [cur_head], 2
    jb load_track

    ; 磁头越界: 重置磁头, 推进柱面
    mov byte [cur_head], 0
    inc byte [cur_cyl]
    jmp load_track

load_done:
    mov si, msg_ok
    call print_string

; ===== 切换到 32-bit 保护模式 =====
switch_to_pm:
    cli                     ; 关中断

    ; 开启 A20 (快速方法, 端口 0x92) — VMware/实机需要
    in al, 0x92
    or al, 2
    out 0x92, al

    ; 手动编码保护模式切换指令:
    ; 16-bit 模式下无法直接生成 32-bit 操作数大小的指令,
    ; 必须手动添加 0x66 操作数大小前缀以确保 32-bit 操作数。

    ; lgdt: 加载 GDT (32-bit 基址)
    db 0x66, 0x0F, 0x01, 0x16     ; 0x66 lgdt m16:32
    dw gdt_descriptor              ; GDT 描述符偏移

    ; 读取 CR0
    db 0x0F, 0x20, 0xC0           ; mov eax, cr0

    ; 设置 PE 位 (CR0 bit 0)
    db 0x66, 0x83, 0xC8, 0x01     ; or eax, 0x01 (32-bit)

    ; 写回 CR0, 启用保护模式
    db 0x0F, 0x22, 0xC0           ; mov cr0, eax

    ; 远跳转: 进入 32-bit 保护模式, 跳转到内核
    db 0x66, 0xEA                  ; jmp far with 32-bit offset
    dd pm_stub                     ; EIP = pm_stub (32-bit 保护模式桩)
    dw 0x0008                      ; CS = 0x08 (代码段选择子)

; ===== 32-bit 保护模式桩 (内嵌在引导扇区中) =====
bits 32
pm_stub:
    ; 设置数据段选择子
    mov ax, 0x10
    mov ds, ax
    mov es, ax
    mov ss, ax
    mov esp, 0x90000

    ; 跳转到内核
    jmp 0x0008:0x30000

bits 16

; ===== 磁盘错误 =====
disk_error:
    mov si, msg_disk_err
    call print_string
    mov al, ah
    call print_hex_byte
    mov si, msg_newline
    call print_string
    cli
    hlt

; ===== 辅助函数 (实模式) =====
print_string:
    pusha
    mov ah, 0x0E
    mov bx, 0x0007
.ps_loop:
    lodsb
    cmp al, 0
    je .ps_done
    int 0x10
    jmp .ps_loop
.ps_done:
    popa
    ret

; serial_putc — 将 AL 中的字符发送到 COM1 (0x3F8)
serial_putc:
    push edx
    push eax
    mov dx, 0x3F8
    add dx, 5           ; 0x3FD: 线状态寄存器
.sc_wait:
    in al, dx
    test al, 0x20       ; 检查 THRE (发送保持寄存器空)
    jz .sc_wait
    pop eax
    sub dx, 5           ; 0x3F8: 数据寄存器
    out dx, al
    pop edx
    ret

print_hex_byte:
    pusha
    mov cl, al
    shr al, 4
    and al, 0x0F
    call .phex_nibble
    mov al, cl
    and al, 0x0F
    call .phex_nibble
    popa
    ret

.phex_nibble:
    cmp al, 10
    jl .phex_digit
    add al, 0x37
    jmp .phex_out
.phex_digit:
    add al, 0x30
.phex_out:
    mov ah, 0x0E
    int 0x10
    ret

; ===== GDT (全局描述符表) =====
gdt_start:
    ; 空描述符 (必须)
    dq 0x0000000000000000

    ; 代码段: base=0, limit=4GB, 32-bit, 可执行/可读
gdt_code:
    dw 0xFFFF            ; Limit 0-15
    dw 0x0000            ; Base 0-15
    db 0x00              ; Base 16-23
    db 0x9A              ; Access: Present, Ring 0, Code, Readable
    db 0xCF              ; Flags+Limit 16-19: 4KB granularity, 32-bit
    db 0x00              ; Base 24-31

    ; 数据段: base=0, limit=4GB, 32-bit, 可读/可写
gdt_data:
    dw 0xFFFF
    dw 0x0000
    db 0x00
    db 0x92              ; Access: Present, Ring 0, Data, Writable
    db 0xCF
    db 0x00

gdt_end:

gdt_descriptor:
    dw gdt_end - gdt_start - 1   ; GDT 大小 - 1
    dd gdt_start                  ; GDT 线性地址

; ===== 数据 =====
KERNEL_SECTORS equ 47

boot_drive:    db 0
cur_cyl:       db 0
cur_head:      db 0
cur_sec:       db 2
track_count:   dw 0

; 标记: 构建工具通过搜索 "KSEC" 找到此位置并修补 sectors_left 值
ksec_marker:   db "KSEC"
sectors_left:  dw KERNEL_SECTORS
msg_loading:   db "PlexsDOS loading...", 13, 10, 0
msg_ok:        db "OK", 13, 10, 0
msg_disk_err:  db "Disk err: 0x", 0
msg_newline:   db 13, 10, 0

; ===== 引导签名 =====
; 使用 $ 与 $$ 计算偏移 — $ 是当前位置, $$ 是当前 section 起始
times 510 - ($ - $$) db 0
dw 0xAA55
