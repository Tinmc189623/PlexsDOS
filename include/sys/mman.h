/*
 * Nexsteaduser — PlexsDOS
 * sys/mman.h — 存根 (LightDM 移植)
 * 作者: Tinmc189623 | 团队: Nexlyh
 */
#ifndef _SYS_MMAN_H
#define _SYS_MMAN_H

#define PROT_READ   0x1
#define PROT_WRITE  0x2
#define MAP_SHARED  0x01
#define MAP_PRIVATE 0x02

static inline void *mmap(void *addr, unsigned long length, int prot, int flags, int fd, long offset) {
    (void)addr; (void)length; (void)prot; (void)flags; (void)fd; (void)offset;
    return (void*)-1;
}

static inline int munmap(void *addr, unsigned long length) {
    (void)addr; (void)length;
    return -1;
}

#endif /* _SYS_MMAN_H */
