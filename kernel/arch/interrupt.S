/*
 * Nexsteaduser — PlexsDOS
 * 中断处理入口 (32-bit 保护模式)
 * 作者: Tinmc189623 | 团队: Nexlyh
 *
 * 定义 ISR 入口点, 保存/恢复寄存器并调用 C 处理程序。
 * 汇编语法: GAS AT&T
 */

.code32
.section .text

.global _isr_default
.global _isr_keyboard
.global _idt_load
.global _isr_syscall
.global _isr_fdc
.global _isr_mouse
.global _isr_exception_common

/*
 * ISR_SAVE_REGS — 保存所有通用寄存器
 */
.macro ISR_SAVE_REGS
    pusha
    push %ds
    push %es
    push %fs
    push %gs
    /* 设置数据段选择子 */
    mov  $0x10, %ax
    mov  %ax, %ds
    mov  %ax, %es
    mov  %ax, %fs
    mov  %ax, %gs
.endm

/*
 * ISR_RESTORE_REGS — 恢复所有通用寄存器
 */
.macro ISR_RESTORE_REGS
    pop  %gs
    pop  %fs
    pop  %es
    pop  %ds
    popa
.endm

/*
 * isr_default — 默认中断处理程序
 * 处理未注册的中断向量。
 */
_isr_default:
    ISR_SAVE_REGS
    /* 发送 EOI 到主 PIC */
    mov  $0x20, %al
    out  %al, $0x20
    ISR_RESTORE_REGS
    iret

/*
 * isr_keyboard — 键盘中断处理程序 (IRQ1, INT 0x21)
 * 调用 C 函数 kbd_interrupt_handler()。
 */
_isr_keyboard:
    ISR_SAVE_REGS
    call _kbd_interrupt_handler
    /* 发送 EOI */
    mov  $0x20, %al
    out  %al, $0x20
    ISR_RESTORE_REGS
    iret

/*
 * isr_fdc — 软盘控制器中断处理程序 (IRQ6, INT 0x26)
 * 调用 C 函数 fdc_interrupt_handler()。
 */
_isr_fdc:
    ISR_SAVE_REGS
    call _fdc_interrupt_handler
    /* EOI 由 C 处理程序发送 */
    ISR_RESTORE_REGS
    iret

/*
 * isr_mouse — PS/2 鼠标中断处理程序 (IRQ12, INT 0x2C)
 * 调用 C 函数 mouse_interrupt_handler()。
 * IRQ12 位于从片 (slave PIC), 需向两片发送 EOI。
 */
_isr_mouse:
    ISR_SAVE_REGS
    call _mouse_interrupt_handler
    /* EOI: 先从片 (0xA0), 后主片 (0x20) */
    mov  $0x20, %al
    out  %al, $0xA0
    out  %al, $0x20
    ISR_RESTORE_REGS
    iret

/*
 * idt_load — 加载 IDT 寄存器
 * 输入: 栈顶 = 指向 idt_ptr 结构的指针
 */
_idt_load:
    push %ebp
    mov  %esp, %ebp
    mov  8(%ebp), %eax      /* 获取 idt_ptr 地址 */
    lidt (%eax)              /* 加载 IDTR */
    pop  %ebp
    ret

/*
 * _isr_syscall — INT 0x22 系统调用入口 (DOS INT 21h 兼容)
 * 32-bit 保护模式软件中断。
 *
 * 支持 Ring 0 和 Ring 3 调用者。CPU 自动处理特权级切换:
 * - 从 Ring 0: 栈上 [EIP][CS][EFLAGS] (3 words)
 * - 从 Ring 3: 栈上 [EIP][CS][EFLAGS][ESP][SS] (5 words)
 *
 * Ring 3 比 Ring 0 多压入 ESP 和 SS (8 字节), 因此参数偏移不同。
 * 通过检查栈上 CS 的 RPL 位检测调用者特权级。
 *
 * 调用 C: _cpp_syscall_dispatch(eax, edx, esi)。
 * 返回值 1 = 程序请求终止 (AH=0x4C)。
 */
_isr_syscall:
    pusha
    push %ds
    push %es
    push %fs
    push %gs

    /* 设置内核数据段 */
    mov  $0x10, %ax
    mov  %ax, %ds
    mov  %ax, %es
    mov  %ax, %fs
    mov  %ax, %gs

    /* 检测调用者特权级: 栈上 CS 的 RPL (bit 0-1)
     * pusha + 4 段寄存器 = 48 字节
     * CS 在 pusha 之后的偏移: [ESP+52] (Ring 0) 或 [ESP+60] (Ring 3) */
    mov  52(%esp), %eax      /* 读取 CS (Ring 0 偏移) */
    test $3, %eax            /* 检查 RPL */
    jnz  .syscall_from_ring3

    /* Ring 0 调用者: [EIP][CS][EFLAGS]
     * EAX = [ESP+44], EDX = [ESP+36], ESI = [ESP+20] */
    push 44(%esp)            /* EAX */
    push 36+4(%esp)          /* EDX */
    push 20+8(%esp)          /* ESI */
    jmp  .syscall_dispatch

