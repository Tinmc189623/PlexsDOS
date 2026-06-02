/*
 * Nexsteaduser — PlexsDOS
 * 串口驱动接口
 * 作者: Tinmc189623 | 团队: Nexlyh
 */

#ifndef _PLXSDOS_SERIAL_H
#define _PLXSDOS_SERIAL_H

#include <plexsdos/types.h>

#ifdef __cplusplus
extern "C" {
#endif

void serial_init(void);
void serial_putchar(char c);
void serial_puts(const char *str);
void serial_put_hex(uint32_t val);

#ifdef __cplusplus
}
#endif

#endif /* _PLXSDOS_SERIAL_H */
