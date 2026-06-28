/* ==========================================================================
 * pwd.h — Windows MSVC stub
 * 项目：xorriso 1.5.8 Windows 移植
 *
 * MSVC 不提供 <pwd.h>。提供 uid_t/gid_t 定义和基本 passwd 结构体。
 * ========================================================================== */

#ifndef _PWD_H_
#define _PWD_H_

#ifdef __cplusplus
extern "C" {
#endif

/* POSIX uid_t/gid_t */
#ifndef uid_t
#define uid_t unsigned int
#endif
#ifndef gid_t
#define gid_t unsigned int
#endif

/* passwd 结构体（简化版） */
struct passwd {
    char   *pw_name;   /* 用户名 */
    uid_t   pw_uid;    /* 用户 ID */
    gid_t   pw_gid;    /* 组 ID */
    char   *pw_dir;    /* 家目录 */
    char   *pw_shell;  /* Shell */
};

/* 返回当前用户 passwd 条目（始终返回 NULL，表示不可用） */
struct passwd *getpwuid(uid_t uid);
struct passwd *getpwnam(const char *name);

#ifdef __cplusplus
}
#endif

#endif /* _PWD_H_ */
