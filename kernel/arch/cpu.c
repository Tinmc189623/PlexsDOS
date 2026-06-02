/*
 * Nexsteaduser — PlexsDOS
 * CPU 检测与通用处理器优化实现
 * 作者: Tinmc189623 | 团队: Nexlyh
 *
 * 通过 CPUID 检测 CPU 特性, 启用可用的 SIMD 扩展。
 * 支持从 i686 到现代处理器 (SSE2/AVX2) 的全系列特性检测。
 * 运行时分派: 根据检测结果选择最优代码路径。
 */

#include <plexsdos/types.h>
#include <plexsdos/cpu.h>
#include <plexsdos/screen.h>

/* CPUID leaf 1 特性 (EDX 和 ECX) */
static uint32_t features_edx = 0;
static uint32_t features_ecx = 0;

/* CPUID leaf 7 特性 (EBX) */
static uint32_t features_leaf7_ebx = 0;

/* CPUID extended 0x80000001 特性 */
static uint32_t features_ext_edx = 0;
static uint32_t features_ext_ecx = 0;

/* 最大标准/扩展 CPUID 叶子号 */
static uint32_t max_leaf = 0;
static uint32_t max_ext_leaf = 0;

/*
 * cpuid — 执行 CPUID 指令
 * @eax_in: EAX 输入值 (功能号)
 * @eax: EAX 输出指针
 * @ebx: EBX 输出指针
 * @ecx: ECX 输出指针
 * @edx: EDX 输出指针
 */
static void cpuid(uint32_t eax_in,
                  uint32_t *eax, uint32_t *ebx,
                  uint32_t *ecx, uint32_t *edx)
{
    __asm__ __volatile__(
        "cpuid"
        : "=a"(*eax), "=b"(*ebx), "=c"(*ecx), "=d"(*edx)
        : "a"(eax_in)
    );
}

/*
 * cpu_get_vendor — 获取 CPU 厂商字符串
 * @buf: 至少 13 字节的缓冲区
 *
 * 通过 CPUID 功能 0 获取厂商字符串。
 */
void cpu_get_vendor(char *buf)
{
    uint32_t eax, ebx, ecx, edx;
    cpuid(0, &eax, &ebx, &ecx, &edx);
    *(uint32_t *)(buf + 0) = ebx;
    *(uint32_t *)(buf + 4) = edx;
    *(uint32_t *)(buf + 8) = ecx;
    buf[12] = '\0';
    max_leaf = eax;
}

/*
 * cpu_get_brand — 获取 CPU 品牌字符串
 * @buf: 至少 49 字节的缓冲区
 *
 * 通过 CPUID 功能 0x80000002-0x80000004 获取品牌字符串。
 */
void cpu_get_brand(char *buf)
{
    uint32_t eax, ebx, ecx, edx;
    uint32_t i;

    /* 检查是否支持扩展 CPUID */
    cpuid(0x80000000, &eax, &ebx, &ecx, &edx);
    max_ext_leaf = eax;

    if (max_ext_leaf < 0x80000004) {
        /* 不支持品牌字符串 */
        buf[0] = '\0';
        return;
    }

    for (i = 0; i < 3; i++) {
        cpuid(0x80000002 + i, &eax, &ebx, &ecx, &edx);
        *(uint32_t *)(buf + i * 16 + 0) = eax;
        *(uint32_t *)(buf + i * 16 + 4) = ebx;
        *(uint32_t *)(buf + i * 16 + 8) = ecx;
        *(uint32_t *)(buf + i * 16 + 12) = edx;
    }
    buf[48] = '\0';
}

/*
 * cpu_has_feature — 检测 CPU 是否支持指定特性
 * @feature: CPU_FEATURE_* 标志位
 *
 * 根据特性位所在的 CPUID 叶子号自动选择正确的寄存器。
 * 返回: true = 支持, false = 不支持。
 */
