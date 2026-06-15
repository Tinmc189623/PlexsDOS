/*
 * Nexsteaduser — PlexsDOS
 * 通用处理器优化内存操作
 * 作者: Tinmc189623 | 团队: Nexlyh
 *
 * 运行时分派: 根据 CPU 检测结果选择最优代码路径。
 * - 基线: REP MOVSD/STOSD (所有 32-bit x86 处理器)
 * - SSE2: MOVDQA 128-bit 块操作 (奔腾4+)
 * - AVX:  VMOVDQA 256-bit 块操作 (Sandy Bridge+)
 *
 * 使用 target 属性启用 SIMD 寄存器, 实际指令在运行时检测后执行。
 * 分派结果缓存为函数指针, 避免每次调用的分支开销。
 */

#include <plexsdos/types.h>
#include <plexsdos/cpu.h>

/* 函数指针类型定义 */
typedef void *(*memcpy_fn_t)(void *, const void *, size_t);
typedef void *(*memset_fn_t)(void *, int, size_t);
typedef int   (*memcmp_fn_t)(const void *, const void *, size_t);

/* 缓存的实现函数指针 (由 fast_mem_init 设置, 之后只读) */
static memcpy_fn_t memcpy_impl = NULL;
static memset_fn_t memset_impl = NULL;
static memcmp_fn_t memcmp_impl = NULL;

/* 基线路径: REP MOVSD (所有 i486+ 处理器) */
static void *memcpy_baseline(void *dst, const void *src, size_t n)
{
    uint8_t *d = (uint8_t *)dst;
    const uint8_t *s = (const uint8_t *)src;

    if (n < 4) {
        while (n--) *d++ = *s++;
        return dst;
    }

    while ((size_t)d & 3) {
        *d++ = *s++;
        n--;
    }

    if (n >= 4) {
        size_t words = n / 4;
        __asm__ __volatile__(
            "rep movsl"
            : "+D"(d), "+S"(s), "+c"(words)
            :
            : "memory"
        );
        n &= 3;
    }

    while (n--) *d++ = *s++;
    return dst;
}

/* SSE2 路径: 128-bit 块拷贝 */
__attribute__((target("sse2")))
static void *memcpy_sse2(void *dst, const void *src, size_t n)
{
    uint8_t *d = (uint8_t *)dst;
    const uint8_t *s = (const uint8_t *)src;

    /* 对齐到 16 字节 */
    while (n > 0 && ((size_t)d & 15)) {
        *d++ = *s++;
        n--;
    }

    while (n >= 128) {
        __asm__ __volatile__(
            "movdqa (%1), %%xmm0\n\t"
            "movdqa 16(%1), %%xmm1\n\t"
            "movdqa 32(%1), %%xmm2\n\t"
            "movdqa 48(%1), %%xmm3\n\t"
            "movdqa 64(%1), %%xmm4\n\t"
            "movdqa 80(%1), %%xmm5\n\t"
            "movdqa 96(%1), %%xmm6\n\t"
            "movdqa 112(%1), %%xmm7\n\t"
            "movdqa %%xmm0, (%0)\n\t"
            "movdqa %%xmm1, 16(%0)\n\t"
            "movdqa %%xmm2, 32(%0)\n\t"
            "movdqa %%xmm3, 48(%0)\n\t"
            "movdqa %%xmm4, 64(%0)\n\t"
            "movdqa %%xmm5, 80(%0)\n\t"
            "movdqa %%xmm6, 96(%0)\n\t"
            "movdqa %%xmm7, 112(%0)\n\t"
            :
            : "r"(d), "r"(s)
            : "memory", "xmm0", "xmm1", "xmm2", "xmm3",
              "xmm4", "xmm5", "xmm6", "xmm7"
        );
        d += 128;
        s += 128;
        n -= 128;
    }

    while (n >= 16) {
        __asm__ __volatile__(
            "movdqu (%1), %%xmm0\n\t"
            "movdqu %%xmm0, (%0)\n\t"
            :
            : "r"(d), "r"(s)
            : "memory", "xmm0"
        );
        d += 16;
        s += 16;
        n -= 16;
    }

    /* 剩余字节 */
    while (n--) *d++ = *s++;
    return dst;
}

