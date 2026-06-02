/*
 * Nexsteaduser — PlexsDOS
 * x12.h — 图形显示服务 (X11/Wayland 兼容层)
 * 作者: Tinmc189623 | 团队: Nexlyh
 *
 * X12 替代 X11/Wayland:
 *   基于 VBE 线性帧缓冲, 800x600x32bpp
 *   提供窗口管理和图形输出服务
 *
 * X11 映射:
 *   x12_init          → XOpenDisplay + XInitThreads
 *   x12_create_window → XCreateWindow
 *   x12_map_window    → XMapWindow
 *   x12_fill_rect     → XFillRectangle
 *   x12_draw_string   → XDrawString
 *   x12_next_event    → XNextEvent
 *   x12_flush         → XFlush
 */

#ifndef _PLXSDOS_X12_H
#define _PLXSDOS_X12_H

#include <plexsdos/types.h>

#ifdef __cplusplus
extern "C" {
#endif

/* 默认分辨率和像素格式 */
#define X12_DEFAULT_WIDTH    800
#define X12_DEFAULT_HEIGHT   600
#define X12_DEFAULT_BPP      32

/* 像素格式: 32-bit ARGB (小端 = BGRA) */
#define X12_PIXEL_BLACK      0xFF000000
#define X12_PIXEL_WHITE      0xFFFFFFFF
#define X12_PIXEL_BLUE       0xFF800000
#define X12_PIXEL_LIGHT_BLUE 0xFFE0D040
#define X12_PIXEL_GRAY       0xFFC0C0C0
#define X12_PIXEL_DARK_BLUE  0xFF800000  /* 深蓝背景 */

/* 事件类型 */
#define X12_EVENT_NONE       0
#define X12_EVENT_KEY_DOWN   1
#define X12_EVENT_KEY_UP     2
#define X12_EVENT_MOUSE_MOVE 3
#define X12_EVENT_MOUSE_DOWN 4
#define X12_EVENT_MOUSE_UP   5
#define X12_EVENT_CLIENT_MSG 6
#define X12_EVENT_EXPOSE     7

/* X12 事件 */
struct x12_event {
    int type;
    union {
        struct { int keycode; char ascii; } key;
        struct { int x; int y; int buttons; } mouse;
        struct { int data[4]; } client_msg;
        struct { int x; int y; int w; int h; } expose;
    };
};

/* X12 窗口 */
struct x12_window;

/* X12 颜色 */
struct x12_color {
    uint8_t r;
    uint8_t g;
    uint8_t b;
    uint8_t a;
};

/*
 * x12_init — 初始化 X12 显示服务
 * @width:  水平分辨率
 * @height: 垂直分辨率
 * 返回: true = 成功。
 */
bool x12_init(int width, int height);

/*
 * x12_shutdown — 关闭 X12 显示服务
 */
void x12_shutdown(void);

/*
 * x12_create_window — 创建窗口
 * @x, @y:      左上角坐标
 * @width, @height: 窗口尺寸
 * @title:      窗口标题
 * 返回: 窗口指针, 失败返回 NULL。
 */
struct x12_window *x12_create_window(int x, int y, int width, int height,
                                     const char *title);

/*
 * x12_destroy_window — 销毁窗口
 * @win: 窗口指针
 */
void x12_destroy_window(struct x12_window *win);

/*
 * x12_map_window — 显示窗口
 * @win: 窗口指针
 */
void x12_map_window(struct x12_window *win);

/*
 * x12_unmap_window — 隐藏窗口
 * @win: 窗口指针
 */
void x12_unmap_window(struct x12_window *win);

/*
 * x12_fill_rect — 填充矩形
 * @win:   窗口指针 (NULL = 全屏)
 * @x, @y, @w, @h: 矩形区域
 * @color: 填充颜色 (ARGB)
 */
void x12_fill_rect(struct x12_window *win, int x, int y,
                   int w, int h, uint32_t color);

/*
 * x12_draw_string — 绘制字符串
 * @win:   窗口指针
 * @x, @y: 左上角坐标
 * @str:   字符串
 * @color: 文字颜色
 */
void x12_draw_string(struct x12_window *win, int x, int y,
                     const char *str, uint32_t color);

/*
 * x12_draw_char — 绘制单个字符
 * @win:   窗口指针
 * @x, @y: 左上角坐标
 * @c:     字符
 * @color: 文字颜色
 */
void x12_draw_char(struct x12_window *win, int x, int y,
                   char c, uint32_t color);

/*
 * x12_get_text_width — 获取文本宽度 (像素)
 * @str: 字符串
 * 返回: 像素宽度 (8 像素每字符)。
 */
int x12_get_text_width(const char *str);

/*
 * x12_get_text_height — 获取文本高度 (像素)
 * 返回: 16 像素。
 */
int x12_get_text_height(void);

/*
 * x12_draw_button — 绘制 Windows 风格按钮
 * @win:    窗口指针
 * @x, @y, @w, @h: 按钮区域
 * @text:   按钮文本
 * @pressed: 是否按下
 */
void x12_draw_button(struct x12_window *win, int x, int y,
                     int w, int h, const char *text, bool pressed);

/*
 * x12_draw_progress_bar — 绘制进度条
 * @win:    窗口指针
 * @x, @y, @w, @h: 进度条区域
 * @percent: 进度百分比 (0-100)
 */
void x12_draw_progress_bar(struct x12_window *win, int x, int y,
                           int w, int h, int percent);

/*
 * x12_next_event — 获取下一个事件 (阻塞)
 * @win:  窗口指针 (NULL = 任意窗口)
 * @event: 输出事件
 * 返回: true = 获取到事件。
 */
bool x12_next_event(struct x12_window *win, struct x12_event *event);

/*
 * x12_poll_event — 非阻塞轮询事件
 * @win:  窗口指针
 * @event: 输出事件
 * 返回: true = 有事件待处理。
 */
bool x12_poll_event(struct x12_window *win, struct x12_event *event);

/*
 * x12_flush — 刷新显示 (提交绘制到帧缓冲)
 */
void x12_flush(void);

/*
 * x12_clear_screen — 清屏
 * @color: 背景色
 */
void x12_clear_screen(uint32_t color);

/*
 * x12_get_width — 获取屏幕宽度
 * 返回: 屏幕宽度 (像素)。
 */
int x12_get_width(void);

/*
 * x12_get_height — 获取屏幕高度
 * 返回: 屏幕高度 (像素)。
 */
int x12_get_height(void);

/*
 * x12_run_modal — 运行模态对话框
 * @win:  窗口指针
 * @buttons: 按钮文本数组 (最后一项必须为 NULL)
 * 返回: 按钮索引 (0 = 第一个按钮), -1 = 关闭。
 *
 * 阻塞直到用户点击某个按钮。
 * 处理键盘输入和鼠标点击。
 */
int x12_run_modal(struct x12_window *win, const char **buttons);

/*
 * x12_set_clip_rect — 设置裁剪区域
 * @win: 窗口指针
 * @x, @y, @w, @h: 裁剪区域 (NULL = 取消裁剪)
 */
void x12_set_clip_rect(struct x12_window *win, int x, int y,
                       int w, int h);

#ifdef __cplusplus
}
#endif

#endif /* _PLXSDOS_X12_H */
