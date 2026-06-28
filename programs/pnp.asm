; Nexsteaduser — PlexsDOS
; PNP.COMX — 即插即用管理器
; 作者: Tinmc189623 | 团队: Nexlyh

; 枚举 PCI/IDE/ISA 硬件, 收集磁盘设备实例 ID。
; 运行在 Ring 3, 通过 INT 0x22 系统调用访问内核设备表。

; 系统调用 (INT 0x22):
; AH=0x30: SYS_PNP_PCI_COUNT → 返回 PCI 设备数量
; AH=0x31: SYS_PNP_PCI_GET   → EDX=索引, ESI=缓冲区, 填充 pci_device
; AH=0x32: SYS_PNP_ISA_COUNT → 返回 ISA 设备数量
; AH=0x33: SYS_PNP_ISA_GET   → EDX=索引, ESI=缓冲区, 填充 isa_device
; AH=0x09: 写字符串 (DS:EDX='$'结尾)
; AH=0x02: 写字符 (DL=字符)
; AH=0x4C: 退出

; pci_device packed (19 字节):
; bus:0, slot:1, func:2, vendor_id:3, device_id:5,
; class_code:7, subclass:8, prog_if:9,
; bar0:10, bar4:14, irq:18

; isa_device packed (24 字节):
; io_base:0, irq:2, dma_chan:3, name:4 (20 字节)

; 语法: NASM Intel, 32-bit 保护模式。

; Copyright © 2026 Nexsteaduser. All Rights Reserved

; ===== 常量 =====
SYS_WRITE_CHAR    equ 0x02
SYS_WRITE_STR     equ 0x09
SYS_EXIT          equ 0x4C
SYS_PNP_PCI_COUNT equ 0x30
SYS_PNP_PCI_GET   equ 0x31
SYS_PNP_ISA_COUNT equ 0x32
SYS_PNP_ISA_GET   equ 0x33

PCI_DEVICE_SIZE equ 19
ISA_DEVICE_SIZE equ 24
MAX_DEVICES     equ 64

; pci_device 字段偏移
PCI_BUS       equ 0
PCI_SLOT      equ 1
PCI_FUNC      equ 2
PCI_VENDOR    equ 3
PCI_DEVICE    equ 5
PCI_CLASS     equ 7
PCI_SUBCLASS  equ 8
PCI_PROGIF    equ 9
PCI_BAR0      equ 10
PCI_IRQ       equ 18

; isa_device 字段偏移
ISA_IOBASE equ 0
ISA_IRQ    equ 2
ISA_NAME   equ 4

bits 32
section .text
global _start

; ===== 入口 =====

_start:
    ; 保存调用者帧
    push ebp
    mov ebp, esp

    ; 清屏 (通过输出大量换行)
    mov ecx, 50
.clear_loop:
    mov dl, 10                  ; '\n'
    mov ah, SYS_WRITE_CHAR
    int 0x22
    loop .clear_loop

    ; ===== 标题 =====
    lea edx, [str_title]
    mov ah, SYS_WRITE_STR
    int 0x22

    ; ===== 第一部分: PCI 设备枚举 =====
    lea edx, [str_pci_header]
    mov ah, SYS_WRITE_STR
    int 0x22

    ; 获取 PCI 设备数量
    mov ah, SYS_PNP_PCI_COUNT
    int 0x22
    mov [pci_count], eax

    ; 打印设备数量
    mov eax, [pci_count]
    push eax
    call print_dec
    add esp, 4
    lea edx, [str_devices_found]
    mov ah, SYS_WRITE_STR
    int 0x22

    ; 画分隔线
    call print_separator

    ; 列标题
    lea edx, [str_pci_cols]
    mov ah, SYS_WRITE_STR
    int 0x22

    call print_separator

    ; 遍历所有 PCI 设备
    xor edi, edi                ; edi = 设备索引
    xor ebx, ebx                ; ebx = 磁盘计数器
