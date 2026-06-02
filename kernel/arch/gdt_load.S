/*
 * Nexsteaduser — PlexsDOS
 * gdt.S — GDT 和 TSS 数据结构及加载函数
 * 作者: Tinmc189623 | 团队: Nexlyh
 *
 * 定义包含 Ring 0-3 段描述符的 GDT 和 TSS。
 * 提供 _gdt_load 函数加载 GDT 并执行 ltr。
 * GAS AT&T 语法, 32-bit 保护模式。
 */

.code32
.section .text

/*
 * GDT 布局 (10 个描述符, 80 字节):
 *   0x00  Null
 *   0x08  Ring 0 代码 (DPL=0, access=0x9A)
 *   0x10  Ring 0 数据 (DPL=0, access=0x92)
 *   0x18  Ring 1 代码 (DPL=1, access=0xBA)
 *   0x20  Ring 1 数据 (DPL=1, access=0xB2)
 *   0x28  Ring 2 代码 (DPL=2, access=0xDA)
 *   0x30  Ring 2 数据 (DPL=2, access=0xD2)
 *   0x38  Ring 3 代码 (DPL=3, access=0xFA)
 *   0x40  Ring 3 数据 (DPL=3, access=0xF2)
 *   0x48  TSS        (access=0x89)
 *
 * 所有代码/数据段: base=0, limit=4GB, 4KB 粒度, 32-bit
 */

.align 8
.global _gdt
_gdt:
    /* 0x00: Null descriptor */
    .word 0xFFFF, 0x0000
    .byte 0x00, 0x00, 0x00, 0x00

    /* 0x08: Ring 0 code — access=0x9A (P=1, DPL=0, S=1, exec/read) */
    .word 0xFFFF, 0x0000
    .byte 0x00, 0x9A, 0xCF, 0x00

    /* 0x10: Ring 0 data — access=0x92 (P=1, DPL=0, S=1, read/write) */
    .word 0xFFFF, 0x0000
    .byte 0x00, 0x92, 0xCF, 0x00

    /* 0x18: Ring 1 code — access=0xBA (P=1, DPL=1, S=1, exec/read) */
    .word 0xFFFF, 0x0000
    .byte 0x00, 0xBA, 0xCF, 0x00

    /* 0x20: Ring 1 data — access=0xB2 (P=1, DPL=1, S=1, read/write) */
    .word 0xFFFF, 0x0000
    .byte 0x00, 0xB2, 0xCF, 0x00

    /* 0x28: Ring 2 code — access=0xDA (P=1, DPL=2, S=1, exec/read) */
    .word 0xFFFF, 0x0000
    .byte 0x00, 0xDA, 0xCF, 0x00

    /* 0x30: Ring 2 data — access=0xD2 (P=1, DPL=2, S=1, read/write) */
    .word 0xFFFF, 0x0000
    .byte 0x00, 0xD2, 0xCF, 0x00

    /* 0x38: Ring 3 code — access=0xFA (P=1, DPL=3, S=1, exec/read) */
    .word 0xFFFF, 0x0000
    .byte 0x00, 0xFA, 0xCF, 0x00

    /* 0x40: Ring 3 data — access=0xF2 (P=1, DPL=3, S=1, read/write) */
    .word 0xFFFF, 0x0000
    .byte 0x00, 0xF2, 0xCF, 0x00

    /* 0x48: TSS descriptor — access=0x89 (P=1, DPL=0, 32-bit available TSS)
     * TSS base 和 limit 由 gdt_init 在运行时填充 */
    .word 0x0067, 0x0000    /* limit=103 (0x67), base 低 16 位 (运行时填充) */
    .byte 0x00, 0x89, 0x00, 0x00  /* base 中 8 位, access, 精度+limit 高, base 高 8 位 */

_gdt_end:

/* GDT 描述符 (lgdt 操作数) */
_gdt_desc:
    .word _gdt_end - _gdt - 1   /* GDT 大小 - 1 */
    .long _gdt                   /* GDT 线性地址 */

/*
 * TSS — 任务状态段 (104 字节)
 * CPU 在 Ring 切换时读取 ESP0/SS0。
 */
.align 16
.global _tss
_tss:
    .long 0              /* 0x00: 前一个 TSS 链接 */
    .long 0x91000        /* 0x04: ESP0 — Ring 0 栈指针 (中断处理用) */
    .long 0x10           /* 0x08: SS0  — Ring 0 栈段 */
    .long 0              /* 0x0C: ESP1 (Ring 1, 未使用) */
    .long 0              /* 0x10: SS1  (Ring 1, 未使用) */
    .long 0              /* 0x14: ESP2 (Ring 2, 未使用) */
    .long 0              /* 0x18: SS2  (Ring 2, 未使用) */
    .long 0              /* 0x1C: CR3  (页目录基址, 由 paging_init 设置) */
    .long 0              /* 0x20: EIP  */
    .long 0              /* 0x24: EFLAGS */
    .long 0              /* 0x28: EAX  */
    .long 0              /* 0x2C: ECX  */
    .long 0              /* 0x30: EDX  */
    .long 0              /* 0x34: EBX  */
    .long 0              /* 0x38: ESP  */
    .long 0              /* 0x3C: EBP  */
    .long 0              /* 0x40: ESI  */
    .long 0              /* 0x44: EDI  */
    .long 0              /* 0x48: ES   */
    .long 0              /* 0x4C: CS   */
    .long 0              /* 0x50: SS   */
    .long 0              /* 0x54: DS   */
    .long 0              /* 0x58: FS   */
    .long 0              /* 0x5C: GS   */
    .long 0              /* 0x60: LDT 段选择子 */
    .word 0              /* 0x64: I/O 位图基址 (0x68 = 无 I/O 位图) */
    .word 0x68           /* 0x66: I/O 位图基址高字节 (必须 >= 0x68) */
_tss_end:

/* TSS 大小常量 */
.equ TSS_SIZE, _tss_end - _tss

.section .text

/*
 * _gdt_load — 加载 GDT 寄存器和任务寄存器
 *
 * 由 gdt_init() 调用。加载 GDT 描述符到 GDTR，
 * 重新加载所有段寄存器为 Ring 0 选择子，
 * 执行 ltr 加载 TSS 选择子到任务寄存器。
 *
 * 调用前需确保 TSS 的 base 字段已由 C 代码填充。
 */
.global _gdt_load
_gdt_load:
    /* 加载 GDT */
    lgdt _gdt_desc

    /* 重新加载段寄存器为 Ring 0 选择子 */
    mov  $0x10, %ax
    mov  %ax, %ds
    mov  %ax, %es
    mov  %ax, %fs
    mov  %ax, %gs
    mov  %ax, %ss

    /* 远跳刷新 CS 为 Ring 0 代码段 */
    ljmp $0x08, $.gdt_reload_cs

.gdt_reload_cs:
    /* 加载 TSS 选择子到任务寄存器 */
    mov  $0x48, %ax
    ltr  %ax

    ret
