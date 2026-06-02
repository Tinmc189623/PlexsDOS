/*
 * Nexsteaduser — PlexsDOS
 * PCI 总线接口实现
 * 作者: Tinmc189623 | 团队: Nexlyh
 *
 * 扫描 PCI 总线, 查找 IDE 控制器 (类码 0x01, 子类 0x01)。
 * 启用总线主控 (Bus Master) 以支持 ATA DMA 传输。
 */

#include <plexsdos/types.h>
#include <plexsdos/pci.h>
#include <plexsdos/screen.h>

/* 找到的 IDE 控制器 */
static struct pci_device ide_controller;
static bool ide_found = false;

/*
 * pci_read_config_dword — 读取 PCI 配置双字
 * @bus: 总线号
 * @slot: 设备号
 * @func: 功能号
 * @offset: 配置寄存器偏移
 *
 * 通过 I/O 端口 0xCF8/0xCFC 访问 PCI 配置空间。
 */
uint32_t pci_read_config_dword(uint8_t bus, uint8_t slot,
                                uint8_t func, uint8_t offset)
{
    uint32_t addr = (uint32_t)(
        (1 << 31) |                         /* 配置空间使能 */
        ((uint32_t)bus << 16) |
        ((uint32_t)(slot & 0x1F) << 11) |
        ((uint32_t)(func & 0x07) << 8) |
        (offset & 0xFC)                     /* 对齐到 4 字节 */
    );

    uint32_t val;
    uint16_t config_addr = 0xCF8;
    uint16_t config_data = 0xCFC;

    __asm__ __volatile__(
        "outl %%eax, %%dx"
        :
        : "a"(addr), "d"(config_addr)
    );

    __asm__ __volatile__(
        "inl %%dx, %%eax"
        : "=a"(val)
        : "d"(config_data)
    );

    return val;
}

/*
 * pci_read_config_word — 读取 PCI 配置字 (16-bit)
 */
uint16_t pci_read_config_word(uint8_t bus, uint8_t slot,
                               uint8_t func, uint8_t offset)
{
    uint32_t val = pci_read_config_dword(bus, slot, func, offset);
    return (uint16_t)((val >> ((offset & 2) * 8)) & 0xFFFF);
}

/*
 * pci_read_config_byte — 读取 PCI 配置字节
 */
uint8_t pci_read_config_byte(uint8_t bus, uint8_t slot,
                              uint8_t func, uint8_t offset)
{
    uint32_t val = pci_read_config_dword(bus, slot, func, offset);
    return (uint8_t)((val >> ((offset & 3) * 8)) & 0xFF);
}

/*
 * pci_write_config_dword — 写入 PCI 配置双字
 */
void pci_write_config_dword(uint8_t bus, uint8_t slot,
                             uint8_t func, uint8_t offset, uint32_t val)
{
    uint32_t addr = (uint32_t)(
        (1 << 31) |
        ((uint32_t)bus << 16) |
        ((uint32_t)(slot & 0x1F) << 11) |
        ((uint32_t)(func & 0x07) << 8) |
        (offset & 0xFC)
    );

    uint16_t config_addr = 0xCF8;
    uint16_t config_data = 0xCFC;

    __asm__ __volatile__(
        "outl %%eax, %%dx"
        :
        : "a"(addr), "d"(config_addr)
    );

    __asm__ __volatile__(
        "outl %%eax, %%dx"
        :
        : "a"(val), "d"(config_data)
    );
}

/*
 * pci_enable_bus_master — 启用 PCI 总线主控
 * @dev: PCI 设备结构
 *
 * 设置 PCI Command 寄存器的 Bus Master 位,
 * 允许设备发起 DMA 传输。
 */
void pci_enable_bus_master(struct pci_device *dev)
{
    uint16_t cmd = pci_read_config_word(dev->bus, dev->slot,
                                         dev->func, PCI_COMMAND);
    cmd |= PCI_CMD_IO_ENABLE | PCI_CMD_BUS_MASTER;
    pci_write_config_dword(dev->bus, dev->slot, dev->func,
                           PCI_COMMAND, (uint32_t)cmd);
}

/*
 * pci_scan_device — 扫描单个 PCI 设备
 * @bus: 总线号
 * @slot: 设备号
 *
 * 检查设备是否存在, 如果是 IDE 控制器则记录信息。
 */
static void pci_scan_device(uint8_t bus, uint8_t slot)
{
    uint16_t vendor = pci_read_config_word(bus, slot, 0, PCI_VENDOR_ID);
    uint32_t class_reg;
    uint8_t class_code, subclass, prog_if;

    /* 0xFFFF 表示设备不存在 */
    if (vendor == 0xFFFF)
        return;

    /* 读取类码 */
    class_reg = pci_read_config_dword(bus, slot, 0, PCI_CLASS_CODE);
    class_code = (uint8_t)((class_reg >> 24) & 0xFF);
    subclass   = (uint8_t)((class_reg >> 16) & 0xFF);
    prog_if    = (uint8_t)((class_reg >> 8) & 0xFF);

    /* 检查是否为 IDE 控制器 (类码 0x01, 子类 0x01) */
    if (class_code == PCI_CLASS_MASS_STORAGE &&
        subclass == PCI_SUBCLASS_IDE) {
        ide_controller.bus       = bus;
        ide_controller.slot      = slot;
        ide_controller.func      = 0;
        ide_controller.vendor_id = vendor;
        ide_controller.device_id = pci_read_config_word(bus, slot, 0, 0x02);
        ide_controller.class_code = class_code;
        ide_controller.subclass  = subclass;
        ide_controller.prog_if   = prog_if;
        ide_controller.bar0      = pci_read_config_dword(bus, slot, 0, PCI_BAR0);
        ide_controller.bar4      = pci_read_config_dword(bus, slot, 0, PCI_BAR4);
        ide_controller.irq       = pci_read_config_byte(bus, slot, 0, PCI_INTERRUPT_LINE);
        ide_found = true;
    }
}

/*
 * pci_init — 初始化 PCI 子系统
 *
 * 扫描所有 PCI 总线和设备, 查找 IDE 控制器。
 * 找到后启用总线主控以支持 DMA。
 */
void pci_init(void)
{
    uint16_t bus, slot;

    /* 扫描所有总线和设备 */
    for (bus = 0; bus < 256; bus++) {
        for (slot = 0; slot < 32; slot++) {
            pci_scan_device((uint8_t)bus, (uint8_t)slot);
        }
    }

    if (ide_found) {
        screen_puts("[pci] IDE controller found\n");

        /* 启用总线主控 (DMA 需要) */
        pci_enable_bus_master(&ide_controller);

        screen_puts("[pci] Bus Master enabled\n");
    } else {
        screen_puts("[pci] No IDE controller found\n");
    }
}

/*
 * pci_get_ide_controller — 获取 IDE 控制器信息
 * 返回: IDE 控制器 PCI 设备信息, 未找到返回 NULL。
 */
struct pci_device *pci_get_ide_controller(void)
{
    return ide_found ? &ide_controller : NULL;
}
