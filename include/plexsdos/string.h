/*
 * Nexsteaduser — PlexsDOS
 * 字符串操作函数
 * 作者: Tinmc189623 | 团队: Nexlyh
 */

#ifndef _PLXSDOS_STRING_H
#define _PLXSDOS_STRING_H

#include <plexsdos/types.h>

#ifdef __cplusplus
extern "C" {
#endif

/* 计算字符串长度 (不含 null) */
size_t strlen(const char *str);

/* 内存填充 — 快速版本 (SSE2/AVX 分派) */
void *fast_memset(void *dst, int val, size_t n);

/* 内存填充 */
void *memset(void *dst, int val, size_t n);

/* 内存复制 */
void *memcpy(void *dst, const void *src, size_t n);

/* 内存移动 (支持重叠) */
void *memmove(void *dst, const void *src, size_t n);

/* 内存比较 */
int memcmp(const void *s1, const void *s2, size_t n);

/* 字符串复制 */
char *strcpy(char *dst, const char *src);

/* 字符串比较 */
int strcmp(const char *s1, const char *s2);

/* 前 n 字节字符串比较 */
int strncmp(const char *s1, const char *s2, size_t n);

/* 字符串连接 */
char *strcat(char *dst, const char *src);

/* 前 n 字节字符串连接 */
char *strncat(char *dst, const char *src, size_t n);

/* 字符串复制 (最多 n 字节) */
char *strncpy(char *dst, const char *src, size_t n);

/* 查找字符首次出现 */
char *strchr(const char *str, int c);

/* 查找字符最后一次出现 */
char *strrchr(const char *str, int c);

/* 查找子字符串 */
char *strstr(const char *haystack, const char *needle);

/* 查找字符在字符串中的位置 (首次) */
size_t strcspn(const char *s, const char *reject);

#ifdef __cplusplus
}
#endif

#endif /* _PLXSDOS_STRING_H */
