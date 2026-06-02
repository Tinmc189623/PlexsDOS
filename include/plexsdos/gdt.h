/*
 * Nexsteaduser — PlexsDOS
 * gdt.h — GDT 和 TSS 初始化接口
 * 作者: Tinmc189623 | 团队: Nexlyh
 *
 * 提供 Ring 0-3 段描述符和 TSS 初始化。
 */

#ifndef _PLXSDOS_GDT_H
#define _PLXSDOS_GDT_H

#include <plexsdos/types.h>

#ifdef __cplusplus
extern "C" {
#endif

/* GDT 选择子 */
#define GDT_SEL_KERNEL_CODE  0x08   /* Ring 0 代码段 */
#define GDT_SEL_KERNEL_DATA  0x10   /* Ring 0 数据段 */
#define GDT_SEL_RING1_CODE   0x18   /* Ring 1 代码段 */
#define GDT_SEL_RING1_DATA   0x20   /* Ring 1 数据段 */
#define GDT_SEL_RING2_CODE   0x28   /* Ring 2 代码段 */
#define GDT_SEL_RING2_DATA   0x30   /* Ring 2 数据段 */
#define GDT_SEL_USER_CODE    0x38   /* Ring 3 代码段 */
#define GDT_SEL_USER_DATA    0x40   /* Ring 3 数据段 */
#define GDT_SEL_TSS          0x48   /* TSS 段 */

/*
 * gdt_init — 初始化 GDT 和 TSS
 *
 * 加载包含 Ring 0-3 段描述符的新 GDT，
 * 初始化 TSS (ESP0=中断栈顶, SS0=内核数据段)，
 * 执行 ltr 加载任务寄存器。
 */
void gdt_init(void);

#ifdef __cplusplus
}
#endif

#endif /* _PLXSDOS_GDT_H */
