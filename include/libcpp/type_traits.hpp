/*
 * Nexsteaduser — PlexsDOS
 * type_traits.hpp — 基础类型萃取
 * 作者: Tinmc189623 | 团队: Nexlyh
 *
 * 仅包含 freestanding 环境下最常用的类型萃取。
 */

#ifndef _LIBCPP_TYPE_TRAITS_HPP
#define _LIBCPP_TYPE_TRAITS_HPP

namespace std {

/* integral_constant — 编译期常量包装 */
template<typename T, T v>
struct integral_constant {
    static constexpr T value = v;
    using type = integral_constant;
    constexpr operator T() const noexcept { return v; }
};

using true_type = integral_constant<bool, true>;
using false_type = integral_constant<bool, false>;

/* is_same — 判断两个类型是否相同 */
template<typename T, typename U>
struct is_same : false_type {};

template<typename T>
struct is_same<T, T> : true_type {};

template<typename T, typename U>
inline constexpr bool is_same_v = is_same<T, U>::value;

/* is_integral — 判断是否为整数类型 */
template<typename T> struct is_integral : false_type {};
template<> struct is_integral<bool> : true_type {};
template<> struct is_integral<char> : true_type {};
template<> struct is_integral<signed char> : true_type {};
template<> struct is_integral<unsigned char> : true_type {};
template<> struct is_integral<short> : true_type {};
template<> struct is_integral<unsigned short> : true_type {};
template<> struct is_integral<int> : true_type {};
template<> struct is_integral<unsigned int> : true_type {};
template<> struct is_integral<long> : true_type {};
template<> struct is_integral<unsigned long> : true_type {};

template<typename T>
inline constexpr bool is_integral_v = is_integral<T>::value;

/* is_floating_point — 判断是否为浮点类型 */
template<typename T> struct is_floating_point : false_type {};
template<> struct is_floating_point<float> : true_type {};
template<> struct is_floating_point<double> : true_type {};
template<> struct is_floating_point<long double> : true_type {};

template<typename T>
inline constexpr bool is_floating_point_v = is_floating_point<T>::value;

/* is_arithmetic — 判断是否为算术类型 */
template<typename T>
struct is_arithmetic : integral_constant<bool, is_integral_v<T> || is_floating_point_v<T>> {};

template<typename T>
inline constexpr bool is_arithmetic_v = is_arithmetic<T>::value;

/* is_pointer — 判断是否为指针类型 */
template<typename T> struct is_pointer : false_type {};
template<typename T> struct is_pointer<T*> : true_type {};

template<typename T>
inline constexpr bool is_pointer_v = is_pointer<T>::value;

/* conditional — 条件类型选择 */
template<bool B, typename T, typename F>
struct conditional { using type = T; };

template<typename T, typename F>
struct conditional<false, T, F> { using type = F; };

template<bool B, typename T, typename F>
using conditional_t = typename conditional<B, T, F>::type;

/* enable_if — SFINAE 工具 */
template<bool B, typename T = void>
struct enable_if {};

template<typename T>
struct enable_if<true, T> { using type = T; };

template<bool B, typename T = void>
using enable_if_t = typename enable_if<B, T>::type;

/* decay — 类型退化 */
template<typename T>
struct decay {
private:
    using U = typename remove_reference<T>::type;
public:
    using type = conditional_t<is_integral_v<U>, int, U>;
};

template<typename T>
using decay_t = typename decay<T>::type;

/* remove_const — 移除 const 修饰符 */
template<typename T> struct remove_const { using type = T; };
template<typename T> struct remove_const<const T> { using type = T; };

template<typename T>
using remove_const_t = typename remove_const<T>::type;

/* remove_volatile — 移除 volatile 修饰符 */
template<typename T> struct remove_volatile { using type = T; };
template<typename T> struct remove_volatile<volatile T> { using type = T; };

template<typename T>
using remove_volatile_t = typename remove_volatile<T>::type;

/* remove_cv — 移除 const 和 volatile */
template<typename T>
struct remove_cv {
    using type = remove_const_t<remove_volatile_t<T>>;
};

template<typename T>
using remove_cv_t = typename remove_cv<T>::type;

} /* namespace std */

#endif /* _LIBCPP_TYPE_TRAITS_HPP */
