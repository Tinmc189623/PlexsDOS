/*
 * Nexsteaduser — PlexsDOS
 * 红屏 (Red Screen of Death) 实现
 * 作者: Tinmc189623 | 团队: Nexlyh
 *
 * 不可恢复内核错误时显示全屏红色诊断信息。
 * 同时通过串口输出便于远程调试。
 */

#include <plexsdos/types.h>
#include <plexsdos/panic.h>
#include <plexsdos/config.h>
#include <plexsdos/interrupt.h>
#include <plexsdos/serial.h>

/* 外部汇编桩 — 异常入口点 */
extern void isr_exception_common(void);

/* 外部汇编桩 — 各异常专用入口 (由宏生成) */
extern void isr_exception_0x00(void); /* #DE */
extern void isr_exception_0x01(void); /* #DB */
extern void isr_exception_0x02(void); /* NMI */
extern void isr_exception_0x03(void); /* #BP */
extern void isr_exception_0x04(void); /* #OF */
extern void isr_exception_0x05(void); /* #BR */
extern void isr_exception_0x06(void); /* #UD */
extern void isr_exception_0x07(void); /* #NM */
extern void isr_exception_0x08(void); /* #DF */
extern void isr_exception_0x09(void); /* Overrun */
extern void isr_exception_0x0A(void); /* #TS */
extern void isr_exception_0x0B(void); /* #NP */
extern void isr_exception_0x0C(void); /* #SS */
extern void isr_exception_0x0D(void); /* #GP */
extern void isr_exception_0x0E(void); /* #PF */
extern void isr_exception_0x0F(void); /* Reserved */
extern void isr_exception_0x10(void); /* #MF */
extern void isr_exception_0x11(void); /* #AC */
extern void isr_exception_0x12(void); /* #MC */
extern void isr_exception_0x13(void); /* #XM */
extern void isr_exception_0x14(void); /* #VE */
extern void isr_exception_0x15(void); /* #CP */

/* VGA 文本模式显存 */
static volatile uint16_t *vga = (volatile uint16_t *)VGA_TEXT_BUFFER;

/* RSOD 颜色属性: 红底白字 */
#define RSOD_COLOR  0x4F  /* bg=0x04(红), fg=0x0F(白) */

/* RSOD 光标位置 */
static int rsod_row;
static int rsod_col;

/* ==================== RSOD 输出函数 ==================== */

/*
 * rsod_putchar — 在红屏上输出单个字符
 * @c: ASCII 字符
 *
 * 直接写 VGA 显存, 自动处理换行和滚屏。
 */
static void rsod_putchar(char c)
{
    if (c == '\n') {
        rsod_col = 0;
        rsod_row++;
        if (rsod_row >= SCREEN_ROWS)
            rsod_row = SCREEN_ROWS - 1;
        return;
    }

    if (c == '\r') {
        rsod_col = 0;
        return;
    }

    vga[rsod_row * SCREEN_COLS + rsod_col] =
        (uint16_t)((RSOD_COLOR << 8) | (uint8_t)c);
    rsod_col++;
    if (rsod_col >= SCREEN_COLS) {
        rsod_col = 0;
        rsod_row++;
        if (rsod_row >= SCREEN_ROWS)
            rsod_row = SCREEN_ROWS - 1;
    }
}

/*
 * rsod_puts — 在红屏上输出字符串
 * @str: 以 null 结尾的字符串
 */
static void rsod_puts(const char *str)
{
    while (*str) {
        rsod_putchar(*str);
        str++;
    }
}

/*
 * rsod_put_hex — 输出 32 位十六进制数 (带 0x 前缀)
 * @val: 要输出的值
 */
static void rsod_put_hex(uint32_t val)
{
    int i;
    bool started = false;
    rsod_puts("0x");
    for (i = 28; i >= 0; i -= 4) {
        uint8_t nibble = (val >> i) & 0x0F;
        if (nibble != 0 || started || i == 0) {
            started = true;
            if (nibble < 10)
                rsod_putchar('0' + nibble);
            else
                rsod_putchar('A' + nibble - 10);
        }
    }
}

/*
 * rsod_put_dec — 输出十进制无符号整数
 * @val: 要输出的值
 */
