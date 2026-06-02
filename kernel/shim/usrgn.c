/*
 * Nexsteaduser — PlexsDOS
 * usrgn.c — 用户管理实现 (getpwnam/setuid 兼容层)
 * 作者: Tinmc189623 | 团队: Nexlyh
 *
 * 预定义用户数据库:
 *   - root: 默认管理员账户
 *   - guest: 访客账户 (可选)
 *
 * 密码存储: 明文 (PlexsDOS 在保护模式下运行, 无需 shadow)
 */

#include <plexsdos/usrgn.h>
#include <plexsdos/string.h>

/* 密码存储 (明文) */
static struct {
    char name[USRGN_MAX_NAME];
    char password[USRGN_MAX_PASS];
} passdb[USRGN_MAX_USERS];

/* 用户数据库 */
static const struct usrgn_user userdb[] = {
    { "root",     "Nexsteaduser Administrator", "/system", "/bin/shell", 0, 0, 0 },
    { "guest",    "Guest User",                 "/home/guest", "/bin/shell", 1, 1, 0 }
};

#define NUM_USERS (sizeof(userdb) / sizeof(userdb[0]))
static int current_index = -1;

/*
 * usrgn_init — 初始化用户数据库
 */
void usrgn_init(void)
{
    /* 初始化密码数据库 */
    for (int i = 0; i < USRGN_MAX_USERS; i++) {
        passdb[i].name[0] = '\0';
        passdb[i].password[0] = '\0';
    }

    /* root 默认密码 (首次安装后提示修改) */
    int i;
    for (i = 0; i < USRGN_MAX_NAME - 1 && userdb[0].name[i]; i++)
        passdb[0].name[i] = userdb[0].name[i];
    passdb[0].name[i] = '\0';

    const char *default_pass = "Nexsteaduser";
    for (i = 0; i < USRGN_MAX_PASS - 1 && default_pass[i]; i++)
        passdb[0].password[i] = default_pass[i];
    passdb[0].password[i] = '\0';

    current_index = -1;
}

/*
 * usrgn_get_user — 按用户名查找用户
 */
const struct usrgn_user *usrgn_get_user(const char *name)
{
    if (!name)
        return NULL;

    for (int i = 0; i < NUM_USERS; i++) {
        int j;
        for (j = 0; userdb[i].name[j] && name[j]; j++) {
            if (userdb[i].name[j] != name[j])
                break;
        }
        if (userdb[i].name[j] == '\0' && name[j] == '\0')
            return &userdb[i];
    }
    return NULL;
}

/*
 * usrgn_get_user_by_uid — 按 UID 查找用户
 */
const struct usrgn_user *usrgn_get_user_by_uid(uint32_t uid)
{
    for (int i = 0; i < NUM_USERS; i++) {
        if (userdb[i].uid == uid)
            return &userdb[i];
    }
    return NULL;
}

/*
 * usrgn_get_first — 获取第一个用户
 */
const struct usrgn_user *usrgn_get_first(void)
{
    if (NUM_USERS == 0)
        return NULL;
    current_index = 0;
    return &userdb[0];
}

/*
 * usrgn_get_next — 获取下一个用户
 */
const struct usrgn_user *usrgn_get_next(void)
{
    current_index++;
    if (current_index >= (int)NUM_USERS)
        return NULL;
    return &userdb[current_index];
}

/*
 * usrgn_verify_password — 验证用户密码
 */
bool usrgn_verify_password(const char *name, const char *password)
{
    if (!name || !password)
        return false;

    for (int i = 0; i < USRGN_MAX_USERS; i++) {
        if (passdb[i].name[0] == '\0')
            continue;

        /* 比较用户名 */
        int match = 1;
        int j;
        for (j = 0; passdb[i].name[j] && name[j]; j++) {
            if (passdb[i].name[j] != name[j]) {
                match = 0;
                break;
            }
        }
        if (!match) continue;
        if (passdb[i].name[j] != name[j]) continue;

        /* 比较密码 */
        for (j = 0; passdb[i].password[j] && password[j]; j++) {
            if (passdb[i].password[j] != password[j]) {
                match = 0;
                break;
            }
        }
        if (match && passdb[i].password[j] == '\0' && password[j] == '\0')
            return true;
    }
    return false;
}

/*
 * usrgn_set_password — 设置用户密码
 */
bool usrgn_set_password(const char *name, const char *password)
{
    if (!name || !password)
        return false;

    for (int i = 0; i < USRGN_MAX_USERS; i++) {
        if (passdb[i].name[0] == '\0')
            continue;

        /* 比较用户名 */
        int match = 1;
        int j;
        for (j = 0; passdb[i].name[j] && name[j]; j++) {
            if (passdb[i].name[j] != name[j]) {
                match = 0;
                break;
            }
        }
        if (!match) continue;
        if (passdb[i].name[j] != name[j]) continue;

        /* 复制新密码 */
        for (j = 0; password[j] && j < USRGN_MAX_PASS - 1; j++)
            passdb[i].password[j] = password[j];
        passdb[i].password[j] = '\0';
        return true;
    }
    return false;
}

/*
 * usrgn_user_count — 获取用户总数
 */
int usrgn_user_count(void)
{
    return NUM_USERS;
}
