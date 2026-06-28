/* grp.h — Windows MSVC stub for xorriso */
#ifndef _GRP_H_
#define _GRP_H_
#include <stdint.h>
#ifndef gid_t
#define gid_t unsigned int
#endif
struct group {
    char   *gr_name;
    gid_t   gr_gid;
    char  **gr_mem;
};
struct group *getgrgid(gid_t gid);
struct group *getgrnam(const char *name);
#endif