bool cpu_has_feature(uint32_t feature)
{
    /* CPUID leaf 1 EDX 特性 */
    if (feature == CPU_FEATURE_FPU || feature == CPU_FEATURE_TSC ||
        feature == CPU_FEATURE_CMOV || feature == CPU_FEATURE_MMX ||
        feature == CPU_FEATURE_SSE || feature == CPU_FEATURE_SSE2) {
        return (features_edx & feature) != 0 ? true : false;
    }
    if (feature == CPU_FEATURE_SSE3 || feature == CPU_FEATURE_SSSE3 ||
        feature == CPU_FEATURE_SSE41 || feature == CPU_FEATURE_SSE42 ||
        feature == CPU_FEATURE_POPCNT || feature == CPU_FEATURE_AVX ||
        feature == CPU_FEATURE_F16C || feature == CPU_FEATURE_FMA) {
        return (features_ecx & feature) != 0 ? true : false;
    }
    if (feature == CPU_FEATURE_BMI1 || feature == CPU_FEATURE_AVX2 ||
        feature == CPU_FEATURE_BMI2 || feature == CPU_FEATURE_ERMSB) {
        return (features_leaf7_ebx & feature) != 0 ? true : false;
    }
    if (feature == CPU_FEATURE_3DNOW || feature == CPU_FEATURE_3DNOWP) {
        return (features_ext_edx & feature) != 0 ? true : false;
    }
    if (feature == CPU_FEATURE_SSE4A || feature == CPU_FEATURE_ABM) {
        return (features_ext_ecx & feature) != 0 ? true : false;
    }
    return false;
}

/*
 * cpu_has_group — 检测 CPU 是否支持完整特性组
 * @group: CPU_GROUP_* 组合标志 (多个 CPU_FEATURE_* 的 OR)
 *
 * 检查组中每个特性位是否都被支持。
 * 返回: true = 全部支持, false = 部分或全部不支持。
 */
bool cpu_has_group(uint32_t group)
{
    /* 将组标志拆分为各叶子的子集 */
    uint32_t edx_mask = group & (CPU_FEATURE_FPU | CPU_FEATURE_TSC |
                                  CPU_FEATURE_CMOV | CPU_FEATURE_MMX |
                                  CPU_FEATURE_SSE | CPU_FEATURE_SSE2);
    uint32_t ecx_mask = group & (CPU_FEATURE_SSE3 | CPU_FEATURE_SSSE3 |
                                  CPU_FEATURE_SSE41 | CPU_FEATURE_SSE42 |
                                  CPU_FEATURE_POPCNT | CPU_FEATURE_AVX |
                                  CPU_FEATURE_F16C | CPU_FEATURE_FMA);
    uint32_t ebx_mask = group & (CPU_FEATURE_BMI1 | CPU_FEATURE_AVX2 |
                                  CPU_FEATURE_BMI2 | CPU_FEATURE_ERMSB);

    if (edx_mask && (features_edx & edx_mask) != edx_mask)
        return false;
    if (ecx_mask && (features_ecx & ecx_mask) != ecx_mask)
        return false;
    if (ebx_mask && (features_leaf7_ebx & ebx_mask) != ebx_mask)
        return false;
    return true;
}

/*
 * cpu_get_features_edx — 获取 CPUID leaf 1 EDX 特性
 * 返回: EDX 寄存器值。
 */
uint32_t cpu_get_features_edx(void)
{
    return features_edx;
}

/*
 * cpu_get_features_ecx — 获取 CPUID leaf 1 ECX 特性
 * 返回: ECX 寄存器值。
 */
uint32_t cpu_get_features_ecx(void)
{
    return features_ecx;
}

/*
 * cpu_rdtsc — 读取时间戳计数器 (TSC)
 * 返回: 64-bit TSC 值。
 */
uint64_t cpu_rdtsc(void)
{
    uint32_t lo, hi;
    __asm__ __volatile__(
        "rdtsc"
        : "=a"(lo), "=d"(hi)
    );
    return ((uint64_t)hi << 32) | lo;
}

/*
 * fpu_init — 初始化 x87 FPU
 *
 * 执行 FINIT 指令重置 FPU 状态。
 */
static void fpu_init(void)
{
    __asm__ __volatile__("finit");
}

/*
 * sse_enable — 启用 SSE 支持
 *
 * 设置 CR4.OSFXSR 和 CR4.OSXMMEXCPT 位。
 */
static void sse_enable(void)
{
    uint32_t cr4;

    __asm__ __volatile__("mov %%cr4, %0" : "=r"(cr4));
    cr4 |= CR4_OSFXSR | CR4_OSXMMEXCPT;
    __asm__ __volatile__("mov %0, %%cr4" : : "r"(cr4));
}

/*
 * avx_enable — 启用 AVX 支持
 *
 * 设置 CR4.OSXSAVE, 然后通过 XSETBV 启用 XMM/YMM 状态保存。
 */
