/*
 * Nexsteaduser — PlexsDOS
 * plxdm_lightdm.h — 显示管理器入口 (LightDM 移植)
 * 作者: Tinmc189623 | 团队: Nexlyh
 *
 * 提供 PlexsDOS 内核可调用的 DM 启动入口。
 * plxdm_lightdm_start() 从 kernel_main 或 Shell 调用,
 * 初始化 LightDM 各子系统并进入主循环。
 */
#ifndef _PLXDM_LIGHTDM_H
#define _PLXDM_LIGHTDM_H

/*
 * plxdm_lightdm_start — 启动显示管理器
 * 返回: 0 = 正常退出, 非 0 = 错误
 *
 * 初始化会话管理、座位管理、显示服务,
 * 然后进入 GLib 主循环 (g_main_loop_run)。
 */
int plxdm_lightdm_start(void);

#endif /* _PLXDM_LIGHTDM_H */