pci_loop:
    cmp edi, [pci_count]
    je pci_done

    ; 调用 SYS_PNP_PCI_GET
    mov edx, edi                ; EDX = 索引
    lea esi, [pci_buf]          ; ESI = 缓冲区
    mov ah, SYS_PNP_PCI_GET
    int 0x22
    cmp eax, 0xFFFFFFFF
    je pci_next

    ; 打印 B:D.F
    movzx eax, byte [pci_buf + PCI_BUS]
    push eax
    call print_dec
    add esp, 4
    mov dl, ':'
    mov ah, SYS_WRITE_CHAR
    int 0x22

    movzx eax, byte [pci_buf + PCI_SLOT]
    push eax
    call print_dec
    add esp, 4
    mov dl, '.'
    mov ah, SYS_WRITE_CHAR
    int 0x22

    movzx eax, byte [pci_buf + PCI_FUNC]
    push eax
    call print_dec
    add esp, 4

    ; 打印 Vendor:Device
    call print_spaces_pci

    movzx eax, word [pci_buf + PCI_VENDOR]
    push eax
    call print_hex16
    add esp, 4

    mov dl, ':'
    mov ah, SYS_WRITE_CHAR
    int 0x22

    movzx eax, word [pci_buf + PCI_DEVICE]
    push eax
    call print_hex16
    add esp, 4

    ; 打印 Class:Subclass:ProgIF
    call print_spaces_pci

    movzx eax, byte [pci_buf + PCI_CLASS]
    push eax
    call print_hex8
    add esp, 4

    mov dl, ':'
    mov ah, SYS_WRITE_CHAR
    int 0x22

    movzx eax, byte [pci_buf + PCI_SUBCLASS]
    push eax
    call print_hex8
    add esp, 4

    mov dl, ':'
    mov ah, SYS_WRITE_CHAR
    int 0x22

    movzx eax, byte [pci_buf + PCI_PROGIF]
    push eax
    call print_hex8
    add esp, 4

    ; 打印 IRQ
    call print_spaces_pci

    movzx eax, byte [pci_buf + PCI_IRQ]
    push eax
    call print_dec
    add esp, 4

    ; 打印设备类型描述
    call print_spaces_pci
    call print_pci_class_name

    ; 如果是磁盘控制器 (class 0x01), 增加计数
    movzx eax, byte [pci_buf + PCI_CLASS]
    cmp eax, 0x01
    jne pci_print_done

    ; 收集磁盘实例 ID
    mov ecx, ebx
    shl ecx, 1                  ; 每个条目 2 字节 (存索引)
    mov word [disk_indices + ecx], di
    inc ebx

pci_print_done:
    mov dl, 10                  ; '\n'
    mov ah, SYS_WRITE_CHAR
    int 0x22

pci_next:
    inc edi
    jmp pci_loop

pci_done:
    call print_separator
    mov [disk_count], ebx

    ; 打印磁盘设备数
    lea edx, [str_disk_count]
    mov ah, SYS_WRITE_STR
    int 0x22
    mov eax, [disk_count]
    push eax
    call print_dec
    add esp, 4
    lea edx, [str_newline]
    mov ah, SYS_WRITE_STR
    int 0x22

    ; ===== 如果有磁盘设备, 详细列出 =====
    cmp dword [disk_count], 0
    je no_disks

    lea edx, [str_disk_header]
    mov ah, SYS_WRITE_STR
    int 0x22

    call print_separator

    xor edi, edi
disk_loop:
    cmp edi, [disk_count]
    je disk_loop_done

    ; 获取磁盘设备的 PCI 索引
    mov ecx, edi
    shl ecx, 1
    movzx eax, word [disk_indices + ecx]
    mov edx, eax
    lea esi, [pci_buf]
    mov ah, SYS_PNP_PCI_GET
    int 0x22

    ; 打印 "  PCI "
    lea edx, [str_pci_prefix]
    mov ah, SYS_WRITE_STR
    int 0x22

    ; 打印 BUS:SLOT.FUNC
    movzx eax, byte [pci_buf + PCI_BUS]
    push eax
    call print_hex8
    add esp, 4
    mov dl, ':'
    mov ah, SYS_WRITE_CHAR
    int 0x22
    movzx eax, byte [pci_buf + PCI_SLOT]
    push eax
    call print_hex8
    add esp, 4
    mov dl, '.'
    mov ah, SYS_WRITE_CHAR
    int 0x22
    movzx eax, byte [pci_buf + PCI_FUNC]
    push eax
    call print_hex8
    add esp, 4

    ; 分隔
    lea edx, [str_dash]
    mov ah, SYS_WRITE_STR
    int 0x22

    ; VEN_XXXX&DEV_XXXX
    lea edx, [str_ven_prefix]
    mov ah, SYS_WRITE_STR
    int 0x22
    movzx eax, word [pci_buf + PCI_VENDOR]
    push eax
    call print_hex16
    add esp, 4

    lea edx, [str_dev_prefix]
    mov ah, SYS_WRITE_STR
    int 0x22
    movzx eax, word [pci_buf + PCI_DEVICE]
    push eax
    call print_hex16
    add esp, 4

    ; 打印子类名
    lea edx, [str_dash]
    mov ah, SYS_WRITE_STR
    int 0x22
    movzx eax, byte [pci_buf + PCI_SUBCLASS]
    push eax
    call print_subclass_name
    add esp, 4

    mov dl, 10                  ; '\n'
    mov ah, SYS_WRITE_CHAR
    int 0x22

    inc edi
    jmp disk_loop

