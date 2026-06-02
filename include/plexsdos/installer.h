/*
 * Nexsteaduser — PlexsDOS
 * 安装程序接口
 * 作者: Tinmc189623 | 团队: Nexlyh
 *
 * 将 PlexsDOS 安装到硬盘:
 * 1. 写入 MBR (含分区表)
 * 2. 写入 FAT32 VBR
 * 3. 写入预构建的 FAT32 文件系统
 * 4. 从安装软盘复制文件 (MS-DOS 风格换盘提示)
 *
 * 安装盘编号:
 *   disk1 — 启动盘 (引导 + 内核)
 *   disk2 — 内核核心文件
 *   disk3 — 程序文件
 *   disk4 — 驱动/库
 *   disk5 — 文档/示例
 */

#ifndef _PLXSDOS_INSTALLER_H
#define _PLXSDOS_INSTALLER_H

#include <plexsdos/types.h>

#ifdef __cplusplus
extern "C" {
#endif

/* 最大换盘重试次数 */
#define INSTALL_MAX_RETRIES  3

/*
 * installer_run — 运行安装程序
 *
 * MS-DOS 风格交互式安装流程:
 * 1. 显示欢迎信息
 * 2. 检测硬盘
 * 3. 确认擦除
 * 4. 写入 MBR + VBR + FAT32 文件系统
 * 5. 逐盘提示插入安装软盘 (带验证)
 * 6. 复制文件并显示进度
 * 7. 显示完成信息
 *
 * 返回: true = 安装成功, false = 失败或用户取消。
 */
bool installer_run(void);

#ifdef __cplusplus
}
#endif

#endif /* _PLXSDOS_INSTALLER_H */
