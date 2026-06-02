/*
 * Nexsteaduser — PlexsDOS
 * dfan.c — 认证框架实现 (PAM 兼容层)
 * 作者: Tinmc189623 | 团队: Nexlyh
 *
 * 实现 DFAN (Display Manager Authentication) API:
 *   - 硬编码 root 用户 + Nexsteaduser 密码验证
 *   - 支持多用户扩展
 *   - 无外部依赖
 */

#include <plexsdos/dfan.h>
#include <plexsdos/usrgn.h>
#include <plexsdos/string.h>

/* DFAN 会话句柄 */
struct dfan_handle {
    char service[32];         /* 服务名 */
    char user[USRGN_MAX_NAME]; /* 用户名 */
    int authenticated;        /* 已认证标志 */
    int session_open;         /* 会话已打开 */
    int cred_established;     /* 凭证已建立 */
};

/*
 * dfan_start — 开始 DFAN 会话
 */
int dfan_start(const char *service, const char *user,
               void *conv, struct dfan_handle **handle)
{
    (void)conv;

    if (!service || !user || !handle)
        return DFAN_SYSTEM_ERR;

    *handle = (struct dfan_handle *)0;  /* 静态分配, 无堆 */

    /* 使用内部静态句柄 (PlexsDOS 无堆) */
    static struct dfan_handle internal_handle;
    internal_handle.service[0] = '\0';
    internal_handle.user[0] = '\0';
    internal_handle.authenticated = 0;
    internal_handle.session_open = 0;
    internal_handle.cred_established = 0;

    /* 复制服务名 */
    int i;
    for (i = 0; service[i] && i < 31; i++)
        internal_handle.service[i] = service[i];
    internal_handle.service[i] = '\0';

    /* 复制用户名 */
    for (i = 0; user[i] && i < (USRGN_MAX_NAME - 1); i++)
        internal_handle.user[i] = user[i];
    internal_handle.user[i] = '\0';

    /* 验证用户是否存在 */
    const struct usrgn_user *u = usrgn_get_user(user);
    if (!u)
        return DFAN_USER_UNKNOWN;

    *handle = &internal_handle;
    return DFAN_SUCCESS;
}

/*
 * dfan_authenticate — 验证用户密码
 */
int dfan_authenticate(struct dfan_handle *handle, int flags,
                      const char *password)
{
    (void)flags;

    if (!handle)
        return DFAN_SYSTEM_ERR;

    if (!password || password[0] == '\0')
        return DFAN_AUTHINFO_UNAVAIL;

    if (!usrgn_get_user(handle->user))
        return DFAN_USER_UNKNOWN;

    if (usrgn_verify_password(handle->user, password)) {
        handle->authenticated = 1;
        return DFAN_SUCCESS;
    }

    return DFAN_AUTH_ERR;
}

/*
 * dfan_acct_mgmt — 账户管理检查
 */
int dfan_acct_mgmt(struct dfan_handle *handle, int flags)
{
    (void)flags;

    if (!handle)
        return DFAN_SYSTEM_ERR;

    const struct usrgn_user *u = usrgn_get_user(handle->user);
    if (!u)
        return DFAN_USER_UNKNOWN;

    /* 检查账户是否被禁用 */
    if (u->flags & USRGN_FLAG_DISABLED)
        return DFAN_ACCT_EXPIRED;

    return DFAN_SUCCESS;
}

/*
 * dfan_open_session — 打开用户会话
 */
int dfan_open_session(struct dfan_handle *handle, int flags)
{
    (void)flags;

    if (!handle)
        return DFAN_SYSTEM_ERR;

    if (!handle->authenticated)
        return DFAN_AUTH_ERR;

    handle->session_open = 1;
    return DFAN_SUCCESS;
}

/*
 * dfan_close_session — 关闭用户会话
 */
int dfan_close_session(struct dfan_handle *handle, int flags)
{
    (void)flags;

    if (!handle)
        return DFAN_SYSTEM_ERR;

    handle->session_open = 0;
    handle->authenticated = 0;
    return DFAN_SUCCESS;
}

/*
 * dfan_setcred — 设置用户凭证
 */
int dfan_setcred(struct dfan_handle *handle, int flags)
{
    (void)flags;

    if (!handle)
        return DFAN_SYSTEM_ERR;

    handle->cred_established = 1;
    return DFAN_SUCCESS;
}

/*
 * dfan_end — 结束 DFAN 会话
 */
int dfan_end(struct dfan_handle *handle, int status)
{
    (void)status;

    if (!handle)
        return DFAN_SYSTEM_ERR;

    handle->authenticated = 0;
    handle->session_open = 0;
    handle->cred_established = 0;
    return DFAN_SUCCESS;
}

/*
 * dfan_strerror — 获取错误描述
 */
const char *dfan_strerror(int errnum)
{
    switch (errnum) {
    case DFAN_SUCCESS:          return "Success";
    case DFAN_AUTH_ERR:         return "Authentication failed";
    case DFAN_ACCT_EXPIRED:     return "Account expired or disabled";
    case DFAN_CRED_INSUFFICIENT: return "Insufficient credentials";
    case DFAN_AUTHINFO_UNAVAIL: return "Authentication info unavailable";
    case DFAN_USER_UNKNOWN:     return "User unknown";
    case DFAN_PERM_DENIED:      return "Permission denied";
    case DFAN_SYSTEM_ERR:       return "System error";
    default:                    return "Unknown error";
    }
}
