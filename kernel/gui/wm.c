/*
 * Nexsteaduser — PlexsDOS
 * wm.c — 窗口管理器实现
 * 作者: Tinmc189623 | 团队: Nexlyh
 *
 * Windows 3.x 风格窗口管理器。
 * 实现窗口创建/销毁、Z-order、标题栏绘制、拖拽移动、
 * 最小化/最大化/关闭、事件分发。
 */

#include <plexsdos/wm.h>
#include <plexsdos/keyboard.h>
#include <plexsdos/string.h>
#include <libc/stddef.h>

/* ==================== 内部状态 ==================== */

/* 窗口数组 */
static struct window g_windows[WM_MAX_WINDOWS];
static int g_window_count = 0;
static int g_focused_id = -1;

/* 窗口 ID 分配计数器 */
static int g_next_id = 1;

/* Z-order: g_zorder[0] 是最顶层窗口 */
static int g_zorder[WM_MAX_WINDOWS];
static int g_zorder_count = 0;

/* 鼠标状态 */
static int g_mouse_x = VGA12_WIDTH / 2;
static int g_mouse_y = VGA12_HEIGHT / 2;
static bool g_mouse_left = false;
static bool g_mouse_dragging = false;
static int g_drag_win_id = -1;
static int g_drag_offset_x = 0;
static int g_drag_offset_y = 0;

/* ==================== 内部辅助函数 ==================== */

/*
 * wm_find_window — 根据 ID 查找窗口
 * @win_id: 窗口 ID
 * 返回: 窗口数组索引，未找到返回 -1
 */
static int wm_find_window(int win_id)
{
    int i;
    for (i = 0; i < WM_MAX_WINDOWS; i++) {
        if (g_windows[i].id == win_id)
            return i;
    }
    return -1;
}

/*
 * wm_find_by_pos — 查找指定屏幕位置的最顶层可见窗口
 * @x, @y: 屏幕坐标
 * 返回: 窗口 ID，未找到返回 -1
 */
static int wm_find_by_pos(int x, int y)
{
    int i;
    for (i = 0; i < g_zorder_count; i++) {
        int idx = wm_find_window(g_zorder[i]);
        if (idx < 0) continue;
        struct window *win = &g_windows[idx];
        if (!(win->flags & WM_FLAG_VISIBLE)) continue;
        if (win->flags & WM_FLAG_MINIMIZED) continue;
        if (x >= win->bounds.x && x < win->bounds.x + win->bounds.width &&
            y >= win->bounds.y && y < win->bounds.y + win->bounds.height) {
            return win->id;
        }
    }
    return -1;
}

/*
 * wm_bring_to_front — 将窗口移到 Z-order 最前端
 * @win_id: 窗口 ID
 */
static void wm_bring_to_front(int win_id)
{
    int i, j;
    /* 从 zorder 中移除 */
    for (i = 0; i < g_zorder_count; i++) {
        if (g_zorder[i] == win_id) {
            for (j = i; j < g_zorder_count - 1; j++)
                g_zorder[j] = g_zorder[j + 1];
            g_zorder_count--;
            break;
        }
    }
    /* 插入到最前面 */
    for (i = g_zorder_count; i > 0; i--)
        g_zorder[i] = g_zorder[i - 1];
    g_zorder[0] = win_id;
    g_zorder_count++;
}

/*
 * wm_paint_titlebar — 绘制窗口标题栏
 * @win: 窗口指针
 */
