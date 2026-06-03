/*
 * Nexsteaduser — PlexsDOS
 * PCI 总线接口实现
 * 作者: Tinmc189623 | 团队: Nexlyh
 *
 * 扫描 256 条 PCI 总线 × 32 个设备, 枚举全部 PCI 设备。
 * 维护全局设备表, 支持按索引查询。
 */

#include <plexsdos/types.h>
#include <plexsdos/pci.h>
#include <plexsdos/screen.h>
#include <plexsdos/serial.h>

/* PCI 设备表 */
static struct pci_device pci_devices[PCI_MAX_DEVICES];
static int pci_device_count_val = 0;

/* 找到的 IDE 控制器索引 */
static int ide_index = -1;

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
 * pci_add_device — 将发现的 PCI 设备加入设备表
 * @bus:   总线号
 * @slot:  设备号
 * @func:  功能号
 * @vendor: 厂商 ID
 *
 * 填充 pci_device 结构并加入静态表。超过 PCI_MAX_DEVICES 时丢弃。
 */
static void pci_add_device(uint8_t bus, uint8_t slot, uint8_t func,
                            uint16_t vendor, uint16_t device,
                            uint8_t class_code, uint8_t subclass,
                            uint8_t prog_if)
{
    if (pci_device_count_val >= PCI_MAX_DEVICES)
        return;

    struct pci_device *dev = &pci_devices[pci_device_count_val++];
    dev->bus        = bus;
    dev->slot       = slot;
    dev->func       = func;
    dev->vendor_id  = vendor;
    dev->device_id  = device;
    dev->class_code = class_code;
    dev->subclass   = subclass;
    dev->prog_if    = prog_if;

    if (func == 0) {
        dev->bar0 = pci_read_config_dword(bus, slot, func, PCI_BAR0);
        dev->bar4 = pci_read_config_dword(bus, slot, func, PCI_BAR4);
        dev->irq  = pci_read_config_byte(bus, slot, func, PCI_INTERRUPT_LINE);
    } else {
        dev->bar0 = 0;
        dev->bar4 = 0;
        dev->irq  = 0;
    }

    /* 记录 IDE 控制器索引 */
    if (class_code == PCI_CLASS_MASS_STORAGE &&
        subclass == PCI_SUBCLASS_IDE && ide_index < 0) {
        ide_index = pci_device_count_val - 1;
    }
}

/*
 * pci_scan_device — 扫描单个 PCI 设备
 * @bus:  总线号
 * @slot: 设备号
 *
 * 检查设备是否存在, 枚举所有功能。
 */
static void pci_scan_device(uint8_t bus, uint8_t slot)
{
    uint16_t vendor = pci_read_config_word(bus, slot, 0, PCI_VENDOR_ID);

    /* 0xFFFF 表示设备不存在 */
    if (vendor == 0xFFFF)
        return;

    uint8_t header_type = pci_read_config_byte(bus, slot, 0, PCI_HEADER_TYPE);
    uint8_t max_func = (header_type & 0x80) ? 8 : 1;
    uint32_t class_reg;

    for (uint8_t func = 0; func < max_func; func++) {
        if (func > 0) {
            vendor = pci_read_config_word(bus, slot, func, PCI_VENDOR_ID);
            if (vendor == 0xFFFF)
                continue;
        }

        uint16_t device = pci_read_config_word(bus, slot, func, 0x02);
        class_reg = pci_read_config_dword(bus, slot, func, PCI_CLASS_CODE);

        uint8_t class_code = (uint8_t)((class_reg >> 24) & 0xFF);
        uint8_t subclass   = (uint8_t)((class_reg >> 16) & 0xFF);
        uint8_t prog_if    = (uint8_t)((class_reg >> 8) & 0xFF);

        pci_add_device(bus, slot, func, vendor, device,
                       class_code, subclass, prog_if);
    }
}

/*
 * pci_init — 初始化 PCI 子系统
 *
 * 扫描所有 PCI 总线和设备, 填充全局设备表。
 * 找到 IDE 控制器后启用总线主控以支持 DMA。
 */
void pci_init(void)
{
    pci_device_count_val = 0;
    ide_index = -1;

    /* 扫描所有总线和设备 */
    for (uint16_t bus = 0; bus < 256; bus++) {
        for (uint16_t slot = 0; slot < 32; slot++) {
            pci_scan_device((uint8_t)bus, (uint8_t)slot);
        }
    }

    serial_puts("[pci] found devices: ");
    serial_put_hex((uint32_t)pci_device_count_val);
    serial_puts("h\n");

    if (ide_index >= 0) {
        screen_puts("[pci] IDE controller found\n");
        pci_enable_bus_master(&pci_devices[ide_index]);
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
    return (ide_index >= 0) ? &pci_devices[ide_index] : NULL;
}

/*
 * pci_device_count — 获取已发现的 PCI 设备总数
 * 返回: PCI 设备数量。
 */
int pci_device_count(void)
{
    return pci_device_count_val;
}

/*
 * pci_get_device — 获取指定索引的 PCI 设备
 * @index: 设备索引 (0 ~ count-1)
 * 返回: PCI 设备结构指针, 无效索引返回 NULL。
 */
struct pci_device *pci_get_device(int index)
{
    if (index < 0 || index >= pci_device_count_val)
        return NULL;
    return &pci_devices[index];
}
