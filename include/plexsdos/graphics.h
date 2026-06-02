/*
 * Nexsteaduser — PlexsDOS
 * VGA 图形模式驱动接口
 * 作者: Tinmc189623 | 团队: Nexlyh
 *
 * VGA Mode 12h: 640x480x16 色 (4 平面)
 * VGA Mode 13h: 320x200x256 色 (线性帧缓冲)
 *
 * 类 Windows 3.x GUI 使用 Mode 12h (640x480x16)。
 * 提供基础图形原语和字体渲染。
 */

#ifndef _PLXSDOS_GRAPHICS_H
#define _PLXSDOS_GRAPHICS_H

#include <plexsdos/types.h>

#ifdef __cplusplus
extern "C" {
#endif

/* ==================== VGA 模式常量 ==================== */

/* VGA 模式号 */
#define VGA_MODE_TEXT    0x03   /* 80x25 文本模式 */
#define VGA_MODE_12H     0x12   /* 640x480x16 色 */
#define VGA_MODE_13H     0x13   /* 320x200x256 色 */

/* Mode 12h 参数 */
#define VGA12_WIDTH      640
#define VGA12_HEIGHT     480
#define VGA12_COLORS     16
#define VGA12_PLANES     4
#define VGA12_FB_ADDR    0xA0000
#define VGA12_PLANE_SIZE 0x10000  /* 64KB per plane */

/* Mode 13h 参数 */
#define VGA13_WIDTH      320
#define VGA13_HEIGHT     200
#define VGA13_COLORS     256
#define VGA13_FB_ADDR    0xA0000
#define VGA13_FB_SIZE    64000    /* 320*200 */

/* ==================== 颜色定义 (Mode 12h 16 色) ==================== */

enum vga_color {
    COLOR_BLACK        = 0,
    COLOR_BLUE         = 1,
    COLOR_GREEN        = 2,
    COLOR_CYAN         = 3,
    COLOR_RED          = 4,
    COLOR_MAGENTA      = 5,
    COLOR_BROWN        = 6,
    COLOR_LIGHT_GRAY   = 7,
    COLOR_DARK_GRAY    = 8,
    COLOR_LIGHT_BLUE   = 9,
    COLOR_LIGHT_GREEN  = 10,
    COLOR_LIGHT_CYAN   = 11,
    COLOR_LIGHT_RED    = 12,
    COLOR_LIGHT_MAGENTA= 13,
    COLOR_YELLOW       = 14,
    COLOR_WHITE        = 15
};

/* ==================== Windows 3.x 风格配色方案 ==================== */

/* 窗口颜色 */
#define GUI_COLOR_DESKTOP       COLOR_CYAN          /* 桌面背景 */
#define GUI_COLOR_WINDOW_BG     COLOR_WHITE         /* 窗口背景 */
#define GUI_COLOR_WINDOW_FG     COLOR_BLACK         /* 窗口前景 */
#define GUI_COLOR_TITLE_BG      COLOR_BLUE          /* 标题栏背景 */
#define GUI_COLOR_TITLE_FG      COLOR_WHITE         /* 标题栏前景 */
#define GUI_COLOR_TITLE_INACTIVE COLOR_DARK_GRAY    /* 非活动标题栏 */
#define GUI_COLOR_BORDER        COLOR_DARK_GRAY     /* 窗口边框 */
#define GUI_COLOR_BUTTON_BG     COLOR_LIGHT_GRAY    /* 按钮背景 */
#define GUI_COLOR_BUTTON_FG     COLOR_BLACK         /* 按钮前景 */
#define GUI_COLOR_MENU_BG       COLOR_LIGHT_GRAY    /* 菜单背景 */
#define GUI_COLOR_MENU_FG       COLOR_BLACK         /* 菜单前景 */
#define GUI_COLOR_MENU_HIGHLIGHT COLOR_BLUE         /* 菜单高亮 */
#define GUI_COLOR_TEXT_BG       COLOR_WHITE         /* 文本框背景 */
#define GUI_COLOR_TEXT_FG       COLOR_BLACK         /* 文本框前景 */
#define GUI_COLOR_SCROLLBAR     COLOR_DARK_GRAY     /* 滚动条 */
#define GUI_COLOR_SCROLLBAR_BG  COLOR_LIGHT_GRAY    /* 滚动条背景 */

/* ==================== 矩形结构 ==================== */

struct rect {
    int x;       /* 左上角 X */
    int y;       /* 左上角 Y */
    int width;   /* 宽度 */
    int height;  /* 高度 */
};

/* ==================== 位图结构 ==================== */

struct bitmap {
    int width;
    int height;
    uint8_t *data;     /* 像素数据 (每像素 4 bit, 打包) */
};

/* ==================== 字体结构 ==================== */

/* 8x16 位图字体 (VGA 标准) */
#define FONT_WIDTH   8
#define FONT_HEIGHT  16

/* ==================== 公共 API ==================== */

/*
 * graphics_init — 初始化 VGA 图形模式
 * @mode: VGA 模式号 (VGA_MODE_12H 或 VGA_MODE_13H)
 * 返回: true = 成功。
 */
bool graphics_init(uint8_t mode);

/*
 * graphics_restore — 恢复文本模式
 */
void graphics_restore(void);

/*
 * graphics_get_width — 获取屏幕宽度
 */
int graphics_get_width(void);

/*
 * graphics_get_height — 获取屏幕高度
 */
int graphics_get_height(void);

/* ==================== 基础图形原语 ==================== */

/*
 * gfx_put_pixel — 画点
 * @x, @y: 坐标
 * @color: 颜色
 */
void gfx_put_pixel(int x, int y, uint8_t color);

/*
 * gfx_get_pixel — 读取点颜色
 * @x, @y: 坐标
 * 返回: 颜色值。
 */
uint8_t gfx_get_pixel(int x, int y);

/*
 * gfx_draw_hline — 画水平线
 * @x, @y: 起点
 * @len:   长度
 * @color: 颜色
 */
void gfx_draw_hline(int x, int y, int len, uint8_t color);

/*
 * gfx_draw_vline — 画垂直线
 * @x, @y: 起点
 * @len:   长度
 * @color: 颜色
 */
void gfx_draw_vline(int x, int y, int len, uint8_t color);

/*
 * gfx_draw_line — 画任意直线 (Bresenham 算法)
 * @x1, @y1: 起点
 * @x2, @y2: 终点
 * @color:   颜色
 */
void gfx_draw_line(int x1, int y1, int x2, int y2, uint8_t color);

/*
 * gfx_draw_rect — 画矩形边框
 * @x, @y:       左上角
 * @width, @height: 尺寸
 * @color:       颜色
 */
void gfx_draw_rect(int x, int y, int width, int height, uint8_t color);

/*
 * gfx_fill_rect — 填充矩形
 * @x, @y:       左上角
 * @width, @height: 尺寸
 * @color:       颜色
 */
void gfx_fill_rect(int x, int y, int width, int height, uint8_t color);

/*
 * gfx_draw_circle — 画圆 (中点圆算法)
 * @cx, @cy: 圆心
 * @radius:  半径
 * @color:   颜色
 */
void gfx_draw_circle(int cx, int cy, int radius, uint8_t color);

/*
 * gfx_fill_circle — 填充圆
 */
void gfx_fill_circle(int cx, int cy, int radius, uint8_t color);

/* ==================== 文本渲染 ==================== */

/*
 * gfx_draw_char — 绘制单个字符
 * @x, @y:  左上角坐标
 * @c:      字符
 * @fg:     前景色
 * @bg:     背景色
 */
void gfx_draw_char(int x, int y, char c, uint8_t fg, uint8_t bg);

/*
 * gfx_draw_string — 绘制字符串
 * @x, @y:  左上角坐标
 * @str:    字符串
 * @fg:     前景色
 * @bg:     背景色
 */
void gfx_draw_string(int x, int y, const char *str, uint8_t fg, uint8_t bg);

/*
 * gfx_draw_string_transparent — 绘制透明背景字符串
 * @x, @y:  左上角坐标
 * @str:    字符串
 * @fg:     前景色 (背景不绘制)
 */
void gfx_draw_string_transparent(int x, int y, const char *str, uint8_t fg);

/* ==================== 位图操作 ==================== */

/*
 * gfx_draw_bitmap — 绘制位图
 * @x, @y: 左上角坐标
 * @bmp:   位图指针
 */
void gfx_draw_bitmap(int x, int y, const struct bitmap *bmp);

/* ==================== 屏幕操作 ==================== */

/*
 * gfx_clear — 清屏
 * @color: 填充颜色
 */
void gfx_clear(uint8_t color);

/*
 * gfx_scroll_up — 屏幕向上滚动
 * @lines: 滚动行数
 * @color: 空白区域颜色
 */
void gfx_scroll_up(int lines, uint8_t color);

/*
 * gfx_update_region — 更新屏幕区域 (如使用双缓冲)
 * @x, @y, @width, @height: 区域
 */
void gfx_update_region(int x, int y, int width, int height);

/* ==================== 调色板操作 ==================== */

/*
 * gfx_set_palette_entry — 设置调色板条目
 * @index: 颜色索引 (0-255)
 * @r, @g, @b: RGB 分量 (0-63)
 */
void gfx_set_palette_entry(uint8_t index, uint8_t r, uint8_t g, uint8_t b);

/*
 * gfx_set_default_palette — 设置 Windows 3.x 默认调色板
 */
void gfx_set_default_palette(void);

#ifdef __cplusplus
}
#endif

#endif /* _PLXSDOS_GRAPHICS_H */
