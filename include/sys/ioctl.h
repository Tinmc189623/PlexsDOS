/*
 * Nexsteaduser — PlexsDOS
 * sys/ioctl.h — 存根 (LightDM 移植)
 * 作者: Tinmc189623 | 团队: Nexlyh
 *
 * PlexsDOS 使用 X12 显示服务, 无需 VT 切换。
 * ioctl VT 操作均为空操作。
 */
#ifndef _SYS_IOCTL_H
#define _SYS_IOCTL_H

/* VT ioctl 常量 (占位) */
#define VT_GETSTATE   0x5603
#define VT_ACTIVATE   0x5606
#define VT_WAITACTIVE 0x5607

static inline int ioctl(int fd, unsigned long request, ...) {
    (void)fd; (void)request;
    return 0;
}

#endif /* _SYS_IOCTL_H */