static void wm_paint_titlebar(struct window *win)
{
    uint8_t bg, fg;
    int x, y, w;
    int btn_x;

    if (!(win->flags & WM_FLAG_HAS_TITLE))
        return;

    x = win->bounds.x;
    y = win->bounds.y;
    w = win->bounds.width;

    /* 标题栏颜色: 焦点窗口蓝色，非焦点灰色 */
    if (win->flags & WM_FLAG_FOCUSED) {
        bg = GUI_COLOR_TITLE_BG;
        fg = GUI_COLOR_TITLE_FG;
    } else {
        bg = GUI_COLOR_TITLE_INACTIVE;
        fg = COLOR_WHITE;
    }

    /* 标题栏背景 */
    gfx_fill_rect(x + WM_BORDER_WIDTH, y + WM_BORDER_WIDTH,
                  w - 2 * WM_BORDER_WIDTH, WM_TITLE_HEIGHT, bg);

    /* 标题文字 (居中偏左) */
    gfx_draw_string(x + WM_BORDER_WIDTH + 4,
                    y + WM_BORDER_WIDTH + (WM_TITLE_HEIGHT - FONT_HEIGHT) / 2,
                    win->title, fg, bg);

    /* 关闭按钮 (右上角) */
    if (win->flags & WM_FLAG_HAS_CLOSE) {
        btn_x = x + w - WM_BORDER_WIDTH - WM_BUTTON_SIZE - 2;
        gfx_fill_rect(btn_x, y + WM_BORDER_WIDTH + 2,
                      WM_BUTTON_SIZE, WM_BUTTON_SIZE - 4, GUI_COLOR_BUTTON_BG);
        gfx_draw_rect(btn_x, y + WM_BORDER_WIDTH + 2,
                      WM_BUTTON_SIZE, WM_BUTTON_SIZE - 4, COLOR_BLACK);
        /* X 符号 */
        gfx_draw_line(btn_x + 3, y + WM_BORDER_WIDTH + 4,
                      btn_x + WM_BUTTON_SIZE - 4, y + WM_BORDER_WIDTH + WM_BUTTON_SIZE - 6,
                      COLOR_BLACK);
        gfx_draw_line(btn_x + WM_BUTTON_SIZE - 4, y + WM_BORDER_WIDTH + 4,
                      btn_x + 3, y + WM_BORDER_WIDTH + WM_BUTTON_SIZE - 6,
                      COLOR_BLACK);
    }

    /* 最小化按钮 */
    {
        int min_x = x + w - WM_BORDER_WIDTH - 2 * WM_BUTTON_SIZE - 4;
        gfx_fill_rect(min_x, y + WM_BORDER_WIDTH + 2,
                      WM_BUTTON_SIZE, WM_BUTTON_SIZE - 4, GUI_COLOR_BUTTON_BG);
        gfx_draw_rect(min_x, y + WM_BORDER_WIDTH + 2,
                      WM_BUTTON_SIZE, WM_BUTTON_SIZE - 4, COLOR_BLACK);
        /* 底部横线 */
        gfx_draw_hline(min_x + 3, y + WM_BORDER_WIDTH + WM_BUTTON_SIZE - 6,
                       WM_BUTTON_SIZE - 6, COLOR_BLACK);
    }

    /* 最大化按钮 */
    {
        int max_x = x + w - WM_BORDER_WIDTH - 3 * WM_BUTTON_SIZE - 6;
        gfx_fill_rect(max_x, y + WM_BORDER_WIDTH + 2,
                      WM_BUTTON_SIZE, WM_BUTTON_SIZE - 4, GUI_COLOR_BUTTON_BG);
        gfx_draw_rect(max_x, y + WM_BORDER_WIDTH + 2,
                      WM_BUTTON_SIZE, WM_BUTTON_SIZE - 4, COLOR_BLACK);
        /* 矩形图标 */
        gfx_draw_rect(max_x + 3, y + WM_BORDER_WIDTH + 4,
                      WM_BUTTON_SIZE - 6, WM_BUTTON_SIZE - 8, COLOR_BLACK);
    }
}

/*
 * wm_paint_window — 绘制单个窗口
 * @win: 窗口指针
 */
