/*
 * Nexsteaduser — PlexsDOS
 * 键盘驱动接口
 * 作者: Tinmc189623 | 团队: Nexlyh
 */

#ifndef _PLXSDOS_KEYBOARD_H
#define _PLXSDOS_KEYBOARD_H

#include <plexsdos/types.h>

#ifdef __cplusplus
extern "C" {
#endif

/* 初始化键盘中断 (注册 INT 0x09 处理程序) */
void keyboard_init(void);

/* 阻塞读取一个字符 (等待按键) */
char keyboard_getchar(void);

/* 检查键盘缓冲区是否有数据 (非阻塞) */
int keyboard_available(void);

/* 读取一行输入 (回显字符, 支持退格, 以 Enter 结束) */
int keyboard_read_line(char *buf, int max_len);

#ifdef __cplusplus
}
#endif

#endif /* _PLXSDOS_KEYBOARD_H */
