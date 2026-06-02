/*
 * Nexsteaduser — PlexsDOS
 * utmpx.h — 存根 (LightDM 移植)
 * 作者: Tinmc189623 | 团队: Nexlyh
 *
 * utmpx 记录扩展用户会计信息。
 * PlexsDOS 中登录信息由 usrgn 管理, 这些是空操作。
 */
#ifndef _UTMPX_H
#define _UTMPX_H

#include "utmp.h"

struct utmpx {
    char ut_user[32];
    char ut_line[32];
    char ut_host[256];
    int ut_type;
    int ut_pid;
    char ut_id[16];
    struct timeval ut_tv;
};

static inline void setutxent(void) { }
static inline void endutxent(void) { }
static inline struct utmpx *pututxline(const struct utmpx *ut) { (void)ut; return NULL; }

#endif /* _UTMPX_H */
