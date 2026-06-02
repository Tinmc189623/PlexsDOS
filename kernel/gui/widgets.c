/*
 * Nexsteaduser — PlexsDOS
 * widgets.c — GUI 控件系统实现
 * 作者: Tinmc189623 | 团队: Nexlyh
 *
 * 实现 Windows 3.x 风格控件的绘制和交互。
 */

#include <plexsdos/widgets.h>
#include <plexsdos/graphics.h>
#include <plexsdos/string.h>
#include <libc/stddef.h>

/* ==================== 内部状态 ==================== */

#define MAX_WIDGETS 64

static struct widget g_widgets[MAX_WIDGETS];
static int g_widget_count = 0;
static int g_next_widget_id = 1;

/* ==================== 内部辅助函数 ==================== */

/*
 * widget_find — 根据 ID 查找控件
 * @widget_id: 控件 ID
 * 返回: 控件数组索引，未找到返回 -1
 */
static int widget_find(int widget_id)
{
    int i;
    for (i = 0; i < MAX_WIDGETS; i++) {
        if (g_widgets[i].id == widget_id)
            return i;
    }
    return -1;
}

/*
 * widget_paint_button — 绘制按钮
 * @w: 控件指针
 * @ox, @oy: 屏幕偏移
 */
static void widget_paint_button(struct widget *w, int ox, int oy)
{
    int x = w->bounds.x + ox;
    int y = w->bounds.y + oy;
    int bw = w->bounds.width;
    int bh = w->bounds.height;
    uint8_t bg, fg;
    int text_x, text_y;

    bg = (w->flags & WIDGET_FLAG_PRESSED) ? COLOR_DARK_GRAY : w->bg;
    fg = w->fg;

    /* 按钮背景 */
    gfx_fill_rect(x, y, bw, bh, bg);

    /* 3D 边框效果 */
    if (w->flags & WIDGET_FLAG_PRESSED) {
        /* 凹陷 */
        gfx_draw_hline(x, y, bw, COLOR_DARK_GRAY);
        gfx_draw_vline(x, y, bh, COLOR_DARK_GRAY);
        gfx_draw_hline(x, y + bh - 1, bw, COLOR_WHITE);
        gfx_draw_vline(x + bw - 1, y, bh, COLOR_WHITE);
    } else {
        /* 凸起 */
        gfx_draw_hline(x, y, bw, COLOR_WHITE);
        gfx_draw_vline(x, y, bh, COLOR_WHITE);
        gfx_draw_hline(x, y + bh - 1, bw, COLOR_DARK_GRAY);
        gfx_draw_vline(x + bw - 1, y, bh, COLOR_DARK_GRAY);
    }

    /* 文字居中 */
    text_x = x + (bw - (int)strlen(w->text) * FONT_WIDTH) / 2;
    text_y = y + (bh - FONT_HEIGHT) / 2;
    gfx_draw_string(text_x, text_y, w->text, fg, bg);
}

/*
 * widget_paint_textbox — 绘制文本框
 * @w: 控件指针
 * @ox, @oy: 屏幕偏移
 */
static void widget_paint_textbox(struct widget *w, int ox, int oy)
{
    int x = w->bounds.x + ox;
    int y = w->bounds.y + oy;
    int bw = w->bounds.width;
    int bh = w->bounds.height;

    /* 文本框背景 */
    gfx_fill_rect(x, y, bw, bh, GUI_COLOR_TEXT_BG);

    /* 凹陷边框 */
    gfx_draw_hline(x, y, bw, COLOR_DARK_GRAY);
    gfx_draw_vline(x, y, bh, COLOR_DARK_GRAY);
    gfx_draw_hline(x, y + bh - 1, bw, COLOR_WHITE);
    gfx_draw_vline(x + bw - 1, y, bh, COLOR_WHITE);

    /* 文本 */
    gfx_draw_string(x + 2, y + (bh - FONT_HEIGHT) / 2,
                    w->text, GUI_COLOR_TEXT_FG, GUI_COLOR_TEXT_BG);
}

