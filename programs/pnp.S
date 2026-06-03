/*
 * Nexsteaduser — PlexsDOS
 * PNP.COMX — 即插即用管理器
 * 作者: Tinmc189623 | 团队: Nexlyh
 *
 * 枚举 PCI/IDE/ISA 硬件, 收集磁盘设备实例 ID。
 * 运行在 Ring 3, 通过 INT 0x22 系统调用访问内核设备表。
 *
 * 系统调用 (INT 0x22):
 *   AH=0x30: SYS_PNP_PCI_COUNT → 返回 PCI 设备数量
 *   AH=0x31: SYS_PNP_PCI_GET   → EDX=索引, ESI=缓冲区, 填充 pci_device
 *   AH=0x32: SYS_PNP_ISA_COUNT → 返回 ISA 设备数量
 *   AH=0x33: SYS_PNP_ISA_GET   → EDX=索引, ESI=缓冲区, 填充 isa_device
 *   AH=0x09: 写字符串 (DS:EDX='$'结尾)
 *   AH=0x02: 写字符 (DL=字符)
 *   AH=0x4C: 退出
 *
 * pci_device packed (19 字节):
 *   bus:0, slot:1, func:2, vendor_id:3, device_id:5,
 *   class_code:7, subclass:8, prog_if:9,
 *   bar0:10, bar4:14, irq:18
 *
 * isa_device packed (24 字节):
 *   io_base:0, irq:2, dma_chan:3, name:4 (20 字节)
 */

.code32
.section .text
.global _start

/* ===== 常量 ===== */
.equ SYS_WRITE_CHAR,      0x02
.equ SYS_WRITE_STR,       0x09
.equ SYS_EXIT,            0x4C
.equ SYS_PNP_PCI_COUNT,   0x30
.equ SYS_PNP_PCI_GET,     0x31
.equ SYS_PNP_ISA_COUNT,   0x32
.equ SYS_PNP_ISA_GET,     0x33

.equ PCI_DEVICE_SIZE,     19
.equ ISA_DEVICE_SIZE,     24
.equ MAX_DEVICES,         64

/* pci_device 字段偏移 */
.equ PCI_BUS,       0
.equ PCI_SLOT,      1
.equ PCI_FUNC,      2
.equ PCI_VENDOR,    3
.equ PCI_DEVICE,    5
.equ PCI_CLASS,     7
.equ PCI_SUBCLASS,  8
.equ PCI_PROGIF,    9
.equ PCI_BAR0,     10
.equ PCI_IRQ,      18

/* isa_device 字段偏移 */
.equ ISA_IOBASE,    0
.equ ISA_IRQ,       2
.equ ISA_NAME,      4

/* ===== 入口 ===== */

_start:
    /* 保存调用者帧 */
    pushl %ebp
    movl  %esp, %ebp

    /* 清屏 (通过输出大量换行) */
    movl  $50, %ecx
1:  movb  $'\n', %dl
    movb  $SYS_WRITE_CHAR, %ah
    int   $0x22
    loop  1b

    /* ===== 标题 ===== */
    leal  str_title, %edx
    movb  $SYS_WRITE_STR, %ah
    int   $0x22

    /* ===== 第一部分: PCI 设备枚举 ===== */
    leal  str_pci_header, %edx
    movb  $SYS_WRITE_STR, %ah
    int   $0x22

    /* 获取 PCI 设备数量 */
    movb  $SYS_PNP_PCI_COUNT, %ah
    int   $0x22
    movl  %eax, pci_count

    /* 打印设备数量 */
    movl  pci_count, %eax
    pushl %eax
    call  print_dec
    addl  $4, %esp
    leal  str_devices_found, %edx
    movb  $SYS_WRITE_STR, %ah
    int   $0x22

    /* 画分隔线 */
    call  print_separator

    /* 列标题 */
    leal  str_pci_cols, %edx
    movb  $SYS_WRITE_STR, %ah
    int   $0x22

    call  print_separator

    /* 遍历所有 PCI 设备 */
    xorl  %edi, %edi                /* edi = 设备索引 */
    xorl  %ebx, %ebx                /* ebx = 磁盘计数器 */
