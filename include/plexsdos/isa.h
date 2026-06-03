/*
 * Nexsteaduser — PlexsDOS
 * ISA 设备表 — 传统 ISA/legacy 设备枚举
 * 作者: Tinmc189623 | 团队: Nexlyh
 *
 * 维护已知 ISA 传统设备表 (键盘, FDC, COM, LPT, PIT, VGA 等)。
 * 这些设备无法通过 PCI 总线枚举, 使用固定的 I/O 地址和 IRQ。
 */

#ifndef _PLXSDOS_ISA_H
#define _PLXSDOS_ISA_H

#include <plexsdos/types.h>

#ifdef __cplusplus
extern "C" {
#endif

/* ISA 设备最大数量 */
#define ISA_MAX_DEVICES     16

/* ISA 设备结构 */
struct isa_device {
    uint16_t io_base;         /* I/O 基地址 */
    uint8_t  irq;             /* IRQ 号 (0=无) */
    uint8_t  dma_chan;        /* DMA 通道 (0xFF=无) */
    char     name[20];        /* 设备名称 */
} __attribute__((packed));

/*
 * isa_init — 初始化 ISA 设备表
 * 填充已知 ISA 传统设备。
 */
void isa_init(void);

/*
 * isa_device_count — 获取 ISA 设备数量
 * 返回: ISA 设备数量。
 */
int isa_device_count(void);

/*
 * isa_get_device — 获取指定索引的 ISA 设备
 * @index: 设备索引 (0 ~ count-1)
 * 返回: ISA 设备结构指针, 无效索引返回 NULL。
 */
struct isa_device *isa_get_device(int index);

#ifdef __cplusplus
}
#endif

#endif /* _PLXSDOS_ISA_H */