/*
 * widget_paint_label — 绘制标签
 * @w: 控件指针
 * @ox, @oy: 屏幕偏移
 */
static void widget_paint_label(struct widget *w, int ox, int oy)
{
    int x = w->bounds.x + ox;
    int y = w->bounds.y + oy;

    gfx_draw_string(x, y, w->text, w->fg, w->bg);
}

/*
 * widget_paint_checkbox — 绘制复选框
 * @w: 控件指针
 * @ox, @oy: 屏幕偏移
 */
static void widget_paint_checkbox(struct widget *w, int ox, int oy)
{
    int x = w->bounds.x + ox;
    int y = w->bounds.y + oy;
    int box_size = 12;
    int text_x;

    /* 复选框方块 */
    gfx_fill_rect(x, y + (FONT_HEIGHT - box_size) / 2, box_size, box_size, COLOR_WHITE);
    gfx_draw_rect(x, y + (FONT_HEIGHT - box_size) / 2, box_size, box_size, COLOR_BLACK);

    /* 勾选标记 */
    if (w->extra.checkbox.checked) {
        int bx = x + 2;
        int by = y + (FONT_HEIGHT - box_size) / 2 + 3;
        gfx_draw_line(bx, by + 3, bx + 2, by + 5, COLOR_BLACK);
        gfx_draw_line(bx + 2, by + 5, bx + 6, by, COLOR_BLACK);
    }

    /* 文本 */
    text_x = x + box_size + 4;
    gfx_draw_string(text_x, y, w->text, w->fg, w->bg);
}

/*
 * widget_paint_listbox — 绘制列表框
 * @w: 控件指针
 * @ox, @oy: 屏幕偏移
 */
static void widget_paint_listbox(struct widget *w, int ox, int oy)
{
    int x = w->bounds.x + ox;
    int y = w->bounds.y + oy;
    int bw = w->bounds.width;
    int bh = w->bounds.height;
    int i, item_y;
    int visible_items;

    /* 列表框背景 */
    gfx_fill_rect(x, y, bw, bh, COLOR_WHITE);

    /* 凹陷边框 */
    gfx_draw_hline(x, y, bw, COLOR_DARK_GRAY);
    gfx_draw_vline(x, y, bh, COLOR_DARK_GRAY);
    gfx_draw_hline(x, y + bh - 1, bw, COLOR_WHITE);
    gfx_draw_vline(x + bw - 1, y, bh, COLOR_WHITE);

    /* 绘制项 */
    visible_items = bh / FONT_HEIGHT;
    for (i = 0; i < visible_items && i + w->extra.listbox.scroll_offset < w->extra.listbox.item_count; i++) {
        int idx = i + w->extra.listbox.scroll_offset;
        item_y = y + i * FONT_HEIGHT;

        /* 选中项高亮 */
        if (idx == w->extra.listbox.selected) {
            gfx_fill_rect(x + 1, item_y, bw - 2, FONT_HEIGHT, GUI_COLOR_MENU_HIGHLIGHT);
            gfx_draw_string(x + 2, item_y, w->extra.listbox.items[idx],
                            COLOR_WHITE, GUI_COLOR_MENU_HIGHLIGHT);
        } else {
            gfx_draw_string(x + 2, item_y, w->extra.listbox.items[idx],
                            COLOR_BLACK, COLOR_WHITE);
        }
    }
}

/*
 * widget_paint_scrollbar — 绘制滚动条
 * @w: 控件指针
 * @ox, @oy: 屏幕偏移
 */