pci_loop:
    cmpl  %edi, pci_count
    je    pci_done

    /* 调用 SYS_PNP_PCI_GET */
    movl  %edi, %edx               /* EDX = 索引 */
    leal  pci_buf, %esi            /* ESI = 缓冲区 */
    movb  $SYS_PNP_PCI_GET, %ah
    int   $0x22
    cmpl  $0xFFFFFFFF, %eax
    je    pci_next

    /* 打印 B:D.F */
    movzbl pci_buf + PCI_BUS, %eax
    pushl %eax
    call  print_dec
    addl  $4, %esp
    movb  $':', %dl
    movb  $SYS_WRITE_CHAR, %ah
    int   $0x22

    movzbl pci_buf + PCI_SLOT, %eax
    pushl %eax
    call  print_dec
    addl  $4, %esp
    movb  $'.', %dl
    movb  $SYS_WRITE_CHAR, %ah
    int   $0x22

    movzbl pci_buf + PCI_FUNC, %eax
    pushl %eax
    call  print_dec
    addl  $4, %esp

    /* 打印 Vendor:Device */
    call  print_spaces_pci

    movzwl pci_buf + PCI_VENDOR, %eax
    pushl %eax
    call  print_hex16
    addl  $4, %esp

    movb  $':', %dl
    movb  $SYS_WRITE_CHAR, %ah
    int   $0x22

    movzwl pci_buf + PCI_DEVICE, %eax
    pushl %eax
    call  print_hex16
    addl  $4, %esp

    /* 打印 Class:Subclass:ProgIF */
    call  print_spaces_pci

    movzbl pci_buf + PCI_CLASS, %eax
    pushl %eax
    call  print_hex8
    addl  $4, %esp

    movb  $':', %dl
    movb  $SYS_WRITE_CHAR, %ah
    int   $0x22

    movzbl pci_buf + PCI_SUBCLASS, %eax
    pushl %eax
    call  print_hex8
    addl  $4, %esp

    movb  $':', %dl
    movb  $SYS_WRITE_CHAR, %ah
    int   $0x22

    movzbl pci_buf + PCI_PROGIF, %eax
    pushl %eax
    call  print_hex8
    addl  $4, %esp

    /* 打印 IRQ */
    call  print_spaces_pci

    movzbl pci_buf + PCI_IRQ, %eax
    pushl %eax
    call  print_dec
    addl  $4, %esp

    /* 打印设备类型描述 */
    call  print_spaces_pci
    call  print_pci_class_name

    /* 如果是磁盘控制器 (class 0x01), 增加计数 */
    movzbl pci_buf + PCI_CLASS, %eax
    cmpl  $0x01, %eax
    jne   pci_print_done

    /* 收集磁盘实例 ID */
    movl  %ebx, %ecx
    shll  $1, %ecx                  /* 每个条目 2 字节 (存索引) */
    movw  %di, disk_indices(%ecx)
    incl  %ebx

pci_print_done:
    movb  $'\n', %dl
    movb  $SYS_WRITE_CHAR, %ah
    int   $0x22

pci_next:
    incl  %edi
    jmp   pci_loop

pci_done:
    call  print_separator
    movl  %ebx, disk_count

    /* 打印磁盘设备数 */
    leal  str_disk_count, %edx
    movb  $SYS_WRITE_STR, %ah
    int   $0x22
    movl  disk_count, %eax
    pushl %eax
    call  print_dec
    addl  $4, %esp
    leal  str_newline, %edx
    movb  $SYS_WRITE_STR, %ah
    int   $0x22

    /* ===== 如果有磁盘设备, 详细列出 ===== */
    cmpl  $0, disk_count
    je    no_disks

    leal  str_disk_header, %edx
    movb  $SYS_WRITE_STR, %ah
    int   $0x22

    call  print_separator

    xorl  %edi, %edi
