/* ==========================================================================
 * win-compat.h — Windows POSIX 兼容层头文件
 * 项目：xorriso 1.5.8 Windows 移植
 * 作者：Tinmc189623
 * 团队：Nexlyh
 * 品牌：Nexsteaduser
 * Copyright (C) 2026 Nexlyh, Tinmc189623 / Nexsteaduser
 *
 * 为 MSVC 编译器提供 POSIX 函数和类型的兼容定义，
 * 使 xorriso/libisofs 可在 Windows 原生编译。
 *
 * 包含：unistd.h/dirent.h/pthread.h/gettimeofday 等 POSIX 接口。
 * ========================================================================== */

#ifndef WIN_COMPAT_H
#define WIN_COMPAT_H

/* ===================================================================
 * include first: config.win.h
 * =================================================================== */
#include "config.win.h"

/* ===================================================================
 * Windows 平台标识（用于 #ifdef _WIN32 检测）
 * =================================================================== */
#ifndef _WIN32
#error "win-compat.h is for Windows (MSVC) only"
#endif

/* ===================================================================
 * MSVC 安全注解
 * =================================================================== */
#pragma once

/* ===================================================================
 * 标准 Windows 头文件
 * =================================================================== */
#include <windows.h>
#include <io.h>
#include <process.h>
#include <direct.h>
#include <errno.h>
#include <time.h>
#include <signal.h>
#include <errno.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <stdint.h>

/* ===================================================================
 * POSIX fcntl 常量（Windows 不提供 <fcntl.h>）
 * =================================================================== */
#ifndef F_GETFL
#define F_GETFL 3
#define F_SETFL 4
#define O_NONBLOCK 0x4000
#endif

/* ===================================================================
 * POSIX 信号类型与函数（Windows SDK 不提供信号集/sigprocmask）
 * =================================================================== */
#ifndef sigset_t
#define sigset_t unsigned int
#endif
#ifndef SIG_BLOCK
#define SIG_BLOCK   0
#define SIG_UNBLOCK 1
#define SIG_SETMASK 2
#endif
#ifndef SIGCONT
#define SIGCONT 19
#endif
#ifndef SIGIO
#define SIGIO   29
#endif
static __inline int sigprocmask(int how, const sigset_t *set, sigset_t *oldset)
    { (void)how; (void)set; (void)oldset; return 0; }
static __inline int sigsuspend(const sigset_t *mask)
    { (void)mask; return -1; }
static __inline int sigemptyset(sigset_t *set)
    { *set = 0; return 0; }
static __inline int sigfillset(sigset_t *set)
    { *set = ~0U; return 0; }
static __inline int sigaddset(sigset_t *set, int signo)
    { *set |= (1U << (signo - 1)); return 0; }
static __inline int sigdelset(sigset_t *set, int signo)
    { *set &= ~(1U << (signo - 1)); return 0; }
static __inline int sigismember(const sigset_t *set, int signo)
    { return (*set >> (signo - 1)) & 1; }

/* ===================================================================
 * POSIX 信号常量（Windows SDK <signal.h> 可能不提供）
 * =================================================================== */
#ifndef SIGHUP
#define SIGHUP  1
#endif
#ifndef SIGQUIT
#define SIGQUIT 3
#endif
#ifndef SIGTRAP
#define SIGTRAP 5
#endif
#ifndef SIGKILL
#define SIGKILL 9
#endif
#ifndef SIGUSR1
#define SIGUSR1 10
#endif
#ifndef SIGUSR2
#define SIGUSR2 12
#endif
#ifndef SIGCHLD
#define SIGCHLD 17
#endif
#ifndef SIGSTOP
#define SIGSTOP 23
#endif
#ifndef SIGURG
#define SIGURG 16
#endif
#ifndef SIGWINCH
#define SIGWINCH 28
#endif
#ifndef SIGVTALRM
#define SIGVTALRM 26
#endif
#ifndef SIGXCPU
#define SIGXCPU 30
#endif
#ifndef SIGPIPE
#define SIGPIPE 13
#endif
#ifndef SIGALRM
#define SIGALRM 14
#endif
#ifndef SIGBUS
#define SIGBUS  7
#endif
#ifndef SIGPOLL
#define SIGPOLL 29
#endif
#ifndef SIGPROF
#define SIGPROF 27
#endif
#ifndef SIGSYS
#define SIGSYS  31
#endif
#ifndef SIGTSTP
#define SIGTSTP 18
#endif
#ifndef SIGTTIN
#define SIGTTIN 21
#endif
#ifndef SIGTTOU
#define SIGTTOU 22
#endif
#ifndef SIGXFSZ
#define SIGXFSZ 25
#endif

