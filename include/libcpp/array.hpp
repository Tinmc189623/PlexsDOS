/*
 * Nexsteaduser — PlexsDOS
 * array.hpp — 固定大小数组容器
 * 作者: Tinmc189623 | 团队: Nexlyh
 *
 * 类似 std::array<T, N>, 编译期确定大小。
 */

#ifndef _LIBCPP_ARRAY_HPP
#define _LIBCPP_ARRAY_HPP

#include <libc/stddef.h>

namespace std {

template<typename T, size_t N>
struct array {
    T m_data[N];

    /* 元素访问 */
    constexpr T& operator[](size_t n) { return m_data[n]; }
    constexpr const T& operator[](size_t n) const { return m_data[n]; }

    constexpr T& front() { return m_data[0]; }
    constexpr const T& front() const { return m_data[0]; }

    constexpr T& back() { return m_data[N - 1]; }
    constexpr const T& back() const { return m_data[N - 1]; }

    constexpr T* data() { return m_data; }
    constexpr const T* data() const { return m_data; }

    /* 容量 */
    constexpr bool empty() const { return N == 0; }
    constexpr size_t size() const { return N; }

    /* 迭代器 (原始指针) */
    constexpr T* begin() { return m_data; }
    constexpr const T* begin() const { return m_data; }
    constexpr T* end() { return m_data + N; }
    constexpr const T* end() const { return m_data + N; }

    /* 填充 */
    void fill(const T& value) {
        for (size_t i = 0; i < N; i++)
            m_data[i] = value;
    }

    /* 交换 */
    void swap(array& other) {
        for (size_t i = 0; i < N; i++) {
            T tmp = m_data[i];
            m_data[i] = other.m_data[i];
            other.m_data[i] = tmp;
        }
    }
};

/* 特化: 空数组 */
template<typename T>
struct array<T, 0> {
    T m_data[1];  /* 避免零大小数组 */

    constexpr bool empty() const { return true; }
    constexpr size_t size() const { return 0; }
    constexpr T* begin() { return nullptr; }
    constexpr const T* begin() const { return nullptr; }
    constexpr T* end() { return nullptr; }
    constexpr const T* end() const { return nullptr; }
};

} /* namespace std */

#endif /* _LIBCPP_ARRAY_HPP */
