/*
 * Nexsteaduser — PlexsDOS
 * .comx 程序加载器接口
 * 作者: Tinmc189623 | 团队: Nexlyh
 */

#ifndef _PLXSDOS_LOADER_H
#define _PLXSDOS_LOADER_H

#include <plexsdos/types.h>

#ifdef __cplusplus
extern "C" {
#endif

/*
 * loader_run — 加载并执行 .comx 程序
 * @filename: FAT32 文件名
 *
 * 返回: true 表示程序已执行并返回, false 表示加载失败。
 */
bool loader_run(const char *filename);

/*
 * loader_enter_ring3 — 从 Ring 0 切换到 Ring 3 执行用户程序
 * @user_eip: 用户程序入口地址
 * @user_esp: 用户栈顶地址
 *
 * 保存内核返回上下文, 构造 Ring 3 iret 帧, 执行 iret 进入 Ring 3。
 * 程序通过 INT 0x22 AH=0x4C 退出时恢复内核上下文并返回。
 */
void loader_enter_ring3(uint32_t user_eip, uint32_t user_esp);

#ifdef __cplusplus
}
#endif

#endif /* _PLXSDOS_LOADER_H */
