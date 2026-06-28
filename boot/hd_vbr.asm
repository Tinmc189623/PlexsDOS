; Nexsteaduser — PlexsDOS
; FAT32 卷引导记录 (VBR)
; 作者: Tinmc189623 | 团队: Nexlyh

; FAT32 引导扇区, 从 FAT32 根目录加载 KERNEL.BIN:
; 1. 读取 FAT 表到临时缓冲区
; 2. 扫描根目录簇链查找 "KERNEL  BIN"
; 3. 沿 FAT 链逐簇加载到 0x30000
; 4. 切换到 32-bit 保护模式
; 5. 跳转到内核

; 必须在 512 字节内完成 (含 BPB)。
; 语法: NASM Intel, 16-bit 实模式 (32-bit 保护模式切换段)。

; Copyright © 2026 Nexsteaduser. All Rights Reserved

bits 16
section .text
global _start

; ==================== FAT32 BPB (偏移 0x00-0x5A) ====================
_start:
    jmp boot_code
    nop

; 标准 BPB (偏移 0x03-0x3F)
bpb_oem:           db "PLXSDOS "   ; 8 字节 OEM 标识
bpb_bytes_per_sec: dw 512
bpb_sec_per_clust: db 8
bpb_reserved_secs: dw 32
bpb_num_fats:      db 2
bpb_root_entries:  dw 0             ; FAT32 为 0
bpb_total_secs16:  dw 0             ; FAT32 使用 32-bit 字段
bpb_media_type:    db 0xF8          ; 固定磁盘
bpb_fat_size16:    dw 0             ; FAT32 使用 32-bit 字段
bpb_sec_per_track: dw 63
bpb_num_heads:     dw 16
bpb_hidden_secs:   dd 2048          ; 分区起始 LBA
bpb_total_secs32:  dd 0             ; 由安装程序填充

; FAT32 扩展 BPB (偏移 0x40-0x52)
bpb_fat_size32:    dd 0             ; 由安装程序填充
bpb_ext_flags:     dw 0
bpb_fs_version:    dw 0
bpb_root_cluster:  dd 2             ; 根目录起始簇
bpb_fs_info_sec:   dw 1             ; FSINFO 扇区号
bpb_backup_boot:   dw 6             ; 备份引导扇区
bpb_reserved:      times 12 db 0
bpb_drive_num:     db 0x80          ; 硬盘
bpb_reserved2:     db 0
bpb_boot_sig:      db 0x29          ; 扩展引导签名
bpb_serial:        dd 0x12345678    ; 卷序列号
bpb_label:         db "PLXSDOS    " ; 11 字节卷标
bpb_fs_type:       db "FAT32   "    ; 8 字节文件系统类型

; ==================== 引导代码 (偏移 0x5A) ====================
boot_code:
    cli
    xor ax, ax
    mov ds, ax
    mov es, ax
    mov ss, ax
    mov sp, 0x7C00
    sti

    ; SI 指向数据区基址 (16-bit 寻址, 避免 32-bit 前缀)
    mov si, data_area

    ; 保存启动驱动器号
    mov [si], dl

    ; 计算数据区起始 LBA = reserved + num_fats * fat_size32
    mov ax, [bpb_reserved_secs]
    mov [si + 4], ax              ; fat_lba (16-bit)
    movzx ecx, byte [bpb_num_fats]
    mov eax, [bpb_fat_size32]
    imul eax, ecx
    add eax, [si + 4]
    mov [si + 6], eax             ; data_lba (32-bit)

    ; 读取 FAT 表前 32 扇区到 0x2000
    mov ax, [si + 4]              ; fat_lba → EAX
    movzx eax, ax
    mov bx, 0x2000
    mov cl, 32
    call read_sectors
    jc die

    ; 读取根目录第一个簇到 0x4000
    ; 根目录 LBA = data_lba + (root_cluster - 2) * sec_per_clust
    mov eax, [bpb_root_cluster]
    sub eax, 2
    movzx ecx, byte [bpb_sec_per_clust]
    imul eax, ecx
    add eax, [si + 6]             ; + data_lba
    mov bx, 0x4000
    mov cl, [bpb_sec_per_clust]
    call read_sectors
    jc die

    ; 扫描根目录查找 "KERNEL  BIN"
    mov di, 0x4000

find_loop:
    cmp byte [di], 0x00           ; 空条目 = 目录结束
    je die
    cmp byte [di], 0xE5           ; 已删除
    je find_next
    cmp byte [di + 11], 0x0F      ; LFN
    je find_next

    ; 比较前 8 字节 (主文件名)
    mov si, kernel_name
    push di
    mov cx, 8
    repe cmpsb
    pop di
    jne find_next

    ; 匹配! 读取簇号和文件大小
    mov ax, [di + 20]             ; cluster_hi
    shl eax, 16
    mov ax, [di + 26]             ; cluster_lo
    mov [cur_cluster], eax

    mov eax, [di + 28]            ; file_size
    mov [file_size], eax

    jmp load_setup

