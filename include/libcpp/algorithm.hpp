/*
 * Nexsteaduser — PlexsDOS
 * algorithm.hpp — 基础算法
 * 作者: Tinmc189623 | 团队: Nexlyh
 *
 * 提供 min, max, clamp, fill, copy, find 等基础算法。
 */

#ifndef _LIBCPP_ALGORITHM_HPP
#define _LIBCPP_ALGORITHM_HPP

#include <libcpp/utility.hpp>

namespace std {

/*
 * min — 返回两个值中较小的
 */
template<typename T>
constexpr const T& min(const T& a, const T& b)
{
    return (b < a) ? b : a;
}

/*
 * max — 返回两个值中较大的
 */
template<typename T>
constexpr const T& max(const T& a, const T& b)
{
    return (a < b) ? b : a;
}

/*
 * clamp — 将值限制在 [lo, hi] 范围内
 */
template<typename T>
constexpr const T& clamp(const T& val, const T& lo, const T& hi)
{
    if (val < lo) return lo;
    if (hi < val) return hi;
    return val;
}

/*
 * fill — 用指定值填充范围
 */
template<typename ForwardIt, typename T>
void fill(ForwardIt first, ForwardIt last, const T& value)
{
    for (; first != last; ++first)
        *first = value;
}

/*
 * copy — 复制范围
 */
template<typename InputIt, typename OutputIt>
OutputIt copy(InputIt first, InputIt last, OutputIt d_first)
{
    while (first != last) {
        *d_first = *first;
        ++first;
        ++d_first;
    }
    return d_first;
}

/*
 * find — 在范围内查找值
 */
template<typename InputIt, typename T>
InputIt find(InputIt first, InputIt last, const T& value)
{
    for (; first != last; ++first) {
        if (*first == value)
            return first;
    }
    return last;
}

/*
 * equal — 比较两个范围是否相等
 */
template<typename InputIt1, typename InputIt2>
bool equal(InputIt1 first1, InputIt1 last1, InputIt2 first2)
{
    for (; first1 != last1; ++first1, ++first2) {
        if (*first1 != *first2)
            return false;
    }
    return true;
}

/*
 * lexicographical_compare — 字典序比较
 */
template<typename InputIt1, typename InputIt2>
bool lexicographical_compare(InputIt1 first1, InputIt1 last1,
                             InputIt2 first2, InputIt2 last2)
{
    for (; first1 != last1 && first2 != last2; ++first1, ++first2) {
        if (*first1 < *first2) return true;
        if (*first2 < *first1) return false;
    }
    return (first1 == last1) && (first2 != last2);
}

} /* namespace std */

#endif /* _LIBCPP_ALGORITHM_HPP */
