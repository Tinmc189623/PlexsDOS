/*
 * Nexsteaduser — PlexsDOS
 * widgets.h — GUI 控件系统接口
 * 作者: Tinmc189623 | 团队: Nexlyh
 *
 * Windows 3.x 风格控件:
 * - Button (按钮)
 * - TextBox (文本框)
 * - Label (标签)
 * - CheckBox (复选框)
 * - RadioButton (单选按钮)
 * - ListBox (列表框)
 * - ScrollBar (滚动条)
 * - MenuBar (菜单栏)
 * - MenuItem (菜单项)
 */

#ifndef _PLXSDOS_WIDGETS_H
#define _PLXSDOS_WIDGETS_H

#include <plexsdos/types.h>
#include <plexsdos/graphics.h>

#ifdef __cplusplus
extern "C" {
#endif

/* ==================== 控件常量 ==================== */

#define WIDGET_MAX_CHILDREN  16   /* 最大子控件数 */
#define WIDGET_MAX_TEXT      128  /* 文本最大长度 */
#define WIDGET_MAX_MENU_ITEMS 8   /* 菜单最大项数 */

/* 控件类型 */
enum widget_type {
    WIDGET_BUTTON     = 0,
    WIDGET_TEXTBOX    = 1,
    WIDGET_LABEL      = 2,
    WIDGET_CHECKBOX   = 3,
    WIDGET_RADIO      = 4,
    WIDGET_LISTBOX    = 5,
    WIDGET_SCROLLBAR  = 6,
    WIDGET_MENUBAR    = 7,
    WIDGET_MENUITEM   = 8,
    WIDGET_PANEL      = 9
};

/* 控件标志 */
#define WIDGET_FLAG_VISIBLE    0x01
#define WIDGET_FLAG_ENABLED    0x02
#define WIDGET_FLAG_FOCUSED    0x04
#define WIDGET_FLAG_PRESSED    0x08
#define WIDGET_FLAG_CHECKED    0x10

/* ==================== 控件结构 ==================== */

/* 控件点击回调 */
typedef void (*widget_click_fn)(int widget_id);

/* 控件绘制回调 */
typedef void (*widget_paint_fn)(int widget_id);

/* 控件结构体 */
struct widget {
    int id;                           /* 控件 ID */
    enum widget_type type;            /* 控件类型 */
    uint16_t flags;                   /* 控件标志 */
    struct rect bounds;               /* 位置和大小 (父控件坐标) */
    char text[WIDGET_MAX_TEXT];       /* 显示文本 */
    uint8_t fg;                       /* 前景色 */
    uint8_t bg;                       /* 背景色 */
    widget_click_fn on_click;         /* 点击回调 */
    widget_paint_fn on_paint;         /* 绘制回调 */
    int parent_id;                    /* 父控件 ID (-1 = 无) */
    int children[WIDGET_MAX_CHILDREN]; /* 子控件 ID 列表 */
    int child_count;                  /* 子控件数量 */
    void *user_data;                  /* 用户数据 */
    union {
        struct { int checked; } checkbox;
        struct { int selected; int scroll_offset; int item_count; char items[16][64]; } listbox;
        struct { int min_val; int max_val; int cur_val; int thumb_pos; } scrollbar;
        struct { int item_count; char items[WIDGET_MAX_MENU_ITEMS][32]; widget_click_fn callbacks[WIDGET_MAX_MENU_ITEMS]; } menu;
    } extra;
};

/* ==================== 公共 API ==================== */

/*
 * widgets_init — 初始化控件系统
 */
void widgets_init(void);

/*
 * widget_create — 创建控件
 * @type:      控件类型
 * @x, @y:     位置 (父控件坐标)
 * @width, @height: 大小
 * @text:      显示文本
 * @fg, @bg:   颜色
 * @on_click:  点击回调
 * 返回: 控件 ID，失败返回 -1
 */
int widget_create(enum widget_type type, int x, int y, int width, int height,
                  const char *text, uint8_t fg, uint8_t bg,
                  widget_click_fn on_click);

/*
 * widget_destroy — 销毁控件
 * @widget_id: 控件 ID
 */
void widget_destroy(int widget_id);

/*
 * widget_add_child — 添加子控件
 * @parent_id: 父控件 ID
 * @child_id:  子控件 ID
 */
void widget_add_child(int parent_id, int child_id);

/*
 * widget_set_text — 设置控件文本
 * @widget_id: 控件 ID
 * @text:      新文本
 */
void widget_set_text(int widget_id, const char *text);

/*
 * widget_set_visible — 设置控件可见性
 * @widget_id: 控件 ID
 * @visible:   是否可见
 */
void widget_set_visible(int widget_id, bool visible);

/*
 * widget_set_enabled — 设置控件启用状态
 * @widget_id: 控件 ID
 * @enabled:   是否启用
 */
void widget_set_enabled(int widget_id, bool enabled);

/*
 * widget_paint — 绘制控件及其子控件
 * @widget_id: 控件 ID
 * @offset_x, @offset_y: 屏幕偏移
 */
void widget_paint(int widget_id, int offset_x, int offset_y);

/*
 * widget_handle_click — 处理点击事件
 * @widget_id: 控件 ID
 * @x, @y: 点击坐标 (屏幕坐标)
 * 返回: 被点击的控件 ID，无则返回 -1
 */
int widget_handle_click(int widget_id, int x, int y);

/*
 * widget_get — 获取控件结构体指针
 * @widget_id: 控件 ID
 * 返回: 控件指针，无效返回 NULL
 */
struct widget *widget_get(int widget_id);

/* ==================== 特化控件 API ==================== */

/*
 * checkbox_set_checked — 设置复选框状态
 * @widget_id: 复选框控件 ID
 * @checked:   是否选中
 */
void checkbox_set_checked(int widget_id, bool checked);

/*
 * checkbox_is_checked — 获取复选框状态
 * @widget_id: 复选框控件 ID
 * 返回: 是否选中
 */
bool checkbox_is_checked(int widget_id);

/*
 * listbox_add_item — 向列表框添加项
 * @widget_id: 列表框控件 ID
 * @item:      项文本
 */
void listbox_add_item(int widget_id, const char *item);

/*
 * listbox_get_selected — 获取列表框选中项索引
 * @widget_id: 列表框控件 ID
 * 返回: 选中项索引，无选中返回 -1
 */
int listbox_get_selected(int widget_id);

/*
 * scrollbar_set_range — 设置滚动条范围
 * @widget_id: 滚动条控件 ID
 * @min:       最小值
 * @max:       最大值
 * @current:   当前值
 */
void scrollbar_set_range(int widget_id, int min, int max, int current);

/*
 * scrollbar_get_value — 获取滚动条当前值
 * @widget_id: 滚动条控件 ID
 * 返回: 当前值
 */
int scrollbar_get_value(int widget_id);

/*
 * menubar_add_item — 向菜单栏添加项
 * @widget_id:  菜单栏控件 ID
 * @text:       项文本
 * @callback:   点击回调
 */
void menubar_add_item(int widget_id, const char *text, widget_click_fn callback);

#ifdef __cplusplus
}
#endif

#endif /* _PLXSDOS_WIDGETS_H */
