/*
 * Nexsteaduser — PlexsDOS
 * debug.h — 调试工具接口
 * 作者: Tinmc189623 | 团队: Nexlyh
 *
 * 类 DOS DEBUG.COM 的交互式调试工具。
 * 子命令: D(内存转储), E(修改内存), R(寄存器), P(端口), U(反汇编), H(十六进制计算), Q(退出)
 */

#ifndef _PLXSDOS_DEBUG_H
#define _PLXSDOS_DEBUG_H

#include <plexsdos/types.h>

#ifdef __cplusplus
extern "C" {
#endif

/*
 * debug_main — 启动调试工具
 *
 * 进入交互式调试命令循环。输入 Q 退出。
 */
void debug_main(void);

#ifdef __cplusplus
}
#endif

#endif /* _PLXSDOS_DEBUG_H */
