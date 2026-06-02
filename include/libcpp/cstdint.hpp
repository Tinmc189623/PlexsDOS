/*
 * Nexsteaduser — PlexsDOS
 * cstdint.hpp — C++ 固定宽度整数类型
 * 作者: Tinmc189623 | 团队: Nexlyh
 */

#ifndef _LIBCPP_CSTDINT_HPP
#define _LIBCPP_CSTDINT_HPP

#include <plexsdos/types.h>

/* types.h 已定义所有固定宽度类型, 此处仅添加别名 */
namespace std {
    using ::uint8_t;
    using ::uint16_t;
    using ::uint32_t;
    using ::uint64_t;
    using ::int8_t;
    using ::int16_t;
    using ::int32_t;
    using ::int64_t;
    using ::size_t;
}

#endif /* _LIBCPP_CSTDINT_HPP */