static void wm_paint_window(struct window *win)
{
    int x, y, w, h;

    if (!(win->flags & WM_FLAG_VISIBLE))
        return;
    if (win->flags & WM_FLAG_MINIMIZED)
        return;

    x = win->bounds.x;
    y = win->bounds.y;
    w = win->bounds.width;
    h = win->bounds.height;

    /* 外边框 */
    gfx_draw_rect(x, y, w, h, GUI_COLOR_BORDER);

    /* 内边框 (3D 凸起效果) */
    gfx_draw_hline(x + 1, y + 1, w - 2, COLOR_WHITE);
    gfx_draw_vline(x + 1, y + 1, h - 2, COLOR_WHITE);
    gfx_draw_hline(x + 1, y + h - 2, w - 2, COLOR_DARK_GRAY);
    gfx_draw_vline(x + w - 2, y + 1, h - 2, COLOR_DARK_GRAY);

    /* 窗口背景 */
    gfx_fill_rect(x + WM_BORDER_WIDTH, y + WM_BORDER_WIDTH + WM_TITLE_HEIGHT,
                  w - 2 * WM_BORDER_WIDTH,
                  h - 2 * WM_BORDER_WIDTH - WM_TITLE_HEIGHT,
                  GUI_COLOR_WINDOW_BG);

    /* 标题栏 */
    wm_paint_titlebar(win);

    /* 客户区绘制 (调用回调) */
    if (win->on_paint) {
        int client_x = x + WM_BORDER_WIDTH;
        int client_y = y + WM_BORDER_WIDTH + WM_TITLE_HEIGHT;
        int client_w = w - 2 * WM_BORDER_WIDTH;
        int client_h = h - 2 * WM_BORDER_WIDTH - WM_TITLE_HEIGHT;
        win->on_paint(win->id, client_x, client_y, client_w, client_h);
    }
}

/*
 * wm_paint_mouse — 绘制鼠标光标
 * 简单箭头形状
 */
static void wm_paint_mouse(void)
{
    int x = g_mouse_x;
    int y = g_mouse_y;
    int i;

    /* 箭头光标 */
    for (i = 0; i < 10; i++) {
        gfx_put_pixel(x + i, y + i, COLOR_WHITE);
        gfx_put_pixel(x + i + 1, y + i, COLOR_WHITE);
    }
    for (i = 0; i < 7; i++) {
        gfx_put_pixel(x + i, y + i + 3, COLOR_WHITE);
    }
    /* 黑色轮廓 */
    gfx_draw_hline(x, y, 10, COLOR_BLACK);
    gfx_draw_vline(x, y, 10, COLOR_BLACK);
    gfx_draw_line(x, y, x + 9, y + 9, COLOR_BLACK);
}

/*
 * wm_check_titlebar_click — 检查标题栏按钮点击
 * @win: 窗口指针
 * @click_x, @click_y: 点击坐标
 * 返回: 按钮类型
 */
static int wm_check_titlebar_click(struct window *win, int click_x, int click_y)
{
    int title_y_min, title_y_max;
    int btn_x;

    if (!(win->flags & WM_FLAG_HAS_TITLE))
        return WM_BTN_NONE;

    title_y_min = win->bounds.y + WM_BORDER_WIDTH;
    title_y_max = title_y_min + WM_TITLE_HEIGHT;

    if (click_y < title_y_min || click_y >= title_y_max)
        return WM_BTN_NONE;

    /* 关闭按钮 */
    if (win->flags & WM_FLAG_HAS_CLOSE) {
        btn_x = win->bounds.x + win->bounds.width - WM_BORDER_WIDTH - WM_BUTTON_SIZE - 2;
        if (click_x >= btn_x && click_x < btn_x + WM_BUTTON_SIZE)
            return WM_BTN_CLOSE;
    }

    /* 最小化按钮 */
    btn_x = win->bounds.x + win->bounds.width - WM_BORDER_WIDTH - 2 * WM_BUTTON_SIZE - 4;
    if (click_x >= btn_x && click_x < btn_x + WM_BUTTON_SIZE)
        return WM_BTN_MINIMIZE;

    /* 最大化按钮 */
    btn_x = win->bounds.x + win->bounds.width - WM_BORDER_WIDTH - 3 * WM_BUTTON_SIZE - 6;
    if (click_x >= btn_x && click_x < btn_x + WM_BUTTON_SIZE)
        return WM_BTN_MAXIMIZE;

    /* 标题栏区域 (用于拖拽) */
    return WM_BTN_NONE;
}