static void rsod_put_dec(uint32_t val)
{
    char buf[11];
    int i = 0;

    if (val == 0) {
        rsod_putchar('0');
        return;
    }

    while (val > 0) {
        buf[i++] = '0' + (val % 10);
        val /= 10;
    }

    while (i > 0)
        rsod_putchar(buf[--i]);
}

/*
 * rsod_put_reg — 输出一个寄存器名和值
 * @name: 寄存器名称 (如 "EAX")
 * @val:  寄存器值
 */
static void rsod_put_reg(const char *name, uint32_t val)
{
    rsod_puts(name);
    rsod_puts("=");
    rsod_put_hex(val);
}

/*
 * rsod_clear — 清屏为红色背景
 */
static void rsod_clear(void)
{
    for (int i = 0; i < SCREEN_SIZE; i++)
        vga[i] = (uint16_t)((RSOD_COLOR << 8) | ' ');
    rsod_row = 0;
    rsod_col = 0;
}

/*
 * rsod_serial_puts — 同时输出到屏幕和串口
 * @str: 字符串
 */
static void rsod_serial_puts(const char *str)
{
    rsod_puts(str);
    serial_puts(str);
}

/*
 * rsod_serial_hex — 同时输出十六进制到屏幕和串口
 * @val: 值
 */
static void rsod_serial_hex(uint32_t val)
{
    rsod_put_hex(val);
    serial_puts("0x");
    /* 串口输出完整 8 位十六进制 */
    for (int i = 28; i >= 0; i -= 4) {
        uint8_t nibble = (val >> i) & 0x0F;
        if (nibble < 10)
            serial_putchar('0' + nibble);
        else
            serial_putchar('A' + nibble - 10);
    }
}

/* ==================== 异常名称表 ==================== */

static const char *exception_names[] = {
    "Division Error",              /* 0x00 #DE */
    "Debug Exception",             /* 0x01 #DB */
    "NMI Interrupt",               /* 0x02 */
    "Breakpoint",                  /* 0x03 #BP */
    "Overflow",                    /* 0x04 #OF */
    "Bound Range Exceeded",        /* 0x05 #BR */
    "Invalid Opcode",              /* 0x06 #UD */
    "Device Not Available",        /* 0x07 #NM */
    "Double Fault",                /* 0x08 #DF */
    "Coprocessor Segment Overrun", /* 0x09 */
    "Invalid TSS",                 /* 0x0A #TS */
    "Segment Not Present",         /* 0x0B #NP */
    "Stack Segment Fault",         /* 0x0C #SS */
    "General Protection Fault",    /* 0x0D #GP */
    "Page Fault",                  /* 0x0E #PF */
    "Reserved",                    /* 0x0F */
    "x87 FPU Error",              /* 0x10 #MF */
    "Alignment Check",             /* 0x11 #AC */
    "Machine Check",               /* 0x12 #MC */
    "SIMD Exception",              /* 0x13 #XM */
    "Virtualization Exception",    /* 0x14 #VE */
    "Control Protection",          /* 0x15 #CP */
};

#define EXCEPTION_COUNT (sizeof(exception_names) / sizeof(exception_names[0]))

/* ==================== CR 寄存器读取 ==================== */

/*
 * read_cr0 — 读取 CR0 寄存器
 */
static uint32_t read_cr0(void)
{
    uint32_t val;
    __asm__ __volatile__("mov %%cr0, %0" : "=r"(val));
    return val;
}

/*
 * read_cr2 — 读取 CR2 (页错误线性地址)
 */
static uint32_t read_cr2(void)
{
    uint32_t val;
    __asm__ __volatile__("mov %%cr2, %0" : "=r"(val));
    return val;
}

/*
 * read_cr3 — 读取 CR3 (页目录基址)
 */
static uint32_t read_cr3(void)
{
    uint32_t val;
    __asm__ __volatile__("mov %%cr3, %0" : "=r"(val));
    return val;
}

/* ==================== 异常处理程序 ==================== */

