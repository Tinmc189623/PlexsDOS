/*
 * Nexsteaduser — PlexsDOS
 * 系统调用接口 (INT 21h)
 * 作者: Tinmc189623 | 团队: Nexlyh
 *
 * DOS 兼容的 INT 21h 系统调用, 供 Ring 3 用户程序使用。
 * 扩展 PnP 功能: AH=0x30-0x33 用于 PCI/ISA 设备枚举。
 * PlexsDOS 扩展: AH=0x40-0x4F 用于屏幕/文件/进程控制。
 */

#ifndef _PLXSDOS_SYSCALL_H
#define _PLXSDOS_SYSCALL_H

#include <plexsdos/types.h>

#ifdef __cplusplus
extern "C" {
#endif

/* INT 21h 功能号 — DOS 兼容 */
#define SYS_EXIT       0x4C   /* 程序终止 (AL=返回码) */
#define SYS_READ_CHAR  0x01   /* 读字符并回显, 返回: AL=字符 */
#define SYS_WRITE_CHAR 0x02   /* 写字符 (DL=字符) */
#define SYS_WRITE_STR  0x09   /* 写字符串 (DS:EDX=地址, '$'结尾) */
#define SYS_READ_STR   0x0A   /* 读字符串到缓冲区 (DS:EDX=缓冲区, buf[0]=max_len) */

/* INT 21h 功能号 — PlexsDOS 扩展 (屏幕控制) */
#define SYS_CLEAR_SCREEN   0x50   /* 清屏 */
#define SYS_SET_COLOR      0x51   /* 设置颜色 (DL=前景色, DH=背景色) */
#define SYS_RESET_COLOR    0x52   /* 重置为默认颜色 */
#define SYS_PUT_DEC        0x53   /* 输出十进制数 (EDX=数值) */
#define SYS_PUT_HEX        0x54   /* 输出十六进制数 (EDX=数值) */

/* INT 21h 功能号 — PlexsDOS 扩展 (进程/文件) */
#define SYS_EXEC           0x55   /* 执行程序 (DS:EDX=文件名字符串, '$'结尾) */
#define SYS_FS_LIST        0x56   /* 列出根目录 */
#define SYS_FS_READ        0x57   /* 读取文件 (DS:EDX=文件名, ESI=缓冲区, 返回大小) */
#define SYS_GET_DRIVE      0x58   /* 获取当前驱动器 (返回: AL=驱动器号 'A'-'Z') */
#define SYS_SET_DRIVE      0x59   /* 设置当前驱动器 (DL=驱动器号 'A'-'Z') */
#define SYS_REBOOT         0x5A   /* 重启系统 */
#define SYS_GET_VERSION    0x5B   /* 获取版本信息 */
#define SYS_SHELL_CMD      0x5C   /* 执行 Shell 命令 (DS:EDX=命令字符串, '$'结尾) */

/* INT 21h 功能号 — PnP 设备枚举 */
#define SYS_PNP_PCI_COUNT  0x30   /* 获取 PCI 设备数量 (返回: EAX=数量) */
#define SYS_PNP_PCI_GET    0x31   /* 获取 PCI 设备信息 (EDX=索引, ESI=缓冲区) */
#define SYS_PNP_ISA_COUNT  0x32   /* 获取 ISA 设备数量 (返回: EAX=数量) */
#define SYS_PNP_ISA_GET    0x33   /* 获取 ISA 设备信息 (EDX=索引, ESI=缓冲区) */

/*
 * 初始化 INT 21h 系统调用中断。
 * 注册 INT 0x22 中断处理程序 (DPL=3, 允许 Ring 3 调用)。
 */
void syscall_init(void);

#ifdef __cplusplus
}
#endif

#endif /* _PLXSDOS_SYSCALL_H */