/* ==================== 公共 API 实现 ==================== */

/*
 * wm_init — 初始化窗口管理器
 * 返回: true = 成功
 */
bool wm_init(void)
{
    int i;

    /* 初始化 VGA 图形模式 */
    if (!graphics_init(VGA_MODE_12H))
        return false;

    /* 设置默认调色板 */
    gfx_set_default_palette();

    /* 清空窗口数组 */
    for (i = 0; i < WM_MAX_WINDOWS; i++)
        g_windows[i].id = 0;
    g_window_count = 0;
    g_zorder_count = 0;
    g_focused_id = -1;
    g_next_id = 1;

    /* 绘制桌面 */
    wm_paint_desktop();

    return true;
}

/*
 * wm_shutdown — 关闭窗口管理器，恢复文本模式
 */
void wm_shutdown(void)
{
    graphics_restore();
}

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
                     uint16_t flags, wm_paint_fn on_paint, wm_event_fn on_event)
{
    struct window *win;
    int idx = -1;
    int i;

    /* 查找空闲槽位 */
    for (i = 0; i < WM_MAX_WINDOWS; i++) {
        if (g_windows[i].id == 0) {
            idx = i;
            break;
        }
    }
    if (idx < 0)
        return -1;

    /* 限制窗口大小 */
    if (width < WM_MIN_WIDTH) width = WM_MIN_WIDTH;
    if (height < WM_MIN_HEIGHT) height = WM_MIN_HEIGHT;
    if (width > VGA12_WIDTH) width = VGA12_WIDTH;
    if (height > VGA12_HEIGHT) height = VGA12_HEIGHT;

    /* 限制窗口位置 */
    if (x < 0) x = 0;
    if (y < 0) y = 0;
    if (x + width > VGA12_WIDTH) x = VGA12_WIDTH - width;
    if (y + height > VGA12_HEIGHT) y = VGA12_HEIGHT - height;

    win = &g_windows[idx];
    win->id = g_next_id++;
    win->bounds.x = x;
    win->bounds.y = y;
    win->bounds.width = width;
    win->bounds.height = height;
    win->old_bounds = win->bounds;
    win->flags = flags | WM_FLAG_HAS_TITLE | WM_FLAG_HAS_CLOSE;
    win->title_fg = GUI_COLOR_TITLE_FG;
    win->title_bg = GUI_COLOR_TITLE_BG;
    win->on_paint = on_paint;
    win->on_event = on_event;
    win->user_data = NULL;

    /* 设置标题 */
    if (title) {
        strncpy(win->title, title, sizeof(win->title) - 1);
        win->title[sizeof(win->title) - 1] = '\0';
    } else {
        win->title[0] = '\0';
    }

    g_window_count++;

    /* 加入 Z-order */
    wm_bring_to_front(win->id);

    return win->id;
}

/*
 * wm_destroy_window — 销毁窗口
 * @win_id: 窗口 ID
 */
