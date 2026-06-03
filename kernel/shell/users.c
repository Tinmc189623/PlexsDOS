/*
 * Nexsteaduser — PlexsDOS
 * users.c — 多用户管理实现
 * 作者: Tinmc189623 | 团队: Nexlyh
 *
 * 用户账户管理、登录认证、会话跟踪。
 */

#include <plexsdos/types.h>
#include <plexsdos/users.h>
#include <plexsdos/screen.h>
#include <plexsdos/serial.h>

/* 用户表 */
static struct user user_table[MAX_USERS];
static int user_count = 0;

/* 当前登录用户指针 */
static struct user *current_user = NULL;

/*
 * user_hash_password — 简单密码散列
 * @password: 明文密码
 * @hash:     [输出] 散列值
 *
 * XOR 旋转哈希 + 固定盐值。非加密安全, 仅防意外窥视。
 */
void user_hash_password(const char *password, char *hash)
{
    uint32_t h = 0x9E3779B9; /* 黄金比例常数 */
    uint32_t salt = 0x6B8B4567;

    while (*password) {
        h = ((h << 5) + h) ^ (uint8_t)(*password) ^ salt;
        salt = (salt << 1) | (salt >> 31);
        password++;
    }

    /* 转十六进制字符串 */
    const char *hex = "0123456789ABCDEF";
    for (int i = 0; i < 7; i++) {
        hash[i * 4]     = hex[(h >> (i * 4)) & 0x0F];
        hash[i * 4 + 1] = hex[(h >> (i * 4 + 4)) & 0x0F];
        hash[i * 4 + 2] = hex[(h >> (i * 4 + 8)) & 0x0F];
        hash[i * 4 + 3] = hex[(h >> (i * 4 + 12)) & 0x0F];
    }
    hash[28] = '\0';
}

/* ===== 内部函数 ===== */

static int find_free_slot(void)
{
    for (int i = 0; i < MAX_USERS; i++) {
        if (!user_table[i].active)
            return i;
    }
    return -1;
}

/* ===== 初始化 ===== */

/*
 * users_init — 初始化用户系统
 *
 * 创建默认 root 用户 (uid=0, gid=GROUP_ADMIN, 密码="admin")。
 */
void users_init(void)
{
    for (int i = 0; i < MAX_USERS; i++) {
        user_table[i].active = false;
    }
    user_count = 0;
    current_user = NULL;

    /* 创建 root 管理员 */
    user_create("root", "admin", "System Administrator", GROUP_ADMIN);

    /* 创建普通用户 demo */
    user_create("demo", "demo", "Demo User", GROUP_USER);

    serial_puts("[users] initialized (root, demo).\n");
}

/* ===== 认证 ===== */

bool user_login(const char *username, const char *password)
{
    struct user *u = user_get_by_name(username);
    if (!u) {
        screen_puts("Login failed: user not found.\n");
        return false;
    }

    char hash[PASSWORD_MAX];
    user_hash_password(password, hash);

    /* 常量时间比较 (防止时序攻击) */
    bool match = true;
    for (int i = 0; i < 28; i++) {
        if (hash[i] != u->password_hash[i])
            match = false;
    }

    if (!match) {
        screen_puts("Login failed: incorrect password.\n");
        return false;
    }

    current_user = u;
    serial_puts("[users] login: ");
    serial_puts(username);
    serial_putchar('\n');
    return true;
}

void user_logout(void)
{
    current_user = NULL;
    screen_puts("Logged out.\n");
}

bool user_is_logged_in(void)
{
    return current_user != NULL;
}

/* ===== 用户管理 ===== */

int user_create(const char *username, const char *password,
                const char *fullname, uint32_t gid)
{
    int slot = find_free_slot();
    if (slot < 0)
        return -1;

    struct user *u = &user_table[slot];
    int i;

    /* 复制用户名 */
    for (i = 0; username[i] && i < USERNAME_MAX - 1; i++)
        u->username[i] = username[i];
    u->username[i] = '\0';

    /* 密码散列 */
    user_hash_password(password, u->password_hash);

    /* 复制全名 */
    if (fullname) {
        for (i = 0; fullname[i] && i < FULLNAME_MAX - 1; i++)
            u->fullname[i] = fullname[i];
        u->fullname[i] = '\0';
    } else {
        u->fullname[0] = '\0';
    }

    u->uid = slot;
    u->gid = gid;
    u->active = true;
    user_count++;

    return slot;
}

bool user_delete(uint32_t uid)
{
    if (uid >= MAX_USERS || !user_table[uid].active)
        return false;

    if (current_user == &user_table[uid])
        current_user = NULL;

    user_table[uid].active = false;
    user_count--;
    return true;
}

struct user *user_get_by_uid(uint32_t uid)
{
    if (uid >= MAX_USERS || !user_table[uid].active)
        return NULL;
    return &user_table[uid];
}

struct user *user_get_by_name(const char *username)
{
    for (int i = 0; i < MAX_USERS; i++) {
        if (!user_table[i].active)
            continue;

        const char *a = username;
        const char *b = user_table[i].username;
        while (*a && *b && *a == *b) {
            a++; b++;
        }
        if (*a == '\0' && *b == '\0')
            return &user_table[i];
    }
    return NULL;
}

struct user *user_get_current(void)
{
    return current_user;
}

int user_get_count(void)
{
    return user_count;
}