disk_loop:
    cmpl  %edi, disk_count
    je    disk_loop_done

    /* 获取磁盘设备的 PCI 索引 */
    movl  %edi, %ecx
    shll  $1, %ecx
    movzbl disk_indices(%ecx), %eax
    movl  %eax, %edx
    leal  pci_buf, %esi
    movb  $SYS_PNP_PCI_GET, %ah
    int   $0x22

    /* 打印 "  PCI " */
    leal  str_pci_prefix, %edx
    movb  $SYS_WRITE_STR, %ah
    int   $0x22

    /* 打印 BUS:SLOT.FUNC */
    movzbl pci_buf + PCI_BUS, %eax
    pushl %eax
    call  print_hex8
    addl  $4, %esp
    movb  $':', %dl
    movb  $SYS_WRITE_CHAR, %ah
    int   $0x22
    movzbl pci_buf + PCI_SLOT, %eax
    pushl %eax
    call  print_hex8
    addl  $4, %esp
    movb  $'.', %dl
    movb  $SYS_WRITE_CHAR, %ah
    int   $0x22
    movzbl pci_buf + PCI_FUNC, %eax
    pushl %eax
    call  print_hex8
    addl  $4, %esp

    /* 分隔 */
    leal  str_dash, %edx
    movb  $SYS_WRITE_STR, %ah
    int   $0x22

    /* VEN_XXXX&DEV_XXXX */
    leal  str_ven_prefix, %edx
    movb  $SYS_WRITE_STR, %ah
    int   $0x22
    movzwl pci_buf + PCI_VENDOR, %eax
    pushl %eax
    call  print_hex16
    addl  $4, %esp

    leal  str_dev_prefix, %edx
    movb  $SYS_WRITE_STR, %ah
    int   $0x22
    movzwl pci_buf + PCI_DEVICE, %eax
    pushl %eax
    call  print_hex16
    addl  $4, %esp

    /* 打印子类名 */
    leal  str_dash, %edx
    movb  $SYS_WRITE_STR, %ah
    int   $0x22
    movzbl pci_buf + PCI_SUBCLASS, %eax
    pushl %eax
    call  print_subclass_name
    addl  $4, %esp

    movb  $'\n', %dl
    movb  $SYS_WRITE_CHAR, %ah
    int   $0x22

    incl  %edi
    jmp   disk_loop

disk_loop_done:
    call  print_separator
    jmp   isa_part

no_disks:
    leal  str_no_disks, %edx
    movb  $SYS_WRITE_STR, %ah
    int   $0x22

    /* ===== 第二部分: ISA 设备枚举 ===== */
isa_part:
    leal  str_isa_header, %edx
    movb  $SYS_WRITE_STR, %ah
    int   $0x22

    /* 获取 ISA 设备数量 */
    movb  $SYS_PNP_ISA_COUNT, %ah
    int   $0x22
    movl  %eax, isa_count

    movl  isa_count, %eax
    pushl %eax
    call  print_dec
    addl  $4, %esp
    leal  str_devices_found, %edx
    movb  $SYS_WRITE_STR, %ah
    int   $0x22

    call  print_separator
    leal  str_isa_cols, %edx
    movb  $SYS_WRITE_STR, %ah
    int   $0x22
    call  print_separator

    xorl  %edi, %edi
isa_loop:
    cmpl  %edi, isa_count
    je    isa_done

    movl  %edi, %edx
    leal  isa_buf, %esi
    movb  $SYS_PNP_ISA_GET, %ah
    int   $0x22
    cmpl  $0xFFFFFFFF, %eax
    je    isa_next

    /* I/O Base */
    leal  str_io_prefix, %edx
    movb  $SYS_WRITE_STR, %ah
    int   $0x22
    movzwl isa_buf + ISA_IOBASE, %eax
    pushl %eax
    call  print_hex16
    addl  $4, %esp

    /* IRQ */
    call  print_spaces_isa
    leal  str_irq_prefix, %edx
    movb  $SYS_WRITE_STR, %ah
    int   $0x22
    movzbl isa_buf + ISA_IRQ, %eax
    cmpl  $0xFF, %eax
    je    isa_no_irq
    pushl %eax
    call  print_dec
    addl  $4, %esp
    jmp   isa_irq_done
