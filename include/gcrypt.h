/*
 * Nexsteaduser — PlexsDOS
 * gcrypt.h — 存根 (LightDM 移植)
 * 作者: Tinmc189623 | 团队: Nexlyh
 *
 * LightDM 使用 libgcrypt 进行安全内存分配和 MD5 哈希。
 * PlexsDOS 使用静态池, 直接映射到标准内存。
 */
#ifndef _GCRYPT_H
#define _GCRYPT_H

#include <stddef.h>

void *gcry_malloc_secure(size_t n);  /* 函数声明 (用于函数指针场景) */
#define gcry_malloc_secure(n)  malloc(n)
#define gcry_realloc(p, n)     realloc(p, n)
#define gcry_free(p)           free(p)

#endif /* _GCRYPT_H */
