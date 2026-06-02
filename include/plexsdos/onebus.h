/*
 * Nexsteaduser — PlexsDOS
 * onebus.h — 消息总线 (D-Bus 兼容层)
 * 作者: Tinmc189623 | 团队: Nexlyh
 *
 * 1-Bus 替代 D-Bus:
 *   onebus_init        → dbus_connection_open
 *   onebus_send        → dbus_connection_send
 *   onebus_recv        → dbus_connection_pop_message
 *   onebus_add_match   → dbus_bus_add_match
 *
 * 实现: 简单的单机消息队列, 无网络/无权限模型。
 */

#ifndef _PLXSDOS_ONEBUS_H
#define _PLXSDOS_ONEBUS_H

#include <plexsdos/types.h>

#ifdef __cplusplus
extern "C" {
#endif

/* 最大消息大小 */
#define ONEBUS_MAX_MSG_SIZE   4096
#define ONEBUS_MAX_MSG        64

/* 消息类型 */
#define ONEBUS_MSG_METHOD_CALL  0
#define ONEBUS_MSG_METHOD_RETURN 1
#define ONEBUS_MSG_SIGNAL       2

/* 1-Bus 消息结构 */
struct onebus_msg {
    uint32_t type;                       /* 消息类型 */
    char interface[64];                  /* 接口名 (如 "org.freedesktop.DisplayManager") */
    char method[64];                     /* 方法名 */
    char object_path[128];               /* 对象路径 */
    char sender[32];                     /* 发送者 */
    char destination[32];                /* 目标 */
    uint32_t serial;                     /* 序列号 */
    uint32_t reply_serial;               /* 应答序列号 */
    uint8_t data[ONEBUS_MAX_MSG_SIZE];  /* 消息体 */
    uint32_t data_len;                   /* 消息体长度 */
};

/* 1-Bus 侦听回调 */
typedef void (*onebus_filter_fn)(const struct onebus_msg *msg, void *userdata);

/*
 * onebus_init — 初始化 1-Bus 系统
 * 返回: true = 成功。
 */
bool onebus_init(void);

/*
 * onebus_shutdown — 关闭 1-Bus 系统
 */
void onebus_shutdown(void);

/*
 * onebus_send — 发送消息
 * @msg: 消息指针
 * 返回: true = 成功。
 */
bool onebus_send(const struct onebus_msg *msg);

/*
 * onebus_recv — 接收消息
 * @msg:    输出消息缓冲
 * @timeout_ms: 超时 (毫秒), 0 = 不等待, -1 = 无限等待
 * 返回: true = 收到消息。
 */
bool onebus_recv(struct onebus_msg *msg, int timeout_ms);

/*
 * onebus_add_filter — 添加消息过滤器
 * @interface:  接口名 (如 "org.freedesktop.DisplayManager.Seat")
 * @callback:   回调函数
 * @userdata:   用户数据
 */
void onebus_add_filter(const char *interface,
                       onebus_filter_fn callback, void *userdata);

/*
 * onebus_remove_filter — 移除消息过滤器
 * @callback: 回调函数
 */
void onebus_remove_filter(onebus_filter_fn callback);

/*
 * onebus_dispatch — 派发所有待处理消息
 * 在事件循环中调用, 处理消息队列中的消息。
 */
void onebus_dispatch(void);

/*
 * onebus_create_reply — 创建应答消息
 * @request:    请求消息
 * @reply:      输出应答消息
 * @data:       应答数据
 * @data_len:   应答数据长度
 */
void onebus_create_reply(const struct onebus_msg *request,
                         struct onebus_msg *reply,
                         const void *data, uint32_t data_len);

#ifdef __cplusplus
}
#endif

#endif /* _PLXSDOS_ONEBUS_H */
