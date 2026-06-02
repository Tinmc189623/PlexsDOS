/*
 * Nexsteaduser — PlexsDOS
 * new.hpp — 全局 new/delete 运算符
 * 作者: Tinmc189623 | 团队: Nexlyh
 *
 * freestanding 环境: 无堆分配器, 仅提供 placement new。
 * 如果内核实现了 kmalloc/kfree, 可扩展为标准 new/delete。
 */

#ifndef _LIBCPP_NEW_HPP
#define _LIBCPP_NEW_HPP

#include <libc/stddef.h>

/*
 * placement new — 在指定地址构造对象
 */
inline void *operator new(size_t, void *ptr) noexcept { return ptr; }
inline void *operator new[](size_t, void *ptr) noexcept { return ptr; }

/*
 * placement delete — 与 placement new 配对 (异常安全)
 */
inline void operator delete(void *, void *) noexcept {}
inline void operator delete[](void *, void *) noexcept {}

/*
 * 标准 new/delete — 使用内核堆分配器 (待实现)
 *
 * 当内核内存管理器就绪后, 取消下面的注释:
 *
 * void *operator new(size_t size);
 * void *operator new[](size_t size);
 * void operator delete(void *ptr) noexcept;
 * void operator delete[](void *ptr) noexcept;
 * void operator delete(void *ptr, size_t) noexcept;
 * void operator delete[](void *ptr, size_t) noexcept;
 */

#endif /* _LIBCPP_NEW_HPP */