/* ===================================================================
 * POSIX 用户/组类型（Windows SDK <sys/types.h> 不提供）
 * =================================================================== */
#ifndef uid_t
#define uid_t unsigned int
#endif
#ifndef gid_t
#define gid_t unsigned int
#endif

/* ===================================================================
 * unistd.h 兼容 — MSVC 使用 io.h/process.h/direct.h
 *
 * POSIX 函数     MSVC 等效
 * read(fd,buf,n)   _read(fd,buf,n)
 * write(fd,buf,n)  _write(fd,buf,n)
 * close(fd)        _close(fd)
 * lseek(fd,o,w)    _lseek(fd,o,w)
 * ssize_t           int（x86）或 __int64（x64）
 * STDIN_FILENO      0
 * STDOUT_FILENO     1
 * STDERR_FILENO     2
 * =================================================================== */
#define STDIN_FILENO  0
#define STDOUT_FILENO 1
#define STDERR_FILENO 2

/* ===================================================================
 * stat 结构扩展 — Windows SDK 不包含 POSIX 文件类型/权限宏
 * =================================================================== */
#ifndef S_IFIFO
#define S_IFIFO  0010000
#endif
#ifndef S_IFBLK
#define S_IFBLK  0060000
#endif
#ifndef S_IFLNK
#define S_IFLNK  0120000
#endif
#ifndef S_IFSOCK
#define S_IFSOCK 0140000
#endif
#ifndef S_ISUID
#define S_ISUID  0004000
#endif
#ifndef S_ISGID
#define S_ISGID  0002000
#endif
#ifndef S_ISVTX
#define S_ISVTX  0001000
#endif
#ifndef S_IRWXU
#define S_IRWXU  00700
#endif
#ifndef S_IRWXG
#define S_IRWXG  00070
#endif
#ifndef S_IRWXO
#define S_IRWXO  00007
#endif
#ifndef S_IXUSR
#define S_IXUSR  00100
#endif
#ifndef S_IXGRP
#define S_IXGRP  00010
#endif
#ifndef S_IXOTH
#define S_IXOTH  00001
#endif
#ifndef S_IRUSR
#define S_IRUSR _S_IREAD
#endif
#ifndef S_IWUSR
#define S_IWUSR _S_IWRITE
#endif
#ifndef S_IRGRP
#define S_IRGRP 0
#endif
#ifndef S_IWGRP
#define S_IWGRP 0
#endif
#ifndef S_IROTH
#define S_IROTH 0
#endif
#ifndef S_IWOTH
#define S_IWOTH 0
#endif
#ifndef S_IXUSR
#define S_IXUSR 0
#endif
#ifndef S_IXGRP
#define S_IXGRP 0
#endif
#ifndef S_IXOTH
#define S_IXOTH 0
#endif

#ifndef O_RDONLY
#define O_RDONLY _O_RDONLY
#endif
#ifndef O_WRONLY
#define O_WRONLY _O_WRONLY
#endif
#ifndef O_RDWR
#define O_RDWR _O_RDWR
#endif
#ifndef O_CREAT
#define O_CREAT _O_CREAT
#endif
#ifndef O_TRUNC
#define O_TRUNC _O_TRUNC
#endif
#ifndef O_APPEND
#define O_APPEND _O_APPEND
#endif
#ifndef O_BINARY
#define O_BINARY _O_BINARY
#endif
#ifndef O_EXCL
#define O_EXCL _O_EXCL
#endif

