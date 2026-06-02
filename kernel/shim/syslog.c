/*
 * Nexsteaduser — PlexsDOS
 * syslog.c — 会话/日志管理实现 (systemd-logind 兼容层)
 * 作者: Tinmc189623 | 团队: Nexlyh
 *
 * 实现简单会话和 seat 管理:
 *   - 单 seat (seat0)
 *   - 多会话跟踪
 *   - 虚拟终端切换 (通过 INT 16h 键盘控制器)
 */

#include <plexsdos/syslog.h>
#include <plexsdos/string.h>

/* 内部会话存储 */
static struct syslog_session sessions[SYSLOG_MAX_SESSIONS];
static int session_count = 0;

/* 内部 Seat 存储 */
static struct syslog_seat seats[SYSLOG_MAX_SEATS];
static int seat_count = 0;

/* 下一个会话 ID */
static int next_session_id = 1;

/*
 * syslog_init — 初始化会话管理器
 */
void syslog_init(void)
{
    session_count = 0;
    seat_count = 0;
    next_session_id = 1;

    /* 创建默认 seat */
    struct syslog_seat *s = &seats[0];
    int i;
    const char *seat_name = "seat0";
    for (i = 0; seat_name[i] && i < 15; i++)
        s->name[i] = seat_name[i];
    s->name[i] = '\0';
    s->state = SYSLOG_SEAT_INACTIVE;
    s->active_session = -1;
    seat_count = 1;

    /* 清零所有会话 */
    for (i = 0; i < SYSLOG_MAX_SESSIONS; i++) {
        sessions[i].id = -1;
        sessions[i].state = 0;
        sessions[i].user[0] = '\0';
        sessions[i].seat[0] = '\0';
    }
}

/*
 * syslog_session_create — 创建会话
 */
int syslog_session_create(const char *user, const char *seat,
                          const char *display)
{
    if (!user || !seat || session_count >= SYSLOG_MAX_SESSIONS)
        return -1;

    (void)display;

    /* 查找空闲槽位 */
    int slot = -1;
    for (int i = 0; i < SYSLOG_MAX_SESSIONS; i++) {
        if (sessions[i].id < 0) {
            slot = i;
            break;
        }
    }
    if (slot < 0)
        return -1;

    struct syslog_session *s = &sessions[slot];
    s->id = next_session_id++;
    s->state = SYSLOG_SESSION_ONLINE;

    int j;
    for (j = 0; user[j] && j < 31; j++)
        s->user[j] = user[j];
    s->user[j] = '\0';

    for (j = 0; seat[j] && j < 15; j++)
        s->seat[j] = seat[j];
    s->seat[j] = '\0';

    session_count++;
    return s->id;
}

/*
 * syslog_session_release — 释放会话
 */
bool syslog_session_release(int session_id)
{
    for (int i = 0; i < SYSLOG_MAX_SESSIONS; i++) {
        if (sessions[i].id == session_id) {
            sessions[i].id = -1;
            sessions[i].state = 0;
            sessions[i].user[0] = '\0';
            sessions[i].seat[0] = '\0';
            session_count--;

            /* 更新 seat 活跃状态 */
            for (int j = 0; j < seat_count; j++) {
                if (seats[j].active_session == session_id) {
                    seats[j].active_session = -1;
                    seats[j].state = SYSLOG_SEAT_INACTIVE;
                }
            }
            return true;
        }
    }
    return false;
}

/*
 * syslog_session_set_active — 设置会话活跃状态
 */
void syslog_session_set_active(int session_id, bool active)
{
    for (int i = 0; i < SYSLOG_MAX_SESSIONS; i++) {
        if (sessions[i].id == session_id) {
            sessions[i].state = active ? SYSLOG_SESSION_ACTIVE
                                      : SYSLOG_SESSION_ONLINE;
            /* 更新 seat */
            for (int j = 0; j < seat_count; j++) {
                if (seats[j].name[0]) {
                    seats[j].active_session = active ? session_id : -1;
                    seats[j].state = active ? SYSLOG_SEAT_ACTIVE
                                            : SYSLOG_SEAT_INACTIVE;
                }
            }
            return;
        }
    }
}

/*
 * syslog_session_get — 获取会话信息
 */
const struct syslog_session *syslog_session_get(int session_id)
{
    for (int i = 0; i < SYSLOG_MAX_SESSIONS; i++) {
        if (sessions[i].id == session_id)
            return &sessions[i];
    }
    return NULL;
}

/*
 * syslog_seat_get — 获取 Seat 信息
 */
const struct syslog_seat *syslog_seat_get(const char *name)
{
    if (!name)
        return NULL;

    for (int i = 0; i < seat_count; i++) {
        int j;
        for (j = 0; seats[i].name[j] && name[j]; j++) {
            if (seats[i].name[j] != name[j])
                break;
        }
        if (seats[i].name[j] == '\0' && name[j] == '\0')
            return &seats[i];
    }
    return NULL;
}

/*
 * syslog_seat_get_active — 获取 Seat 活跃会话
 */
int syslog_seat_get_active(const char *name)
{
    const struct syslog_seat *s = syslog_seat_get(name);
    return s ? s->active_session : -1;
}

/*
 * syslog_switch_vt — 切换虚拟终端
 */
bool syslog_switch_vt(int vt)
{
    (void)vt;
    /* PlexsDOS 当前为单终端, 直接返回成功 */
    return true;
}

/*
 * syslog_get_seats — 获取所有 Seat 列表
 */
int syslog_get_seats(struct syslog_seat *out, int max)
{
    if (!out)
        return 0;

    int count = (seat_count < max) ? seat_count : max;
    for (int i = 0; i < count; i++) {
        int j;
        out[i].state = seats[i].state;
        out[i].active_session = seats[i].active_session;
        for (j = 0; seats[i].name[j] && j < 15; j++)
            out[i].name[j] = seats[i].name[j];
        out[i].name[j] = '\0';
    }
    return count;
}

/*
 * syslog_get_sessions — 获取所有会话列表
 */
int syslog_get_sessions(struct syslog_session *out, int max)
{
    if (!out)
        return 0;

    int count = 0;
    for (int i = 0; i < SYSLOG_MAX_SESSIONS && count < max; i++) {
        if (sessions[i].id >= 0) {
            int j;
            out[count].id = sessions[i].id;
            out[count].state = sessions[i].state;
            for (j = 0; sessions[i].user[j] && j < 31; j++)
                out[count].user[j] = sessions[i].user[j];
            out[count].user[j] = '\0';
            for (j = 0; sessions[i].seat[j] && j < 15; j++)
                out[count].seat[j] = sessions[i].seat[j];
            out[count].seat[j] = '\0';
            count++;
        }
    }
    return count;
}
