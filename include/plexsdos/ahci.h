/*
 * Nexsteaduser — PlexsDOS
 * AHCI SATA 驱动接口
 * 作者: Tinmc189623 | 团队: Nexlyh
 *
 * 通过 PCI BAR5 (ABAR) 访问 AHCI HBA 寄存器,
 * 支持 48-bit LBA DMA 传输, 兼容 Q35 等 AHCI 控制器。
 */

#ifndef _PLXSDOS_AHCI_H
#define _PLXSDOS_AHCI_H

#include <plexsdos/types.h>

#ifdef __cplusplus
extern "C" {
#endif

/* AHCI 设备状态 */
#define AHCI_DEV_NONE       0   /* 无设备 */
#define AHCI_DEV_SATA       1   /* SATA 设备 */
#define AHCI_DEV_SATAPI     2   /* SATAPI (光驱) */
#define AHCI_DEV_SEMB       3   /* SEMB */
#define AHCI_DEV_PM         4   /* Port Multiplier */

/*
 * ahci_init — 初始化 AHCI 控制器
 * 扫描 PCI 查找 AHCI 设备, 初始化所有 SATA 端口。
 * 返回: true = AHCI 就绪且有磁盘设备, false = 无 AHCI。
 */
bool ahci_init(void);

/*
 * ahci_device_count — 获取 AHCI 检测到的磁盘数
 * 返回: SATA 磁盘数量。
 */
int ahci_device_count(void);

/*
 * ahci_read_sectors — 从 AHCI 磁盘读取扇区
 * @dev:   设备索引 (0 ~ count-1)
 * @lba:   起始 LBA (48-bit, 高 16 位通过参数传递)
 * @count: 扇区数 (最大 255)
 * @buf:   目标缓冲区
 * 返回: true = 成功。
 */
bool ahci_read_sectors(int dev, uint64_t lba, uint8_t count, void *buf);

/*
 * ahci_write_sectors — 向 AHCI 磁盘写入扇区
 * @dev:   设备索引
 * @lba:   起始 LBA (48-bit)
 * @count: 扇区数
 * @buf:   源数据缓冲区
 * 返回: true = 成功。
 */
bool ahci_write_sectors(int dev, uint64_t lba, uint8_t count, const void *buf);

#ifdef __cplusplus
}
#endif

#endif /* _PLXSDOS_AHCI_H */