static void widget_paint_scrollbar(struct widget *w, int ox, int oy)
{
    int x = w->bounds.x + ox;
    int y = w->bounds.y + oy;
    int bw = w->bounds.width;
    int bh = w->bounds.height;
    int range, thumb_y, thumb_h;
    int btn_size = 16;

    /* 滚动条背景 */
    gfx_fill_rect(x, y, bw, bh, GUI_COLOR_SCROLLBAR_BG);

    /* 上箭头按钮 */
    gfx_fill_rect(x, y, bw, btn_size, GUI_COLOR_BUTTON_BG);
    gfx_draw_rect(x, y, bw, btn_size, COLOR_BLACK);
    /* 箭头 */
    gfx_draw_line(x + bw / 2, y + 3, x + 3, y + btn_size - 3, COLOR_BLACK);
    gfx_draw_line(x + bw / 2, y + 3, x + bw - 3, y + btn_size - 3, COLOR_BLACK);

    /* 下箭头按钮 */
    gfx_fill_rect(x, y + bh - btn_size, bw, btn_size, GUI_COLOR_BUTTON_BG);
    gfx_draw_rect(x, y + bh - btn_size, bw, btn_size, COLOR_BLACK);
    /* 箭头 */
    gfx_draw_line(x + bw / 2, y + bh - 3, x + 3, y + bh - btn_size + 3, COLOR_BLACK);
    gfx_draw_line(x + bw / 2, y + bh - 3, x + bw - 3, y + bh - btn_size + 3, COLOR_BLACK);

    /* 滑块 */
    range = w->extra.scrollbar.max_val - w->extra.scrollbar.min_val;
    if (range > 0) {
        int track_h = bh - 2 * btn_size;
        thumb_h = track_h / (range + 1);
        if (thumb_h < 8) thumb_h = 8;
        thumb_y = btn_size + (w->extra.scrollbar.cur_val - w->extra.scrollbar.min_val) *
                  (track_h - thumb_h) / range;
        gfx_fill_rect(x + 1, y + thumb_y, bw - 2, thumb_h, GUI_COLOR_BUTTON_BG);
        gfx_draw_rect(x + 1, y + thumb_y, bw - 2, thumb_h, COLOR_BLACK);
    }
}

/*
 * widget_paint_menubar — 绘制菜单栏
 * @w: 控件指针
 * @ox, @oy: 屏幕偏移
 */
static void widget_paint_menubar(struct widget *w, int ox, int oy)
{
    int x = w->bounds.x + ox;
    int y = w->bounds.y + oy;
    int bw = w->bounds.width;
    int bh = w->bounds.height;
    int i, item_x;

    /* 菜单栏背景 */
    gfx_fill_rect(x, y, bw, bh, GUI_COLOR_MENU_BG);

    /* 底部线 */
    gfx_draw_hline(x, y + bh - 1, bw, COLOR_DARK_GRAY);

    /* 菜单项 */
    item_x = x + 2;
    for (i = 0; i < w->extra.menu.item_count; i++) {
        int item_w = (int)strlen(w->extra.menu.items[i]) * FONT_WIDTH + 8;

        /* 高亮项 (焦点状态) */
        if (w->flags & WIDGET_FLAG_PRESSED) {
            gfx_fill_rect(item_x, y, item_w, bh, GUI_COLOR_MENU_HIGHLIGHT);
            gfx_draw_string(item_x + 4, y + (bh - FONT_HEIGHT) / 2,
                            w->extra.menu.items[i], COLOR_WHITE, GUI_COLOR_MENU_HIGHLIGHT);
        } else {
            gfx_draw_string(item_x + 4, y + (bh - FONT_HEIGHT) / 2,
                            w->extra.menu.items[i], GUI_COLOR_MENU_FG, GUI_COLOR_MENU_BG);
        }
        item_x += item_w;
    }
}

/* ==================== 公共 API 实现 ==================== */

/*
 * widgets_init — 初始化控件系统
 */