/* AVX 路径: 256-bit 块拷贝 */
__attribute__((target("avx")))
static void *memcpy_avx(void *dst, const void *src, size_t n)
{
    uint8_t *d = (uint8_t *)dst;
    const uint8_t *s = (const uint8_t *)src;

    /* 对齐到 32 字节 */
    while (n > 0 && ((size_t)d & 31)) {
        *d++ = *s++;
        n--;
    }

    while (n >= 256) {
        __asm__ __volatile__(
            "vmovdqa (%1), %%ymm0\n\t"
            "vmovdqa 32(%1), %%ymm1\n\t"
            "vmovdqa 64(%1), %%ymm2\n\t"
            "vmovdqa 96(%1), %%ymm3\n\t"
            "vmovdqa 128(%1), %%ymm4\n\t"
            "vmovdqa 160(%1), %%ymm5\n\t"
            "vmovdqa 192(%1), %%ymm6\n\t"
            "vmovdqa 224(%1), %%ymm7\n\t"
            "vmovdqa %%ymm0, (%0)\n\t"
            "vmovdqa %%ymm1, 32(%0)\n\t"
            "vmovdqa %%ymm2, 64(%0)\n\t"
            "vmovdqa %%ymm3, 96(%0)\n\t"
            "vmovdqa %%ymm4, 128(%0)\n\t"
            "vmovdqa %%ymm5, 160(%0)\n\t"
            "vmovdqa %%ymm6, 192(%0)\n\t"
            "vmovdqa %%ymm7, 224(%0)\n\t"
            :
            : "r"(d), "r"(s)
            : "memory", "ymm0", "ymm1", "ymm2", "ymm3",
              "ymm4", "ymm5", "ymm6", "ymm7"
        );
        d += 256;
        s += 256;
        n -= 256;
    }

    while (n >= 32) {
        __asm__ __volatile__(
            "vmovdqa (%1), %%ymm0\n\t"
            "vmovdqa %%ymm0, (%0)\n\t"
            :
            : "r"(d), "r"(s)
            : "memory", "ymm0"
        );
        d += 32;
        s += 32;
        n -= 32;
    }

    __asm__ __volatile__("vzeroupper" ::: "memory");

    while (n--) *d++ = *s++;
    return dst;
}

/* 前向声明: 各 SIMD 路径的静态实现 (定义在 fast_mem_init 之后) */
static void *memcpy_sse2(void *dst, const void *src, size_t n);
static void *memcpy_avx(void *dst, const void *src, size_t n);
static void *memset_baseline(void *dst, int c, size_t n);
static void *memset_sse2(void *dst, int c, size_t n);
static void *memset_avx(void *dst, int c, size_t n);
static int memcmp_sse2(const void *s1, const void *s2, size_t n);

/*
 * fast_mem_init — 初始化内存操作分派
 *
 * 在 cpu_init() 之后调用一次, 缓存最优实现函数指针。
 * 之后 fast_memcpy/memset/memcmp 直接通过指针调用, 零分支开销。
 */
void fast_mem_init(void)
{
    if (cpu_has_group(CPU_GROUP_AVX)) {
        memcpy_impl = memcpy_avx;
        memset_impl = memset_avx;
    } else if (cpu_has_feature(CPU_FEATURE_SSE2)) {
        memcpy_impl = memcpy_sse2;
        memset_impl = memset_sse2;
    } else {
        memcpy_impl = memcpy_baseline;
        memset_impl = memset_baseline;
    }

    if (cpu_has_feature(CPU_FEATURE_SSE2))
        memcmp_impl = memcmp_sse2;
    else
        memcmp_impl = NULL;  /* 使用内联基线 */
}

/*
 * fast_memcpy — 优化内存拷贝
 * @dst: 目标地址
 * @src: 源地址
 * @n: 字节数
 *
 * 通过缓存的函数指针零开销分派。
 * 返回: dst 指针。
 */
void *fast_memcpy(void *dst, const void *src, size_t n)
{
    return memcpy_impl(dst, src, n);
}

/*
 * fast_memset — 优化内存填充
 * @dst: 目标地址
 * @c: 填充值
 * @n: 字节数
 *
 * 通过缓存的函数指针零开销分派。
 * 返回: dst 指针。
 */
void *fast_memset(void *dst, int c, size_t n)
{
    return memset_impl(dst, c, n);
}

/*
 * fast_memcmp — 优化内存比较
 * @s1: 第一个缓冲区
 * @s2: 第二个缓冲区
 * @n: 比较字节数
 *
 * 返回: 0 = 相等, 非零 = 不等。
 */
int fast_memcmp(const void *s1, const void *s2, size_t n)
{
    if (memcmp_impl && n >= 16)
        return memcmp_impl(s1, s2, n);

    /* 内联基线 (通常被 fast_mem_init 设置后走上面的路径) */
    const uint8_t *p1 = (const uint8_t *)s1;
    const uint8_t *p2 = (const uint8_t *)s2;

    while (n >= 4) {
        uint32_t a = *(const uint32_t *)p1;
        uint32_t b = *(const uint32_t *)p2;
        if (a != b) break;
        p1 += 4;
        p2 += 4;
        n -= 4;
    }

    while (n--) {
        if (*p1 != *p2)
            return (int)*p1 - (int)*p2;
        p1++;
        p2++;
    }
    return 0;
}

/* SSE2 路径: 16字节比较 */
__attribute__((target("sse2")))
static int memcmp_sse2(const void *s1, const void *s2, size_t n)
{
    const uint8_t *p1 = (const uint8_t *)s1;
    const uint8_t *p2 = (const uint8_t *)s2;

    while (n >= 16) {
        uint32_t result;
        __asm__ __volatile__(
            "movdqu (%1), %%xmm0\n\t"
            "movdqu (%2), %%xmm1\n\t"
            "pcmpeqb %%xmm1, %%xmm0\n\t"
            "pmovmskb %%xmm0, %0\n\t"
            : "=r"(result)
            : "r"(p1), "r"(p2)
            : "xmm0", "xmm1"
        );
        if (result != 0xFFFF)
            break;
        p1 += 16;
        p2 += 16;
        n -= 16;
    }

    while (n--) {
        if (*p1 != *p2)
            return (int)*p1 - (int)*p2;
        p1++;
        p2++;
    }
    return 0;
}

