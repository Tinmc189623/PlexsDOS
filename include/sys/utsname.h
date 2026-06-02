/*
 * Nexsteaduser — PlexsDOS
 * sys/utsname.h — 存根 (LightDM 移植)
 * 作者: Tinmc189623 | 团队: Nexlyh
 */
#ifndef _SYS_UTSNAME_H
#define _SYS_UTSNAME_H

struct utsname {
    char sysname[65];
    char nodename[65];
    char release[65];
    char version[65];
    char machine[65];
};

static inline int uname(struct utsname *buf) {
    (void)buf;
    return 0;
}

#endif /* _SYS_UTSNAME_H */
