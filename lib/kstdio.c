/*
 * Nexsteaduser — PlexsDOS
 * stdio.h 实现 — 格式化输出函数
 * 作者: Tinmc189623 | 团队: Nexlyh
 *
 * 实现 sprintf/snprintf/vsprintf/vsnprintf。
 * 内核环境, 不依赖任何宿主 I/O。
 */

#include <libc/stdio.h>
#include <libc/stdlib.h>
#include <plexsdos/string.h>
#include <libc/ctype.h>

/* 内部数字转字符辅助 */
static char *format_number(char *buf, int *pos, int max,
                           unsigned long num, int base, int width,
                           int pad_zero, int uppercase, int is_signed,
                           long signed_val)
{
    char tmp[32];
    int len = 0;
    int neg = 0;
    unsigned long val = num;

    if (is_signed && signed_val < 0) {
        neg = 1;
        val = (unsigned long)(-(signed_val + 1)) + 1;
    }

    if (val == 0) {
        tmp[len++] = '0';
    } else {
        while (val > 0) {
            int digit = (int)(val % (unsigned long)base);
            if (uppercase)
                tmp[len++] = (digit < 10) ? '0' + digit : 'A' + digit - 10;
            else
                tmp[len++] = (digit < 10) ? '0' + digit : 'a' + digit - 10;
            val /= (unsigned long)base;
        }
    }

    /* 计算总长度 */
    int total = len + neg;
    int pad = (width > total) ? width - total : 0;
    char pad_char = pad_zero ? '0' : ' ';

    /* 填充 */
    for (int i = 0; i < pad && *pos < max; i++)
        buf[(*pos)++] = pad_char;

    /* 负号 */
    if (neg && *pos < max)
        buf[(*pos)++] = '-';

    /* 数字 (反向输出) */
    for (int i = len - 1; i >= 0 && *pos < max; i--)
        buf[(*pos)++] = tmp[i];

    return buf;
}

/*
 * vsnprintf — 格式化输出到字符串 (核心实现)
 * @buf:    目标缓冲区
 * @size:   缓冲区大小
 * @format: 格式字符串
 * @args:   可变参数列表
 * 返回: 期望写入的字符数 (不含 null)。
 */
int vsnprintf(char *buf, size_t size, const char *format, va_list args)
{
    int pos = 0;
    int max = (int)size - 1;

    if (max < 0) max = 0;

    while (*format) {
        if (*format != '%') {
            if (pos < max)
                buf[pos] = *format;
            pos++;
            format++;
            continue;
        }

        format++;  /* 跳过 '%' */

        /* 解析标志 */
        int pad_zero = 0;
        int width = 0;

        if (*format == '0') {
            pad_zero = 1;
            format++;
        }

        /* 解析宽度 */
        while (isdigit(*format)) {
            width = width * 10 + (*format - '0');
            format++;
        }

        /* 解析长度修饰符 */
        int is_long = 0;
        if (*format == 'l') {
            is_long = 1;
            format++;
        }

        /* 格式说明符 */
        switch (*format) {
        case 'd':
        case 'i': {
            long val = is_long ? va_arg(args, long) : (long)va_arg(args, int);
            format_number(buf, &pos, max, 0, 10, width, pad_zero, 0,
                          1, val);
            break;
        }
        case 'u': {
            unsigned long val = is_long ? va_arg(args, unsigned long)
                                        : (unsigned long)va_arg(args, unsigned int);
            format_number(buf, &pos, max, val, 10, width, pad_zero, 0,
                          0, 0);
            break;
        }
        case 'x': {
            unsigned long val = is_long ? va_arg(args, unsigned long)
                                        : (unsigned long)va_arg(args, unsigned int);
            format_number(buf, &pos, max, val, 16, width, pad_zero, 0,
                          0, 0);
            break;
        }
        case 'X': {
            unsigned long val = is_long ? va_arg(args, unsigned long)
                                        : (unsigned long)va_arg(args, unsigned int);
            format_number(buf, &pos, max, val, 16, width, pad_zero, 1,
                          0, 0);
            break;
        }
        case 'o': {
            unsigned long val = is_long ? va_arg(args, unsigned long)
                                        : (unsigned long)va_arg(args, unsigned int);
            format_number(buf, &pos, max, val, 8, width, pad_zero, 0,
                          0, 0);
            break;
        }
        case 'p': {
            unsigned long val = (unsigned long)va_arg(args, void *);
            if (pos < max) buf[pos++] = '0';
            if (pos < max) buf[pos++] = 'x';
            format_number(buf, &pos, max, val, 16, 8, 1, 0, 0, 0);
            break;
        }
        case 'c': {
            char c = (char)va_arg(args, int);
            if (pos < max) buf[pos] = c;
            pos++;
            break;
        }
        case 's': {
            const char *str = va_arg(args, const char *);
            if (!str) str = "(null)";
            int slen = 0;
            while (str[slen]) slen++;
            /* 右对齐填充 */
            for (int i = slen; i < width && pos < max; i++)
                buf[pos++] = ' ';
            while (*str && pos < max)
                buf[pos++] = *str++;
            break;
        }
        case '%':
            if (pos < max) buf[pos] = '%';
            pos++;
            break;
        default:
            if (pos < max) buf[pos] = '%';
            pos++;
            if (pos < max) buf[pos] = *format;
            pos++;
            break;
        }

        if (*format)
            format++;
    }

    /* null 终止 */
    if (size > 0) {
        if (pos < max)
            buf[pos] = '\0';
        else
            buf[max] = '\0';
    }

    return pos;
}

/*
 * vsprintf — 格式化输出到字符串 (无长度限制)
 * @buf:    目标缓冲区
 * @format: 格式字符串
 * @args:   可变参数列表
 * 返回: 写入的字符数。
 */
int vsprintf(char *buf, const char *format, va_list args)
{
    return vsnprintf(buf, 0x7FFFFFFF, format, args);
}

/*
 * sprintf — 格式化输出到字符串
 * @buf:    目标缓冲区
 * @format: 格式字符串
 * 返回: 写入的字符数 (不含 null)。
 */
int sprintf(char *buf, const char *format, ...)
{
    va_list args;
    va_start(args, format);
    int ret = vsnprintf(buf, 0x7FFFFFFF, format, args);
    va_end(args);
    return ret;
}

/*
 * snprintf — 格式化输出到字符串 (带长度限制)
 * @buf:    目标缓冲区
 * @size:   缓冲区大小
 * @format: 格式字符串
 * 返回: 期望写入的字符数。
 */
int snprintf(char *buf, size_t size, const char *format, ...)
{
    va_list args;
    va_start(args, format);
    int ret = vsnprintf(buf, size, format, args);
    va_end(args);
    return ret;
}
