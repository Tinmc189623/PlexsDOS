/*
 * Nexsteaduser — PlexsDOS
 * math.h — 基础数学函数
 * 作者: Tinmc189623 | 团队: Nexlyh
 *
 * freestanding 环境: 仅提供整数数学和基础浮点。
 * 浮点函数使用 x87 FPU 指令。
 */

#ifndef _LIBC_MATH_H
#define _LIBC_MATH_H

#ifdef __cplusplus
extern "C" {
#endif

/* 数学常量 */
#define M_PI    3.14159265358979323846
#define M_PI_2  1.57079632679489661923
#define M_E     2.71828182845904523536
#define M_LN2   0.69314718055994530942

/* 整数数学 */

/*
 * imin — 两个 int 中较小的
 */
static inline int imin(int a, int b) { return a < b ? a : b; }

/*
 * imax — 两个 int 中较大的
 */
static inline int imax(int a, int b) { return a > b ? a : b; }

/*
 * iclamp — 将值限制在 [lo, hi] 范围内
 */
static inline int iclamp(int val, int lo, int hi)
{
    if (val < lo) return lo;
    if (val > hi) return hi;
    return val;
}

/* 浮点数学 (x87 FPU) */

/*
 * fabs — 浮点绝对值
 */
double fabs(double x);

/*
 * sqrt — 平方根 (x87 FSQRT)
 */
double sqrt(double x);

/*
 * floor — 向下取整
 */
double floor(double x);

/*
 * ceil — 向上取整
 */
double ceil(double x);

/*
 * sin — 正弦 (x87 FSIN)
 */
double sin(double x);

/*
 * cos — 余弦 (x87 FCOS)
 */
double cos(double x);

/*
 * tan — 正切
 */
double tan(double x);

/*
 * fmod — 浮点取模
 */
double fmod(double x, double y);

/*
 * pow — 幂运算 (整数指数快速路径)
 */
double pow(double base, int exp);

#ifdef __cplusplus
}
#endif

#endif /* _LIBC_MATH_H */