isa_no_irq:
    leal  str_dash, %edx
    movb  $SYS_WRITE_STR, %ah
    int   $0x22
isa_irq_done:

    /* 设备名 */
    call  print_spaces_isa
    leal  isa_buf + ISA_NAME, %edx
    movb  $SYS_WRITE_STR, %ah
    int   $0x22

    movb  $'\n', %dl
    movb  $SYS_WRITE_CHAR, %ah
    int   $0x22

isa_next:
    incl  %edi
    jmp   isa_loop

isa_done:
    call  print_separator

    /* ===== 结束 ===== */
    leal  str_footer, %edx
    movb  $SYS_WRITE_STR, %ah
    int   $0x22

    /* 退出 */
    movb  $SYS_EXIT, %ah
    int   $0x22

/* ===== 辅助函数 ===== */

/*
 * print_separator — 打印分隔线
 */
print_separator:
    pushl %eax
    pushl %edx
    leal  str_separator, %edx
    movb  $SYS_WRITE_STR, %ah
    int   $0x22
    popl  %edx
    popl  %eax
    ret

/*
 * print_spaces_pci — 打印 PCI 字段间空格对齐
 */
print_spaces_pci:
    pushl %eax
    pushl %edx
    leal  str_spaces_pci, %edx
    movb  $SYS_WRITE_STR, %ah
    int   $0x22
    popl  %edx
    popl  %eax
    ret

/*
 * print_spaces_isa — 打印 ISA 字段间空格对齐
 */
print_spaces_isa:
    pushl %eax
    pushl %edx
    leal  str_spaces_isa, %edx
    movb  $SYS_WRITE_STR, %ah
    int   $0x22
    popl  %edx
    popl  %eax
    ret

/*
 * print_dec — 打印十进制数
 * 栈(4): 要打印的值
 */
print_dec:
    pushl %ebp
    movl  %esp, %ebp
    pushl %eax
    pushl %ecx
    pushl %edx
    pushl %edi

    movl  8(%ebp), %eax          /* 值 */
    movl  $10, %ecx
    leal  dec_buf_end, %edi
    movb  $0, (%edi)             /* 字符串终止 */

dec_loop:
    decl  %edi
    xorl  %edx, %edx
    divl  %ecx                  /* EAX / 10 → EAX=商, EDX=余数 */
    addb  $'0', %dl             /* 转 ASCII */
    movb  %dl, (%edi)
    testl %eax, %eax
    jnz   dec_loop

    /* 打印 */
    movl  %edi, %edx
    movb  $SYS_WRITE_STR, %ah
    int   $0x22

    popl  %edi
    popl  %edx
    popl  %ecx
    popl  %eax
    popl  %ebp
    ret

/*
 * print_hex16 — 打印 16-bit 十六进制数 (4 位)
 * 栈(4): 要打印的值 (低 16 位)
 */
print_hex16:
    pushl %ebp
    movl  %esp, %ebp
    pushl %eax
    pushl %ecx
    pushl %edx
    pushl %edi

    movl  8(%ebp), %eax
    leal  hex_buf + 4, %edi
    movb  $0, (%edi)

    movl  $4, %ecx               /* 4 个十六进制位 */
hex16_loop:
    decl  %edi
    movb  %al, %dl
    andb  $0x0F, %dl
    movb  hex_chars(%edx), %dl
    movb  %dl, (%edi)
    shrl  $4, %eax
    loop  hex16_loop

    movl  %edi, %edx
    movb  $SYS_WRITE_STR, %ah
    int   $0x22

    popl  %edi
    popl  %edx
    popl  %ecx
    popl  %eax
    popl  %ebp
    ret

