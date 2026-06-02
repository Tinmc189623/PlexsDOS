/*
 * Nexsteaduser — PlexsDOS
 * stdio.h — 格式化输入/输出
 * 作者: Tinmc189623 | 团队: Nexlyh
 *
 * freestanding 环境: 无 FILE*, fopen/fclose/fread/fwrite。
 * 提供 sprintf/snprintf 和基本 I/O 函数。
 */

#ifndef _LIBC_STDIO_H
#define _LIBC_STDIO_H

#include <libc/stdarg.h>
#include <libc/stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

/*
 * sprintf — 格式化输出到字符串
 * @buf:    目标缓冲区
 * @format: 格式字符串
 * 返回: 写入的字符数 (不含 null)。
 *
 * 支持的格式说明符:
 *   %d, %i  — 有符号十进制整数
 *   %u      — 无符号十进制整数
 *   %x, %X  — 十六进制整数
 *   %o      — 八进制整数
 *   %c      — 字符
 *   %s      — 字符串
 *   %p      — 指针
 *   %%      — 字面 '%'
 *   %0Nd    — 前导零填充, 宽度 N
 */
int sprintf(char *buf, const char *format, ...);

/*
 * snprintf — 格式化输出到字符串 (带长度限制)
 * @buf:    目标缓冲区
 * @size:   缓冲区大小
 * @format: 格式字符串
 * 返回: 期望写入的字符数 (不含 null), 截断时 >= size。
 */
int snprintf(char *buf, size_t size, const char *format, ...);

/*
 * vsprintf — 格式化输出到字符串 (va_list 版本)
 */
int vsprintf(char *buf, const char *format, va_list args);

/*
 * vsnprintf — 格式化输出到字符串 (va_list 版本, 带长度限制)
 */
int vsnprintf(char *buf, size_t size, const char *format, va_list args);

#ifdef __cplusplus
}
#endif

#endif /* _LIBC_STDIO_H */
