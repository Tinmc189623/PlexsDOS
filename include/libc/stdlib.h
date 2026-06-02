/*
 * Nexsteaduser — PlexsDOS
 * stdlib.h — 标准库函数
 * 作者: Tinmc189623 | 团队: Nexlyh
 *
 * freestanding 环境: 无 malloc/free/exit/system。
 * 提供数值转换和基本数学函数。
 */

#ifndef _LIBC_STDLIB_H
#define _LIBC_STDLIB_H

#include <libc/stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

/*
 * abs — 绝对值 (int)
 */
int abs(int x);

/*
 * labs — 绝对值 (long)
 */
long labs(long x);

/*
 * atoi — 字符串转整数
 */
int atoi(const char *str);

/*
 * strtol — 字符串转 long (指定进制)
 */
long strtol(const char *str, char **endptr, int base);

/*
 * strtoul — 字符串转 unsigned long
 */
unsigned long strtoul(const char *str, char **endptr, int base);

/*
 * itoa — 整数转字符串 (指定进制)
 * @value: 要转换的值
 * @buf:   目标缓冲区
 * @base:  进制 (2-36)
 * 返回: 指向 buf 的指针。
 */
char *itoa(int value, char *buf, int base);

/*
 * utoa — 无符号整数转字符串
 */
char *utoa(unsigned int value, char *buf, int base);

/*
 * ltoa — long 转字符串
 */
char *ltoa(long value, char *buf, int base);

#ifdef __cplusplus
}
#endif

#endif /* _LIBC_STDLIB_H */