/*
 * panic_exception_handler — CPU 异常处理入口
 * @vector:     异常向量号
 * @error_code: 错误码 (部分异常无, 由汇编桩传 0)
 * @regs:       保存的寄存器快照
 *
 * 显示完整的红屏诊断信息:
 * 1. 异常类型和错误码
 * 2. 所有通用寄存器 + 段寄存器 + EFLAGS
 * 3. 控制寄存器 CR0/CR2/CR3
 * 4. 栈内容 (8 个 32 位字)
 * 5. EBP 链回溯 (最多 10 帧)
 * 6. 串口同步输出
 */
void panic_exception_handler(uint32_t vector, uint32_t error_code,
                             struct panic_regs *regs)
{
    const char *name;
    uint32_t *stack_ptr;
    uint32_t *ebp;

    /* 关闭中断 */
    __asm__ __volatile__("cli");

    /* 获取异常名称 */
    if (vector < EXCEPTION_COUNT)
        name = exception_names[vector];
    else
        name = "Unknown Exception";

    /* 清屏为红色 */
    rsod_clear();

    /* 输出到串口 */
    serial_puts("\n\n=== Nexsteaduser PlexsDOS KERNEL PANIC ===\n");

    /* 标题 */
    rsod_puts("            Nexsteaduser PlexsDOS\n");
    rsod_puts("           *** KERNEL PANIC ***\n\n");

    /* 异常信息 */
    rsod_serial_puts("Exception: ");
    rsod_serial_puts(name);
    rsod_serial_puts(" (0x");
    /* 串口输出向量号 */
    {
        uint8_t nibble;
        nibble = (vector >> 4) & 0x0F;
        serial_putchar(nibble < 10 ? '0' + nibble : 'A' + nibble - 10);
        nibble = vector & 0x0F;
        serial_putchar(nibble < 10 ? '0' + nibble : 'A' + nibble - 10);
    }
    serial_puts(")\n");
    rsod_puts("\n");

    /* 错误码 */
    if (error_code != 0 || vector == 0x08 || vector == 0x0A ||
        vector == 0x0B || vector == 0x0C || vector == 0x0D ||
        vector == 0x0E || vector == 0x11 || vector == 0x15) {
        rsod_serial_puts("Error Code: ");
        rsod_serial_hex(error_code);
        rsod_serial_puts("\n");
    }

    /* 故障地址 (EIP) */
    rsod_serial_puts("Fault Address: ");
    rsod_serial_hex(regs->eip);
    rsod_serial_puts("\n\n");

    /* 通用寄存器 */
    rsod_puts("Registers:\n");

    rsod_put_reg("  EAX", regs->eax);
    rsod_puts("  ");
    rsod_put_reg("EBX", regs->ebx);
    rsod_puts("  ");
    rsod_put_reg("ECX", regs->ecx);
    rsod_puts("\n");

    rsod_put_reg("  EDX", regs->edx);
    rsod_puts("  ");
    rsod_put_reg("ESI", regs->esi);
    rsod_puts("  ");
    rsod_put_reg("EDI", regs->edi);
    rsod_puts("\n");

    rsod_put_reg("  EBP", regs->ebp);
    rsod_puts("  ");
    rsod_put_reg("ESP", regs->esp);
    rsod_puts("  ");
    rsod_put_reg("EIP", regs->eip);
    rsod_puts("\n");

    /* 段寄存器和 EFLAGS */
    rsod_puts("  CS=0x");
    rsod_put_hex(regs->cs & 0xFFFF);
    rsod_puts("  DS=0x");
    rsod_put_hex(regs->ds & 0xFFFF);
    rsod_puts("  SS=0x");
    rsod_put_hex(regs->ss & 0xFFFF);
    rsod_puts("\n");

    rsod_put_reg("  EFLAGS", regs->eflags);
    rsod_puts("\n\n");

    /* 控制寄存器 */
    rsod_puts("Control Registers:\n");
    rsod_put_reg("  CR0", read_cr0());
    rsod_puts("  ");
    rsod_put_reg("CR2", read_cr2());
    rsod_puts("  ");
    rsod_put_reg("CR3", read_cr3());
    rsod_puts("\n\n");