.syscall_from_ring3:
    /* Ring 3 调用者: [EIP][CS][EFLAGS][ESP][SS]
     * 多了 ESP+SS (8 字节), 偏移各 +8:
     * EAX = [ESP+52], EDX = [ESP+44], ESI = [ESP+28] */
    push 52(%esp)            /* EAX */
    push 44+4(%esp)          /* EDX */
    push 28+8(%esp)          /* ESI */

.syscall_dispatch:
    call _cpp_syscall_dispatch
    add  $12, %esp

    /* 保存返回值到 EBX (EBX 在 pusha 中会被恢复) */
    mov  %eax, %ebx

    /* 恢复寄存器 */
    pop  %gs
    pop  %fs
    pop  %es
    pop  %ds
    popa

    /* 检查退出标志 */
    test %ebx, %ebx
    jnz  .syscall_exit

    /* 正常返回: iret 根据 CS RPL 自动处理 Ring 3 → Ring 3 或 Ring 0 → Ring 0 */
    iret

/*
 * .syscall_exit — 程序通过 INT 21h AH=4C 请求终止
 *
 * 从 Ring 3 运行时, 不能简单 iret (会返回 Ring 3)。
 * 需要恢复 loader_enter_ring3 保存的内核上下文。
 */
.syscall_exit:
    /* 恢复内核栈指针和帧指针 */
    mov  _g_return_ctx, %esp
    mov  _g_return_ctx + 4, %ebp

    /* 压入保存的返回地址 (loader_run 的调用者) */
    mov  _g_return_ctx + 8, %eax
    push %eax

    /* 返回值 0 表示程序已退出 */
    xor  %eax, %eax
    ret

/*
 * _loader_enter_ring3 — 从 Ring 0 切换到 Ring 3
 *
 * C 原型: void loader_enter_ring3(uint32_t user_eip, uint32_t user_esp);
 *
 * 保存内核返回上下文 (ESP, EBP, 返回地址) 到 _g_return_ctx,
 * 构造 iret 帧 (SS=0x40, ESP, EFLAGS(IF=1), CS=0x38, EIP),
 * 执行 iret 进入 Ring 3。
 *
 * 程序通过 INT 0x22 AH=0x4C 退出时, .syscall_exit 恢复此上下文。
 */
.global _loader_enter_ring3
_loader_enter_ring3:
    /* 读取参数 (C 调用约定, 参数在栈上) */
    mov  4(%esp), %eax         /* user EIP */
    mov  8(%esp), %ecx         /* user ESP */

    /* 保存内核返回上下文 */
    mov  %esp, _g_return_ctx         /* ESP (指向返回地址) */
    mov  %ebp, _g_return_ctx + 4    /* EBP */
    mov  (%esp), %edx               /* 返回地址 (loader_run 中的下一条指令) */
    mov  %edx, _g_return_ctx + 8   /* 保存返回地址 */

    /* 读取当前 EFLAGS 并启用中断 */
    pushf
    pop  %edx
    or   $0x200, %edx               /* IF=1 */

    /* 构造 Ring 3 iret 帧 */
    pushl $0x40                /* SS:  Ring 3 数据段 */
    pushl %ecx                 /* ESP: 用户栈顶 */
    pushl %edx                 /* EFLAGS: 中断已启用 */
    pushl $0x38                /* CS:  Ring 3 代码段 */
    pushl %eax                 /* EIP: 程序入口点 */
    iret                       /* 切换到 Ring 3 */

/*
 * 内核返回上下文 (由 _loader_enter_ring3 保存, 由 .syscall_exit 恢复)
 * 布局: [ESP][EBP][返回地址]
 */
.section .data
.align 4
.global _g_return_ctx
_g_return_ctx:
    .long 0    /* ESP */
    .long 0    /* EBP */
    .long 0    /* 返回地址 (EIP) */

/*
 * _isr_exception_common — CPU 异常通用入口
 *
 * 由各异常专用桩跳转至此。栈上已有:
 * - 向量号 (由专用桩压入)
 * - 错误码 (CPU 压入或专用桩压入 0 占位)
 * - EIP, CS, EFLAGS (CPU 压入)
 *
 * 栈布局 (从低地址到高地址):
 *   [ESP+0]  异常向量号 (由专用桩压入)
 *   [ESP+4]  错误码 (CPU 或专用桩)
 *   [ESP+8]  EIP     (CPU)
 *   [ESP+12] CS      (CPU)
 *   [ESP+16] EFLAGS  (CPU)
 *
 * 调用 C: _panic_exception_handler(vector, error_code, regs_ptr)
 */
