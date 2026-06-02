/*
 * Nexsteaduser — PlexsDOS
 * syslog.h — 会话/日志管理 (systemd-logind 兼容层)
 * 作者: Tinmc189623 | 团队: Nexlyh
 *
 * syslog 替代 systemd-logind:
 *   会话跟踪 + seat 管理 + 虚拟终端切换
 *   不依赖 systemd, 纯 PlexsDOS 原生实现。
 *
 * logind API 映射:
 *   syslog_session_create    → sd_session_create / CreateSession
 *   syslog_session_release   → sd_session_release / ReleaseSession
 *   syslog_seat_get_active   → sd_seat_get_active
 *   syslog_switch_vt         → sd_switch_vt / ChVT
 */

#ifndef _PLXSDOS_SYSLOG_H
#define _PLXSDOS_SYSLOG_H

#include <plexsdos/types.h>

#ifdef __cplusplus
extern "C" {
#endif

/* 最大会话数 */
#define SYSLOG_MAX_SESSIONS    8
#define SYSLOG_MAX_SEATS       4

/* 会话状态 */
#define SYSLOG_SESSION_ONLINE  1   /* 已连接, 未认证 */
#define SYSLOG_SESSION_ACTIVE  2   /* 已认证, 活跃 */
#define SYSLOG_SESSION_CLOSING 3   /* 正在关闭 */

/* Seat 状态 */
#define SYSLOG_SEAT_ACTIVE     1
#define SYSLOG_SEAT_INACTIVE   2

/* 会话信息 */
struct syslog_session {
    int id;                          /* 会话 ID */
    char user[32];                   /* 用户名 */
    int state;                       /* 会话状态 */
    char seat[16];                   /* Seat 名 (如 "seat0") */
};

/* Seat 信息 */
struct syslog_seat {
    char name[16];                   /* Seat 名 */
    int state;                       /* Seat 状态 */
    int active_session;              /* 活跃会话 ID (-1 = 无) */
};

/*
 * syslog_init — 初始化会话管理器
 */
void syslog_init(void);

/*
 * syslog_session_create — 创建会话
 * @user:   用户名
 * @seat:   Seat 名 (如 "seat0")
 * @display: 显示名 (如 ":0")
 * 返回: 会话 ID, 失败返回 -1。
 *
 * 对应 CreateSession D-Bus 调用。
 */
int syslog_session_create(const char *user, const char *seat,
                          const char *display);

/*
 * syslog_session_release — 释放会话
 * @session_id: 会话 ID
 * 返回: true = 成功。
 *
 * 对应 ReleaseSession D-Bus 调用。
 */
bool syslog_session_release(int session_id);

/*
 * syslog_session_set_active — 设置会话活跃状态
 * @session_id: 会话 ID
 * @active:     true = 激活
 */
void syslog_session_set_active(int session_id, bool active);

/*
 * syslog_session_get — 获取会话信息
 * @session_id: 会话 ID
 * 返回: 会话信息指针, 无效返回 NULL。
 */
const struct syslog_session *syslog_session_get(int session_id);

/*
 * syslog_seat_get — 获取 Seat 信息
 * @name: Seat 名
 * 返回: Seat 信息指针, 未找到返回 NULL。
 */
const struct syslog_seat *syslog_seat_get(const char *name);

/*
 * syslog_seat_get_active — 获取 Seat 活跃会话
 * @name: Seat 名
 * 返回: 活跃会话 ID, 无活跃返回 -1。
 */
int syslog_seat_get_active(const char *name);

/*
 * syslog_switch_vt — 切换虚拟终端
 * @vt: 终端号
 * 返回: true = 成功。
 */
bool syslog_switch_vt(int vt);

/*
 * syslog_get_seats — 获取所有 Seat 列表
 * @seats:  输出 Seat 数组
 * @max:    数组最大长度
 * 返回: Seat 数量。
 */
int syslog_get_seats(struct syslog_seat *seats, int max);

/*
 * syslog_get_sessions — 获取所有会话列表
 * @sessions: 输出会话数组
 * @max:      数组最大长度
 * 返回: 会话数量。
 */
int syslog_get_sessions(struct syslog_session *sessions, int max);

#ifdef __cplusplus
}
#endif

#endif /* _PLXSDOS_SYSLOG_H */
