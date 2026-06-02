/*
 * Nexsteaduser — PlexsDOS
 * hal.cpp — 硬件抽象层 C++ 扩展实现
 * 作者: Tinmc189623 | 团队: Nexlyh
 *
 * C++ 特有的 HAL 功能:
 *   - IoPort 方法实现 (头文件中已内联)
 *   - InterruptGuard / SpinLock / LockGuard (头文件中已内联)
 *   - 全局 HAL 初始化函数
 */

#include <plexsdos/hal.hpp>
#include <plexsdos/types.h>
#include <plexsdos/screen.h>

/* HAL 版本信息 */
#define HAL_VERSION_MAJOR  1
#define HAL_VERSION_MINOR  0

/*
 * hal_init — 初始化硬件抽象层
 *
 * 打印 HAL 版本和硬件信息到屏幕。
 * 由 kernel_main() 在各子系统初始化前调用。
 */
void hal_init(void)
{
    uint32_t eax, ebx, ecx, edx;

    screen_puts("[hal] Hardware Abstraction Layer v");
    screen_put_dec(HAL_VERSION_MAJOR);
    screen_putchar('.');
    screen_put_dec(HAL_VERSION_MINOR);
    screen_putchar('\n');

    /* 检测 CPU 厂商 */
    hal_cpuid(0, 0, &eax, &ebx, &ecx, &edx);
    char vendor[13];
    *(uint32_t *)(vendor + 0) = ebx;
    *(uint32_t *)(vendor + 4) = edx;
    *(uint32_t *)(vendor + 8) = ecx;
    vendor[12] = '\0';
    screen_puts("[hal] CPU: ");
    screen_puts(vendor);
    screen_putchar('\n');

    /* 检测 TSC 支持 */
    hal_cpuid(1, 0, &eax, &ebx, &ecx, &edx);
    if (edx & (1 << 4)) {
        screen_puts("[hal] TSC: supported\n");
    }

    /* 检测 MMX/SSE 支持 */
    if (edx & (1 << 23)) {
        screen_puts("[hal] MMX: supported\n");
    }
    if (edx & (1 << 25)) {
        screen_puts("[hal] SSE: supported\n");
    }
    if (edx & (1 << 26)) {
        screen_puts("[hal] SSE2: supported\n");
    }

    /* PIC 状态 */
    uint8_t master_mask = hal_inb(0x21);
    uint8_t slave_mask = hal_inb(0xA1);
    screen_puts("[hal] PIC mask: master=0x");
    screen_put_hex(master_mask);
    screen_puts(" slave=0x");
    screen_put_hex(slave_mask);
    screen_putchar('\n');

    screen_puts("[hal] ready\n");
}