/* MSVC 2015+ 提供 _fseeki64/_ftelli64 */
#ifndef fseeko
#define fseeko(stream, offset, whence) _fseeki64(stream, offset, whence)
#endif
#ifndef ftello
#define ftello(stream) _ftelli64(stream)
#endif

/* ===================================================================
 * dirent.h 兼容 — POSIX 目录遍历
 *
 * MSVC 不提供 <dirent.h>。通过 FindFirstFile/FindNextFile API 模拟。
 * =================================================================== */

/* 预定义兼容常数 */
#ifndef NAME_MAX
#define NAME_MAX 255
#endif

struct dirent {
    long d_ino;              /* inode 编号（Windows 无意义） */
    unsigned short d_reclen; /* 记录长度 */
    unsigned char d_type;    /* 文件类型（DT_UNKNOWN 等） */
    char d_name[NAME_MAX + 1]; /* 文件名 */
};

/* 文件类型常量 */
#ifndef DT_UNKNOWN
#define DT_UNKNOWN  0
#define DT_DIR      4
#define DT_REG      8
#define DT_LNK      10
#endif

typedef struct _win_compat_DIR {
    HANDLE          hFind;
    WIN32_FIND_DATAW findData;
    struct dirent   entry;
    char            searchPath[MAX_PATH + 1];
    int             firstCall;
    long            inoCounter;
} DIR;

DIR *opendir(const char *name);
struct dirent *readdir(DIR *dirp);
int closedir(DIR *dirp);
void rewinddir(DIR *dirp);

/* ===================================================================
 * pthread.h 兼容 — 使用 Windows 原生同步原语
 *
 * 所需 API：
 *   mutex:    CRITICAL_SECTION
 *   condvar:  CONDITION_VARIABLE (Vista+)
 *   thread:   CreateThread / WaitForSingleObject
 * =================================================================== */

/* 线程类型 */
typedef HANDLE pthread_t;
typedef DWORD  pthread_key_t;

/* 线程属性 */
typedef struct {
    int detachstate;
} pthread_attr_t;

#define PTHREAD_CREATE_JOINABLE 0
#define PTHREAD_CREATE_DETACHED 1

/* 互斥锁 */
typedef CRITICAL_SECTION pthread_mutex_t;

typedef struct {
    int  is_initialized;
    int  type;
} pthread_mutexattr_t;

#define PTHREAD_MUTEX_NORMAL      0
#define PTHREAD_MUTEX_RECURSIVE   1
#define PTHREAD_MUTEX_ERRORCHECK 2
#define PTHREAD_MUTEX_DEFAULT    PTHREAD_MUTEX_NORMAL

/* 条件变量 */
typedef CONDITION_VARIABLE pthread_cond_t;

typedef struct {
    int dummy;
} pthread_condattr_t;

/* 一次性初始化 */
typedef struct {
    int done;
} pthread_once_t;

#define PTHREAD_ONCE_INIT { 0 }

/* ===================================================================
 * Mutex 操作
 * =================================================================== */
static __inline int pthread_mutex_init(pthread_mutex_t *mutex,
                                       const pthread_mutexattr_t *attr)
{
    (void)attr;
    InitializeCriticalSection(mutex);
    return 0;
}

static __inline int pthread_mutex_lock(pthread_mutex_t *mutex)
{
    EnterCriticalSection(mutex);
    return 0;
}

static __inline int pthread_mutex_trylock(pthread_mutex_t *mutex)
{
    return TryEnterCriticalSection(mutex) ? 0 : EBUSY;
}

static __inline int pthread_mutex_unlock(pthread_mutex_t *mutex)
{
    LeaveCriticalSection(mutex);
    return 0;
}

