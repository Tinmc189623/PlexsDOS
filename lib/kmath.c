/*
 * Nexsteaduser — PlexsDOS
 * math.h 实现 — 浮点数学函数 (x87 FPU)
 * 作者: Tinmc189623 | 团队: Nexlyh
 *
 * 使用 x87 FPU 指令实现浮点运算。
 * 所有函数通过 FPU 栈传递和返回 double 值。
 */

#include <libc/math.h>
#include <plexsdos/types.h>

/*
 * fabs — 浮点绝对值
 * @x: 输入值
 * 返回: |x|。
 */
double fabs(double x)
{
    double result;
    __asm__ __volatile__(
        "fldl %1\n\t"
        "fabs\n\t"
        "fstpl %0"
        : "=m"(result)
        : "m"(x)
    );
    return result;
}

/*
 * sqrt — 平方根 (x87 FSQRT)
 * @x: 输入值 (必须 >= 0)
 * 返回: sqrt(x)。
 */
double sqrt(double x)
{
    double result;
    __asm__ __volatile__(
        "fldl %1\n\t"
        "fsqrt\n\t"
        "fstpl %0"
        : "=m"(result)
        : "m"(x)
    );
    return result;
}

/*
 * floor — 向下取整
 * @x: 输入值
 * 返回: 不大于 x 的最大整数。
 */
double floor(double x)
{
    double result;
    uint16_t cw;
    uint32_t new_cw;
    __asm__ __volatile__(
        "fnstcw %0\n\t"
        "movzwl %0, %1\n\t"
        "orl $0x0400, %1\n\t"    /* 设置 RC = 向下取整 */
        "andl $0xF7FF, %1\n\t"
        "movw %w1, %0\n\t"
        "fldcw %0\n\t"
        "fldl %2\n\t"
        "frndint\n\t"
        "fstpl %3\n\t"
        "fnstcw %0\n\t"
        : "=m"(cw), "=r"(new_cw)
        : "m"(x), "m"(result)
        : "memory"
    );
    return result;
}

/*
 * ceil — 向上取整
 * @x: 输入值
 * 返回: 不小于 x 的最小整数。
 */
double ceil(double x)
{
    double result;
    uint16_t cw;
    uint32_t new_cw;
    __asm__ __volatile__(
        "fnstcw %0\n\t"
        "movzwl %0, %1\n\t"
        "orl $0x0800, %1\n\t"    /* 设置 RC = 向上取整 */
        "andl $0xFBFF, %1\n\t"
        "movw %w1, %0\n\t"
        "fldcw %0\n\t"
        "fldl %2\n\t"
        "frndint\n\t"
        "fstpl %3\n\t"
        "fnstcw %0\n\t"
        : "=m"(cw), "=r"(new_cw)
        : "m"(x), "m"(result)
        : "memory"
    );
    return result;
}

/*
 * sin — 正弦 (x87 FSIN)
 * @x: 弧度值
 * 返回: sin(x)。
 */
double sin(double x)
{
    double result;
    __asm__ __volatile__(
        "fldl %1\n\t"
        "fsin\n\t"
        "fstpl %0"
        : "=m"(result)
        : "m"(x)
    );
    return result;
}

/*
 * cos — 余弦 (x87 FCOS)
 * @x: 弧度值
 * 返回: cos(x)。
 */
double cos(double x)
{
    double result;
    __asm__ __volatile__(
        "fldl %1\n\t"
        "fcos\n\t"
        "fstpl %0"
        : "=m"(result)
        : "m"(x)
    );
    return result;
}

/*
 * tan — 正切 (x87 FPTAN)
 * @x: 弧度值
 * 返回: tan(x)。
 */
double tan(double x)
{
    double result;
    __asm__ __volatile__(
        "fldl %1\n\t"
        "fptan\n\t"
        "fstp %%st(0)\n\t"   /* 弹出 FPU 栈上的 1.0 */
        "fstpl %0"
        : "=m"(result)
        : "m"(x)
    );
    return result;
}

/*
 * fmod — 浮点取模
 * @x: 被除数
 * @y: 除数
 * 返回: x mod y (使用 x87 FPREM)。
 */
double fmod(double x, double y)
{
    double result;
    __asm__ __volatile__(
        "fldl %2\n\t"
        "fldl %1\n\t"
        "1:\n\t"
        "fprem\n\t"
        "fnstsw %%ax\n\t"
        "sahf\n\t"
        "jp 1b\n\t"           /* 如果 C2=1, 继续部分余数 */
        "fstp %%st(1)\n\t"    /* 弹出除数 */
        "fstpl %0"
        : "=m"(result)
        : "m"(x), "m"(y)
        : "ax", "memory"
    );
    return result;
}

/*
 * pow — 幂运算 (整数指数快速路径)
 * @base: 底数
 * @exp:  指数 (int)
 * 返回: base^exp。
 */
double pow(double base, int exp)
{
    double result = 1.0;
    int neg = 0;

    if (exp < 0) {
        neg = 1;
        exp = -exp;
    }

    /* 快速幂算法 */
    while (exp > 0) {
        if (exp & 1)
            result *= base;
        base *= base;
        exp >>= 1;
    }

    if (neg)
        result = 1.0 / result;

    return result;
}