find_next:
    add di, 32
    mov ax, di
    sub ax, 0x4000
    movzx cx, byte [bpb_sec_per_clust]
    shl cx, 9                     ; * 512
    cmp ax, cx
    ja find_loop
    ; 第一个簇搜索完毕, 未找到 → halt

die:
    cli
    hlt
    jmp die

; ==================== 内核加载 ====================
load_setup:
    mov ax, 0x3000
    mov es, ax                    ; ES = 0x3000 → 物理地址 0x30000
    xor di, di                    ; DI = 0x0000
    mov si, data_area             ; 恢复 SI 指向数据区

load_loop:
    ; 检查是否加载完成
    mov eax, [bytes_loaded]
    cmp eax, [file_size]
    jae load_done

    ; 计算当前簇的 LBA
    mov eax, [cur_cluster]
    sub eax, 2
    movzx ecx, byte [bpb_sec_per_clust]
    imul eax, ecx
    add eax, [si + 6]             ; + data_lba

    ; 读取簇到目标地址
    mov cl, [bpb_sec_per_clust]
    mov bx, di
    call read_sectors
    jc die

    ; 更新字节数和目标地址
    movzx eax, byte [bpb_sec_per_clust]
    shl eax, 9                    ; * 512
    add [bytes_loaded], eax
    add di, ax                    ; DI 按簇前进 (每次 0x1000)
    jnc .ll_no_wrap               ; 无进位 = 仍在当前 64KB 段内
    ; DI 溢出: ES += 0x1000 (前进 64KB), DI 已自动回绕
    mov ax, es
    add ax, 0x1000
    mov es, ax
.ll_no_wrap:

    ; FAT 链: 下一簇 = FAT[cluster] & 0x0FFFFFFF
    mov eax, [cur_cluster]
    shl eax, 2
    add eax, 0x2000
    mov eax, [eax]
    and eax, 0x0FFFFFFF
    mov [cur_cluster], eax
    cmp eax, 0x0FFFFFF8
    jb load_loop

load_done:
    ; 切换到 32-bit 保护模式
    cli

    ; 开启 A20 (快速方法, 端口 0x92)
    in al, 0x92
    or al, 2
    out 0x92, al

    ; 加载 GDT
    lgdt [gdt_desc]

    ; 设置 CR0.PE
    mov eax, cr0
    or eax, 1
    mov cr0, eax

    ; 在 16-bit 保护模式下预设段选择子 (节省 32-bit 代码空间)
    mov ax, 0x10
    mov ds, ax
    mov es, ax
    mov ss, ax

    ; 远跳到 32-bit 代码段
    jmp 0x0008:pm_entry

; ==================== 辅助函数 ====================

; read_sectors — INT 13h 扩展读取扇区
; 输入: EAX = LBA, CL = 扇区数, ES:BX = 缓冲区
; 输出: CF = 成功/失败

read_sectors:
    add eax, [bpb_hidden_secs]   ; 分区相对 LBA → 绝对 LBA
    pusha
    push word 0x0010          ; DAP 大小
    push word cx              ; 扇区数
    push word es              ; 段
    push word bx              ; 偏移
    push dword 0              ; LBA 高 32
    push dword eax            ; LBA 低 32
    mov ah, 0x42
    mov dl, [data_area]       ; boot_drive
    mov si, sp
    int 0x13
    add sp, 16
    popa
    ret

; ==================== 数据区 (16-bit 可寻址) ====================
align 2
data_area:
boot_drive:  db 0               ; 偏移 0
fat_lba:     dw 0               ; 偏移 1 (16-bit)
             dw 0               ; padding
data_lba:    dd 0               ; 偏移 4
cur_cluster: dd 0               ; 偏移 8
file_size:   dd 0               ; 偏移 12
bytes_loaded:dd 0               ; 偏移 16

kernel_name: db "KERNEL  "      ; 8 字节, 只比较主文件名

; ==================== GDT ====================
gdt_null:
    dq 0
gdt_code:
    dw 0xFFFF, 0x0000
    db 0x00, 0x9A, 0xCF, 0x00
gdt_data:
    dw 0xFFFF, 0x0000
    db 0x00, 0x92, 0xCF, 0x00
gdt_end:

gdt_desc:
    dw gdt_end - gdt_null - 1
    dd gdt_null

; ==================== 32-bit 保护模式入口 ====================
bits 32
pm_entry:
    ; ESP 由 kernel_entry.asm 设置, 此处无需重复设置
    jmp 0x0008:0x30000

; 填充到 510 字节, 之后是引导签名
; 注: NASM 编译时如果代码超过 510 字节, fix_vbr.py 会截断到 510 + 写 0x55AA
times 510 db 0
dw 0xAA55