disk_loop_done:
    call print_separator
    jmp isa_part

no_disks:
    lea edx, [str_no_disks]
    mov ah, SYS_WRITE_STR
    int 0x22

    ; ===== 第二部分: ISA 设备枚举 =====
isa_part:
    lea edx, [str_isa_header]
    mov ah, SYS_WRITE_STR
    int 0x22

    ; 获取 ISA 设备数量
    mov ah, SYS_PNP_ISA_COUNT
    int 0x22
    mov [isa_count], eax

    mov eax, [isa_count]
    push eax
    call print_dec
    add esp, 4
    lea edx, [str_devices_found]
    mov ah, SYS_WRITE_STR
    int 0x22

    call print_separator
    lea edx, [str_isa_cols]
    mov ah, SYS_WRITE_STR
    int 0x22
    call print_separator

    xor edi, edi
isa_loop:
    cmp edi, [isa_count]
    je isa_done

    mov edx, edi
    lea esi, [isa_buf]
    mov ah, SYS_PNP_ISA_GET
    int 0x22
    cmp eax, 0xFFFFFFFF
    je isa_next

    ; I/O Base
    lea edx, [str_io_prefix]
    mov ah, SYS_WRITE_STR
    int 0x22
    movzx eax, word [isa_buf + ISA_IOBASE]
    push eax
    call print_hex16
    add esp, 4

    ; IRQ
    call print_spaces_isa
    lea edx, [str_irq_prefix]
    mov ah, SYS_WRITE_STR
    int 0x22
    movzx eax, byte [isa_buf + ISA_IRQ]
    cmp eax, 0xFF
    je isa_no_irq
    push eax
    call print_dec
    add esp, 4
    jmp isa_irq_done
isa_no_irq:
    lea edx, [str_dash]
    mov ah, SYS_WRITE_STR
    int 0x22
isa_irq_done:

    ; 设备名
    call print_spaces_isa
    lea edx, [isa_buf + ISA_NAME]
    mov ah, SYS_WRITE_STR
    int 0x22

    mov dl, 10                  ; '\n'
    mov ah, SYS_WRITE_CHAR
    int 0x22

isa_next:
    inc edi
    jmp isa_loop

isa_done:
    call print_separator

    ; ===== 结束 =====
    lea edx, [str_footer]
    mov ah, SYS_WRITE_STR
    int 0x22

    ; 退出
    mov ah, SYS_EXIT
    int 0x22

; ===== 辅助函数 =====

; print_separator — 打印分隔线

print_separator:
    push eax
    push edx
    lea edx, [str_separator]
    mov ah, SYS_WRITE_STR
    int 0x22
    pop edx
    pop eax
    ret

; print_spaces_pci — 打印 PCI 字段间空格对齐

print_spaces_pci:
    push eax
    push edx
    lea edx, [str_spaces_pci]
    mov ah, SYS_WRITE_STR
    int 0x22
    pop edx
    pop eax
    ret

; print_spaces_isa — 打印 ISA 字段间空格对齐

print_spaces_isa:
    push eax
    push edx
    lea edx, [str_spaces_isa]
    mov ah, SYS_WRITE_STR
    int 0x22
    pop edx
    pop eax
    ret

; print_dec — 打印十进制数
; 栈(4): 要打印的值

print_dec:
    push ebp
    mov ebp, esp
    push eax
    push ecx
    push edx
    push edi

    mov eax, [ebp + 8]          ; 值
    mov ecx, 10
    lea edi, [dec_buf_end]
    mov byte [edi], 0           ; 字符串终止

