/*
 * Nexsteaduser — PlexsDOS
 * gdt.c — GDT 和 TSS 初始化
 * 作者: Tinmc189623 | 团队: Nexlyh
 *
 * 设置 TSS 的 ESP0/SS0 字段，填充 TSS 描述符的 base 地址，
 * 调用汇编 _gdt_load 加载 GDT 并执行 ltr。
 */

#include <plexsdos/gdt.h>
#include <plexsdos/config.h>
#include <plexsdos/types.h>

/* 外部汇编符号 (PE/COFF 编译器自动加 _ 前缀, 这里用原始名) */
extern uint8_t tss[];          /* TSS 数据 (在 gdt_load.S 中定义为 _tss) */
extern uint8_t gdt[];          /* GDT 数据 (在 gdt_load.S 中定义为 _gdt) */
extern void gdt_load(void);    /* 加载 GDT + ltr (在 gdt_load.S 中定义为 _gdt_load) */

/*
 * tss_set_base — 设置 TSS 描述符的基地址
 * @base: TSS 的物理地址
 *
 * TSS 描述符位于 GDT 偏移 0x48 (索引 9)。
 * 描述符格式:
 *   +0: limit 低 16 位
 *   +2: base 低 16 位
 *   +4: base 中 8 位
 *   +5: access byte
 *   +6: granularity + limit 高 4 位
 *   +7: base 高 8 位
 */
static void tss_set_base(uint32_t base)
{
    uint8_t *tss_desc = gdt + 0x48;

    /* base 低 16 位 */
    tss_desc[2] = (uint8_t)(base & 0xFF);
    tss_desc[3] = (uint8_t)((base >> 8) & 0xFF);

    /* base 中 8 位 */
    tss_desc[4] = (uint8_t)((base >> 16) & 0xFF);

    /* base 高 8 位 */
    tss_desc[7] = (uint8_t)((base >> 24) & 0xFF);
}

/*
 * gdt_init — 初始化 GDT 和 TSS
 *
 * 1. 设置 TSS 的 ESP0 (Ring 0 中断栈顶) 和 SS0 (Ring 0 数据段)
 * 2. 设置 TSS 的 I/O 位图基址 (0x68 = 无 I/O 位图)
 * 3. 填充 GDT 中 TSS 描述符的 base 字段
 * 4. 调用 _gdt_load 加载 GDT 并执行 ltr
 */
void gdt_init(void)
{
    /* 设置 TSS 关键字段 */
    uint32_t *tss32 = (uint32_t *)tss;

    /* ESP0: Ring 3 → Ring 0 时 CPU 自动切换到此栈 */
    tss32[1] = INTERRUPT_STACK_TOP;  /* offset 0x04 */

    /* SS0: Ring 0 数据段选择子 */
    tss32[2] = GDT_SEL_KERNEL_DATA;  /* offset 0x08 */

    /* I/O 位图基址: 0x68 表示无 I/O 位图
     * (TSS 基本大小 = 104 字节 = 0x68) */
    {
        uint16_t *tss16 = (uint16_t *)tss;
        tss16[50] = 0x68;  /* offset 0x64: I/O map base */
    }

    /* 填充 TSS 描述符的 base 地址 */
    tss_set_base((uint32_t)tss);

    /* 加载 GDT + ltr (汇编函数) */
    gdt_load();
}

/*
 * tss_set_esp0 — 更新 TSS 的 ESP0 字段
 * @esp0: Ring 0 栈顶地址
 *
 * 进程切换时调用, 设置 CPU 从 Ring 3 进入 Ring 0 (中断/系统调用)
 * 时自动切换到的内核栈指针。
 */
void tss_set_esp0(uint32_t esp0)
{
    uint32_t *tss32 = (uint32_t *)tss;
    tss32[1] = esp0;  /* TSS offset 0x04 = ESP0 */
}
