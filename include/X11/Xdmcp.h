/*
 * Nexsteaduser — PlexsDOS
 * X11/Xdmcp.h — 存根 (LightDM 移植)
 * 作者: Tinmc189623 | 团队: Nexlyh
 *
 * XDMCP (X Display Manager Control Protocol) 类型和函数占位。
 * PlexsDOS 使用 X12 协议, XDMCP 无需实际实现。
 */
#ifndef _X11_XDMCP_H
#define _X11_XDMCP_H

#include <stddef.h>

/* ARRAY 类型 (XDMCP 协议使用) */
typedef struct {
    int len;
    unsigned char *data;
} ARRAY8, ARRAY16;

/* XdmAuthKeyRec — XDMCP 认证密钥 */
typedef struct {
    unsigned char data[8];
} XdmAuthKeyRec;

/* Xdmcp 认证包装函数 (存根) */
static inline void XdmcpUnwrap(unsigned char *input, unsigned char *key, unsigned char *output, int len) {
    (void)input; (void)key; (void)output; (void)len;
}
static inline void XdmcpWrap(unsigned char *input, unsigned char *key, unsigned char *output, int len) {
    (void)input; (void)key; (void)output; (void)len;
}
static inline void XdmcpIncrementKey(XdmAuthKeyRec *key) { (void)key; }
static inline void XdmcpDecrementKey(XdmAuthKeyRec *key) { (void)key; }

#endif /* _X11_XDMCP_H */
