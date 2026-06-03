/*
 * Nexsteaduser — PlexsDOS
 * 中断描述符表 (IDT) 管理 — 32-bit 保护模式
 * 作者: Tinmc189623 | 团队: Nexlyh
 *
 * 实现 32-bit 保护模式的 IDT 初始化和中断处理。
 */

#include <plexsdos/types.h>
#include <plexsdos/interrupt.h>

/* 外部汇编函数 */
/* 汇编符号 (PE 格式编译器自动加 _ 前缀, 这里用原始名) */
extern void isr_default(void);
extern void isr_keyboard(void);
extern void isr_fdc(void);
extern void isr_mouse(void);
extern void idt_load(void *idt_desc);

/* IDT 条目结构 (8 字节) */
struct idt_entry {
    uint16_t base_low;      /* 处理程序地址低 16 位 */
    uint16_t selector;      /* 代码段选择子 */
    uint8_t  zero;          /* 保留, 必须为 0 */
    uint8_t  flags;         /* 类型和属性 */
    uint16_t base_high;     /* 处理程序地址高 16 位 */
} __attribute__((packed));

/* IDT 寄存器描述符 */
struct idt_ptr {
    uint16_t limit;         /* IDT 大小 - 1 */
    uint32_t base;          /* IDT 线性基址 */
} __attribute__((packed));

/* IDT 表 (256 个条目) */
static struct idt_entry idt[IDT_ENTRIES];

/* IDT 描述符 */
static struct idt_ptr idt_desc;

/* 中断处理程序表 */
static interrupt_handler_t handlers[IDT_ENTRIES];

/*
 * idt_set_entry — 设置 IDT 条目
 * @vector:   中断向量号
 * @handler:  处理程序地址
 * @selector: 代码段选择子
 * @flags:    类型和属性标志
 */
static void idt_set_entry(uint8_t vector, uint32_t handler,
                           uint16_t selector, uint8_t flags)
{
    idt[vector].base_low  = (uint16_t)(handler & 0xFFFF);
    idt[vector].base_high = (uint16_t)((handler >> 16) & 0xFFFF);
    idt[vector].selector  = selector;
    idt[vector].zero      = 0;
    idt[vector].flags     = flags;
}

/*
 * idt_init — 初始化 IDT
 * 设置 256 个中断向量, 重映射 PIC, 加载 IDTR。
 */
void idt_init(void)
{
    int i;

    /* 初始化处理程序表 */
    for (i = 0; i < IDT_ENTRIES; i++)
        handlers[i] = isr_default;

    /* 重映射 PIC */
    pic_init();

    /* 初始化 IDT 条目 */
    for (i = 0; i < IDT_ENTRIES; i++)
        idt_set_entry(i, (uint32_t)isr_default, 0x08, 0x8E);

    /* 注册键盘中断 (IRQ1 → INT 0x21) */
    idt_set_gate(0x21, isr_keyboard);

    /* 注册鼠标中断 (IRQ12 → INT 0x2C) */
    idt_set_gate(0x2C, isr_mouse);

    /* 设置 IDT 描述符 */
    idt_desc.limit = (uint16_t)(sizeof(idt) - 1);
    idt_desc.base  = (uint32_t)&idt;

    /* 加载 IDTR */
    idt_load(&idt_desc);
}

/*
 * idt_set_gate — 设置中断门 (DPL=0)
 * @vector:  中断向量号
 * @handler: 处理程序地址
 */
void idt_set_gate(uint8_t vector, interrupt_handler_t handler)
{
    idt_set_entry(vector, (uint32_t)handler, 0x08, 0x8E);
    handlers[vector] = handler;
}

/*
 * idt_set_gate_dpl — 设置指定 DPL 的中断门
 * @vector:  中断向量号
 * @handler: 处理程序地址
 * @dpl:     描述符特权级 (0-3)
 *
 * 用于系统调用门 (DPL=3), 允许 Ring 3 代码通过 INT 指令触发。
 * flags = 0x8E | (DPL << 5):
 *   bit 7:   P=1 (present)
 *   bit 6-5: DPL
 *   bit 4:   0 (system gate)
 *   bit 3-0: 0xE (32-bit interrupt gate)
 */
void idt_set_gate_dpl(uint8_t vector, interrupt_handler_t handler, uint8_t dpl)
{
    uint8_t flags = 0x8E | ((dpl & 0x03) << 5);
    idt_set_entry(vector, (uint32_t)handler, 0x08, flags);
    handlers[vector] = handler;
}

/*
 * interrupt_register — 注册中断处理程序
 * @vector:  中断向量号
 * @handler: 处理程序函数指针
 */
void interrupt_register(uint8_t vector, interrupt_handler_t handler)
{
    idt_set_gate(vector, handler);
}

/*
 * pic_init — 初始化 8259A 可编程中断控制器
 * 重映射 IRQ 0-7 → INT 0x20-0x27, IRQ 8-15 → INT 0x28-0x2F
 */
void pic_init(void)
{
    __asm__ __volatile__(
        /* ICW1: 初始化, 需要 ICW4 */
        "mov $0x11, %%al\n\t"
        "out %%al, $0x20\n\t"
        "out %%al, $0xA0\n\t"

        /* ICW2: 中断向量偏移 */
        "mov $0x20, %%al\n\t"
        "out %%al, $0x21\n\t"
        "mov $0x28, %%al\n\t"
        "out %%al, $0xA1\n\t"

        /* ICW3: 级联设置 */
        "mov $0x04, %%al\n\t"
        "out %%al, $0x21\n\t"
        "mov $0x02, %%al\n\t"
        "out %%al, $0xA1\n\t"

        /* ICW4: 8086 模式 */
        "mov $0x01, %%al\n\t"
        "out %%al, $0x21\n\t"
        "out %%al, $0xA1\n\t"

        /* 屏蔽除定时器、键盘、从片级联、软盘外的所有主片中断
         * 0xB8 = 10111000: IRQ0(定时器) IRQ1(键盘) IRQ2(从片级联) IRQ6(FDC) 启用
         * IRQ2 必须启用才能使从片 (IRQ8-15) 中断通过
         * 从片: 0xEF = 11101111: 启用 IRQ12(鼠标) */
        "mov $0xB9, %%al\n\t"
        "out %%al, $0x21\n\t"
        "mov $0xEF, %%al\n\t"
        "out %%al, $0xA1\n\t"

        :
        :
        : "ax", "memory"
    );
}

/*
 * pic_eoi — 发送中断结束信号 (EOI)
 * @irq: IRQ 号 (0-15)
 */
void pic_eoi(uint8_t irq)
{
    if (irq >= 8) {
        __asm__ __volatile__(
            "outb %b0, $0xA0\n\t"
            :
            : "a"((uint8_t)0x20)
            : "memory"
        );
    }
    __asm__ __volatile__(
        "outb %b0, $0x20\n\t"
        :
        : "a"((uint8_t)0x20)
        : "memory"
    );
}
