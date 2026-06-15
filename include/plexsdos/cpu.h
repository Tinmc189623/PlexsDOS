/*
 * Nexsteaduser — PlexsDOS
 * CPU 检测与通用处理器优化接口
 * 作者: Tinmc189623 | 团队: Nexlyh
 *
 * 支持从 i686 到现代处理器的全系列特性检测。
 * 通过 CPUID 指令检测可用特性, 运行时分派最优代码路径。
 */

#ifndef _PLXSDOS_CPU_H
#define _PLXSDOS_CPU_H

#include <plexsdos/types.h>

#ifdef __cplusplus
extern "C" {
#endif

/* ===== CPU 特性标志 (CPUID leaf 1 EDX) ===== */
#define CPU_FEATURE_FPU     (1 << 0)    /* x87 FPU */
#define CPU_FEATURE_PSE     (1 << 3)    /* Page Size Extension (4MB pages) */
#define CPU_FEATURE_TSC     (1 << 4)    /* Time Stamp Counter */
#define CPU_FEATURE_CMOV    (1 << 15)   /* Conditional Move (i686+) */
#define CPU_FEATURE_MMX     (1 << 23)   /* MultiMedia Extensions */
#define CPU_FEATURE_SSE     (1 << 25)   /* Streaming SIMD Extensions */
#define CPU_FEATURE_SSE2    (1 << 26)   /* SSE2 */

/* ===== CPU 特性标志 (CPUID leaf 1 ECX) ===== */
#define CPU_FEATURE_SSE3    (1 << 0)    /* SSE3 */
#define CPU_FEATURE_SSSE3   (1 << 9)    /* Supplemental SSE3 */
#define CPU_FEATURE_SSE41   (1 << 19)   /* SSE4.1 */
#define CPU_FEATURE_SSE42   (1 << 20)   /* SSE4.2 */
#define CPU_FEATURE_POPCNT  (1 << 23)   /* POPCNT instruction */
#define CPU_FEATURE_AVX     (1 << 28)   /* Advanced Vector Extensions */
#define CPU_FEATURE_F16C    (1 << 29)   /* Half-precision float conversion */
#define CPU_FEATURE_FMA     (1 << 12)   /* Fused Multiply-Add */

/* ===== CPU 特性标志 (CPUID leaf 7 EBX) ===== */
#define CPU_FEATURE_BMI1    (1 << 3)    /* Bit Manipulation 1 */
#define CPU_FEATURE_AVX2    (1 << 5)    /* AVX2 */
#define CPU_FEATURE_BMI2    (1 << 8)    /* Bit Manipulation 2 */
#define CPU_FEATURE_ERMSB   (1 << 9)    /* Enhanced REP MOVSB/STOSB */

/* ===== CPU 特性标志 (CPUID extended 0x80000001 EDX) ===== */
#define CPU_FEATURE_3DNOW   (1 << 31)   /* AMD 3DNow! */
#define CPU_FEATURE_3DNOWP  (1 << 30)   /* AMD 3DNow! Professional */

/* ===== CPU 特性标志 (CPUID extended 0x80000001 ECX) ===== */
#define CPU_FEATURE_SSE4A   (1 << 6)    /* AMD SSE4a */
#define CPU_FEATURE_ABM     (1 << 5)    /* AMD Advanced Bit Manipulation (LZCNT) */

/* ===== CR4 寄存器位 ===== */
#define CR4_PSE         (1 << 4)    /* Page Size Extension (4MB pages) */
#define CR4_OSFXSR      (1 << 9)    /* OS FXSAVE/FXRSTOR 支持 */
#define CR4_OSXMMEXCPT  (1 << 10)   /* OS SIMD 异常处理 */
#define CR4_OSXSAVE     (1 << 18)   /* OS XSAVE 支持 (AVX 需要) */

/* ===== 特性组查询 ===== */
/* 运行时分派用: 查询特性组是否完整支持 */
#define CPU_GROUP_SSE       (CPU_FEATURE_SSE)
#define CPU_GROUP_SSE2      (CPU_FEATURE_SSE | CPU_FEATURE_SSE2)
#define CPU_GROUP_SSE3      (CPU_FEATURE_SSE | CPU_FEATURE_SSE2 | CPU_FEATURE_SSE3)
#define CPU_GROUP_SSSE3     (CPU_GROUP_SSE3 | CPU_FEATURE_SSSE3)
#define CPU_GROUP_SSE41     (CPU_GROUP_SSSE3 | CPU_FEATURE_SSE41)
#define CPU_GROUP_SSE42     (CPU_GROUP_SSE41 | CPU_FEATURE_SSE42)
#define CPU_GROUP_AVX       (CPU_GROUP_SSE42 | CPU_FEATURE_AVX)
#define CPU_GROUP_AVX2      (CPU_GROUP_AVX | CPU_FEATURE_AVX2)

/*
 * cpu_init — 初始化 CPU 特性
 * 检测并启用可用的 SIMD 扩展 (SSE/AVX 等)。
 * 必须在使用任何 SIMD 指令前调用。
 */
void cpu_init(void);

/*
 * cpu_has_feature — 检测 CPU 是否支持指定特性
 * @feature: CPU_FEATURE_* 标志位
 * 返回: true = 支持, false = 不支持。
 */
bool cpu_has_feature(uint32_t feature);

/*
 * cpu_has_group — 检测 CPU 是否支持完整特性组
 * @group: CPU_GROUP_* 组合标志
 * 返回: true = 全部支持, false = 部分或全部不支持。
 */
bool cpu_has_group(uint32_t group);

/*
 * cpu_get_features_edx — 获取 CPUID leaf 1 EDX 特性
 * 返回: EDX 寄存器值。
 */
uint32_t cpu_get_features_edx(void);

/*
 * cpu_get_features_ecx — 获取 CPUID leaf 1 ECX 特性
 * 返回: ECX 寄存器值。
 */
uint32_t cpu_get_features_ecx(void);

/*
 * cpu_get_vendor — 获取 CPU 厂商字符串
 * @buf: 至少 13 字节的缓冲区
 */
void cpu_get_vendor(char *buf);

/*
 * cpu_get_brand — 获取 CPU 品牌字符串
 * @buf: 至少 49 字节的缓冲区
 */
void cpu_get_brand(char *buf);

/*
 * cpu_rdtsc — 读取时间戳计数器 (TSC)
 * 返回: 64-bit TSC 值。
 */
uint64_t cpu_rdtsc(void);

/*
 * cpu_get_tsc_freq — 估算 TSC 频率 (MHz)
 * 返回: 估算频率, 0 表示无法估算。
 */
uint32_t cpu_get_tsc_freq(void);

#ifdef __cplusplus
}
#endif

#endif /* _PLXSDOS_CPU_H */
