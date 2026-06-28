/* ==========================================================================
 * sys/time.h — Windows MSVC stub
 * 项目：xorriso 1.5.8 Windows 移植
 *
 * MSVC 不提供 <sys/time.h>。提供 POSIX gettimeofday 和相关类型。
 * struct timeval 由 Windows SDK <winsock.h> 定义（通过 <windows.h> 间接包含），
 * 此处不再重复定义。
 * ========================================================================== */

#ifndef _SYS_TIME_H_
#define _SYS_TIME_H_

#include <time.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/* struct timeval — 由 Windows SDK <winsock.h> 提供，此处免定义 */

/* struct timezone — 与 win-compat.h 共享同一 include guard */
#ifndef _TIMEZONE_DEFINED
#define _TIMEZONE_DEFINED
struct timezone {
    int tz_minuteswest;
    int tz_dsttime;
};
#endif

/* gettimeofday — 由 win-compat.h 提供（static __inline），此处不声明以免冲突 */

#ifdef __cplusplus
}
#endif

#endif /* _SYS_TIME_H_ */