.dec_loop:
    dec edi
    xor edx, edx
    div ecx                     ; EAX / 10 → EAX=商, EDX=余数
    add dl, '0'                 ; 转 ASCII
    mov byte [edi], dl
    test eax, eax
    jnz .dec_loop

    ; 打印
    mov edx, edi
    mov ah, SYS_WRITE_STR
    int 0x22

    pop edi
    pop edx
    pop ecx
    pop eax
    pop ebp
    ret

; print_hex16 — 打印 16-bit 十六进制数 (4 位)
; 栈(4): 要打印的值 (低 16 位)

print_hex16:
    push ebp
    mov ebp, esp
    push eax
    push ecx
    push edx
    push edi

    mov eax, [ebp + 8]
    lea edi, [hex_buf + 4]
    mov byte [edi], 0

    mov ecx, 4                  ; 4 个十六进制位
.hex16_loop:
    dec edi
    mov dl, al
    and dl, 0x0F
    mov dl, byte [hex_chars + edx]
    mov byte [edi], dl
    shr eax, 4
    loop .hex16_loop

    mov edx, edi
    mov ah, SYS_WRITE_STR
    int 0x22

    pop edi
    pop edx
    pop ecx
    pop eax
    pop ebp
    ret

; print_hex8 — 打印 8-bit 十六进制数 (2 位)
; 栈(4): 要打印的值 (低 8 位)

print_hex8:
    push ebp
    mov ebp, esp
    push eax
    push ecx
    push edx
    push edi

    mov eax, [ebp + 8]
    lea edi, [hex_buf + 2]
    mov byte [edi], 0

    mov ecx, 2
.hex8_loop:
    dec edi
    mov dl, al
    and dl, 0x0F
    mov dl, byte [hex_chars + edx]
    mov byte [edi], dl
    shr eax, 4
    loop .hex8_loop

    mov edx, edi
    mov ah, SYS_WRITE_STR
    int 0x22

    pop edi
    pop edx
    pop ecx
    pop eax
    pop ebp
    ret

; print_pci_class_name — 打印 PCI 类名
; 根据 pci_buf 中的 class_code 和 subclass 打印设备类型。

print_pci_class_name:
    push eax
    push edx

    movzx eax, byte [pci_buf + PCI_CLASS]
    cmp eax, 0x01
    je .pci_mass_storage
    cmp eax, 0x02
    je .pci_network
    cmp eax, 0x03
    je .pci_display
    cmp eax, 0x04
    je .pci_multimedia
    cmp eax, 0x06
    je .pci_bridge
    cmp eax, 0x0C
    je .pci_serial
    cmp eax, 0x08
    je .pci_system

    lea edx, [str_class_other]
    mov ah, SYS_WRITE_STR
    int 0x22
    jmp .pci_class_done

.pci_mass_storage:
    movzx eax, byte [pci_buf + PCI_SUBCLASS]
    push eax
    call print_subclass_name
    add esp, 4
    jmp .pci_class_done

.pci_network:
    lea edx, [str_class_network]
    mov ah, SYS_WRITE_STR
    int 0x22
    jmp .pci_class_done

.pci_display:
    lea edx, [str_class_display]
    mov ah, SYS_WRITE_STR
    int 0x22
    jmp .pci_class_done

.pci_multimedia:
    lea edx, [str_class_mm]
    mov ah, SYS_WRITE_STR
    int 0x22
    jmp .pci_class_done

.pci_bridge:
    lea edx, [str_class_bridge]
    mov ah, SYS_WRITE_STR
    int 0x22
    jmp .pci_class_done

.pci_serial:
    lea edx, [str_class_serial]
    mov ah, SYS_WRITE_STR
    int 0x22
    jmp .pci_class_done

.pci_system:
    lea edx, [str_class_system]
    mov ah, SYS_WRITE_STR
    int 0x22

.pci_class_done:
    pop edx
    pop eax
    ret

; print_subclass_name — 打印存储子类名
; 栈(4): subclass 值

print_subclass_name:
    push ebp
    mov ebp, esp
    push eax
    push edx

    mov eax, [ebp + 8]
    cmp eax, 0x01
    je .sub_ide
    cmp eax, 0x06
    je .sub_sata
    cmp eax, 0x08
    je .sub_nvme
    cmp eax, 0x00
    je .sub_scsi
    cmp eax, 0x04
    je .sub_raid

    lea edx, [str_sub_storage]
    mov ah, SYS_WRITE_STR
    int 0x22
    jmp .sub_done

