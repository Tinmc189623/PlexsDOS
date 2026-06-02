/*
 * Nexsteaduser — PlexsDOS
 * INT 21h 系统调用分发 (32-bit 保护模式)
 * 作者: Tinmc189623 | 团队: Nexlyh
 *
 * 实现 DOS 兼容的 INT 21h 系统调用接口。
 * 外部程序通过 INT 0x22 调用 (避免与 IRQ1 键盘中断冲突), AH=功能号。
 * 32-bit 保护模式平坦内存模型。
 */

#include <plexsdos/types.h>
#include <plexsdos/config.h>
#include <plexsdos/screen.h>
#include <plexsdos/keyboard.h>
#include <plexsdos/syscall.h>
#include <plexsdos/interrupt.h>

/* 外部汇编符号: INT 0x22 中断入口 (PE 格式编译器自动加 _ 前缀) */
extern void isr_syscall(void);

/* 程序退出标志, 由 asm 入口检查 */
volatile bool g_syscall_exit_flag = false;

/*
 * syscall_read_char — 读取一个字符并回显。
 * 返回: 字符 ASCII 码。
 */
static uint8_t syscall_read_char(void)
{
    return (uint8_t)keyboard_getchar();
}

/*
 * syscall_write_char — 输出一个字符。
 * 参数: ch = 要输出的字符。
 */
static void syscall_write_char(uint8_t ch)
{
    screen_putchar((char)ch);
}

/*
 * syscall_write_str — 输出以 '$' 结尾的字符串。
 * 参数: str = 指向字符串的指针。
 */
static void syscall_write_str(const char *str)
{
    while (*str && *str != '$') {
        screen_putchar(*str);
        str++;
    }
}

/*
 * syscall_read_str — 读取一行字符串到缓冲区。
 * 参数: buf = 缓冲区指针, buf[0]=最大长度。
 * 返回后: buf[1]=实际长度, buf[2..]=字符串内容。
 */
static void syscall_read_str(char *buf)
{
    uint8_t max_len = (uint8_t)buf[0];
    if (max_len < 2)
        return;
    keyboard_read_line(buf + 2, max_len - 1);
    uint8_t len = 0;
    while (buf[2 + len] != '\0' && len < max_len - 1)
        len++;
    buf[1] = len;
}

/*
 * syscall_dispatch — INT 21h 系统调用分发。
 * 由汇编中断入口 _isr_syscall 调用。
 *
 * 参数:
 *   eax - AH=功能号, AL=子参数
 *   edx - DL/DX=数据参数 (字符或缓冲区指针)
 *   esi - 附加参数
 *
 * 返回: 0=正常, 1=程序请求终止。
 */
uint32_t syscall_dispatch(uint32_t eax, uint32_t edx, uint32_t esi)
{
    uint8_t func = (uint8_t)((eax >> 8) & 0xFF);
    uint8_t al = (uint8_t)(eax & 0xFF);
    uint8_t dl = (uint8_t)(edx & 0xFF);
    (void)esi;

    switch (func) {
    case SYS_READ_CHAR:
        return (uint32_t)syscall_read_char();

    case SYS_WRITE_CHAR:
        syscall_write_char(dl);
        return 0;

    case SYS_WRITE_STR:
        syscall_write_str((const char *)edx);
        return 0;

    case SYS_READ_STR:
        syscall_read_str((char *)edx);
        return 0;

    case SYS_EXIT:
        screen_puts("\n[exit code ");
        screen_put_dec((uint32_t)al);
        screen_puts("]\n");
        g_syscall_exit_flag = true;
        return 1;

    default:
        screen_puts("[syscall] unknown: 0x");
        screen_put_hex((uint32_t)func);
        screen_putchar('\n');
        return 0;
    }
}

/*
 * syscall_init — 初始化 INT 21h 系统调用。
 * 在 IDT 0x22 号向量注册中断处理程序。
 */
void syscall_init(void)
{
    g_syscall_exit_flag = false;
    idt_set_gate(0x22, isr_syscall);
    screen_puts("[syscall] INT 21h ready (vector 0x22).\n");
}