void widgets_init(void)
{
    int i;
    for (i = 0; i < MAX_WIDGETS; i++)
        g_widgets[i].id = 0;
    g_widget_count = 0;
    g_next_widget_id = 1;
}

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
                  widget_click_fn on_click)
{
    struct widget *w;
    int idx = -1;
    int i;

    /* 查找空闲槽位 */
    for (i = 0; i < MAX_WIDGETS; i++) {
        if (g_widgets[i].id == 0) {
            idx = i;
            break;
        }
    }
    if (idx < 0)
        return -1;

    w = &g_widgets[idx];
    w->id = g_next_widget_id++;
    w->type = type;
    w->flags = WIDGET_FLAG_VISIBLE | WIDGET_FLAG_ENABLED;
    w->bounds.x = x;
    w->bounds.y = y;
    w->bounds.width = width;
    w->bounds.height = height;
    w->fg = fg;
    w->bg = bg;
    w->on_click = on_click;
    w->on_paint = NULL;
    w->parent_id = -1;
    w->child_count = 0;
    w->user_data = NULL;

    /* 设置文本 */
    if (text) {
        strncpy(w->text, text, WIDGET_MAX_TEXT - 1);
        w->text[WIDGET_MAX_TEXT - 1] = '\0';
    } else {
        w->text[0] = '\0';
    }

    /* 初始化类型特有数据 */
    memset(&w->extra, 0, sizeof(w->extra));

    g_widget_count++;
    return w->id;
}

/*
 * widget_destroy — 销毁控件
 * @widget_id: 控件 ID
 */
void widget_destroy(int widget_id)
{
    int idx = widget_find(widget_id);
    if (idx < 0) return;

    /* 从父控件中移除 */
    if (g_widgets[idx].parent_id >= 0) {
        int pidx = widget_find(g_widgets[idx].parent_id);
        if (pidx >= 0) {
            int i, j;
            for (i = 0; i < g_widgets[pidx].child_count; i++) {
                if (g_widgets[pidx].children[i] == widget_id) {
                    for (j = i; j < g_widgets[pidx].child_count - 1; j++)
                        g_widgets[pidx].children[j] = g_widgets[pidx].children[j + 1];
                    g_widgets[pidx].child_count--;
                    break;
                }
            }
        }
    }

    /* 销毁子控件 */
    while (g_widgets[idx].child_count > 0) {
        widget_destroy(g_widgets[idx].children[0]);
    }

    g_widgets[idx].id = 0;
    g_widget_count--;
}

/*
 * widget_add_child — 添加子控件
 * @parent_id: 父控件 ID
 * @child_id:  子控件 ID
 */
void widget_add_child(int parent_id, int child_id)
{
    int pidx = widget_find(parent_id);
    int cidx = widget_find(child_id);
    if (pidx < 0 || cidx < 0) return;
    if (g_widgets[pidx].child_count >= WIDGET_MAX_CHILDREN) return;

    g_widgets[pidx].children[g_widgets[pidx].child_count++] = child_id;
    g_widgets[cidx].parent_id = parent_id;
}

/*
 * widget_set_text — 设置控件文本
 * @widget_id: 控件 ID
 * @text:      新文本
 */
void widget_set_text(int widget_id, const char *text)
{
    int idx = widget_find(widget_id);
    if (idx < 0) return;
    if (text) {
        strncpy(g_widgets[idx].text, text, WIDGET_MAX_TEXT - 1);
        g_widgets[idx].text[WIDGET_MAX_TEXT - 1] = '\0';
    }
}

/*
 * widget_set_visible — 设置控件可见性
 * @widget_id: 控件 ID
 * @visible:   是否可见
 */
void widget_set_visible(int widget_id, bool visible)
{
    int idx = widget_find(widget_id);
    if (idx < 0) return;
    if (visible)
        g_widgets[idx].flags |= WIDGET_FLAG_VISIBLE;
    else
        g_widgets[idx].flags &= ~WIDGET_FLAG_VISIBLE;
}

/*
 * widget_set_enabled — 设置控件启用状态
 * @widget_id: 控件 ID
 * @enabled:   是否启用
 */
void widget_set_enabled(int widget_id, bool enabled)
{
    int idx = widget_find(widget_id);
    if (idx < 0) return;
    if (enabled)
        g_widgets[idx].flags |= WIDGET_FLAG_ENABLED;
    else
        g_widgets[idx].flags &= ~WIDGET_FLAG_ENABLED;
}

/*
 * widget_paint — 绘制控件及其子控件
 * @widget_id: 控件 ID
 * @offset_x, @offset_y: 屏幕偏移
 */