void wm_destroy_window(int win_id)
{
    int idx = wm_find_window(win_id);
    int i, j;

    if (idx < 0) return;

    /* 发送关闭事件 */
    if (g_windows[idx].on_event)
        g_windows[idx].on_event(win_id, WM_EVENT_CLOSE, 0, 0);

    /* 从 Z-order 移除 */
    for (i = 0; i < g_zorder_count; i++) {
        if (g_zorder[i] == win_id) {
            for (j = i; j < g_zorder_count - 1; j++)
                g_zorder[j] = g_zorder[j + 1];
            g_zorder_count--;
            break;
        }
    }

    /* 清空窗口 */
    g_windows[idx].id = 0;
    g_window_count--;

    /* 更新焦点 */
    if (g_focused_id == win_id) {
        g_focused_id = (g_zorder_count > 0) ? g_zorder[0] : -1;
        if (g_focused_id >= 0) {
            int fidx = wm_find_window(g_focused_id);
            if (fidx >= 0) {
                g_windows[fidx].flags |= WM_FLAG_FOCUSED;
                if (g_windows[fidx].on_event)
                    g_windows[fidx].on_event(g_focused_id, WM_EVENT_FOCUS, 0, 0);
            }
        }
    }

    /* 重绘桌面 (简化: 重绘全部) */
    wm_paint_desktop();
    wm_repaint();
}

/*
 * wm_show_window — 显示窗口
 * @win_id: 窗口 ID
 */
void wm_show_window(int win_id)
{
    int idx = wm_find_window(win_id);
    if (idx < 0) return;
    g_windows[idx].flags |= WM_FLAG_VISIBLE;
    wm_focus_window(win_id);
    wm_invalidate(win_id);
}

/*
 * wm_hide_window — 隐藏窗口
 * @win_id: 窗口 ID
 */
void wm_hide_window(int win_id)
{
    int idx = wm_find_window(win_id);
    if (idx < 0) return;
    g_windows[idx].flags &= ~WM_FLAG_VISIBLE;
    if (g_focused_id == win_id)
        g_focused_id = -1;
    /* 重绘桌面 */
    wm_paint_desktop();
    wm_repaint();
}

/*
 * wm_focus_window — 将窗口置为焦点
 * @win_id: 窗口 ID
 */
void wm_focus_window(int win_id)
{
    int idx, old_idx;

    if (g_focused_id == win_id)
        return;

    idx = wm_find_window(win_id);
    if (idx < 0) return;
    if (!(g_windows[idx].flags & WM_FLAG_VISIBLE))
        return;

    /* 取消旧焦点 */
    if (g_focused_id >= 0) {
        old_idx = wm_find_window(g_focused_id);
        if (old_idx >= 0) {
            g_windows[old_idx].flags &= ~WM_FLAG_FOCUSED;
            if (g_windows[old_idx].on_event)
                g_windows[old_idx].on_event(g_focused_id, WM_EVENT_UNFOCUS, 0, 0);
            /* 重绘旧标题栏 */
            wm_paint_titlebar(&g_windows[old_idx]);
        }
    }

    /* 设置新焦点 */
    g_focused_id = win_id;
    g_windows[idx].flags |= WM_FLAG_FOCUSED;
    wm_bring_to_front(win_id);

    if (g_windows[idx].on_event)
        g_windows[idx].on_event(win_id, WM_EVENT_FOCUS, 0, 0);

    /* 重绘新标题栏 */
    wm_paint_titlebar(&g_windows[idx]);
}

/*
 * wm_move_window — 移动窗口
 * @win_id: 窗口 ID
 * @x, @y: 新位置
 */
void wm_move_window(int win_id, int x, int y)
{
    int idx = wm_find_window(win_id);
    if (idx < 0) return;
    if (!(g_windows[idx].flags & WM_FLAG_MOVABLE))
        return;

    /* 限制位置 */
    if (x < 0) x = 0;
    if (y < 0) y = 0;
    if (x + g_windows[idx].bounds.width > VGA12_WIDTH)
        x = VGA12_WIDTH - g_windows[idx].bounds.width;
    if (y + g_windows[idx].bounds.height > VGA12_HEIGHT)
        y = VGA12_HEIGHT - g_windows[idx].bounds.height;

    g_windows[idx].bounds.x = x;
    g_windows[idx].bounds.y = y;

    if (g_windows[idx].on_event)
        g_windows[idx].on_event(win_id, WM_EVENT_MOVE, x, y);

    /* 重绘 */
    wm_paint_desktop();
    wm_repaint();
}

