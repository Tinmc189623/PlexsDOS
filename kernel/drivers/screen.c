/*
 * Nexsteaduser — PlexsDOS
 * VGA 文本模式屏幕驱动
 * 作者: Tinmc189623 | 团队: Nexlyh
 *
 * 直接写 VGA 显存 0xB8000 实现屏幕输出。
 * 每个字符占 2 字节: 低字节=ASCII, 高字节=颜色属性。
 * 80x25 文本模式, 共 2000 个字符位置。
 */

#include <plexsdos/types.h>
#include <plexsdos/config.h>
#include <plexsdos/screen.h>
#include <plexsdos/serial.h>

/* VGA 文本模式显存基址 */
static volatile uint16_t *vga = (volatile uint16_t *)VGA_TEXT_BUFFER;

/* 当前光标位置 (字符偏移, 0 = 左上角) */
static int cursor_pos = 0;

/* 当前颜色属性 (默认: 黑底白字 0x07) */
static uint8_t current_color = DEFAULT_COLOR;

/*
 * screen_init — 初始化屏幕
 * 清屏并设置默认颜色, 光标归位。
 */
void screen_init(void)
{
    current_color = DEFAULT_COLOR;
    screen_clear();
}

/*
 * screen_clear — 清屏
 * 用空格填充整个屏幕, 光标重置到左上角。
 */
void screen_clear(void)
{
    int i;
    uint16_t blank = (uint16_t)((current_color << 8) | ' ');
    for (i = 0; i < SCREEN_SIZE; i++)
        vga[i] = blank;
    cursor_pos = 0;
    screen_set_cursor(0);
}

/*
 * screen_scroll — 滚屏一行
 * 将第 2-25 行内容上移到第 1-24 行, 清空第 25 行。
 */
static void screen_scroll(void)
{
    int i;
    /* 上移一行 */
    for (i = 0; i < SCREEN_SIZE - SCREEN_COLS; i++)
        vga[i] = vga[i + SCREEN_COLS];
    /* 清空最后一行 */
    for (i = SCREEN_SIZE - SCREEN_COLS; i < SCREEN_SIZE; i++)
        vga[i] = (uint16_t)((current_color << 8) | ' ');
    cursor_pos = SCREEN_SIZE - SCREEN_COLS;
}

/*
 * screen_putchar — 输出单个字符
 * @c: ASCII 字符
 *
 * 支持特殊字符: \n (换行), \r (回车), \b (退格), \t (制表符)。
 * 自动处理滚屏。
 */
void screen_putchar(char c)
{
    /* 串口镜像: 所有 VGA 输出同时发送到 COM1 (串口调试用) */
    if (c == '\n') {
        serial_putchar('\r');
        serial_putchar('\n');
    } else if (c >= 32) {
        serial_putchar(c);
    }

    if (c == '\n') {
        /* 换行: 移到下一行行首 */
        cursor_pos = (cursor_pos / SCREEN_COLS + 1) * SCREEN_COLS;
    } else if (c == '\r') {
        /* 回车: 移到当前行行首 */
        cursor_pos = (cursor_pos / SCREEN_COLS) * SCREEN_COLS;
    } else if (c == '\b') {
        /* 退格: 光标左移一格并清除该位置 */
        if (cursor_pos > 0) {
            cursor_pos--;
            vga[cursor_pos] = (uint16_t)((current_color << 8) | ' ');
        }
    } else if (c == '\t') {
        /* 制表符: 对齐到下一个 8 的倍数 */
        int next_tab = (cursor_pos / SCREEN_COLS) * SCREEN_COLS
                       + ((cursor_pos % SCREEN_COLS) / 8 + 1) * 8;
        if (next_tab >= (cursor_pos / SCREEN_COLS + 1) * SCREEN_COLS)
            next_tab = (cursor_pos / SCREEN_COLS + 1) * SCREEN_COLS - 1;
        cursor_pos = next_tab;
    } else {
        /* 普通字符: 写入显存 */
        vga[cursor_pos] = (uint16_t)((current_color << 8) | (uint8_t)c);
        cursor_pos++;
    }

    /* 滚屏检查 */
    if (cursor_pos >= SCREEN_SIZE)
        screen_scroll();

    /* 更新硬件光标 */
    screen_set_cursor(cursor_pos);
}

/*
 * screen_puts — 输出字符串
 * @str: 以 null 结尾的字符串
 */
void screen_puts(const char *str)
{
    while (*str) {
        screen_putchar(*str);
        str++;
    }
}

/*
 * screen_put_hex — 输出十六进制数值
 * @val: 要输出的 32-bit 值
 *
 * 输出格式: 0x1234ABCD
 */
void screen_put_hex(uint32_t val)
{
    int i;
    int started = 0;
    screen_puts("0x");
    for (i = 28; i >= 0; i -= 4) {
        uint8_t nibble = (val >> i) & 0x0F;
        if (nibble != 0 || started || i == 0) {
            started = 1;
            if (nibble < 10)
                screen_putchar('0' + nibble);
            else
                screen_putchar('A' + nibble - 10);
        }
    }
}

/*
 * screen_put_dec — 输出十进制无符号整数
 * @val: 要输出的值
 */
void screen_put_dec(uint32_t val)
{
    char buf[11]; /* 最大 4294967295, 10 位 + null */
    int i = 0;

    if (val == 0) {
        screen_putchar('0');
        return;
    }

    while (val > 0) {
        buf[i++] = '0' + (val % 10);
        val /= 10;
    }

    /* 反向输出 */
    while (i > 0)
        screen_putchar(buf[--i]);
}

/*
 * screen_set_color — 设置颜色
 * @fg: 前景色 (0-15)
 * @bg: 背景色 (0-15)
 */
void screen_set_color(uint8_t fg, uint8_t bg)
{
    current_color = (uint8_t)((bg << 4) | (fg & 0x0F));
}

/*
 * screen_reset_color — 恢复默认颜色 (黑底白字)
 */
void screen_reset_color(void)
{
    current_color = DEFAULT_COLOR;
}

/*
 * screen_get_cursor — 获取当前光标位置
 * 返回: 字符偏移 (0-1999)
 */
int screen_get_cursor(void)
{
    return cursor_pos;
}

/*
 * screen_set_cursor — 设置硬件光标位置
 * @pos: 字符偏移 (0-1999)
 *
 * 通过 VGA 端口 0x3D4/0x3D5 控制光标位置。
 * 使用 DX 寄存器传递端口号 (端口 > 0xFF 不能用立即数形式)。
 */
void screen_set_cursor(int pos)
{
    uint16_t port = 0x3D4;

    /* 高字节 */
    __asm__ __volatile__(
        "outb %b0, %%dx\n\t"
        :
        : "a"((uint8_t)0x0E), "d"(port)
        : "memory"
    );
    port = 0x3D5;
    __asm__ __volatile__(
        "outb %b0, %%dx\n\t"
        :
        : "a"((uint8_t)((pos >> 8) & 0xFF)), "d"(port)
        : "memory"
    );

    /* 低字节 */
    port = 0x3D4;
    __asm__ __volatile__(
        "outb %b0, %%dx\n\t"
        :
        : "a"((uint8_t)0x0F), "d"(port)
        : "memory"
    );
    port = 0x3D5;
    __asm__ __volatile__(
        "outb %b0, %%dx\n\t"
        :
        : "a"((uint8_t)(pos & 0xFF)), "d"(port)
        : "memory"
    );
}
