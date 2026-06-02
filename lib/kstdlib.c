/*
 * Nexsteaduser — PlexsDOS
 * stdlib.h 实现 — 数值转换和基本数学函数
 * 作者: Tinmc189623 | 团队: Nexlyh
 */

#include <libc/stdlib.h>
#include <libc/ctype.h>

/*
 * abs — 绝对值 (int)
 * @x: 整数值
 * 返回: |x|。
 */
int abs(int x)
{
    return x < 0 ? -x : x;
}

/*
 * labs — 绝对值 (long)
 * @x: long 值
 * 返回: |x|。
 */
long labs(long x)
{
    return x < 0 ? -x : x;
}

/*
 * atoi — 字符串转整数
 * @str: 以 null 结尾的字符串
 * 返回: 转换后的整数值。
 *
 * 跳过前导空白, 处理可选的 +/- 符号,
 * 解析十进制数字直到遇到非数字字符。
 */
int atoi(const char *str)
{
    int result = 0;
    int sign = 1;

    /* 跳过前导空白 */
    while (isspace(*str))
        str++;

    /* 处理符号 */
    if (*str == '-') {
        sign = -1;
        str++;
    } else if (*str == '+') {
        str++;
    }

    /* 解析数字 */
    while (isdigit(*str)) {
        result = result * 10 + (*str - '0');
        str++;
    }

    return sign * result;
}

/*
 * strtol — 字符串转 long (指定进制)
 * @str:    输入字符串
 * @endptr: 输出 — 指向第一个未解析字符的指针 (可为 NULL)
 * @base:   进制 (2-36, 0 = 自动检测)
 * 返回: 转换后的 long 值。
 */
long strtol(const char *str, char **endptr, int base)
{
    long result = 0;
    int sign = 1;
    int digit;

    /* 跳过前导空白 */
    while (isspace(*str))
        str++;

    /* 处理符号 */
    if (*str == '-') {
        sign = -1;
        str++;
    } else if (*str == '+') {
        str++;
    }

    /* 自动检测进制 */
    if (base == 0) {
        if (*str == '0') {
            str++;
            if (*str == 'x' || *str == 'X') {
                base = 16;
                str++;
            } else {
                base = 8;
            }
        } else {
            base = 10;
        }
    } else if (base == 16 && str[0] == '0' &&
               (str[1] == 'x' || str[1] == 'X')) {
        str += 2;
    }

    /* 解析数字 */
    while (*str) {
        if (isdigit(*str))
            digit = *str - '0';
        else if (*str >= 'A' && *str <= 'Z')
            digit = *str - 'A' + 10;
        else if (*str >= 'a' && *str <= 'z')
            digit = *str - 'a' + 10;
        else
            break;

        if (digit >= base)
            break;

        result = result * base + digit;
        str++;
    }

    if (endptr)
        *endptr = (char *)str;

    return sign * result;
}

/*
 * strtoul — 字符串转 unsigned long
 * @str:    输入字符串
 * @endptr: 输出指针 (可为 NULL)
 * @base:   进制
 * 返回: 转换后的 unsigned long 值。
 */
unsigned long strtoul(const char *str, char **endptr, int base)
{
    unsigned long result = 0;
    int digit;

    /* 跳过前导空白 */
    while (isspace(*str))
        str++;

    /* 跳过可选的 '+' */
    if (*str == '+')
        str++;

    /* 自动检测进制 */
    if (base == 0) {
        if (*str == '0') {
            str++;
            if (*str == 'x' || *str == 'X') {
                base = 16;
                str++;
            } else {
                base = 8;
            }
        } else {
            base = 10;
        }
    } else if (base == 16 && str[0] == '0' &&
               (str[1] == 'x' || str[1] == 'X')) {
        str += 2;
    }

    /* 解析数字 */
    while (*str) {
        if (isdigit(*str))
            digit = *str - '0';
        else if (*str >= 'A' && *str <= 'Z')
            digit = *str - 'A' + 10;
        else if (*str >= 'a' && *str <= 'z')
            digit = *str - 'a' + 10;
        else
            break;

        if (digit >= base)
            break;

        result = result * (unsigned long)base + (unsigned long)digit;
        str++;
    }

    if (endptr)
        *endptr = (char *)str;

    return result;
}