/*
 * wm_resize_window — 调整窗口大小
 * @win_id: 窗口 ID
 * @width, @height: 新大小
 */
void wm_resize_window(int win_id, int width, int height)
{
    int idx = wm_find_window(win_id);
    if (idx < 0) return;
    if (!(g_windows[idx].flags & WM_FLAG_RESIZABLE))
        return;

    if (width < WM_MIN_WIDTH) width = WM_MIN_WIDTH;
    if (height < WM_MIN_HEIGHT) height = WM_MIN_HEIGHT;
    if (width > VGA12_WIDTH) width = VGA12_WIDTH;
    if (height > VGA12_HEIGHT) height = VGA12_HEIGHT;

    g_windows[idx].bounds.width = width;
    g_windows[idx].bounds.height = height;

    if (g_windows[idx].on_event)
        g_windows[idx].on_event(win_id, WM_EVENT_RESIZE, width, height);

    /* 重绘 */
    wm_paint_desktop();
    wm_repaint();
}

/*
 * wm_minimize_window — 最小化窗口
 * @win_id: 窗口 ID
 */
void wm_minimize_window(int win_id)
{
    int idx = wm_find_window(win_id);
    if (idx < 0) return;

    g_windows[idx].flags |= WM_FLAG_MINIMIZED;
    g_windows[idx].flags &= ~WM_FLAG_MAXIMIZED;

    if (g_windows[idx].on_event)
        g_windows[idx].on_event(win_id, WM_EVENT_MINIMIZE, 0, 0);

    /* 重绘 */
    wm_paint_desktop();
    wm_repaint();
}

/*
 * wm_maximize_window — 最大化/恢复窗口
 * @win_id: 窗口 ID
 */
void wm_maximize_window(int win_id)
{
    int idx = wm_find_window(win_id);
    if (idx < 0) return;

    if (g_windows[idx].flags & WM_FLAG_MAXIMIZED) {
        /* 恢复 */
        g_windows[idx].bounds = g_windows[idx].old_bounds;
        g_windows[idx].flags &= ~WM_FLAG_MAXIMIZED;
        if (g_windows[idx].on_event)
            g_windows[idx].on_event(win_id, WM_EVENT_RESTORE, 0, 0);
    } else {
        /* 最大化 */
        g_windows[idx].old_bounds = g_windows[idx].bounds;
        g_windows[idx].bounds.x = 0;
        g_windows[idx].bounds.y = 0;
        g_windows[idx].bounds.width = VGA12_WIDTH;
        g_windows[idx].bounds.height = VGA12_HEIGHT;
        g_windows[idx].flags |= WM_FLAG_MAXIMIZED;
        g_windows[idx].flags &= ~WM_FLAG_MINIMIZED;
        if (g_windows[idx].on_event)
            g_windows[idx].on_event(win_id, WM_EVENT_MAXIMIZE, 0, 0);
    }

    /* 重绘 */
    wm_paint_desktop();
    wm_repaint();
}

/*
 * wm_close_window — 关闭窗口 (发送 WM_EVENT_CLOSE)
 * @win_id: 窗口 ID
 */
void wm_close_window(int win_id)
{
    wm_destroy_window(win_id);
}

/*
 * wm_set_title — 设置窗口标题
 * @win_id: 窗口 ID
 * @title:  新标题
 */
void wm_set_title(int win_id, const char *title)
{
    int idx = wm_find_window(win_id);
    if (idx < 0) return;
    if (title) {
        strncpy(g_windows[idx].title, title, sizeof(g_windows[idx].title) - 1);
        g_windows[idx].title[sizeof(g_windows[idx].title) - 1] = '\0';
    }
    wm_invalidate(win_id);
}

