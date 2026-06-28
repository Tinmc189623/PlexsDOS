/*
 * Nexsteaduser — PlexsDOS
 * PlexsDM 桩实现 (PLEXSDM=0 时使用)
 * 作者: Tinmc189623 | 团队: Nexlyh
 *
 * 当 kernel/dm/*.c 不链入时, 提供 plxdm_lightdm_start 空实现
 * 让 shell.c 的 extern 引用能解析。
 */
#include <plexsdos/types.h>

/*
 * plxdm_lightdm_start — PlexsDM 入口桩
 *
 * 默认未启用 PlexsDM 时返回 -1 表示不可用。
 * cmd_dm() 调用此函数, 看到 -1 后给用户提示。
 */
int plxdm_lightdm_start(void)
{
    return -1;
}