void widget_paint(int widget_id, int offset_x, int offset_y)
{
    int idx = widget_find(widget_id);
    int i;
    if (idx < 0) return;
    if (!(g_widgets[idx].flags & WIDGET_FLAG_VISIBLE)) return;

    /* 绘制自身 */
    switch (g_widgets[idx].type) {
    case WIDGET_BUTTON:
        widget_paint_button(&g_widgets[idx], offset_x, offset_y);
        break;
    case WIDGET_TEXTBOX:
        widget_paint_textbox(&g_widgets[idx], offset_x, offset_y);
        break;
    case WIDGET_LABEL:
        widget_paint_label(&g_widgets[idx], offset_x, offset_y);
        break;
    case WIDGET_CHECKBOX:
    case WIDGET_RADIO:
        widget_paint_checkbox(&g_widgets[idx], offset_x, offset_y);
        break;
    case WIDGET_LISTBOX:
        widget_paint_listbox(&g_widgets[idx], offset_x, offset_y);
        break;
    case WIDGET_SCROLLBAR:
        widget_paint_scrollbar(&g_widgets[idx], offset_x, offset_y);
        break;
    case WIDGET_MENUBAR:
    case WIDGET_MENUITEM:
        widget_paint_menubar(&g_widgets[idx], offset_x, offset_y);
        break;
    case WIDGET_PANEL:
        /* 面板: 仅绘制背景 */
        gfx_fill_rect(g_widgets[idx].bounds.x + offset_x,
                      g_widgets[idx].bounds.y + offset_y,
                      g_widgets[idx].bounds.width,
                      g_widgets[idx].bounds.height,
                      g_widgets[idx].bg);
        break;
    }

    /* 自定义绘制回调 */
    if (g_widgets[idx].on_paint)
        g_widgets[idx].on_paint(widget_id);

    /* 绘制子控件 */
    for (i = 0; i < g_widgets[idx].child_count; i++) {
        widget_paint(g_widgets[idx].children[i],
                     offset_x + g_widgets[idx].bounds.x,
                     offset_y + g_widgets[idx].bounds.y);
    }
}

/*
 * widget_handle_click — 处理点击事件
 * @widget_id: 控件 ID
 * @x, @y: 点击坐标 (屏幕坐标)
 * 返回: 被点击的控件 ID，无则返回 -1
 */
int widget_handle_click(int widget_id, int x, int y)
{
    int idx = widget_find(widget_id);
    int i, result;
    if (idx < 0) return -1;
    if (!(g_widgets[idx].flags & WIDGET_FLAG_VISIBLE)) return -1;
    if (!(g_widgets[idx].flags & WIDGET_FLAG_ENABLED)) return -1;

    /* 先检查子控件 (从后向前，后绘制的在上层) */
    for (i = g_widgets[idx].child_count - 1; i >= 0; i--) {
        result = widget_handle_click(g_widgets[idx].children[i], x, y);
        if (result >= 0) return result;
    }

    /* 检查自身 */
    {
        int wx = g_widgets[idx].bounds.x;
        int wy = g_widgets[idx].bounds.y;
        int ww = g_widgets[idx].bounds.width;
        int wh = g_widgets[idx].bounds.height;

        if (x >= wx && x < wx + ww && y >= wy && y < wy + wh) {
            /* 按下效果 */
            g_widgets[idx].flags |= WIDGET_FLAG_PRESSED;

            /* 复选框切换 */
            if (g_widgets[idx].type == WIDGET_CHECKBOX) {
                g_widgets[idx].extra.checkbox.checked ^= 1;
            }

            /* 触发回调 */
            if (g_widgets[idx].on_click)
                g_widgets[idx].on_click(widget_id);

            return widget_id;
        }
    }

    return -1;
}

/*
 * widget_get — 获取控件结构体指针
 * @widget_id: 控件 ID
 * 返回: 控件指针，无效返回 NULL
 */
struct widget *widget_get(int widget_id)
{
    int idx = widget_find(widget_id);
    if (idx < 0) return NULL;
    return &g_widgets[idx];
}