static void avx_enable(void)
{
    uint32_t cr4;
    uint32_t xcr0_lo, xcr0_hi;

    /* 设置 CR4.OSXSAVE */
    __asm__ __volatile__("mov %%cr4, %0" : "=r"(cr4));
    cr4 |= CR4_OSXSAVE;
    __asm__ __volatile__("mov %0, %%cr4" : : "r"(cr4));

    /* 读取 XCR0 */
    __asm__ __volatile__(
        "xgetbv"
        : "=a"(xcr0_lo), "=d"(xcr0_hi)
        : "c"(0)
    );

    /* 启用 X87 + SSE + AVX 状态 (XMM + YMM) */
    xcr0_lo |= 0x07;  /* bit 0: X87, bit 1: SSE, bit 2: AVX */
    __asm__ __volatile__(
        "xsetbv"
        :
        : "a"(xcr0_lo), "d"(xcr0_hi), "c"(0)
    );
}

/*
 * cpu_init — 初始化 CPU 特性
 *
 * 全面检测 CPUID, 获取所有特性标志, 启用可用的 SIMD 扩展。
 * 支持从 i686 到现代处理器的全系列。
 */
void cpu_init(void)
{
    uint32_t eax, ebx, ecx, edx;
    char vendor[13];
    char brand[49];

    /* 获取厂商和最大叶子号 */
    cpu_get_vendor(vendor);

    /* 获取 leaf 1 特性 */
    if (max_leaf >= 1) {
        cpuid(1, &eax, &ebx, &ecx, &edx);
        features_edx = edx;
        features_ecx = ecx;
    }

    /* 获取 leaf 7 特性 */
    if (max_leaf >= 7) {
        cpuid(7, &eax, &ebx, &ecx, &edx);
        features_leaf7_ebx = ebx;
    }

    /* 获取扩展特性 */
    if (max_ext_leaf >= 0x80000001) {
        cpuid(0x80000001, &eax, &ebx, &ecx, &edx);
        features_ext_edx = edx;
        features_ext_ecx = ecx;
    }

    /* 初始化 FPU */
    fpu_init();

    /* 启用 SSE (如果支持) */
    if (features_edx & CPU_FEATURE_SSE) {
        sse_enable();
    }

    /* 启用 AVX (如果支持) */
    if ((features_ecx & CPU_FEATURE_AVX) && (features_ecx & (1 << 27))) {
        /* CPUID.1:ECX.OSXSAVE[bit 27] 表示 OS 是否已设置 OSXSAVE */
        /* 如果 CPU 支持 AVX 且 OSXSAVE 已设置, 启用 AVX 状态 */
        uint32_t cr4_check;
        __asm__ __volatile__("mov %%cr4, %0" : "=r"(cr4_check));
        if (cr4_check & CR4_OSXSAVE) {
            avx_enable();
        }
    }

    /* 获取品牌字符串 */
    cpu_get_brand(brand);

    /* 显示 CPU 信息 */
    screen_set_color(0x0E, 0x00);
    screen_puts("[cpu] Vendor: ");
    screen_puts(vendor);
    screen_putchar('\n');

    if (brand[0] != '\0') {
        screen_puts("[cpu] Brand: ");
        screen_puts(brand);
        screen_putchar('\n');
    }

    screen_puts("[cpu] Features:");
    if (features_edx & CPU_FEATURE_FPU)  screen_puts(" FPU");
    if (features_edx & CPU_FEATURE_TSC)  screen_puts(" TSC");
    if (features_edx & CPU_FEATURE_CMOV) screen_puts(" CMOV");
    if (features_edx & CPU_FEATURE_MMX)  screen_puts(" MMX");
    if (features_edx & CPU_FEATURE_SSE)  screen_puts(" SSE");
    if (features_edx & CPU_FEATURE_SSE2) screen_puts(" SSE2");
    if (features_ecx & CPU_FEATURE_SSE3) screen_puts(" SSE3");
    if (features_ecx & CPU_FEATURE_SSSE3)screen_puts(" SSSE3");
    if (features_ecx & CPU_FEATURE_SSE41)screen_puts(" SSE4.1");
    if (features_ecx & CPU_FEATURE_SSE42)screen_puts(" SSE4.2");
    if (features_ecx & CPU_FEATURE_AVX)  screen_puts(" AVX");
    if (features_leaf7_ebx & CPU_FEATURE_AVX2)  screen_puts(" AVX2");
    if (features_leaf7_ebx & CPU_FEATURE_ERMSB) screen_puts(" ERMSB");
    if (features_ext_edx & CPU_FEATURE_3DNOW)   screen_puts(" 3DNow!");
    if (features_ext_ecx & CPU_FEATURE_SSE4A)   screen_puts(" SSE4a");
    screen_putchar('\n');

    screen_set_color(0x07, 0x00);
}
