/*
 * Nexsteaduser — PlexsDOS
 * 基础类型定义 (C23)
 * 作者: Tinmc189623 | 团队: Nexlyh
 *
 * 使用 C23 原生关键字: bool, true, false, nullptr, static_assert, typeof
 */

#ifndef _PLXSDOS_TYPES_H
#define _PLXSDOS_TYPES_H

#ifdef __cplusplus
extern "C" {
#endif

typedef unsigned char      uint8_t;
typedef unsigned short     uint16_t;
typedef unsigned long      uint32_t;
typedef unsigned long long uint64_t;
typedef signed char        int8_t;
typedef signed short       int16_t;
typedef signed long        int32_t;
typedef signed long long   int64_t;
typedef unsigned int       size_t;

/* C23: nullptr 是原生关键字, 无需定义 */
/* C23: bool/true/false 是原生关键字, 无需定义 */

#define NULL  ((void *)0)

#ifdef __cplusplus
}
#endif

#endif /* _PLXSDOS_TYPES_H */
