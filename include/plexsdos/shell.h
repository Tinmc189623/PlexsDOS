/*
 * Nexsteaduser — PlexsDOS
 * Shell 接口
 * 作者: Tinmc189623 | 团队: Nexlyh
 */

#ifndef _PLXSDOS_SHELL_H
#define _PLXSDOS_SHELL_H

#ifdef __cplusplus
extern "C" {
#endif

/* Shell 主循环 (内核态, 旧接口) */
void shell_main(void);

/*
 * shell_exec_cmd — 执行单条 Shell 命令
 * @cmd: 命令字符串 (以 '\0' 结尾)
 *
 * 供系统调用使用, 用户态 Shell 通过 SYS_SHELL_CMD 调用。
 * 执行命令后返回。
 */
void shell_exec_cmd(const char *cmd);

#ifdef __cplusplus
}
#endif

#endif /* _PLXSDOS_SHELL_H */
