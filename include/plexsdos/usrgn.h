/*
 * Nexsteaduser — PlexsDOS
 * usrgn.h — 用户管理 (getpwnam/setuid 兼容层)
 * 作者: Tinmc189623 | 团队: Nexlyh
 *
 * usrgn 替代 POSIX 用户管理:
 *   usrgn_get_user     → getpwnam
 *   usrgn_get_users    → getpwent (枚举)
 *   usrgn_verify_pass  → crypt + 密码验证
 *   usrgn_list_users   → 列出所有用户
 *
 * PlexsDOS 用户系统: 简单硬编码用户列表 + 密码验证。
 */

#ifndef _PLXSDOS_USRGN_H
#define _PLXSDOS_USRGN_H

#include <plexsdos/types.h>

#ifdef __cplusplus
extern "C" {
#endif

/* 最大用户名长度 */
#define USRGN_MAX_NAME      32
#define USRGN_MAX_PASS      64
#define USRGN_MAX_USERS     8

/* 用户标志 */
#define USRGN_FLAG_DISABLED  0x01
#define USRGN_FLAG_SYSTEM    0x02

/* 用户信息结构 */
struct usrgn_user {
    char name[USRGN_MAX_NAME];       /* 用户名 */
    char display[USRGN_MAX_NAME];    /* 显示名 */
    char home[64];                   /* 主目录 (在 PlexsDOS 上为路径) */
    char shell[32];                  /* 登录 shell */
    uint32_t uid;                    /* 用户 ID */
    uint32_t gid;                    /* 组 ID */
    uint8_t flags;                   /* 标志 */
};

/*
 * usrgn_init — 初始化用户数据库
 * 加载预定义用户列表。
 */
void usrgn_init(void);

/*
 * usrgn_get_user — 按用户名查找用户
 * @name: 用户名
 * 返回: 用户信息指针, 未找到返回 NULL。
 *
 * 对应 getpwnam(name)。
 */
const struct usrgn_user *usrgn_get_user(const char *name);

/*
 * usrgn_get_user_by_uid — 按 UID 查找用户
 * @uid: 用户 ID
 * 返回: 用户信息指针, 未找到返回 NULL。
 */
const struct usrgn_user *usrgn_get_user_by_uid(uint32_t uid);

/*
 * usrgn_get_first — 获取第一个用户 (getpwent 替代)
 * 返回: 第一个用户信息, 无用户返回 NULL。
 */
const struct usrgn_user *usrgn_get_first(void);

/*
 * usrgn_get_next — 获取下一个用户
 * 返回: 下一个用户信息, 遍历完成返回 NULL。
 *
 * 配合 usrgn_get_first 使用, 类似 getpwent 循环。
 */
const struct usrgn_user *usrgn_get_next(void);

/*
 * usrgn_verify_password — 验证用户密码
 * @name:     用户名
 * @password: 明文密码
 * 返回: true = 密码正确。
 */
bool usrgn_verify_password(const char *name, const char *password);

/*
 * usrgn_set_password — 设置用户密码
 * @name:     用户名
 * @password: 新密码
 * 返回: true = 成功。
 */
bool usrgn_set_password(const char *name, const char *password);

/*
 * usrgn_user_count — 获取用户总数
 * 返回: 注册用户数。
 */
int usrgn_user_count(void);

#ifdef __cplusplus
}
#endif

#endif /* _PLXSDOS_USRGN_H */
