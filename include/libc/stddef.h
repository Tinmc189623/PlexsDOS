/*
 * Nexsteaduser — PlexsDOS
 * stddef.h — 标准类型定义
 * 作者: Tinmc189623 | 团队: Nexlyh
 */

#ifndef _LIBC_STDDEF_H
#define _LIBC_STDDEF_H

#include <plexsdos/types.h>

/* 从 types.h 继承 size_t 和 NULL */

/* 结构体成员偏移量宏 */
#define offsetof(type, member) __builtin_offsetof(type, member)

/* ptrdiff_t — 指针差值类型 */
typedef int ptrdiff_t;

/* wchar_t — 宽字符类型 */
typedef unsigned short wchar_t;

#endif /* _LIBC_STDDEF_H */
