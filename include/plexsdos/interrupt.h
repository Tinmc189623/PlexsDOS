/*
 * Nexsteaduser — PlexsDOS
 * 中断处理框架接口
 * 作者: Tinmc189623 | 团队: Nexlyh
 */

#ifndef _PLXSDOS_INTERRUPT_H
#define _PLXSDOS_INTERRUPT_H

#include <plexsdos/types.h>

#ifdef __cplusplus
extern "C" {
#endif

/* 中断向量数量 */
#define IDT_ENTRIES 256

/* 中断处理程序函数指针类型 */
typedef void (*interrupt_handler_t)(void);

/*
 * idt_init — 初始化中断描述符表
 * 设置 256 个中断向量, 重映射 PIC, 注册默认处理程序。
 */
void idt_init(void);

/*
 * idt_set_gate — 设置 IDT 门描述符 (DPL=0)
 * @vector:   中断向量号 (0-255)
 * @handler:  处理程序地址
 */
void idt_set_gate(uint8_t vector, interrupt_handler_t handler);

/*
 * idt_set_gate_dpl — 设置指定 DPL 的 IDT 门描述符
 * @vector:   中断向量号 (0-255)
 * @handler:  处理程序地址
 * @dpl:      描述符特权级 (0-3)
 *
 * DPL=3 允许 Ring 3 代码通过 INT 指令触发该中断。
 */
void idt_set_gate_dpl(uint8_t vector, interrupt_handler_t handler, uint8_t dpl);

/*
 * interrupt_register — 注册中断处理程序
 * @vector:  中断向量号
 * @handler: 处理程序函数指针
 */
void interrupt_register(uint8_t vector, interrupt_handler_t handler);

/*
 * pic_init — 初始化可编程中断控制器 (8259A)
 * 主 PIC: IRQ 0-7  → 中断向量 0x20-0x27
 * 从 PIC: IRQ 8-15 → 中断向量 0x28-0x2F
 */
void pic_init(void);

/*
 * pic_eoi — 发送中断结束信号 (EOI)
 * @irq: IRQ 号 (0-15)
 */
void pic_eoi(uint8_t irq);

#ifdef __cplusplus
}
#endif

#endif /* _PLXSDOS_INTERRUPT_H */
