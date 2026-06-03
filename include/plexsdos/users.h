/*
 * Nexsteaduser — PlexsDOS
 * users.h — 多用户管理接口
 * 作者: Tinmc189623 | 团队: Nexlyh
 *
 * 用户账户管理、登录认证、权限控制。
 */

#ifndef _PLXSDOS_USERS_H
#define _PLXSDOS_USERS_H

#include <plexsdos/types.h>

#ifdef __cplusplus
extern "C" {
#endif

/* 用户常量 */
#define USERNAME_MAX  24
#define PASSWORD_MAX  32
#define FULLNAME_MAX  48
#define MAX_USERS     16

/* 用户组 */
#define GROUP_ADMIN   0
#define GROUP_USER    1
#define GROUP_GUEST   2

/* 用户记录 */
struct user {
    uint32_t uid;               /* 用户 ID */
    uint32_t gid;               /* 组 ID */
    char     username[USERNAME_MAX]; /* 用户名 */
    char     password_hash[PASSWORD_MAX]; /* 密码散列 */
    char     fullname[FULLNAME_MAX]; /* 全名 */
    bool     active;            /* 账户是否启用 */
};

/* ===== 初始化 ===== */

/*
 * users_init — 初始化用户系统
 *
 * 创建默认的 root 管理员账户 (密码: "admin")。
 */
void users_init(void);

/* ===== 认证 ===== */

/*
 * user_login — 用户登录
 * @username: 用户名
 * @password: 密码
 * 返回: true = 登录成功, false = 用户名或密码错误。
 */
bool user_login(const char *username, const char *password);

/*
 * user_logout — 注销当前用户
 */
void user_logout(void);

/*
 * user_is_logged_in — 检查是否有用户已登录
 * 返回: true = 已登录。
 */
bool user_is_logged_in(void);

/* ===== 用户管理 ===== */

/*
 * user_create — 创建新用户
 * @username: 用户名
 * @password: 密码
 * @fullname: 全名
 * @gid:      组 ID (GROUP_ADMIN / GROUP_USER)
 * 返回: UID, 失败返回 -1。
 */
int user_create(const char *username, const char *password,
                const char *fullname, uint32_t gid);

/*
 * user_delete — 删除用户
 * @uid: 用户 ID
 * 返回: true = 成功。
 */
bool user_delete(uint32_t uid);

/*
 * user_get_by_uid — 通过 UID 获取用户信息
 * @uid: 用户 ID
 * 返回: 用户指针, 未找到返回 NULL。
 */
struct user *user_get_by_uid(uint32_t uid);

/*
 * user_get_by_name — 通过用户名获取用户信息
 * @username: 用户名
 * 返回: 用户指针, 未找到返回 NULL。
 */
struct user *user_get_by_name(const char *username);

/*
 * user_get_current — 获取当前登录用户
 * 返回: 当前用户指针。
 */
struct user *user_get_current(void);

/*
 * user_get_count — 获取用户总数
 * 返回: 用户数。
 */
int user_get_count(void);

/* ===== 密码工具 ===== */

/*
 * user_hash_password — 简单密码散列
 * @password: 明文密码
 * @hash:     [输出] 散列值 (至少 PASSWORD_MAX 字节)
 *
 * 使用简单哈希算法 (非加密安全, 仅用于演示)。
 */
void user_hash_password(const char *password, char *hash);

#ifdef __cplusplus
}
#endif

#endif /* _PLXSDOS_USERS_H */
