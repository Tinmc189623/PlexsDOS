/*
 * Nexsteaduser — PlexsDOS
 * ATA 磁盘驱动接口 (PIO + DMA/UDMA)
 * 作者: Tinmc189623 | 团队: Nexlyh
 */

#ifndef _PLXSDOS_DISK_H
#define _PLXSDOS_DISK_H

#include <plexsdos/types.h>

#ifdef __cplusplus
extern "C" {
#endif

/* ATA 主通道 I/O 端口 */
#define ATA_DATA         0x1F0
#define ATA_ERROR        0x1F1
#define ATA_SECTOR_COUNT 0x1F2
#define ATA_LBA_LO       0x1F3
#define ATA_LBA_MID      0x1F4
#define ATA_LBA_HI       0x1F5
#define ATA_DRIVE_HEAD   0x1F6
#define ATA_STATUS       0x1F7
#define ATA_COMMAND      0x1F7
#define ATA_ALT_STATUS   0x3F6
#define ATA_DEV_CTRL     0x3F6

/* ATA 命令 */
#define ATA_CMD_READ_PIO      0x20
#define ATA_CMD_READ_PIO_EXT  0x24
#define ATA_CMD_READ_DMA      0xC8
#define ATA_CMD_READ_DMA_EXT  0x25
#define ATA_CMD_WRITE_PIO     0x30
#define ATA_CMD_WRITE_PIO_EXT 0x34
#define ATA_CMD_WRITE_DMA     0xCA
#define ATA_CMD_WRITE_DMA_EXT 0x35
#define ATA_CMD_CACHE_FLUSH   0xE7
#define ATA_CMD_IDENTIFY      0xEC

/* ATA 状态位 */
#define ATA_STATUS_ERR  0x01
#define ATA_STATUS_DRQ  0x08
#define ATA_STATUS_BSY  0x80
#define ATA_STATUS_RDY  0x40
#define ATA_STATUS_DF   0x20

/* Bus Master IDE 寄存器偏移 (BAR4 基址) */
#define BM_COMMAND      0x00    /* 命令寄存器 */
#define BM_STATUS       0x02    /* 状态寄存器 */
#define BM_PRDT_ADDR    0x04    /* PRDT 地址寄存器 */

/* Bus Master 命令位 */
#define BM_CMD_START    0x01    /* 启动 DMA */
#define BM_CMD_READ     0x08    /* 读取方向 (设备→内存) */

/* Bus Master 状态位 */
#define BM_STATUS_ACTIVE    0x01
#define BM_STATUS_ERROR     0x02
#define BM_STATUS_IRQ       0x04
#define BM_STATUS_DRIVE0    0x20
#define BM_STATUS_DRIVE1    0x40

/* DMA 传输模式 */
#define DMA_MODE_PIO    0
#define DMA_MODE_DMA    1

/* 物理区域描述符 (PRD) 结构 */
struct prd_entry {
    uint32_t phys_addr;     /* 物理地址 */
    uint16_t byte_count;    /* 字节数 (0 = 65536) */
    uint16_t flags;         /* 标志 (bit 15 = EOT) */
} __attribute__((packed));

/* PRD 标志 */
#define PRD_EOT     (1 << 15)   /* End of Transfer */

/*
 * disk_init — 初始化 ATA 磁盘
 * 检测磁盘, 设置 DMA 传输模式。
 * 返回: TRUE = 磁盘就绪, FALSE = 磁盘不可用。
 */
bool disk_init(void);

/*
 * disk_read_sectors — 从 LBA 扇区号读取 count 个扇区到 buf
 * 优先使用 DMA 模式, 回退到 PIO 模式。
 * 返回: TRUE = 成功, FALSE = 失败。
 */
bool disk_read_sectors(uint32_t lba, uint8_t count, void *buf);

/*
 * disk_write_sectors — 将 buf 中的数据写入 LBA 扇区号起始的 count 个扇区
 * 使用 PIO 模式写入。
 * 返回: TRUE = 成功, FALSE = 失败。
 */
bool disk_write_sectors(uint32_t lba, uint8_t count, const void *buf);

/*
 * disk_set_dma_mode — 设置 DMA 传输模式
 * @mode: DMA_MODE_PIO 或 DMA_MODE_DMA
 */
void disk_set_dma_mode(uint8_t mode);

/*
 * disk_get_mode — 获取当前传输模式
 * 返回: DMA_MODE_PIO 或 DMA_MODE_DMA
 */
uint8_t disk_get_mode(void);

#ifdef __cplusplus
}
#endif

#endif /* _PLXSDOS_DISK_H */
