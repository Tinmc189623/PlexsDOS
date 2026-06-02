/*
 * Nexsteaduser — PlexsDOS
 * utility.hpp — 基础工具类和函数
 * 作者: Tinmc189623 | 团队: Nexlyh
 *
 * 提供 move, forward, swap, pair 等基础工具。
 */

#ifndef _LIBCPP_UTILITY_HPP
#define _LIBCPP_UTILITY_HPP

#include <libc/stddef.h>

/* remove_reference — 移除引用修饰符 */
namespace std {

template<typename T>
struct remove_reference { typedef T type; };

template<typename T>
struct remove_reference<T&> { typedef T type; };

template<typename T>
struct remove_reference<T&&> { typedef T type; };

/*
 * move — 将左值转换为右值引用
 */
template<typename T>
constexpr typename remove_reference<T>::type&& move(T&& t) noexcept
{
    return static_cast<typename remove_reference<T>::type&&>(t);
}

/*
 * forward — 完美转发
 */
template<typename T>
constexpr T&& forward(typename remove_reference<T>::type& t) noexcept
{
    return static_cast<T&&>(t);
}

template<typename T>
constexpr T&& forward(typename remove_reference<T>::type&& t) noexcept
{
    return static_cast<T&&>(t);
}

/*
 * swap — 交换两个值
 */
template<typename T>
constexpr void swap(T& a, T& b) noexcept
{
    T tmp = move(a);
    a = move(b);
    b = move(tmp);
}

/*
 * pair — 二元组
 */
template<typename T1, typename T2>
struct pair {
    T1 first;
    T2 second;

    pair() = default;
    pair(const T1& a, const T2& b) : first(a), second(b) {}
    pair(T1&& a, T2&& b) : first(move(a)), second(move(b)) {}
};

/*
 * make_pair — 创建 pair 的辅助函数
 */
template<typename T1, typename T2>
constexpr pair<T1, T2> make_pair(T1&& a, T2&& b)
{
    return pair<T1, T2>(forward<T1>(a), forward<T2>(b));
}

/*
 * initializer_list — 初始化列表 (GCC 内建支持)
 */
template<typename T>
class initializer_list {
public:
    constexpr initializer_list() noexcept : m_data(nullptr), m_size(0) {}

    constexpr size_t size() const noexcept { return m_size; }
    constexpr const T* begin() const noexcept { return m_data; }
    constexpr const T* end() const noexcept { return m_data + m_size; }

private:
    const T* m_data;
    size_t m_size;

    /* 由编译器内部调用的构造函数 */
    constexpr initializer_list(const T* data, size_t size) noexcept
        : m_data(data), m_size(size) {}
};

} /* namespace std */

#endif /* _LIBCPP_UTILITY_HPP */
