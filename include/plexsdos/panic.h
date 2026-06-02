/*
 * Nexsteaduser — PlexsDOS
 * 红屏 (Red Screen of Death) 接口
 * 作者: Tinmc189623 | 团队: Nexlyh
 *
 * 内核恐慌诊断屏幕, 类似 Windows BSOD。
 * 不可恢复错误时显示 CPU 寄存器、栈内容和回溯信息。
 */

#ifndef _PLXSDOS_PANIC_H
#define _PLXSDOS_PANIC_H

#include <plexsdos/types.h>

#ifdef __cplusplus
extern "C" {
#endif

/* 寄存器快照结构 — 由汇编桩在异常入口保存 */
struct panic_regs {
    uint32_t gs, fs, es, ds;
    uint32_t edi, esi, ebp, esp_orig;
    uint32_t ebx, edx, ecx, eax;
    uint32_t eip, cs, eflags;
    uint32_t esp, ss;
};

/*
 * panic_init — 注册 CPU 异常处理程序
 *
 * 将 INT 0x00-0x1F 中的关键异常注册到 IDT:
 * #DE(0x00), #DB(0x01), #BP(0x03), #UD(0x06),
 * #DF(0x08), #TS(0x0A), #NP(0x0B), #SS(0x0C),
 * #GP(0x0D), #PF(0x0E), #MF(0x10), #AC(0x11), #MC(0x12)
 *
 * 必须在 idt_init() 之后调用。
 */
void panic_init(void);

/*
 * kernel_panic — 触发内核恐慌
 * @fmt: 格式化字符串 (支持 %s, %d, %x, %p)
 *
 * 显示红屏诊断信息后进入死循环 (cli; hlt; jmp .)。
 * 同时输出到串口便于远程调试。
 */
_Noreturn void kernel_panic(const char *fmt, ...);

/*
 * panic_exception_handler — CPU 异常处理程序 (由汇编桩调用)
 * @vector:     异常向量号
 * @error_code: 错误码 (部分异常无错误码, 传 0)
 * @regs:       保存的寄存器快照
 */
void panic_exception_handler(uint32_t vector, uint32_t error_code,
                             struct panic_regs *regs);

#ifdef __cplusplus
}
#endif

#endif /* _PLXSDOS_PANIC_H */