/* 基线路径: REP STOSD */
static void *memset_baseline(void *dst, int c, size_t n)
{
    uint8_t *d = (uint8_t *)dst;
    uint8_t val = (uint8_t)c;

    if (n < 4) {
        while (n--) *d++ = val;
        return dst;
    }

    while ((size_t)d & 3) {
        *d++ = val;
        n--;
    }

    uint32_t pattern32 = val | ((uint32_t)val << 8) |
                         ((uint32_t)val << 16) | ((uint32_t)val << 24);

    if (n >= 4) {
        size_t words = n / 4;
        __asm__ __volatile__(
            "rep stosl"
            : "+D"(d), "+c"(words)
            : "a"(pattern32)
            : "memory"
        );
        n &= 3;
    }

    while (n--) *d++ = val;
    return dst;
}

/* SSE2 路径: 128-bit 块填充 */
__attribute__((target("sse2")))
static void *memset_sse2(void *dst, int c, size_t n)
{
    uint8_t *d = (uint8_t *)dst;
    uint8_t val = (uint8_t)c;
    uint32_t pattern32 = val | ((uint32_t)val << 8) |
                         ((uint32_t)val << 16) | ((uint32_t)val << 24);

    while (n > 0 && ((size_t)d & 15)) {
        *d++ = val;
        n--;
    }

    __asm__ __volatile__(
        "movd %0, %%xmm0\n\t"
        "punpcklbw %%xmm0, %%xmm0\n\t"
        "punpcklwd %%xmm0, %%xmm0\n\t"
        "pshufd $0, %%xmm0, %%xmm0\n\t"
        :
        : "r"(pattern32)
        : "xmm0"
    );

    while (n >= 128) {
        __asm__ __volatile__(
            "movdqa %%xmm0, (%0)\n\t"
            "movdqa %%xmm0, 16(%0)\n\t"
            "movdqa %%xmm0, 32(%0)\n\t"
            "movdqa %%xmm0, 48(%0)\n\t"
            "movdqa %%xmm0, 64(%0)\n\t"
            "movdqa %%xmm0, 80(%0)\n\t"
            "movdqa %%xmm0, 96(%0)\n\t"
            "movdqa %%xmm0, 112(%0)\n\t"
            :
            : "r"(d)
            : "memory"
        );
        d += 128;
        n -= 128;
    }

    while (n >= 16) {
        __asm__ __volatile__(
            "movdqa %%xmm0, (%0)\n\t"
            :
            : "r"(d)
            : "memory"
        );
        d += 16;
        n -= 16;
    }

    while (n--) *d++ = val;
    return dst;
}

/* AVX 路径: 256-bit 块填充 */
__attribute__((target("avx")))
static void *memset_avx(void *dst, int c, size_t n)
{
    uint8_t *d = (uint8_t *)dst;
    uint8_t val = (uint8_t)c;
    uint32_t pattern32 = val | ((uint32_t)val << 8) |
                         ((uint32_t)val << 16) | ((uint32_t)val << 24);

    while (n > 0 && ((size_t)d & 31)) {
        *d++ = val;
        n--;
    }

    __asm__ __volatile__(
        "vmovd %0, %%xmm0\n\t"
        "vpunpcklbw %%xmm0, %%xmm0, %%xmm0\n\t"
        "vpunpcklwd %%xmm0, %%xmm0, %%xmm0\n\t"
        "vpshufd $0, %%xmm0, %%xmm0\n\t"
        "vinsertf128 $1, %%xmm0, %%ymm0, %%ymm0\n\t"
        :
        : "r"(pattern32)
        : "ymm0"
    );

    while (n >= 256) {
        __asm__ __volatile__(
            "vmovdqa %%ymm0, (%0)\n\t"
            "vmovdqa %%ymm0, 32(%0)\n\t"
            "vmovdqa %%ymm0, 64(%0)\n\t"
            "vmovdqa %%ymm0, 96(%0)\n\t"
            "vmovdqa %%ymm0, 128(%0)\n\t"
            "vmovdqa %%ymm0, 160(%0)\n\t"
            "vmovdqa %%ymm0, 192(%0)\n\t"
            "vmovdqa %%ymm0, 224(%0)\n\t"
            :
            : "r"(d)
            : "memory"
        );
        d += 256;
        n -= 256;
    }

    while (n >= 32) {
        __asm__ __volatile__(
            "vmovdqa %%ymm0, (%0)\n\t"
            :
            : "r"(d)
            : "memory"
        );
        d += 32;
        n -= 32;
    }

    __asm__ __volatile__("vzeroupper" ::: "memory");

    while (n--) *d++ = val;
    return dst;
}
