/*
 * Nexsteaduser — PlexsDOS
 * C++ 运行时支持 — 最小实现
 * 作者: Tinmc189623 | 团队: Nexlyh
 *
 * 提供 freestanding 环境下 C++ 所需的最小运行时支持:
 * - operator delete (虚拟析构函数需要)
 * - atexit (静态对象析构注册)
 */

#include <plexsdos/types.h>

/*
 * operator delete — 释放内存 (内核 freestanding 环境下为空操作)
 * @ptr: 要释放的指针 (忽略)
 *
 * 内核使用静态分配, 不需要动态内存释放。
 */
void operator delete(void *ptr) noexcept
{
    (void)ptr;
}

/*
 * operator delete — 带大小参数的释放 (C++14)
 * @ptr:  要释放的指针 (忽略)
 * @size: 块大小 (忽略)
 */
void operator delete(void *ptr, unsigned int size) noexcept
{
    (void)ptr;
    (void)size;
}

/*
 * atexit — 注册程序终止时调用的函数 (内核环境下为空操作)
 * @func: 回调函数 (忽略)
 *
 * 内核不会正常终止, 所以 atexit 回调永远不会执行。
 * 返回 0 表示成功。
 */
extern "C" int atexit(void (*func)(void))
{
    (void)func;
    return 0;
}
