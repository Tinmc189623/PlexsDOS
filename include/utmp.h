/*
 * Nexsteaduser — PlexsDOS
 * utmp.h — 存根 (LightDM 移植)
 * 作者: Tinmc189623 | 团队: Nexlyh
 *
 * LightDM 使用 utmp/wtmpx 记录用户登录会话。
 * PlexsDOS 使用 usrgn 管理用户, 这些操作均为空操作。
 */
#ifndef _UTMP_H
#define _UTMP_H

#define UTMP_FILE  "/var/run/utmp"
#define WTMP_FILE  "/var/log/wtmp"

#define EMPTY       0
#define USER_PROCESS 7
#define DEAD_PROCESS 8

struct timeval;
struct utmp {
    char ut_user[32];
    char ut_line[32];
    char ut_host[256];
    int ut_type;
    int ut_pid;
    char ut_id[16];
    struct timeval ut_tv;
};

#endif /* _UTMP_H */