/*
 * print_hex8 — 打印 8-bit 十六进制数 (2 位)
 * 栈(4): 要打印的值 (低 8 位)
 */
print_hex8:
    pushl %ebp
    movl  %esp, %ebp
    pushl %eax
    pushl %ecx
    pushl %edx
    pushl %edi

    movl  8(%ebp), %eax
    leal  hex_buf + 2, %edi
    movb  $0, (%edi)

    movl  $2, %ecx
hex8_loop:
    decl  %edi
    movb  %al, %dl
    andb  $0x0F, %dl
    movb  hex_chars(%edx), %dl
    movb  %dl, (%edi)
    shrl  $4, %eax
    loop  hex8_loop

    movl  %edi, %edx
    movb  $SYS_WRITE_STR, %ah
    int   $0x22

    popl  %edi
    popl  %edx
    popl  %ecx
    popl  %eax
    popl  %ebp
    ret

/*
 * print_pci_class_name — 打印 PCI 类名
 * 根据 pci_buf 中的 class_code 和 subclass 打印设备类型。
 */
print_pci_class_name:
    pushl %eax
    pushl %edx

    movzbl pci_buf + PCI_CLASS, %eax
    cmpl  $0x01, %eax
    je    .pci_mass_storage
    cmpl  $0x02, %eax
    je    .pci_network
    cmpl  $0x03, %eax
    je    .pci_display
    cmpl  $0x04, %eax
    je    .pci_multimedia
    cmpl  $0x06, %eax
    je    .pci_bridge
    cmpl  $0x0C, %eax
    je    .pci_serial
    cmpl  $0x08, %eax
    je    .pci_system

    leal  str_class_other, %edx
    movb  $SYS_WRITE_STR, %ah
    int   $0x22
    jmp   .pci_class_done

.pci_mass_storage:
    movzbl pci_buf + PCI_SUBCLASS, %eax
    pushl %eax
    call  print_subclass_name
    addl  $4, %esp
    jmp   .pci_class_done

.pci_network:
    leal  str_class_network, %edx
    movb  $SYS_WRITE_STR, %ah
    int   $0x22
    jmp   .pci_class_done

.pci_display:
    leal  str_class_display, %edx
    movb  $SYS_WRITE_STR, %ah
    int   $0x22
    jmp   .pci_class_done

.pci_multimedia:
    leal  str_class_mm, %edx
    movb  $SYS_WRITE_STR, %ah
    int   $0x22
    jmp   .pci_class_done

.pci_bridge:
    leal  str_class_bridge, %edx
    movb  $SYS_WRITE_STR, %ah
    int   $0x22
    jmp   .pci_class_done

.pci_serial:
    leal  str_class_serial, %edx
    movb  $SYS_WRITE_STR, %ah
    int   $0x22
    jmp   .pci_class_done

.pci_system:
    leal  str_class_system, %edx
    movb  $SYS_WRITE_STR, %ah
    int   $0x22

.pci_class_done:
    popl  %edx
    popl  %eax
    ret

/*
 * print_subclass_name — 打印存储子类名
 * 栈(4): subclass 值
 */
print_subclass_name:
    pushl %ebp
    movl  %esp, %ebp
    pushl %eax
    pushl %edx

    movl  8(%ebp), %eax
    cmpl  $0x01, %eax
    je    .sub_ide
    cmpl  $0x06, %eax
    je    .sub_sata
    cmpl  $0x08, %eax
    je    .sub_nvme
    cmpl  $0x00, %eax
    je    .sub_scsi
    cmpl  $0x04, %eax
    je    .sub_raid

    leal  str_sub_storage, %edx
    movb  $SYS_WRITE_STR, %ah
    int   $0x22
    jmp   .sub_done

.sub_ide:
    leal  str_sub_ide, %edx
    movb  $SYS_WRITE_STR, %ah
    int   $0x22
    jmp   .sub_done

