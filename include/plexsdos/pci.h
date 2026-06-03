/*
 * Nexsteaduser — PlexsDOS
 * PCI 总线接口
 * 作者: Tinmc189623 | 团队: Nexlyh
 *
 * 扫描 PCI 总线, 枚举所有设备。
 * 提供设备表和查询接口。
 */

#ifndef _PLXSDOS_PCI_H
#define _PLXSDOS_PCI_H

#include <plexsdos/types.h>

#ifdef __cplusplus
extern "C" {
#endif

/* PCI 配置空间端口 */
#define PCI_CONFIG_ADDR     0xCF8
#define PCI_CONFIG_DATA     0xCFC

/* PCI 设备类码 */
#define PCI_CLASS_MASS_STORAGE  0x01
#define PCI_SUBCLASS_IDE        0x01
#define PCI_SUBCLASS_SATA       0x06   /* 串行 ATA (AHCI) */
#define PCI_SUBCLASS_NVME       0x08   /* NVM Express */

#define PCI_CLASS_NETWORK       0x02
#define PCI_CLASS_DISPLAY       0x03
#define PCI_CLASS_MULTIMEDIA    0x04
#define PCI_CLASS_BRIDGE        0x06
#define PCI_SUBCLASS_PCI_BRIDGE 0x04

/* PCI 配置寄存器偏移 */
#define PCI_VENDOR_ID       0x00
#define PCI_DEVICE_ID       0x02
#define PCI_COMMAND         0x04
#define PCI_STATUS          0x06
#define PCI_CLASS_CODE      0x08
#define PCI_HEADER_TYPE     0x0E
#define PCI_BAR0            0x10
#define PCI_BAR4            0x20
#define PCI_INTERRUPT_LINE  0x3C

/* PCI 命令寄存器位 */
#define PCI_CMD_IO_ENABLE       (1 << 0)
#define PCI_CMD_MEM_ENABLE      (1 << 1)
#define PCI_CMD_BUS_MASTER      (1 << 2)

/* PCI 设备最大数量 */
#define PCI_MAX_DEVICES     64

/* PCI 设备结构 */
struct pci_device {
    uint8_t  bus;
    uint8_t  slot;
    uint8_t  func;
    uint16_t vendor_id;
    uint16_t device_id;
    uint8_t  class_code;
    uint8_t  subclass;
    uint8_t  prog_if;
    uint32_t bar0;
    uint32_t bar4;
    uint8_t  irq;
} __attribute__((packed));

/*
 * pci_init — 初始化 PCI 子系统
 * 扫描所有 PCI 总线, 枚举全部设备到设备表。
 */
void pci_init(void);

/*
 * pci_read_config_byte — 读取 PCI 配置字节
 */
uint8_t pci_read_config_byte(uint8_t bus, uint8_t slot,
                              uint8_t func, uint8_t offset);

/*
 * pci_read_config_word — 读取 PCI 配置字 (16-bit)
 */
uint16_t pci_read_config_word(uint8_t bus, uint8_t slot,
                               uint8_t func, uint8_t offset);

/*
 * pci_read_config_dword — 读取 PCI 配置双字 (32-bit)
 */
uint32_t pci_read_config_dword(uint8_t bus, uint8_t slot,
                                uint8_t func, uint8_t offset);

/*
 * pci_write_config_dword — 写入 PCI 配置双字
 */
void pci_write_config_dword(uint8_t bus, uint8_t slot,
                             uint8_t func, uint8_t offset, uint32_t val);

/*
 * pci_enable_bus_master — 启用 PCI 总线主控
 * DMA 传输需要此功能。
 */
void pci_enable_bus_master(struct pci_device *dev);

/*
 * pci_get_ide_controller — 获取 IDE 控制器信息
 * 返回: IDE 控制器 PCI 设备信息, 未找到返回 NULL。
 */
struct pci_device *pci_get_ide_controller(void);

/*
 * pci_device_count — 获取已发现的 PCI 设备总数
 * 返回: PCI 设备数量。
 */
int pci_device_count(void);

/*
 * pci_get_device — 获取指定索引的 PCI 设备
 * @index: 设备索引 (0 ~ count-1)
 * 返回: PCI 设备结构指针, 无效索引返回 NULL。
 */
struct pci_device *pci_get_device(int index);

#ifdef __cplusplus
}
#endif

#endif /* _PLXSDOS_PCI_H */
