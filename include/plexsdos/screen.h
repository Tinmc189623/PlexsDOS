/*
 * Nexsteaduser — PlexsDOS
 * 屏幕驱动接口
 * 作者: Tinmc189623 | 团队: Nexlyh
 */

#ifndef _PLXSDOS_SCREEN_H
#define _PLXSDOS_SCREEN_H

#include <plexsdos/types.h>

#ifdef __cplusplus
extern "C" {
#endif

/* 初始化屏幕 (清屏, 设置默认颜色) */
void screen_init(void);

/* 清屏并重置光标到左上角 */
void screen_clear(void);

/* 输出单个字符 (支持 \n, \r, \b, \t) */
void screen_putchar(char c);

/* 输出以 null 结尾的字符串 */
void screen_puts(const char *str);

/* 输出十六进制 32-bit 数值 (带 0x 前缀) */
void screen_put_hex(uint32_t val);

/* 输出十进制无符号整数 */
void screen_put_dec(uint32_t val);

/* 设置前景/背景颜色 */
void screen_set_color(uint8_t fg, uint8_t bg);

/* 恢复默认颜色 */
void screen_reset_color(void);

/* 获取当前光标位置 (字符偏移) */
int screen_get_cursor(void);

/* 设置光标位置 (字符偏移) */
void screen_set_cursor(int pos);

#ifdef __cplusplus
}
#endif

#endif /* _PLXSDOS_SCREEN_H */
