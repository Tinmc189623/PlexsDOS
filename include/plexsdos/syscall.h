/*
 * Nexsteaduser — PlexsDOS
 * 系统调用接口 (INT 21h)
 * 作者: Tinmc189623 | 团队: Nexlyh
 *
 * DOS 兼容的 INT 21h 系统调用, 供外部程序使用。
 * 扩展 PnP 功能: AH=0x30-0x33 用于 PCI/ISA 设备枚举。
 */

#ifndef _PLXSDOS_SYSCALL_H
#define _PLXSDOS_SYSCALL_H

#include <plexsdos/types.h>

#ifdef __cplusplus
extern "C" {
#endif

/* INT 21h 功能号 — DOS 兼容 */
#define SYS_EXIT       0x4C   /* 程序终止 (AL=返回码) */
#define SYS_READ_CHAR  0x01   /* 读字符并回显 */
#define SYS_WRITE_CHAR 0x02   /* 写字符 (DL=字符) */
#define SYS_WRITE_STR  0x09   /* 写字符串 (DS:DX=地址, '$'结尾) */
#define SYS_READ_STR   0x0A   /* 读字符串到缓冲区 */

/* INT 21h 功能号 — PnP 设备枚举 */
#define SYS_PNP_PCI_COUNT  0x30   /* 获取 PCI 设备数量 (返回: EAX=数量) */
#define SYS_PNP_PCI_GET    0x31   /* 获取 PCI 设备信息 (EDX=索引, ESI=缓冲区) */
#define SYS_PNP_ISA_COUNT  0x32   /* 获取 ISA 设备数量 (返回: EAX=数量) */
#define SYS_PNP_ISA_GET    0x33   /* 获取 ISA 设备信息 (EDX=索引, ESI=缓冲区) */

/*
 * 初始化 INT 21h 系统调用中断。
 * 注册 INT 0x21 中断处理程序。
 */
void syscall_init(void);

#ifdef __cplusplus
}
#endif

#endif /* _PLXSDOS_SYSCALL_H */