/*
 * wm_invalidate — 标记窗口需要重绘
 * @win_id: 窗口 ID
 */
void wm_invalidate(int win_id)
{
    int idx = wm_find_window(win_id);
    if (idx < 0) return;
    if (g_windows[idx].flags & WM_FLAG_VISIBLE)
        wm_paint_window(&g_windows[idx]);
}

/*
 * wm_invalidate_rect — 标记窗口区域需要重绘
 * @win_id: 窗口 ID
 * @rect:   重绘区域 (窗口坐标)
 */
void wm_invalidate_rect(int win_id, const struct rect *rect)
{
    (void)rect;
    /* 简化实现: 重绘整个窗口 */
    wm_invalidate(win_id);
}

/*
 * wm_get_window — 获取窗口结构体指针
 * @win_id: 窗口 ID
 * 返回: 窗口指针，无效返回 NULL
 */
struct window *wm_get_window(int win_id)
{
    int idx = wm_find_window(win_id);
    if (idx < 0) return NULL;
    return &g_windows[idx];
}

/*
 * wm_get_focused — 获取当前焦点窗口 ID
 * 返回: 窗口 ID，无焦点窗口返回 -1
 */
int wm_get_focused(void)
{
    return g_focused_id;
}

/*
 * wm_process_events — 处理事件队列 (键盘、鼠标)
 * 由主循环调用
 *
 * 鼠标协议 (PS/2):
 *   字节1: [Y溢出|X溢出|Y符号|X符号|1|中键|右键|左键]
 *   字节2: X 移动量 (8-bit 有符号)
 *   字节3: Y 移动量 (8-bit 有符号)
 */
void wm_process_events(void)
{
    /* 键盘处理 */
    if (keyboard_available()) {
        char c = keyboard_getchar();

        /* ESC: 关闭焦点窗口 */
        if (c == 0x1B) {
            if (g_focused_id >= 0)
                wm_close_window(g_focused_id);
            return;
        }

        /* Tab (0x09): 切换窗口 */
        if (c == 0x09) {
            if (g_zorder_count > 1) {
                int next_id = g_zorder[1];
                wm_focus_window(next_id);
            }
            return;
        }

        /* 传递给焦点窗口 */
        if (g_focused_id >= 0) {
            int idx = wm_find_window(g_focused_id);
            if (idx >= 0 && g_windows[idx].on_event) {
                g_windows[idx].on_event(g_focused_id, WM_EVENT_KEY,
                                        (int)c, 0);
            }
        }
    }

    /* 鼠标处理 (从 PS/2 端口读取) */
    /* 注意: 鼠标驱动需要单独实现 */
    /* 此处预留接口 */
}

/*
 * wm_repaint — 重绘所有可见窗口
 * 由主循环调用
 *
 * 按 Z-order 从底到顶绘制，确保顶层窗口覆盖底层。
 */
void wm_repaint(void)
{
    int i;

    /* 从 Z-order 底部开始绘制 */
    for (i = g_zorder_count - 1; i >= 0; i--) {
        int idx = wm_find_window(g_zorder[i]);
        if (idx >= 0 && (g_windows[idx].flags & WM_FLAG_VISIBLE))
            wm_paint_window(&g_windows[idx]);
    }

    /* 绘制鼠标光标 */
    wm_paint_mouse();
}

/*
 * wm_paint_desktop — 绘制桌面背景
 *
 * Windows 3.x 风格: 青色背景 + 网格图案
 */
void wm_paint_desktop(void)
{
    int x, y;

    /* 青色背景 */
    gfx_clear(GUI_COLOR_DESKTOP);

    /* 网格图案 (每隔 8 像素画一个深色点) */
    for (y = 0; y < VGA12_HEIGHT; y += 8) {
        for (x = 0; x < VGA12_WIDTH; x += 8) {
            gfx_put_pixel(x, y, COLOR_CYAN);
        }
    }
}
