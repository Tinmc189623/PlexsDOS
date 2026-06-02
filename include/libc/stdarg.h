/*
 * Nexsteaduser — PlexsDOS
 * stdarg.h — 可变参数函数支持
 * 作者: Tinmc189623 | 团队: Nexlyh
 *
 * 基于 GCC 内建的 __builtin_va_* 实现。
 * freestanding 环境下完全可用。
 */

#ifndef _LIBC_STDARG_H
#define _LIBC_STDARG_H

typedef __builtin_va_list va_list;

#define va_start(ap, last) __builtin_va_start(ap, last)
#define va_arg(ap, type)   __builtin_va_arg(ap, type)
#define va_end(ap)         __builtin_va_end(ap)
#define va_copy(dest, src) __builtin_va_copy(dest, src)

#endif /* _LIBC_STDARG_H */
