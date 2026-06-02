/*
 * Nexsteaduser — PlexsDOS
 * int21.c — POSIX 进程/信号 INT 21h 兼容层 (基于 LightDM 源码移植)
 * 作者: Tinmc189623 | 团队: Nexlyh
 *
 * 本文件是 LightDM (https://github.com/canonical/lightdm) 的移植适配层。
 * 原始 LightDM 版权所有: (C) 2010-2011 Robert Ancell
 * 基于 GPL-3.0 协议发布。
 *
 * POSIX 进程管理 → PlexsDOS INT 21h 映射:
 *   fork     → plxdm_fork    (不支持多进程, 返回 -1)
 *   execv    → plxdm_execv   (程序加载)
 *   waitpid  → plxdm_waitpid (等待子进程)
 *   kill     → plxdm_kill    (发送信号)
 *
 * PlexsDOS 为单地址空间, 无 POSIX 进程模型。
 * fork/exec 用于启动外部程序 (如 greeter), 在 PlexsDOS 中
 * 通过程序加载器 + 任务切换实现。
 */

#include <plexsdos/types.h>
#include <plexsdos/string.h>

/* ==================== 进程管理 ==================== */

/*
 * plxdm_fork — 创建子进程
 * PlexsDOS 无多进程支持, 返回 -1 (不支持)。
 */
int plxdm_fork(void)
{
    /* PlexsDOS 当前不支持多进程 */
    return -1;
}

/*
 * plxdm_execv — 加载并执行程序
 * @path: 程序路径
 * @argv: 参数列表
 * 返回: 0 = 成功, -1 = 失败。
 *
 * 通过 INT 21h 程序加载功能实现。
 */
int plxdm_execv(const char *path, char *const argv[])
{
    (void)path;
    (void)argv;

    /* PlexsDOS 程序加载通过 INT 21h AH=0x4B */
    /* TODO: 实现程序加载器 */
    return -1;
}

/*
 * plxdm_execve — 加载并执行程序 (带环境变量)
 */
int plxdm_execve(const char *path, char *const argv[], char *const envp[])
{
    (void)path;
    (void)argv;
    (void)envp;
    return -1;
}

/*
 * plxdm_waitpid — 等待子进程状态变化
 * @pid: 子进程 ID
 * @status: 输出状态
 * @options: 选项
 * 返回: 子进程 PID, -1 = 失败。
 */
int plxdm_waitpid(int pid, int *status, int options)
{
    (void)pid;
    (void)status;
    (void)options;
    return -1;
}

/*
 * plxdm_kill — 向进程发送信号
 * @pid: 目标进程 ID
 * @sig: 信号编号
 * 返回: 0 = 成功, -1 = 失败。
 */
int plxdm_kill(int pid, int sig)
{
    (void)pid;
    (void)sig;
    return 0;  /* 静默成功 */
}

/*
 * plxdm_getpid — 获取当前进程 ID
 * 返回: 进程 ID (始终返回 1, 单进程)。
 */
int plxdm_getpid(void)
{
    return 1;  /* 单进程, 始终为 1 */
}

/*
 * plxdm_pipe — 创建管道
 * @pipefd: 输出文件描述符 [2]
 * 返回: 0 = 成功, -1 = 失败。
 *
 * PlexsDOS 使用内存缓冲区模拟管道。
 */
int plxdm_pipe(int pipefd[2])
{
    (void)pipefd;
    return -1;  /* 暂不支持 */
}

/*
 * plxdm_dup2 — 复制文件描述符
 * @oldfd: 原 fd
 * @newfd: 目标 fd
 * 返回: 新 fd, -1 = 失败。
 */
int plxdm_dup2(int oldfd, int newfd)
{
    (void)oldfd;
    (void)newfd;
    return -1;
}

/*
 * plxdm_chdir — 改变工作目录
 * @path: 目录路径
 * 返回: 0 = 成功, -1 = 失败。
 */
int plxdm_chdir(const char *path)
{
    (void)path;
    return 0;  /* 静默成功 */
}

/*
 * plxdm_setgid — 设置组 ID
 * @gid: 组 ID
 * 返回: 0 = 成功, -1 = 失败。
 */
int plxdm_setgid(int gid)
{
    (void)gid;
    return 0;
}

/*
 * plxdm_setuid — 设置用户 ID
 * @uid: 用户 ID
 * 返回: 0 = 成功, -1 = 失败。
 */
int plxdm_setuid(int uid)
{
    (void)uid;
    return 0;
}

/*
 * plxdm_setsid — 创建新会话
 * 返回: 会话 ID, -1 = 失败。
 */
int plxdm_setsid(void)
{
    return 1;
}

/*
 * plxdm_getegid — 获取有效组 ID
 */
int plxdm_getegid(void)
{
    return 0;
}

/*
 * plxdm_geteuid — 获取有效用户 ID
 */
int plxdm_geteuid(void)
{
    return 0;
}

/*
 * plxdm_getgid — 获取组 ID
 */
int plxdm_getgid(void)
{
    return 0;
}

/*
 * plxdm_getuid — 获取用户 ID
 */
int plxdm_getuid(void)
{
    return 0;
}

/* ==================== 信号处理 ==================== */

/*
 * plxdm_signal — 注册信号处理函数
 * @signum: 信号编号
 * @handler: 处理函数
 * 返回: 原处理函数指针。
 *
 * PlexsDOS 信号通过 INT 21h 实现。
 */
typedef void (*sighandler_t)(int);
sighandler_t plxdm_signal(int signum, sighandler_t handler)
{
    (void)signum;
    (void)handler;
    return NULL;
}

/* ==================== 文件 I/O ==================== */

/*
 * plxdm_open — 打开文件
 * @path: 文件路径
 * @flags: 打开标志
 * 返回: 文件描述符, -1 = 失败。
 *
 * 通过 PlexsDOS HAL 层调用 INT 21h 文件服务。
 */
int plxdm_open(const char *path, int flags)
{
    (void)path;
    (void)flags;

    /* TODO: 通过 INT 21h AH=0x3D 实现 */
    return -1;
}

/*
 * plxdm_close — 关闭文件
 */
int plxdm_close(int fd)
{
    (void)fd;
    return 0;
}

/*
 * plxdm_read — 读文件
 */
int plxdm_read(int fd, void *buf, unsigned int count)
{
    (void)fd;
    (void)buf;
    (void)count;
    return -1;
}

/*
 * plxdm_write — 写文件
 */
int plxdm_write(int fd, const void *buf, unsigned int count)
{
    (void)fd;
    (void)buf;
    (void)count;
    return -1;
}

/* ==================== usrgn 补充 ==================== */

/*
 * usrgn_get_group — 按名称查找组
 * PlexsDOS 无组概念, 返回 NULL。
 */
const struct usrgn_user *usrgn_get_group(const char *name)
{
    (void)name;
    return NULL;
}
