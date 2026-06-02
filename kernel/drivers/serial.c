/*
 * Nexsteaduser — PlexsDOS
 * 串口驱动 (COM1 0x3F8) — 调试输出
 * 作者: Tinmc189623 | 团队: Nexlyh
 *
 * 提供 COM1 串口输出, 用于 QEMU/VMware/真机调试。
 * 完整初始化 16550 UART: 波特率 9600, 8N1, 启用 FIFO。
 */

#include <plexsdos/types.h>
#include <plexsdos/serial.h>

/* COM1 端口基址 */
#define COM1 0x3F8

/* 辅助宏: 向指定端口写入字节 */
#define outb(port, val) \
    __asm__ __volatile__("outb %%al, %%dx" : : "a"((uint8_t)(val)), "d"((uint16_t)(port)))

/*
 * serial_init — 初始化 COM1 串口 (16550 UART)
 *
 * 配置: 9600 波特率, 8 数据位, 无校验, 1 停止位 (8N1)。
 * 步骤:
 * 1. 禁用中断 (DIER=0)
 * 2. 启用 DLAB 设置波特率
 * 3. 设置波特率除数 (115200/9600=12 → DLL=0x0C, DLM=0x00)
 * 4. 8N1: 8 数据位, 无校验, 1 停止位 (LCR=0x03)
 * 5. 启用 FIFO, 清空收发缓冲区 (FCR=0xC7)
 * 6. 启用 IRQ, RTS/DSR (MCR=0x0B)
 * 7. 禁用 DLAB
 */
void serial_init(void)
{
    /* 禁用所有中断 */
    outb(COM1 + 1, 0x00);

    /* 启用 DLAB (设置波特率) */
    outb(COM1 + 3, 0x80);

    /* 波特率除数低字节: 115200 / 9600 = 12 = 0x0C */
    outb(COM1 + 0, 0x0C);

    /* 波特率除数高字节 */
    outb(COM1 + 1, 0x00);

    /* 8N1: 8 数据位, 无校验, 1 停止位, 禁用 DLAB */
    outb(COM1 + 3, 0x03);

    /* 启用 FIFO, 清空收发队列, 14 字节触发 */
    outb(COM1 + 2, 0xC7);

    /* 启用 IRQ, 设置 RTS/DTR */
    outb(COM1 + 4, 0x0B);
}

/*
 * serial_putchar — 发送单个字符到 COM1
 * @c: 要发送的字符
 *
 * 等待发送缓冲区就绪后写入字符。
 */
void serial_putchar(char c)
{
    uint8_t status;
    uint16_t lsr = COM1 + 5;

    /* 等待 THRE (发送保持寄存器空) */
    do {
        __asm__ __volatile__(
            "inb %%dx, %%al"
            : "=a"(status)
            : "d"(lsr)
        );
    } while (!(status & 0x20));

    __asm__ __volatile__(
        "outb %%al, %%dx"
        :
        : "a"((uint8_t)c), "d"((uint16_t)COM1)
    );
}

/*
 * serial_puts — 发送字符串到 COM1
 * @str: 以 null 结尾的字符串
 *
 * 逐字符调用 serial_putchar 输出, 避免内联汇编在不同引导环境下的兼容性问题。
 */
void serial_puts(const char *str)
{
    if (!str) {
        serial_putchar('!');
        return;
    }

    while (*str) {
        if (*str == '\n')
            serial_putchar('\r');
        serial_putchar(*str);
        str++;
    }
}

/*
 * serial_put_hex — 以十六进制输出 32 位值到 COM1
 * @val: 要输出的值
 *
 * 输出 8 位十六进制数 (不带 0x 前缀)。
 */
void serial_put_hex(uint32_t val)
{
    int i;
    static const char hex[] = "0123456789ABCDEF";
    for (i = 28; i >= 0; i -= 4)
        serial_putchar(hex[(val >> i) & 0xF]);
}
