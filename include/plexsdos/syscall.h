/*
 * Nexsteaduser — PlexsDOS
 * 系统调用接口 (INT 21h)
 * 作者: Tinmc189623 | 团队: Nexlyh
 *
 * DOS 兼容的 INT 21h 系统调用, 供外部程序使用。
 */

#ifndef _PLXSDOS_SYSCALL_H
#define _PLXSDOS_SYSCALL_H

#include <plexsdos/types.h>

#ifdef __cplusplus
extern "C" {
#endif

/* INT 21h 功能号 */
#define SYS_EXIT       0x4C   /* 程序终止 (AL=返回码) */
#define SYS_READ_CHAR  0x01   /* 读字符并回显 */
#define SYS_WRITE_CHAR 0x02   /* 写字符 (DL=字符) */
#define SYS_WRITE_STR  0x09   /* 写字符串 (DS:DX=地址, '$'结尾) */
#define SYS_READ_STR   0x0A   /* 读字符串到缓冲区 */

/*
 * 初始化 INT 21h 系统调用中断。
 * 注册 INT 0x21 中断处理程序。
 */
void syscall_init(void);

#ifdef __cplusplus
}
#endif

#endif /* _PLXSDOS_SYSCALL_H */
