/*
 * Nexsteaduser — PlexsDOS
 * 字符串操作函数实现
 * 作者: Tinmc189623 | 团队: Nexlyh
 */

#include <plexsdos/types.h>
#include <plexsdos/string.h>

/*
 * strlen — 计算字符串长度
 * @str: 以 null 结尾的字符串
 * 返回: 字符数 (不含 null 终止符)
 */
size_t strlen(const char *str)
{
    size_t len = 0;
    while (*str++)
        len++;
    return len;
}

/*
 * memset — 内存填充
 * @dst: 目标地址
 * @val: 填充值
 * @n:   字节数
 * 返回: dst
 */
void *memset(void *dst, int val, size_t n)
{
    uint8_t *d = (uint8_t *)dst;
    while (n--)
        *d++ = (uint8_t)val;
    return dst;
}

/*
 * memcpy — 内存复制
 * @dst: 目标地址
 * @src: 源地址
 * @n:   字节数
 * 返回: dst
 */
void *memcpy(void *dst, const void *src, size_t n)
{
    uint8_t *d = (uint8_t *)dst;
    const uint8_t *s = (const uint8_t *)src;
    while (n--)
        *d++ = *s++;
    return dst;
}

/*
 * memmove — 内存移动 (支持重叠区域)
 * @dst: 目标地址
 * @src: 源地址
 * @n:   字节数
 * 返回: dst
 */
void *memmove(void *dst, const void *src, size_t n)
{
    uint8_t *d = (uint8_t *)dst;
    const uint8_t *s = (const uint8_t *)src;

    if (d < s) {
        /* 正向复制 (dst 在 src 前方, 不会覆盖) */
        while (n--)
            *d++ = *s++;
    } else if (d > s) {
        /* 反向复制 (dst 在 src 后方, 避免覆盖) */
        d += n;
        s += n;
        while (n--)
            *--d = *--s;
    }
    return dst;
}

/*
 * memcmp — 内存比较
 * @s1: 第一个缓冲区
 * @s2: 第二个缓冲区
 * @n:  比较字节数
 * 返回: 0=相等, <0=s1<s2, >0=s1>s2
 */
int memcmp(const void *s1, const void *s2, size_t n)
{
    const uint8_t *p1 = (const uint8_t *)s1;
    const uint8_t *p2 = (const uint8_t *)s2;
    while (n--) {
        if (*p1 != *p2)
            return (int)*p1 - (int)*p2;
        p1++;
        p2++;
    }
    return 0;
}

/*
 * strcpy — 字符串复制
 * @dst: 目标缓冲区
 * @src: 源字符串
 * 返回: dst
 */
char *strcpy(char *dst, const char *src)
{
    char *d = dst;
    while ((*d++ = *src++))
        ;
    return dst;
}

/*
 * strcmp — 字符串比较
 * @s1: 第一个字符串
 * @s2: 第二个字符串
 * 返回: 0=相等, <0=s1<s2, >0=s1>s2
 */
int strcmp(const char *s1, const char *s2)
{
    while (*s1 && *s2) {
        if (*s1 != *s2)
            return (int)*s1 - (int)*s2;
        s1++;
        s2++;
    }
    return (int)*s1 - (int)*s2;
}

/*
 * strncmp — 前 n 字节字符串比较
 * @s1: 第一个字符串
 * @s2: 第二个字符串
 * @n:  比较长度
 * 返回: 0=相等, <0=s1<s2, >0=s1>s2
 */
int strncmp(const char *s1, const char *s2, size_t n)
{
    while (*s1 && *s2 && n > 0) {
        if (*s1 != *s2)
            return (int)*s1 - (int)*s2;
        s1++;
        s2++;
        n--;
    }
    if (n == 0)
        return 0;
    return (int)*s1 - (int)*s2;
}

/*
 * strcat — 字符串连接
 * @dst: 目标缓冲区 (必须有足够空间)
 * @src: 源字符串
 * 返回: dst
 */
char *strcat(char *dst, const char *src)
{
    char *d = dst;
    while (*d)
        d++;
    while ((*d++ = *src++))
        ;
    return dst;
}

/*
 * strncat — 前 n 字节字符串连接
 * @dst: 目标缓冲区
 * @src: 源字符串
 * @n:  最多连接的字符数
 * 返回: dst
 */
char *strncat(char *dst, const char *src, size_t n)
{
    char *d = dst;
    while (*d)
        d++;
    while (n > 0 && *src) {
        *d++ = *src++;
        n--;
    }
    *d = '\0';
    return dst;
}

/*
 * strncpy — 字符串复制 (最多 n 字节)
 * @dst: 目标缓冲区
 * @src: 源字符串
 * @n:  最多复制的字节数
 * 返回: dst
 *
 * 如果 src 长度 < n, 剩余部分用 null 填充。
 */
char *strncpy(char *dst, const char *src, size_t n)
{
    char *d = dst;
    while (n > 0 && *src) {
        *d++ = *src++;
        n--;
    }
    while (n > 0) {
        *d++ = '\0';
        n--;
    }
    return dst;
}

/*
 * strchr — 查找字符首次出现
 * @str: 搜索的字符串
 * @c:   要查找的字符
 * 返回: 指向首次出现位置的指针, 未找到返回 NULL。
 */
char *strchr(const char *str, int c)
{
    while (*str) {
        if (*str == (char)c)
            return (char *)str;
        str++;
    }
    if (c == '\0')
        return (char *)str;
    return (void *)0;
}

/*
 * strrchr — 查找字符最后一次出现
 * @str: 搜索的字符串
 * @c:   要查找的字符
 * 返回: 指向最后出现位置的指针, 未找到返回 NULL。
 */
char *strrchr(const char *str, int c)
{
    const char *last = (void *)0;
    while (*str) {
        if (*str == (char)c)
            last = str;
        str++;
    }
    if (c == '\0')
        return (char *)str;
    return (char *)last;
}

/*
 * strstr — 查找子字符串
 * @haystack: 被搜索的字符串
 * @needle:   要查找的子字符串
 * 返回: 指向首次出现位置的指针, 未找到返回 NULL。
 */
char *strstr(const char *haystack, const char *needle)
{
    if (*needle == '\0')
        return (char *)haystack;

    while (*haystack) {
        const char *h = haystack;
        const char *n = needle;
        while (*h && *n && *h == *n) {
            h++;
            n++;
        }
        if (*n == '\0')
            return (char *)haystack;
        haystack++;
    }
    return (void *)0;
}

/*
 * strcspn — 查找 reject 中任意字符首次出现的位置
 * @s:      搜索的字符串
 * @reject: 包含要查找的字符集
 * 返回: s 中第一个属于 reject 的字符的索引。
 */
size_t strcspn(const char *s, const char *reject)
{
    size_t len = 0;
    while (s[len]) {
        const char *r = reject;
        while (*r) {
            if (s[len] == *r)
                return len;
            r++;
        }
        len++;
    }
    return len;
}