    /* 串口输出完整寄存器信息 */
    serial_puts("Registers:\n");
    serial_puts("  EAX="); rsod_serial_hex(regs->eax);
    serial_puts("  EBX="); rsod_serial_hex(regs->ebx);
    serial_puts("  ECX="); rsod_serial_hex(regs->ecx);
    serial_puts("\n  EDX="); rsod_serial_hex(regs->edx);
    serial_puts("  ESI="); rsod_serial_hex(regs->esi);
    serial_puts("  EDI="); rsod_serial_hex(regs->edi);
    serial_puts("\n  EBP="); rsod_serial_hex(regs->ebp);
    serial_puts("  ESP="); rsod_serial_hex(regs->esp);
    serial_puts("  EIP="); rsod_serial_hex(regs->eip);
    serial_puts("\n  CS="); rsod_serial_hex(regs->cs & 0xFFFF);
    serial_puts("  DS="); rsod_serial_hex(regs->ds & 0xFFFF);
    serial_puts("  SS="); rsod_serial_hex(regs->ss & 0xFFFF);
    serial_puts("\n  EFLAGS="); rsod_serial_hex(regs->eflags);
    serial_puts("\n  CR0="); rsod_serial_hex(read_cr0());
    serial_puts("  CR2="); rsod_serial_hex(read_cr2());
    serial_puts("  CR3="); rsod_serial_hex(read_cr3());
    serial_puts("\n");

    /* 栈内容 (8 个 32 位字) */
    rsod_puts("Stack (8 words at ESP):\n  ");
    stack_ptr = (uint32_t *)regs->esp;
    for (int i = 0; i < 8; i++) {
        /* 检查栈指针是否在合理范围 (0x1000 - 0x90000) */
        if ((uint32_t)stack_ptr >= 0x1000 && (uint32_t)stack_ptr < 0x90000) {
            rsod_put_hex(*stack_ptr);
            rsod_putchar(' ');
        } else {
            rsod_puts("???????? ");
        }
        stack_ptr++;
    }
    rsod_puts("\n\n");

    /* EBP 链回溯 (最多 10 帧) */
    rsod_puts("Stack Trace:\n");
    serial_puts("Stack Trace:\n");
    ebp = (uint32_t *)regs->ebp;
    for (int frame = 0; frame < 10; frame++) {
        uint32_t ret_addr;

        /* 检查 EBP 是否有效 */
        if ((uint32_t)ebp < 0x1000 || (uint32_t)ebp >= 0x90000)
            break;

        /* EBP 链: [ebp] = 旧 EBP, [ebp+4] = 返回地址 */
        ret_addr = ebp[1];
        if (ret_addr == 0)
            break;

        rsod_puts("  #");
        rsod_put_dec(frame);
        rsod_puts(" ");
        rsod_put_hex(ret_addr);
        rsod_puts("\n");

        serial_puts("  #");
        /* 串口输出帧号 */
        serial_putchar('0' + frame);
        serial_puts(" ");
        rsod_serial_hex(ret_addr);
        serial_puts("\n");

        /* 沿 EBP 链向上 */
        ebp = (uint32_t *)ebp[0];
    }

    /* 底部提示 */
    rsod_puts("\nSystem halted. Press Ctrl+Alt+Del to reboot.\n");

    serial_puts("\nSystem halted.\n");

    /* 死循环 */
    while (1) {
        __asm__ __volatile__("hlt");
    }
}

/* ==================== kernel_panic ==================== */

/*
 * kernel_panic — 主动触发内核恐慌
 * @fmt: 格式化字符串 (支持 %s, %d, %x)
 *
 * 用于检测到不可恢复错误时主动调用。
 * 创建一个伪造的寄存器快照后调用 panic_exception_handler。
 */