static __inline int pthread_mutex_destroy(pthread_mutex_t *mutex)
{
    DeleteCriticalSection(mutex);
    return 0;
}

/* ===================================================================
 * Condition Variable 操作（Vista+）
 * =================================================================== */
static __inline int pthread_cond_init(pthread_cond_t *cond,
                                      const pthread_condattr_t *attr)
{
    (void)attr;
    InitializeConditionVariable(cond);
    return 0;
}

static __inline int pthread_cond_wait(pthread_cond_t *cond,
                                      pthread_mutex_t *mutex)
{
    return SleepConditionVariableCS(cond, mutex, INFINITE) ? 0 : EINVAL;
}

static __inline int pthread_cond_timedwait(pthread_cond_t *cond,
                                           pthread_mutex_t *mutex,
                                           const struct timespec *abstime)
{
    if (abstime == NULL)
        return pthread_cond_wait(cond, mutex);
    /* 计算相对超时（毫秒） */
    struct timespec now;
    timespec_get(&now, TIME_UTC);
    DWORD ms = (DWORD)((abstime->tv_sec - now.tv_sec) * 1000 +
                       (abstime->tv_nsec - now.tv_nsec) / 1000000);
    if ((int)ms < 0) ms = 0;
    return SleepConditionVariableCS(cond, mutex, ms) ? 0 : ETIMEDOUT;
}

static __inline int pthread_cond_signal(pthread_cond_t *cond)
{
    WakeConditionVariable(cond);
    return 0;
}

static __inline int pthread_cond_broadcast(pthread_cond_t *cond)
{
    WakeAllConditionVariable(cond);
    return 0;
}

static __inline int pthread_cond_destroy(pthread_cond_t *cond)
{
    (void)cond;
    return 0;
}

/* ===================================================================
 * Thread 操作
 * =================================================================== */

/* 线程入口 wrapper */
typedef struct {
    void *(*start_routine)(void *);
    void *arg;
} pthread_startarg_t;

static __inline unsigned __stdcall _pthread_start_wrapper(void *arg)
{
    pthread_startarg_t *sa = (pthread_startarg_t *)arg;
    void *(*start)(void *) = sa->start_routine;
    void *a = sa->arg;
    free(sa);
    start(a);
    return 0;
}

static __inline int pthread_create(pthread_t *thread,
                                   const pthread_attr_t *attr,
                                   void *(*start_routine)(void *),
                                   void *arg)
{
    pthread_startarg_t *sa;
    unsigned threadID;

    sa = (pthread_startarg_t *)malloc(sizeof(*sa));
    if (sa == NULL) return ENOMEM;
    sa->start_routine = start_routine;
    sa->arg = arg;

    *thread = (HANDLE)_beginthreadex(NULL, 0, _pthread_start_wrapper,
                                     sa, 0, &threadID);
    if (*thread == NULL) {
        free(sa);
        return EAGAIN;
    }
    if (attr && attr->detachstate == PTHREAD_CREATE_DETACHED) {
        CloseHandle(*thread);
    }
    return 0;
}

static __inline int pthread_join(pthread_t thread, void **retval)
{
    (void)retval;
    WaitForSingleObject(thread, INFINITE);
    CloseHandle(thread);
    return 0;
}

static __inline int pthread_detach(pthread_t thread)
{
    CloseHandle(thread);
    return 0;
}

static __inline void pthread_exit(void *retval)
{
    (void)retval;
    _endthreadex(0);
}

static __inline pthread_t pthread_self(void)
{
    return GetCurrentThread();
}

/* ===================================================================
 * Thread Attribute 操作
 * =================================================================== */
static __inline int pthread_attr_init(pthread_attr_t *attr)
{
    attr->detachstate = PTHREAD_CREATE_JOINABLE;
    return 0;
}

static __inline int pthread_attr_setdetachstate(pthread_attr_t *attr,
                                                int detachstate)
{
    attr->detachstate = detachstate;
    return 0;
}

