/*
 * Nexsteaduser — PlexsDOS
 * ISA 设备表实现
 * 作者: Tinmc189623 | 团队: Nexlyh
 *
 * 填充已知 ISA 传统设备表。
 * 这些设备在 x86 PC 中始终存在, 通过固定 I/O 地址访问。
 */

#include <plexsdos/isa.h>

/* ISA 设备表 */
static struct isa_device isa_devices[ISA_MAX_DEVICES];
static int isa_device_count_val = 0;

/*
 * isa_init — 初始化 ISA 设备表
 *
 * 注册 x86 平台所有已知 ISA 传统设备。
 */
void isa_init(void)
{
    isa_device_count_val = 0;

    /* 8253/8254 PIT (Programmable Interval Timer) */
    isa_devices[isa_device_count_val].io_base  = 0x40;
    isa_devices[isa_device_count_val].irq      = 0;
    isa_devices[isa_device_count_val].dma_chan = 0xFF;
    /* name */ {
        int i;
        const char *s = "8253 PIT";
        for (i = 0; s[i] && i < 19; i++)
            isa_devices[isa_device_count_val].name[i] = s[i];
        isa_devices[isa_device_count_val].name[i] = '\0';
    }
    isa_device_count_val++;

    /* 8259A PIC Master (Programmable Interrupt Controller) */
    isa_devices[isa_device_count_val].io_base  = 0x20;
    isa_devices[isa_device_count_val].irq      = 0xFF;
    isa_devices[isa_device_count_val].dma_chan = 0xFF;
    {
        int i;
        const char *s = "8259A PIC Master";
        for (i = 0; s[i] && i < 19; i++)
            isa_devices[isa_device_count_val].name[i] = s[i];
        isa_devices[isa_device_count_val].name[i] = '\0';
    }
    isa_device_count_val++;

    /* 8259A PIC Slave */
    isa_devices[isa_device_count_val].io_base  = 0xA0;
    isa_devices[isa_device_count_val].irq      = 0xFF;
    isa_devices[isa_device_count_val].dma_chan = 0xFF;
    {
        int i;
        const char *s = "8259A PIC Slave";
        for (i = 0; s[i] && i < 19; i++)
            isa_devices[isa_device_count_val].name[i] = s[i];
        isa_devices[isa_device_count_val].name[i] = '\0';
    }
    isa_device_count_val++;

    /* 8042 Keyboard Controller */
    isa_devices[isa_device_count_val].io_base  = 0x60;
    isa_devices[isa_device_count_val].irq      = 1;
    isa_devices[isa_device_count_val].dma_chan = 0xFF;
    {
        int i;
        const char *s = "8042 Keyboard";
        for (i = 0; s[i] && i < 19; i++)
            isa_devices[isa_device_count_val].name[i] = s[i];
        isa_devices[isa_device_count_val].name[i] = '\0';
    }
    isa_device_count_val++;

    /* NEC 765 FDC (Floppy Disk Controller) */
    isa_devices[isa_device_count_val].io_base  = 0x3F0;
    isa_devices[isa_device_count_val].irq      = 6;
    isa_devices[isa_device_count_val].dma_chan = 2;
    {
        int i;
        const char *s = "NEC 765 FDC";
        for (i = 0; s[i] && i < 19; i++)
            isa_devices[isa_device_count_val].name[i] = s[i];
        isa_devices[isa_device_count_val].name[i] = '\0';
    }
    isa_device_count_val++;

    /* 16550 UART COM1 */
    isa_devices[isa_device_count_val].io_base  = 0x3F8;
    isa_devices[isa_device_count_val].irq      = 4;
    isa_devices[isa_device_count_val].dma_chan = 0xFF;
    {
        int i;
        const char *s = "16550 UART COM1";
        for (i = 0; s[i] && i < 19; i++)
            isa_devices[isa_device_count_val].name[i] = s[i];
        isa_devices[isa_device_count_val].name[i] = '\0';
    }
    isa_device_count_val++;

    /* 16550 UART COM2 */
    isa_devices[isa_device_count_val].io_base  = 0x2F8;
    isa_devices[isa_device_count_val].irq      = 3;
    isa_devices[isa_device_count_val].dma_chan = 0xFF;
    {
        int i;
        const char *s = "16550 UART COM2";
        for (i = 0; s[i] && i < 19; i++)
            isa_devices[isa_device_count_val].name[i] = s[i];
        isa_devices[isa_device_count_val].name[i] = '\0';
    }
    isa_device_count_val++;

    /* Parallel Port LPT1 */
    isa_devices[isa_device_count_val].io_base  = 0x378;
    isa_devices[isa_device_count_val].irq      = 7;
    isa_devices[isa_device_count_val].dma_chan = 0xFF;
    {
        int i;
        const char *s = "Parallel LPT1";
        for (i = 0; s[i] && i < 19; i++)
            isa_devices[isa_device_count_val].name[i] = s[i];
        isa_devices[isa_device_count_val].name[i] = '\0';
    }
    isa_device_count_val++;

    /* VGA Controller */
    isa_devices[isa_device_count_val].io_base  = 0x3C0;
    isa_devices[isa_device_count_val].irq      = 0xFF;
    isa_devices[isa_device_count_val].dma_chan = 0xFF;
    {
        int i;
        const char *s = "VGA Compatible";
        for (i = 0; s[i] && i < 19; i++)
            isa_devices[isa_device_count_val].name[i] = s[i];
        isa_devices[isa_device_count_val].name[i] = '\0';
    }
    isa_device_count_val++;

    /* 8237 DMA Controller */
    isa_devices[isa_device_count_val].io_base  = 0x00;
    isa_devices[isa_device_count_val].irq      = 0xFF;
    isa_devices[isa_device_count_val].dma_chan = 0xFF;
    {
        int i;
        const char *s = "8237 DMA Ctrl";
        for (i = 0; s[i] && i < 19; i++)
            isa_devices[isa_device_count_val].name[i] = s[i];
        isa_devices[isa_device_count_val].name[i] = '\0';
    }
    isa_device_count_val++;

    /* CMOS/RTC */
    isa_devices[isa_device_count_val].io_base  = 0x70;
    isa_devices[isa_device_count_val].irq      = 8;
    isa_devices[isa_device_count_val].dma_chan = 0xFF;
    {
        int i;
        const char *s = "MC146818 RTC";
        for (i = 0; s[i] && i < 19; i++)
            isa_devices[isa_device_count_val].name[i] = s[i];
        isa_devices[isa_device_count_val].name[i] = '\0';
    }
    isa_device_count_val++;
}

/*
 * isa_device_count — 获取 ISA 设备数量
 */
int isa_device_count(void)
{
    return isa_device_count_val;
}

/*
 * isa_get_device — 获取指定索引的 ISA 设备
 * @index: 设备索引 (0 ~ count-1)
 * 返回: ISA 设备结构指针, 无效索引返回 NULL。
 */
struct isa_device *isa_get_device(int index)
{
    if (index < 0 || index >= isa_device_count_val)
        return NULL;
    return &isa_devices[index];
}
