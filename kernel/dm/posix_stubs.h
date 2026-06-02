/*
 * Nexsteaduser — PlexsDOS
 * posix_stubs.h — POSIX 系统调用占位 (PlexsDOS 原生实现)
 * 作者: Tinmc189623 | 团队: Nexlyh
 *
 * 提供 PlexsDOS 上 LightDM 所需的最小 POSIX 类型和函数声明。
 * 实际实现在 kernel/shim/int21.c 中。
 */

#ifndef _POSIX_STUBS_H
#define _POSIX_STUBS_H

#include <plexsdos/types.h>

#ifdef __cplusplus
extern "C" {
#endif

/* ==================== 类型定义 ==================== */
typedef int pid_t;
typedef unsigned int uid_t;
typedef unsigned int gid_t;
typedef unsigned int mode_t;
typedef long off_t;
typedef int ssize_t;

/* ==================== 错误码 ==================== */
#define EINTR           4
#define EAGAIN          11
#define EEXIST          17
#define ENOENT          2
#define EACCES          13
#define EIO             5
#define ENOMEM          12
#define EBADF           9
#define ECHILD          10
#define ENOEXEC         8
#define ESRCH           3
#define EPIPE           32
#define ENOSPC          28
#define EROFS           30
#define ELOOP           40
#define ENAMETOOLONG    36
#define ENOTDIR         20
#define EISDIR          21
#define EMFILE          24
#define ENFILE          23

/* ==================== 文件控制 ==================== */
#define O_RDONLY        0
#define O_WRONLY        1
#define O_RDWR          2
#define O_CREAT         0x40
#define O_TRUNC         0x200
#define O_APPEND        0x400
#define O_EXCL          0x80
#define O_CLOEXEC       0x8000

#define O_NOCTTY        0x10000

/* 文件访问模式 */
#define F_OK            0
#define R_OK            4
#define W_OK            2
#define X_OK            1

#define F_DUPFD         0
#define F_GETFD         1
#define F_SETFD         2
#define F_GETFL         3
#define F_SETFL         4
#define FD_CLOEXEC      1

#define SEEK_SET        0
#define SEEK_CUR        1
#define SEEK_END        2

/* ==================== 文件状态 ==================== */
struct stat {
    mode_t  st_mode;
    off_t   st_size;
    int     st_blksize;
};

#define S_IFMT          0xF000
#define S_IFDIR         0x4000
#define S_IFREG         0x8000
#define S_IRWXU         0x1C0
#define S_IRUSR         0x100
#define S_IWUSR         0x080
#define S_IXUSR         0x040
#define S_IRWXG         0x038
#define S_IRGRP         0x020
#define S_IWGRP         0x010
#define S_IXGRP         0x008
#define S_IRWXO         0x007
#define S_IROTH         0x004
#define S_IWOTH         0x002
#define S_IXOTH         0x001
#define S_ISDIR(m)      (((m) & S_IFMT) == S_IFDIR)
#define S_ISREG(m)      (((m) & S_IFMT) == S_IFREG)

/* ==================== 信号 ==================== */
#define SIGTERM         15
#define SIGINT          2
#define SIGKILL         9
#define SIGCHLD         17
#define SIGHUP          1
#define SIGUSR1         10
#define SIGUSR2         12
#define SIGPIPE         13
#define SIGSEGV         11
#define SIGBUS          7
#define SIGQUIT         3
#define SIGTRAP         5
#define SIGALRM         14
#define SIGSTOP         19
#define SIGTSTP         20
#define SIGCONT         18

#define SIG_DFL         ((void (*)(int))0)
#define SIG_IGN         ((void (*)(int))1)
#define SIG_ERR         ((void (*)(int))-1)

/* ==================== 进程等待 ==================== */
#define WNOHANG         1
#define WUNTRACED       2
#define WEXITSTATUS(s)  (((s) & 0xFF00) >> 8)
#define WIFEXITED(s)    (((s) & 0x7F) == 0)
#define WIFSIGNALED(s)  (((s) & 0x7F) > 0 && ((s) & 0x7F) < 0x7F)
#define WTERMSIG(s)     ((s) & 0x7F)
#define WSTOPSIG(s)     WEXITSTATUS(s)
#define WIFSTOPPED(s)   (((s) & 0xFF) == 0x7F)

/* ==================== 文件描述符 ==================== */
#define STDIN_FILENO    0
#define STDOUT_FILENO   1
#define STDERR_FILENO   2

/* ==================== UID/GID 映射 ==================== */
#define getuid()  plxdm_getuid()
#define getgid()  plxdm_getgid()
#define geteuid() plxdm_geteuid()
#define getegid() plxdm_getegid()

/* ==================== POSIX 系统调用映射 ==================== */
#define getpid()    plxdm_getpid()
#define pipe(fds)   plxdm_pipe(fds)
#define setuid(uid) plxdm_setuid(uid)
#define setgid(gid) plxdm_setgid(gid)
#define seteuid(e)  plxdm_seteuid(e)
#define setegid(e)  plxdm_setegid(e)

/* 声明 seteuid/setegid (在 posix_stubs.h 有 setuid/setgid 但缺 seteuid/setegid) */
int plxdm_seteuid(int euid);
int plxdm_setegid(int egid);

/* ==================== errno ==================== */
#ifndef errno
extern int errno;
#endif

/* ==================== gettext 占位 ==================== */
#define _(str) (str)
#define N_(str) (str)
#define GETTEXT_PACKAGE "lightdm"

/* ==================== POSIX 附加系统调用映射 ==================== */
#define fork()          plxdm_fork()
#define dup2(o, n)      plxdm_dup2(o, n)
#define execvp(p, a)    plxdm_execv(p, a)
#define _exit(s)        exit(s)
#define setenv(n, v, o) (0)
#define unsetenv(n)     (0)

/* signal.h 简化 */
typedef unsigned long sigset_t;
/* siginfo_t — 轻量定义 (仅用于 sigaction::sa_sigaction 指针类型) */
typedef struct { int si_signo; int si_errno; int si_code; int si_pid; } siginfo_t;
struct sigaction {
    void (*sa_handler)(int);
    void (*sa_sigaction)(int, siginfo_t *, void *);
    sigset_t sa_mask;
    unsigned long sa_flags;
};
#define sigemptyset(s)  (*(s) = 0)
#define sigaction(s, a, o) (0)
#define SA_RESTART      0x10000000
#define SA_SIGINFO      0x00000004

/* signal() */
#define signal(sig, handler) plxdm_signal(sig, handler)

/* gethostname */
#define gethostname(buf, len) (void)0

/* execlp */
#define execlp(path, arg, ...) plxdm_execv(path, (char*[]){arg, NULL})

/* chmod */
#define chmod(path, mode) 0

/* fsync */
#define fsync(fd) 0

/* 文件 I/O 宏映射到 plxdm_* */
#define write(fd, buf, count)   plxdm_write(fd, buf, count)
#define close(fd)               plxdm_close(fd)
#define read(fd, buf, count)    plxdm_read(fd, buf, count)
#define open(path, flags, ...)  plxdm_open(path, flags)
#define access(path, mode)      (0)

/* VERSION */
#define VERSION "1.0.0-plexsdos"

/* LightDM 构建时路径定义 (configure.ac) */
#define SBIN_DIR          "/usr/sbin"
#define LOG_DIR            "/var/log/lightdm"
#define RUN_DIR            "/var/run/lightdm"
#define CACHE_DIR          "/var/cache/lightdm"
#define GREETER_USER       "lightdm"
#define DEFAULT_GREETER_SESSION "default"
#define DEFAULT_USER_SESSION     "default"
#define SESSIONS_DIR       "/usr/share/lightdm/sessions:/usr/share/xsessions:/usr/share/wayland-sessions"
#define REMOTE_SESSIONS_DIR "/usr/share/lightdm/remote-sessions"

/* stdio 存根 */
#define fopen(p, m)       ((FILE*)NULL)
#define fclose(f)         (-1)

/* stdlib 存根 */
#define getenv(n)         ((const char*)NULL)

/* gobject 导出 */
#define G_TYPE_FROM_CLASS(c) 0
#define G_TYPE_FROM_INSTANCE(i) 0

/* ==================== 标准 I/O ==================== */
typedef struct { int fd; } FILE;
extern FILE *stderr;
extern FILE *stdout;
extern FILE *stdin;

#define stderr ((FILE*)2)
#define stdout ((FILE*)1)
#define stdin  ((FILE*)0)

/* 退出状态 */
#define EXIT_SUCCESS 0
#define EXIT_FAILURE 1

/* ==================== PAM 占位 (使用 DFAN 替代) ==================== */
/* pam_handle_t 映射到 dfan_handle (DFAN 是 PlexsDOS 的 PAM 替代) */
struct dfan_handle;
typedef struct dfan_handle pam_handle_t;
#define PAM_PROMPT_ECHO_OFF 1
#define PAM_PROMPT_ECHO_ON  2
#define PAM_CONV_ERR        3
#define PAM_USER            4
#define PAM_SERVICE         5
#define PAM_TTY             6
#define PAM_NEW_AUTHTOK_REQD 7
#define PAM_CHANGE_EXPIRED_AUTHTOK 1

struct pam_message;
struct pam_response;
struct pam_conv {
    int (*conv)(int, const struct pam_message **, struct pam_response **, void *);
    void *appdata_ptr;
};

/* ==================== struct passwd (使用 usrgn 替代) ==================== */
struct passwd {
    char   *pw_name;
    char   *pw_passwd;
    uid_t   pw_uid;
    gid_t   pw_gid;
    char   *pw_gecos;
    char   *pw_dir;
    char   *pw_shell;
};

/* ==================== 函数声明 ==================== */

/* 内存 (使用 arena — 在 plxdm_compat.c 中实现) */
void *malloc(size_t size);
void *calloc(size_t nmemb, size_t size);
void *realloc(void *ptr, size_t size);
void free(void *ptr);

/* 进程管理 (在 kernel/shim/int21.c 中实现) */
int plxdm_fork(void);
int plxdm_execv(const char *path, char *const argv[]);
int plxdm_execve(const char *path, char *const argv[], char *const envp[]);
int plxdm_waitpid(int pid, int *status, int options);
int plxdm_kill(int pid, int sig);

/* 系统调用存根 */
#define setsid()           (0)
#define chdir(p)           (0)
#define mlockall(f)        (0)
#define gettimeofday(t, z) ((void)(t), (void)(z), 0)
#define MCL_CURRENT        1
#define MCL_FUTURE         2

/* struct timeval */
typedef long time_t;
struct timeval { time_t tv_sec; long tv_usec; };

/* PAM 函数存根 (使用 DFAN 替代, 空操作) */
#define pam_get_item(p, t, i)       (0)
#define pam_set_item(p, t, i)       (0)
#define pam_getenv(p, n)            ((const char*)NULL)
#define pam_getenvlist(p)           ((char**)NULL)
#define pam_putenv(p, nv)           (0)
#define pam_chauthtok(p, f)         (0)
#define PAM_NEW_AUTHTOK_REQD        7
#define PAM_CHANGE_EXPIRED_AUTHTOK  1

/* updwtmp — utmp 写入存根 */
#define updwtmp(f, u)       ((void)(f), (void)(u))

/* initgroups */
#define initgroups(u, g)    (0)

/* chown */
#define chown(p, o, g)      (0)

/* pwd 存根 */
#define setpwent()          ((void)0)
#define endpwent()          ((void)0)
#define getpwent()          ((struct passwd*)NULL)
int plxdm_getpid(void);
int plxdm_pipe(int pipefd[2]);
int plxdm_dup2(int oldfd, int newfd);
int plxdm_chdir(const char *path);
int plxdm_setgid(int gid);
int plxdm_setegid(int egid);
int plxdm_setuid(int uid);
int plxdm_seteuid(int euid);
int plxdm_setsid(void);
int plxdm_getegid(void);
int plxdm_geteuid(void);
int plxdm_getgid(void);
int plxdm_getuid(void);
typedef void (*sighandler_t)(int);
sighandler_t plxdm_signal(int signum, sighandler_t handler);

/* 文件 I/O (在 kernel/shim/int21.c 中实现) */
int plxdm_open(const char *path, int flags);
int plxdm_close(int fd);
int plxdm_read(int fd, void *buf, unsigned int count);
int plxdm_write(int fd, const void *buf, unsigned int count);

/* 标准 C 辅助 (在 plxdm_compat.c 中实现) */
int atoi(const char *nptr);
long strtol(const char *nptr, char **endptr, int base);
double strtod(const char *nptr, char **endptr);
void exit(int status);
int system(const char *command);

/* 信号描述 */
const char *strsignal(int sig);

/* 错误描述 */
char *strerror(int errnum);
int fprintf(void *stream, const char *format, ...);
#define fprintf(stream, ...) (0)

#ifdef __cplusplus
}
#endif

#endif /* _POSIX_STUBS_H */
