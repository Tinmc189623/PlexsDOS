/* ==========================================================================
 * pthread.h — Windows MSVC stub
 * 项目：xorriso 1.5.8 Windows 移植
 *
 * MSVC 不提供 <pthread.h>。重定向到 win-compat.h（后者通过 ForcedIncludeFiles
 * 已全局注入 pthread 兼容定义）。
 * ========================================================================== */

#ifndef _PTHREAD_H_
#define _PTHREAD_H_

/* win-compat.h 已通过 /FI 全局注入，此处仅提供占位。
 * 若 win-compat.h 尚未被包含，则显式包含之。 */
#ifndef WIN_COMPAT_H
#include "win-compat.h"
#endif

#endif /* _PTHREAD_H_ */
