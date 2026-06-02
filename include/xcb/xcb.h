/*
 * Nexsteaduser — PlexsDOS
 * xcb/xcb.h — 存根 (LightDM 移植)
 * 作者: Tinmc189623 | 团队: Nexlyh
 *
 * LightDM 使用 XCB 连接 X 显示服务器。
 * PlexsDOS 使用 X12 显示服务, XCB 操作为占位。
 */
#ifndef _XCB_XCB_H
#define _XCB_XCB_H

#include <stddef.h>

/* XCB 连接类型 */
typedef struct { int dummy; } xcb_connection_t;

/* XCB 认证信息 */
typedef struct {
    int namelen;
    char *name;
    int datalen;
    char *data;
} xcb_auth_info_t;

static inline xcb_connection_t *xcb_connect_to_display_with_auth_info(const char *display, xcb_auth_info_t *auth, int *screen) {
    (void)display; (void)auth; (void)screen;
    return NULL;
}

static inline int xcb_connection_has_error(xcb_connection_t *c) {
    (void)c;
    return -1; /* 始终返回错误, 表示无 X 服务器 */
}

static inline void xcb_disconnect(xcb_connection_t *c) {
    (void)c;
}

#endif /* _XCB_XCB_H */
