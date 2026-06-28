/* sys/wait.h — Windows MSVC stub for xorriso */
#ifndef _SYS_WAIT_H_
#define _SYS_WAIT_H_
/* Minimal stub — waitpid not available on Windows */
#define WNOHANG 1
#define WUNTRACED 2
#define WIFEXITED(s) (((s) & 0xFF) == 0)
#define WEXITSTATUS(s) (((s) >> 8) & 0xFF)
#define WIFSIGNALED(s) (((s) & 0xFF) != 0)
#define WTERMSIG(s) ((s) & 0x7F)
pid_t waitpid(pid_t pid, int *status, int options);
#endif