_Noreturn void kernel_panic(const char *fmt, ...)
{
    struct panic_regs regs;
    uint32_t sp;

    __asm__ __volatile__("cli");
    __asm__ __volatile__("mov %%esp, %0" : "=r"(sp));

    /* 清零寄存器快照 */
    regs.eax = 0;
    regs.ebx = 0;
    regs.ecx = 0;
    regs.edx = 0;
    regs.esi = 0;
    regs.edi = 0;
    regs.esp = sp;
    regs.ebp = 0;
    regs.eip = 0;
    regs.cs = 0x08;
    regs.ds = 0x10;
    regs.es = 0x10;
    regs.fs = 0x10;
    regs.gs = 0x10;
    regs.ss = 0x10;
    regs.eflags = 0;

    /* 读取调用者的 EBP 和返回地址 */
    __asm__ __volatile__(
        "mov %%ebp, %0\n\t"
        : "=r"(regs.ebp)
    );
    if (regs.ebp >= 0x1000 && regs.ebp < 0x90000) {
        uint32_t *frame = (uint32_t *)regs.ebp;
        regs.eip = frame[1];  /* 返回地址 */
    }

    /* 先显示自定义消息 */
    rsod_clear();
    rsod_puts("            Nexsteaduser PlexsDOS\n");
    rsod_puts("           *** KERNEL PANIC ***\n\n");
    rsod_puts("Error: ");
    rsod_puts(fmt);
    rsod_puts("\n\n");

    serial_puts("\n=== KERNEL PANIC ===\n");
    serial_puts("Error: ");
    serial_puts(fmt);
    serial_puts("\n");

    /* 然后显示寄存器和栈信息 */
    panic_exception_handler(0xFF, 0, &regs);

    /* panic_exception_handler 不会返回, 但编译器需要 */
    while (1) {
        __asm__ __volatile__("hlt");
    }
}

/* ==================== IDT 注册 ==================== */

/*
 * panic_init — 注册 CPU 异常处理程序到 IDT
 *
 * 将关键异常 (INT 0x00-0x15) 注册为使用各自的汇编入口桩。
 * 在 kernel_main.c 中 idt_init() 之后调用。
 */
void panic_init(void)
{
    /* 注册无错误码的异常 (汇编桩会压入 0 作为占位) */
    idt_set_gate(0x00, (interrupt_handler_t)isr_exception_0x00); /* #DE */
    idt_set_gate(0x01, (interrupt_handler_t)isr_exception_0x01); /* #DB */
    idt_set_gate(0x02, (interrupt_handler_t)isr_exception_0x02); /* NMI */
    idt_set_gate(0x03, (interrupt_handler_t)isr_exception_0x03); /* #BP */
    idt_set_gate(0x04, (interrupt_handler_t)isr_exception_0x04); /* #OF */
    idt_set_gate(0x05, (interrupt_handler_t)isr_exception_0x05); /* #BR */
    idt_set_gate(0x06, (interrupt_handler_t)isr_exception_0x06); /* #UD */
    idt_set_gate(0x07, (interrupt_handler_t)isr_exception_0x07); /* #NM */
    idt_set_gate(0x09, (interrupt_handler_t)isr_exception_0x09); /* Overrun */
    idt_set_gate(0x0F, (interrupt_handler_t)isr_exception_0x0F); /* Reserved */
    idt_set_gate(0x10, (interrupt_handler_t)isr_exception_0x10); /* #MF */
    idt_set_gate(0x12, (interrupt_handler_t)isr_exception_0x12); /* #MC */
    idt_set_gate(0x13, (interrupt_handler_t)isr_exception_0x13); /* #XM */
    idt_set_gate(0x14, (interrupt_handler_t)isr_exception_0x14); /* #VE */

    /* 注册有错误码的异常 (CPU 自动压入错误码) */
    idt_set_gate(0x08, (interrupt_handler_t)isr_exception_0x08); /* #DF */
    idt_set_gate(0x0A, (interrupt_handler_t)isr_exception_0x0A); /* #TS */
    idt_set_gate(0x0B, (interrupt_handler_t)isr_exception_0x0B); /* #NP */
    idt_set_gate(0x0C, (interrupt_handler_t)isr_exception_0x0C); /* #SS */
    idt_set_gate(0x0D, (interrupt_handler_t)isr_exception_0x0D); /* #GP */
    idt_set_gate(0x0E, (interrupt_handler_t)isr_exception_0x0E); /* #PF */
    idt_set_gate(0x11, (interrupt_handler_t)isr_exception_0x11); /* #AC */
    idt_set_gate(0x15, (interrupt_handler_t)isr_exception_0x15); /* #CP */

    serial_puts("[panic] CPU exception handlers registered.\n");
}