_isr_exception_common:
    /* VGA 调试标记: 第 3 行显示 'EX' (异常触发) */
    pushl %eax
    pushl %ebx
    movl  $0xB8000, %ebx
    movw  $0x4F45, 320(%ebx)  /* 'E' 红底白字, 第 3 行 */
    movw  $0x4F58, 322(%ebx)  /* 'X' */
    popl  %ebx
    popl  %eax

    /* 保存所有通用寄存器 */
    pusha
    push %ds
    push %es
    push %fs
    push %gs

    /* 设置内核数据段 */
    mov  $0x10, %ax
    mov  %ax, %ds
    mov  %ax, %es
    mov  %ax, %fs
    mov  %ax, %gs

    /* 检测调用者特权级
     *
     * 栈布局因 Ring 级别不同:
     * Ring 0: [vector][err][EIP][CS][EFLAGS]           — CS 在 ESP+56
     * Ring 3: [vector][err][EIP][CS][EFLAGS][ESP][SS]  — CS 在 ESP+60
     *
     * 检查 ESP+56 处的值: Ring 0 CS=0x08, Ring 3 CS=0x38 */
    mov  56(%esp), %eax
    cmp  $0x08, %eax
    je   .exception_ring0

    /* Ring 3: 偏移各 +8 */
    /* 预留 12 字节给 3 个参数 */
    sub  $12, %esp

    lea  12(%esp), %eax             /* arg3: regs_ptr */
    mov  %eax, 8(%esp)

    mov  64+8(%esp), %eax           /* arg2: error_code [ESP+72] */
    mov  %eax, 4(%esp)

    mov  60+8(%esp), %eax           /* arg1: vector [ESP+68] */
    mov  %eax, 0(%esp)
    jmp  .exception_call

.exception_ring0:
    /* Ring 0: 原始偏移 */
    sub  $12, %esp

    lea  12(%esp), %eax             /* arg3: regs_ptr */
    mov  %eax, 8(%esp)

    mov  64(%esp), %eax             /* arg2: error_code [ESP+64] */
    mov  %eax, 4(%esp)

    mov  60(%esp), %eax             /* arg1: vector [ESP+60] */
    mov  %eax, 0(%esp)

.exception_call:
    call _panic_exception_handler
    /* 不返回 */
1:  hlt
    jmp  1b

/*
 * 异常专用桩 — 为无错误码的异常压入 0 作为占位
 * 然后跳转到通用入口
 */
.macro EXCEPTION_STUB_NO_ERROR vector
.global _isr_exception_\vector
_isr_exception_\vector:
    pushl $0                    /* 压入占位错误码 */
    pushl $\vector              /* 压入异常向量号 */
    jmp _isr_exception_common
.endm

/*
 * 异常专用桩 — 为有错误码的异常
 * CPU 已自动压入错误码, 只需压入向量号
 */
.macro EXCEPTION_STUB_WITH_ERROR vector
.global _isr_exception_\vector
_isr_exception_\vector:
    pushl $\vector              /* 压入异常向量号 */
    jmp _isr_exception_common
.endm

/* 无错误码的异常 */
EXCEPTION_STUB_NO_ERROR 0x00    /* #DE Division Error */
EXCEPTION_STUB_NO_ERROR 0x01    /* #DB Debug */
EXCEPTION_STUB_NO_ERROR 0x02    /* NMI */
EXCEPTION_STUB_NO_ERROR 0x03    /* #BP Breakpoint */
EXCEPTION_STUB_NO_ERROR 0x04    /* #OF Overflow */
EXCEPTION_STUB_NO_ERROR 0x05    /* #BR Bound Range */
EXCEPTION_STUB_NO_ERROR 0x06    /* #UD Invalid Opcode */
EXCEPTION_STUB_NO_ERROR 0x07    /* #NM Device Not Available */
EXCEPTION_STUB_NO_ERROR 0x09    /* Coprocessor Overrun */
EXCEPTION_STUB_NO_ERROR 0x0F    /* Reserved */
EXCEPTION_STUB_NO_ERROR 0x10    /* #MF x87 FPU Error */
EXCEPTION_STUB_NO_ERROR 0x12    /* #MC Machine Check */
EXCEPTION_STUB_NO_ERROR 0x13    /* #XM SIMD Exception */
EXCEPTION_STUB_NO_ERROR 0x14    /* #VE Virtualization */

/* 有错误码的异常 */
EXCEPTION_STUB_WITH_ERROR 0x08  /* #DF Double Fault */
EXCEPTION_STUB_WITH_ERROR 0x0A  /* #TS Invalid TSS */
EXCEPTION_STUB_WITH_ERROR 0x0B  /* #NP Segment Not Present */
EXCEPTION_STUB_WITH_ERROR 0x0C  /* #SS Stack Segment Fault */
EXCEPTION_STUB_WITH_ERROR 0x0D  /* #GP General Protection Fault */
EXCEPTION_STUB_WITH_ERROR 0x0E  /* #PF Page Fault */
EXCEPTION_STUB_WITH_ERROR 0x11  /* #AC Alignment Check */
EXCEPTION_STUB_WITH_ERROR 0x15  /* #CP Control Protection */
