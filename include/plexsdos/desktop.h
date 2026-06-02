/*
 * Nexsteaduser — PlexsDOS
 * desktop.h — 桌面环境接口
 * 作者: Tinmc189623 | 团队: Nexlyh
 *
 * Windows 3.x 风格桌面环境:
 * - 桌面图标
 * - 任务栏 (底部)
 * - 开始菜单
 * - 程序管理器
 * - 文件管理器
 * - 记事本
 * - 计算器
 */

#ifndef _PLXSDOS_DESKTOP_H
#define _PLXSDOS_DESKTOP_H

#include <plexsdos/types.h>

#ifdef __cplusplus
extern "C" {
#endif

/* ==================== 桌面常量 ==================== */

#define DESKTOP_MAX_ICONS     16   /* 最大桌面图标数 */
#define TASKBAR_HEIGHT        28   /* 任务栏高度 */
#define ICON_SIZE             32   /* 图标大小 */
#define ICON_TEXT_HEIGHT      12   /* 图标文字高度 */
#define ICON_SPACING          80   /* 图标间距 */

/* 开始菜单常量 */
#define START_MENU_WIDTH      160
#define START_MENU_ITEM_HEIGHT 20
#define START_MENU_MAX_ITEMS  8

/* ==================== 桌面图标结构 ==================== */

struct desktop_icon {
    int id;                    /* 图标 ID */
    char name[32];             /* 显示名称 */
    int x, y;                  /* 屏幕位置 */
    uint8_t color;             /* 图标颜色 */
    void (*on_open)(void);     /* 双击打开回调 */
};

/* ==================== 公共 API ==================== */

/*
 * desktop_init — 初始化桌面环境
 * 返回: true = 成功
 */
bool desktop_init(void);

/*
 * desktop_shutdown — 关闭桌面环境
 */
void desktop_shutdown(void);

/*
 * desktop_run — 运行桌面主循环
 * 处理事件、重绘、直到用户退出
 */
void desktop_run(void);

/*
 * desktop_add_icon — 添加桌面图标
 * @name:     图标名称
 * @x, @y:    位置
 * @color:    图标颜色
 * @on_open:  双击打开回调
 * 返回: 图标 ID
 */
int desktop_add_icon(const char *name, int x, int y, uint8_t color,
                     void (*on_open)(void));

/*
 * desktop_paint — 重绘桌面
 */
void desktop_paint(void);

/*
 * desktop_show_start_menu — 显示开始菜单
 */
void desktop_show_start_menu(void);

/* ==================== 应用程序入口 ==================== */

/*
 * app_program_manager — 启动程序管理器
 */
void app_program_manager(void);

/*
 * app_file_manager — 启动文件管理器
 */
void app_file_manager(void);

/*
 * app_notepad — 启动记事本
 */
void app_notepad(void);

/*
 * app_calculator — 启动计算器
 */
void app_calculator(void);

/*
 * app_about — 显示关于对话框
 */
void app_about(void);

#ifdef __cplusplus
}
#endif

#endif /* _PLXSDOS_DESKTOP_H */