/*
 * itoa — 整数转字符串 (指定进制)
 * @value: 要转换的值
 * @buf:   目标缓冲区 (至少 33 字节)
 * @base:  进制 (2-36)
 * 返回: 指向 buf 的指针。
 */
char *itoa(int value, char *buf, int base)
{
    char *p = buf;
    char *start;
    char tmp;
    int neg = 0;
    unsigned int uval;

    if (base < 2 || base > 36) {
        buf[0] = '\0';
        return buf;
    }

    /* 处理负数 (仅十进制) */
    if (value < 0 && base == 10) {
        neg = 1;
        uval = (unsigned int)(-(value + 1)) + 1;
    } else {
        uval = (unsigned int)value;
    }

    /* 特殊情况: 值为 0 */
    if (uval == 0) {
        *p++ = '0';
        *p = '\0';
        return buf;
    }

    /* 转换数字 */
    while (uval > 0) {
        int digit = (int)(uval % (unsigned int)base);
        *p++ = (digit < 10) ? (char)('0' + digit)
                            : (char)('a' + digit - 10);
        uval /= (unsigned int)base;
    }

    if (neg)
        *p++ = '-';

    *p = '\0';

    /* 反转字符串 */
    start = buf;
    p--;
    while (start < p) {
        tmp = *start;
        *start = *p;
        *p = tmp;
        start++;
        p--;
    }

    return buf;
}

/*
 * utoa — 无符号整数转字符串
 * @value: 无符号值
 * @buf:   目标缓冲区
 * @base:  进制
 * 返回: 指向 buf 的指针。
 */
char *utoa(unsigned int value, char *buf, int base)
{
    char *p = buf;
    char *start;
    char tmp;

    if (base < 2 || base > 36) {
        buf[0] = '\0';
        return buf;
    }

    if (value == 0) {
        *p++ = '0';
        *p = '\0';
        return buf;
    }

    while (value > 0) {
        int digit = (int)(value % (unsigned int)base);
        *p++ = (digit < 10) ? (char)('0' + digit)
                            : (char)('a' + digit - 10);
        value /= (unsigned int)base;
    }

    *p = '\0';

    /* 反转 */
    start = buf;
    p--;
    while (start < p) {
        tmp = *start;
        *start = *p;
        *p = tmp;
        start++;
        p--;
    }

    return buf;
}

/*
 * ltoa — long 转字符串
 * @value: long 值
 * @buf:   目标缓冲区
 * @base:  进制
 * 返回: 指向 buf 的指针。
 */
char *ltoa(long value, char *buf, int base)
{
    char *p = buf;
    char *start;
    char tmp;
    int neg = 0;
    unsigned long uval;

    if (base < 2 || base > 36) {
        buf[0] = '\0';
        return buf;
    }

    if (value < 0 && base == 10) {
        neg = 1;
        uval = (unsigned long)(-(value + 1)) + 1;
    } else {
        uval = (unsigned long)value;
    }

    if (uval == 0) {
        *p++ = '0';
        *p = '\0';
        return buf;
    }

    while (uval > 0) {
        int digit = (int)(uval % (unsigned long)base);
        *p++ = (digit < 10) ? (char)('0' + digit)
                            : (char)('a' + digit - 10);
        uval /= (unsigned long)base;
    }

    if (neg)
        *p++ = '-';

    *p = '\0';

    start = buf;
    p--;
    while (start < p) {
        tmp = *start;
        *start = *p;
        *p = tmp;
        start++;
        p--;
    }

    return buf;
}
