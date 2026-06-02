/*
 * Nexsteaduser — PlexsDOS
 * dfan.h — 认证框架 (PAM 兼容层)
 * 作者: Tinmc189623 | 团队: Nexlyh
 *
 * DFAN (Display Manager Authentication) 替代 PAM:
 *   dfan_start       → pam_start
 *   dfan_authenticate → pam_authenticate
 *   dfan_acct_mgmt   → pam_acct_mgmt
 *   dfan_open_session → pam_open_session
 *   dfan_close_session → pam_close_session
 *   dfan_end         → pam_end
 *   dfan_strerror    → pam_strerror
 *
 * 实现: 硬编码 root 用户 + 密码验证, 无外部依赖。
 */

#ifndef _PLXSDOS_DFAN_H
#define _PLXSDOS_DFAN_H

#include <plexsdos/types.h>

#ifdef __cplusplus
extern "C" {
#endif

/* DFAN 错误码 (兼容 PAM 语义) */
#define DFAN_SUCCESS         0
#define DFAN_AUTH_ERR        1
#define DFAN_ACCT_EXPIRED    2
#define DFAN_CRED_INSUFFICIENT 3
#define DFAN_AUTHINFO_UNAVAIL 4
#define DFAN_USER_UNKNOWN    5
#define DFAN_PERM_DENIED     6
#define DFAN_SYSTEM_ERR      7

/* DFAN 标志 */
#define DFAN_SILENT          0x01
#define DFAN_DISALLOW_NULL   0x02
#define DFAN_ESTABLISH_CRED  0x04
#define DFAN_DELETE_CRED     0x08
#define DFAN_REINITIALIZE_CRED 0x10

/* DFAN 会话句柄 (不透明) */
struct dfan_handle;

/*
 * dfan_start — 开始 DFAN 会话
 * @service:  服务名 (如 "lightdm")
 * @user:     用户名
 * @conv:     会话回调函数 (可 NULL)
 * @handle:   输出句柄
 * 返回: DFAN_SUCCESS 或错误码。
 */
int dfan_start(const char *service, const char *user,
               void *conv, struct dfan_handle **handle);

/*
 * dfan_authenticate — 验证用户密码
 * @handle:   DFAN 会话句柄
 * @flags:    标志
 * @password: 密码
 * 返回: DFAN_SUCCESS 或错误码。
 */
int dfan_authenticate(struct dfan_handle *handle, int flags,
                      const char *password);

/*
 * dfan_acct_mgmt — 账户管理检查
 * @handle: DFAN 会话句柄
 * @flags:  标志
 * 返回: DFAN_SUCCESS 或错误码。
 */
int dfan_acct_mgmt(struct dfan_handle *handle, int flags);

/*
 * dfan_open_session — 打开用户会话
 * @handle: DFAN 会话句柄
 * @flags:  标志
 * 返回: DFAN_SUCCESS 或错误码。
 */
int dfan_open_session(struct dfan_handle *handle, int flags);

/*
 * dfan_close_session — 关闭用户会话
 * @handle: DFAN 会话句柄
 * @flags:  标志
 * 返回: DFAN_SUCCESS 或错误码。
 */
int dfan_close_session(struct dfan_handle *handle, int flags);

/*
 * dfan_setcred — 设置用户凭证
 * @handle: DFAN 会话句柄
 * @flags:  标志
 * 返回: DFAN_SUCCESS 或错误码。
 */
int dfan_setcred(struct dfan_handle *handle, int flags);

/*
 * dfan_end — 结束 DFAN 会话
 * @handle: DFAN 会话句柄
 * @status: 终止状态
 * 返回: DFAN_SUCCESS 或错误码。
 */
int dfan_end(struct dfan_handle *handle, int status);

/*
 * dfan_strerror — 获取错误描述
 * @errnum: 错误码
 * 返回: 错误描述字符串。
 */
const char *dfan_strerror(int errnum);

#ifdef __cplusplus
}
#endif

#endif /* _PLXSDOS_DFAN_H */
