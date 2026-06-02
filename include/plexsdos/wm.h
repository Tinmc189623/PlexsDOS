/*
 * Nexsteaduser — PlexsDOS
 * wm.h — 窗口管理器接口
 * 作者: Tinmc189623 | 团队: Nexlyh
 *
 * Windows 3.x 风格窗口管理器。
 * 支持 Z-order、标题栏、拖拽移动、最小化/最大化/关闭。
 */

#ifndef _PLXSDOS_WM_H
#define _PLXSDOS_WM_H

#include <plexsdos/types.h>
#include <plexsdos/graphics.h>

#ifdef __cplusplus
extern "C" {
#endif

/* ==================== 窗口常量 ==================== */

#define WM_MAX_WINDOWS     16   /* 最大窗口数 */
#define WM_TITLE_HEIGHT    20   /* 标题栏高度 */
#define WM_BORDER_WIDTH    2    /* 边框宽度 */
#define WM_BUTTON_SIZE     16   /* 标题栏按钮大小 */
#define WM_MIN_WIDTH       100  /* 最小窗口宽度 */
#define WM_MIN_HEIGHT      60   /* 最小窗口高度 */

/* 窗口标志 */
#define WM_FLAG_VISIBLE    0x01  /* 可见 */
#define WM_FLAG_FOCUSED    0x02  /* 获得焦点 */
#define WM_FLAG_MINIMIZED  0x04  /* 最小化 */
#define WM_FLAG_MAXIMIZED  0x08  /* 最大化 */
#define WM_FLAG_MOVABLE    0x10  /* 可移动 */
#define WM_FLAG_RESIZABLE  0x20  /* 可调整大小 */
#define WM_FLAG_HAS_TITLE  0x40  /* 有标题栏 */
#define WM_FLAG_HAS_CLOSE  0x80  /* 有关闭按钮 */

/* 标题栏按钮类型 */
#define WM_BTN_NONE        0
#define WM_BTN_CLOSE       1
#define WM_BTN_MINIMIZE    2
#define WM_BTN_MAXIMIZE    3

/* ==================== 窗口结构 ==================== */

/* 窗口绘制回调 */
typedef void (*wm_paint_fn)(int win_id, int x, int y, int width, int height);

/* 窗口事件回调 */
typedef void (*wm_event_fn)(int win_id, int event, int param1, int param2);

/* 窗口结构体 */
struct window {
    int id;                    /* 窗口 ID */
    struct rect bounds;        /* 窗口位置和大小 (屏幕坐标) */
    struct rect old_bounds;    /* 最大化前的位置 */
    uint16_t flags;            /* 窗口标志 */
    uint8_t title_fg;          /* 标题栏前景色 */
    uint8_t title_bg;          /* 标题栏背景色 */
    char title[64];            /* 标题文字 */
    wm_paint_fn on_paint;      /* 绘制回调 */
    wm_event_fn on_event;      /* 事件回调 */
    void *user_data;           /* 用户数据指针 */
};

/* ==================== 事件类型 ==================== */

enum wm_event {
    WM_EVENT_PAINT     = 0,    /* 需要重绘 */
    WM_EVENT_CLOSE     = 1,    /* 关闭请求 */
    WM_EVENT_FOCUS     = 2,    /* 获得焦点 */
    WM_EVENT_UNFOCUS   = 3,    /* 失去焦点 */
    WM_EVENT_MINIMIZE  = 4,    /* 最小化 */
    WM_EVENT_MAXIMIZE  = 5,    /* 最大化 */
    WM_EVENT_RESTORE   = 6,    /* 恢复 */
    WM_EVENT_MOVE      = 7,    /* 移动 */
    WM_EVENT_RESIZE    = 8,    /* 大小改变 */
    WM_EVENT_KEY       = 9,    /* 键盘事件 */
    WM_EVENT_CLICK     = 10    /* 鼠标点击 */
};

/* ==================== 公共 API ==================== */

/*
 * wm_init — 初始化窗口管理器
 * 返回: true = 成功
 */
bool wm_init(void);

/*
 * wm_shutdown — 关闭窗口管理器，恢复文本模式
 */
void wm_shutdown(void);

/*
 * wm_create_window — 创建窗口
 * @title:     窗口标题
 * @x, @y:     位置
 * @width, @height: 大小
 * @flags:     窗口标志
 * @on_paint:  绘制回调
 * @on_event:  事件回调
 * 返回: 窗口 ID，失败返回 -1
 */
int wm_create_window(const char *title, int x, int y, int width, int height,
                     uint16_t flags, wm_paint_fn on_paint, wm_event_fn on_event);

/*
 * wm_destroy_window — 销毁窗口
 * @win_id: 窗口 ID
 */
void wm_destroy_window(int win_id);

/*
 * wm_show_window — 显示窗口
 * @win_id: 窗口 ID
 */
void wm_show_window(int win_id);

/*
 * wm_hide_window — 隐藏窗口
 * @win_id: 窗口 ID
 */
void wm_hide_window(int win_id);

/*
 * wm_focus_window — 将窗口置为焦点
 * @win_id: 窗口 ID
 */
void wm_focus_window(int win_id);

/*
 * wm_move_window — 移动窗口
 * @win_id: 窗口 ID
 * @x, @y: 新位置
 */
void wm_move_window(int win_id, int x, int y);

/*
 * wm_resize_window — 调整窗口大小
 * @win_id: 窗口 ID
 * @width, @height: 新大小
 */
void wm_resize_window(int win_id, int width, int height);

/*
 * wm_minimize_window — 最小化窗口
 * @win_id: 窗口 ID
 */
void wm_minimize_window(int win_id);

/*
 * wm_maximize_window — 最大化/恢复窗口
 * @win_id: 窗口 ID
 */
void wm_maximize_window(int win_id);

/*
 * wm_close_window — 关闭窗口 (发送 WM_EVENT_CLOSE)
 * @win_id: 窗口 ID
 */
void wm_close_window(int win_id);

/*
 * wm_set_title — 设置窗口标题
 * @win_id: 窗口 ID
 * @title:  新标题
 */
void wm_set_title(int win_id, const char *title);

/*
 * wm_invalidate — 标记窗口需要重绘
 * @win_id: 窗口 ID
 */
void wm_invalidate(int win_id);

/*
 * wm_invalidate_rect — 标记窗口区域需要重绘
 * @win_id: 窗口 ID
 * @rect:   重绘区域 (窗口坐标)
 */
void wm_invalidate_rect(int win_id, const struct rect *rect);

/*
 * wm_get_window — 获取窗口结构体指针
 * @win_id: 窗口 ID
 * 返回: 窗口指针，无效返回 NULL
 */
struct window *wm_get_window(int win_id);

/*
 * wm_get_focused — 获取当前焦点窗口 ID
 * 返回: 窗口 ID，无焦点窗口返回 -1
 */
int wm_get_focused(void);

/*
 * wm_process_events — 处理事件队列 (键盘、鼠标)
 * 由主循环调用
 */
void wm_process_events(void);

/*
 * wm_repaint — 重绘所有可见窗口
 * 由主循环调用
 */
void wm_repaint(void);

/*
 * wm_paint_desktop — 绘制桌面背景
 */
void wm_paint_desktop(void);

#ifdef __cplusplus
}
#endif

#endif /* _PLXSDOS_WM_H */