.sub_ide:
    lea edx, [str_sub_ide]
    mov ah, SYS_WRITE_STR
    int 0x22
    jmp .sub_done

.sub_sata:
    lea edx, [str_sub_sata]
    mov ah, SYS_WRITE_STR
    int 0x22
    jmp .sub_done

.sub_nvme:
    lea edx, [str_sub_nvme]
    mov ah, SYS_WRITE_STR
    int 0x22
    jmp .sub_done

.sub_scsi:
    lea edx, [str_sub_scsi]
    mov ah, SYS_WRITE_STR
    int 0x22
    jmp .sub_done

.sub_raid:
    lea edx, [str_sub_raid]
    mov ah, SYS_WRITE_STR
    int 0x22

.sub_done:
    pop edx
    pop eax
    pop ebp
    ret

; ===== 只读数据 =====

section .rodata

str_title:
    db "Nexsteaduser PlexsDOS — PnP Manager v1.0", 10
    db "============================================", 10
    db "Enumerating PCI/IDE/ISA hardware...", 10
    db 10, '$'

str_pci_header:
    db 10, "--- PCI Devices ---", 10, '$'

str_pci_cols:
    db " B:D.F     Vendor:Device  Class:Scls:PIF  IRQ  Type", 10, '$'

str_separator:
    db "----------------------------------------------", 10, '$'

str_spaces_pci:
    db "  $"

str_spaces_isa:
    db "  $"

str_devices_found:
    db " device(s) found.", 10, '$'

str_newline:
    db 10, '$'

str_disk_count:
    db 10, "Mass storage controllers: $"

str_disk_header:
    db 10, "--- Disk Device Instance IDs ---", 10, '$'

str_no_disks:
    db "No mass storage controllers found.", 10, '$'

str_pci_prefix:
    db "  PCI $"

str_dash:
    db "-$"

str_ven_prefix:
    db "VEN_$"

str_dev_prefix:
    db "&DEV_$"

str_io_prefix:
    db " IO=0x$"

str_irq_prefix:
    db "IRQ=$"

str_isa_header:
    db 10, "--- ISA Legacy Devices ---", 10, '$'

str_isa_cols:
    db " I/O Base   IRQ  Device Name", 10, '$'

str_footer:
    db 10, "PnP enumeration complete.", 10, '$'

; 设备类型字符串 (PCI)
str_class_other:   db "Other$"
str_class_network: db "Network$"
str_class_display: db "Display$"
str_class_mm:      db "Multimedia$"
str_class_bridge:  db "Bridge$"
str_class_serial:  db "Serial Bus$"
str_class_system:  db "System$"

; 存储子类字符串
str_sub_storage: db "Mass Storage$"
str_sub_ide:     db "IDE Controller$"
str_sub_sata:    db "SATA/AHCI$"
str_sub_nvme:    db "NVMe SSD$"
str_sub_scsi:    db "SCSI$"
str_sub_raid:    db "RAID$"

; 十六进制字符表
hex_chars:
    db "0123456789ABCDEF"

; ===== BSS (可写数据) =====

section .bss
align 4

; PCI 设备信息缓冲区
pci_buf:    resb PCI_DEVICE_SIZE
; ISA 设备信息缓冲区
isa_buf:    resb ISA_DEVICE_SIZE
; PCI 设备数量
pci_count:  resd 1
; ISA 设备数量
isa_count:  resd 1
; 磁盘设备数量
disk_count: resd 1
; 磁盘设备索引表 (最大 16 个 × 2 字节)
disk_indices: resb 32

; 十进制转换缓冲区
dec_buf:    resb 12
; NASM 中 dec_buf_end 在 BSS 段后的常量定义 (这里用 equ 引用同一缓冲区内偏移)
; 注意: 在 NASM 中 resb 后使用 equ 表达式, 必须指向 resb 之后的标签, 不能跨段
; 这里 dec_buf 在 .bss 段, 12 字节, dec_buf_end = dec_buf + 11
dec_buf_end: equ dec_buf + 11

; 十六进制转换缓冲区
hex_buf:    resb 8