.sub_sata:
    leal  str_sub_sata, %edx
    movb  $SYS_WRITE_STR, %ah
    int   $0x22
    jmp   .sub_done

.sub_nvme:
    leal  str_sub_nvme, %edx
    movb  $SYS_WRITE_STR, %ah
    int   $0x22
    jmp   .sub_done

.sub_scsi:
    leal  str_sub_scsi, %edx
    movb  $SYS_WRITE_STR, %ah
    int   $0x22
    jmp   .sub_done

.sub_raid:
    leal  str_sub_raid, %edx
    movb  $SYS_WRITE_STR, %ah
    int   $0x22

.sub_done:
    popl  %edx
    popl  %eax
    popl  %ebp
    ret

/* ===== 只读数据 ===== */

.section .rodata

str_title:
    .asciz "Nexsteaduser PlexsDOS — PnP Manager v1.0\n\
============================================\n\
Enumerating PCI/IDE/ISA hardware...\n\
\n$"

str_pci_header:
    .asciz "\n--- PCI Devices ---\n$"

str_pci_cols:
    .asciz " B:D.F     Vendor:Device  Class:Scls:PIF  IRQ  Type\n$"

str_separator:
    .asciz "----------------------------------------------\n$"

str_spaces_pci:
    .asciz "  $"

str_spaces_isa:
    .asciz "  $"

str_devices_found:
    .asciz " device(s) found.\n$"

str_newline:
    .asciz "\n$"

str_disk_count:
    .asciz "\nMass storage controllers: $"

str_disk_header:
    .asciz "\n--- Disk Device Instance IDs ---\n$"

str_no_disks:
    .asciz "No mass storage controllers found.\n$"

str_pci_prefix:
    .asciz "  PCI $"

str_dash:
    .asciz "-$"

str_ven_prefix:
    .asciz "VEN_$"

str_dev_prefix:
    .asciz "&DEV_$"

str_io_prefix:
    .asciz " IO=0x$"

str_irq_prefix:
    .asciz "IRQ=$"

str_isa_header:
    .asciz "\n--- ISA Legacy Devices ---\n$"

str_isa_cols:
    .asciz " I/O Base   IRQ  Device Name\n$"

str_footer:
    .asciz "\nPnP enumeration complete.\n$"

/* 设备类型字符串 (PCI) */
str_class_other:   .asciz "Other$"
str_class_network: .asciz "Network$"
str_class_display: .asciz "Display$"
str_class_mm:      .asciz "Multimedia$"
str_class_bridge:  .asciz "Bridge$"
str_class_serial:  .asciz "Serial Bus$"
str_class_system:  .asciz "System$"

/* 存储子类字符串 */
str_sub_storage: .asciz "Mass Storage$"
str_sub_ide:     .asciz "IDE Controller$"
str_sub_sata:    .asciz "SATA/AHCI$"
str_sub_nvme:    .asciz "NVMe SSD$"
str_sub_scsi:    .asciz "SCSI$"
str_sub_raid:    .asciz "RAID$"

/* 十六进制字符表 */
hex_chars:
    .ascii "0123456789ABCDEF"

/* ===== BSS (可写数据) ===== */

.section .bss
.align 4

/* PCI 设备信息缓冲区 */
pci_buf:    .space PCI_DEVICE_SIZE, 0
/* ISA 设备信息缓冲区 */
isa_buf:    .space ISA_DEVICE_SIZE, 0
/* PCI 设备数量 */
pci_count:  .space 4, 0
/* ISA 设备数量 */
isa_count:  .space 4, 0
/* 磁盘设备数量 */
disk_count: .space 4, 0
/* 磁盘设备索引表 (最大 16 个 × 2 字节) */
disk_indices: .space 32, 0

/* 十进制转换缓冲区 */
dec_buf:    .space 12, 0
dec_buf_end = dec_buf + 11

/* 十六进制转换缓冲区 */
hex_buf:    .space 8, 0