static __inline int pthread_attr_getdetachstate(pthread_attr_t const *attr,
                                                int *detachstate)
{
    *detachstate = attr->detachstate;
    return 0;
}

static __inline int pthread_attr_destroy(pthread_attr_t *attr)
{
    (void)attr;
    return 0;
}

/* ===================================================================
 * Thread Key 操作
 * =================================================================== */
static __inline int pthread_key_create(pthread_key_t *key,
                                       void (*destructor)(void *))
{
    (void)destructor;
    *key = TlsAlloc();
    return (*key == TLS_OUT_OF_INDEXES) ? EAGAIN : 0;
}

static __inline int pthread_setspecific(pthread_key_t key, const void *value)
{
    return TlsSetValue(key, (LPVOID)value) ? 0 : EINVAL;
}

static __inline void *pthread_getspecific(pthread_key_t key)
{
    return TlsGetValue(key);
}

static __inline int pthread_key_delete(pthread_key_t key)
{
    return TlsFree(key) ? 0 : EINVAL;
}

static __inline int pthread_once(pthread_once_t *once_control,
                                 void (*init_routine)(void))
{
    if (!once_control->done) {
        once_control->done = 1;
        init_routine();
    }
    return 0;
}

/* ===================================================================
 * timezone 结构（Windows SDK 不提供 struct timezone）
 * =================================================================== */
#ifndef _TIMEZONE_DEFINED
#define _TIMEZONE_DEFINED
struct timezone {
    int tz_minuteswest;
    int tz_dsttime;
};
#endif

/* ===================================================================
 * mkdir 兼容宏（变参，避免与 <direct.h> 中 mkdir() 函数声明冲突）
 * 所有 SDK 头文件已包含完毕，此处安全定义。
 * =================================================================== */
#ifndef mkdir
#define mkdir(path, ...) _mkdir(path)
#endif

/* ===================================================================
 * gettimeofday — Windows 实现
 * =================================================================== */
#ifndef HAVE_GETTIMEOFDAY
#define HAVE_GETTIMEOFDAY 1

static __inline int gettimeofday(struct timeval *tv, struct timezone *tz)
{
    FILETIME ft;
    unsigned __int64 t;

    GetSystemTimePreciseAsFileTime(&ft);
    t = ((unsigned __int64)ft.dwHighDateTime << 32) | ft.dwLowDateTime;
    /* 转换 1601-01-01 → 1970-01-01 纪元 */
    t -= 116444736000000000ULL;
    t /= 10; /* 100ns → microseconds */
    if (tv) {
        tv->tv_sec  = (long)(t / 1000000UL);
        tv->tv_usec = (long)(t % 1000000UL);
    }
    if (tz) {
        TIME_ZONE_INFORMATION tzi;
        GetTimeZoneInformation(&tzi);
        tz->tz_minuteswest = tzi.Bias;
    }
    return 0;
}
#endif

/* ===================================================================
 * usleep — Windows 实现（Sleep 接受毫秒）
 * =================================================================== */
#ifndef HAVE_USLEEP
#define HAVE_USLEEP 1

static __inline int usleep(unsigned long usec)
{
    Sleep((DWORD)(usec / 1000));
    return 0;
}
#endif

/* ===================================================================
 * sleep — Windows 实现
 * =================================================================== */
#ifndef HAVE_SLEEP
#define HAVE_SLEEP 1

static __inline unsigned int sleep(unsigned int seconds)
{
    Sleep(seconds * 1000);
    return 0;
}
#endif

/* ===================================================================
 * strptime — 简单实现（仅用于读取 ISO 日期）
 * =================================================================== */
#ifndef HAVE_STRPTIME
#define HAVE_STRPTIME 1

char *win_strptime(const char *buf, const char *fmt, struct tm *tm);

#ifndef strptime
#define strptime win_strptime
#endif

#endif

#endif /* WIN_COMPAT_H */