/* ==================== 特化控件 API ==================== */

/*
 * checkbox_set_checked — 设置复选框状态
 * @widget_id: 复选框控件 ID
 * @checked:   是否选中
 */
void checkbox_set_checked(int widget_id, bool checked)
{
    int idx = widget_find(widget_id);
    if (idx < 0) return;
    if (g_widgets[idx].type != WIDGET_CHECKBOX) return;
    g_widgets[idx].extra.checkbox.checked = checked ? 1 : 0;
}

/*
 * checkbox_is_checked — 获取复选框状态
 * @widget_id: 复选框控件 ID
 * 返回: 是否选中
 */
bool checkbox_is_checked(int widget_id)
{
    int idx = widget_find(widget_id);
    if (idx < 0) return false;
    if (g_widgets[idx].type != WIDGET_CHECKBOX) return false;
    return g_widgets[idx].extra.checkbox.checked != 0;
}

/*
 * listbox_add_item — 向列表框添加项
 * @widget_id: 列表框控件 ID
 * @item:      项文本
 */
void listbox_add_item(int widget_id, const char *item)
{
    int idx = widget_find(widget_id);
    if (idx < 0) return;
    if (g_widgets[idx].type != WIDGET_LISTBOX) return;
    if (g_widgets[idx].extra.listbox.item_count >= 16) return;

    strncpy(g_widgets[idx].extra.listbox.items[g_widgets[idx].extra.listbox.item_count],
            item, 63);
    g_widgets[idx].extra.listbox.items[g_widgets[idx].extra.listbox.item_count][63] = '\0';
    g_widgets[idx].extra.listbox.item_count++;
}

/*
 * listbox_get_selected — 获取列表框选中项索引
 * @widget_id: 列表框控件 ID
 * 返回: 选中项索引，无选中返回 -1
 */
int listbox_get_selected(int widget_id)
{
    int idx = widget_find(widget_id);
    if (idx < 0) return -1;
    if (g_widgets[idx].type != WIDGET_LISTBOX) return -1;
    return g_widgets[idx].extra.listbox.selected;
}

/*
 * scrollbar_set_range — 设置滚动条范围
 * @widget_id: 滚动条控件 ID
 * @min:       最小值
 * @max:       最大值
 * @current:   当前值
 */
void scrollbar_set_range(int widget_id, int min, int max, int current)
{
    int idx = widget_find(widget_id);
    if (idx < 0) return;
    if (g_widgets[idx].type != WIDGET_SCROLLBAR) return;
    g_widgets[idx].extra.scrollbar.min_val = min;
    g_widgets[idx].extra.scrollbar.max_val = max;
    g_widgets[idx].extra.scrollbar.cur_val = current;
}

/*
 * scrollbar_get_value — 获取滚动条当前值
 * @widget_id: 滚动条控件 ID
 * 返回: 当前值
 */
int scrollbar_get_value(int widget_id)
{
    int idx = widget_find(widget_id);
    if (idx < 0) return 0;
    if (g_widgets[idx].type != WIDGET_SCROLLBAR) return 0;
    return g_widgets[idx].extra.scrollbar.cur_val;
}

/*
 * menubar_add_item — 向菜单栏添加项
 * @widget_id:  菜单栏控件 ID
 * @text:       项文本
 * @callback:   点击回调
 */
void menubar_add_item(int widget_id, const char *text, widget_click_fn callback)
{
    int idx = widget_find(widget_id);
    if (idx < 0) return;
    if (g_widgets[idx].type != WIDGET_MENUBAR) return;
    if (g_widgets[idx].extra.menu.item_count >= WIDGET_MAX_MENU_ITEMS) return;

    strncpy(g_widgets[idx].extra.menu.items[g_widgets[idx].extra.menu.item_count],
            text, 31);
    g_widgets[idx].extra.menu.items[g_widgets[idx].extra.menu.item_count][31] = '\0';
    g_widgets[idx].extra.menu.callbacks[g_widgets[idx].extra.menu.item_count] = callback;
    g_widgets[idx].extra.menu.item_count++;
}
