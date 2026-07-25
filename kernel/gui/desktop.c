/*
 * Nexsteaduser — PlexsDOS
 * desktop.c — 桌面环境实现
 * 作者: Tinmc189623 | 团队: Nexlyh
 *
 * 实现 Windows 3.x 风格桌面环境。
 * 包含桌面图标、任务栏、开始菜单和应用程序入口。
 */

#include <plexsdos/desktop.h>
#include <plexsdos/wm.h>
#include <plexsdos/widgets.h>
#include <plexsdos/graphics.h>
#include <plexsdos/keyboard.h>
#include <plexsdos/string.h>
#include <libc/stddef.h>

/* ==================== 内部状态 ==================== */

static struct desktop_icon g_icons[DESKTOP_MAX_ICONS];
static int g_icon_count = 0;
static int g_next_icon_id = 1;
static bool g_desktop_running = false;
static bool g_start_menu_visible = false;

/* ==================== 内部辅助函数 ==================== */

/*
 * desktop_paint_icon — 绘制桌面图标
 * @icon: 图标指针
 */
static void desktop_paint_icon(const struct desktop_icon *icon)
{
    int text_x, text_y;
    int icon_x, icon_y;

    icon_x = icon->x;
    icon_y = icon->y;

    /* 图标背景 (简化为彩色方块) */
    gfx_fill_rect(icon_x, icon_y, ICON_SIZE, ICON_SIZE, icon->color);
    gfx_draw_rect(icon_x, icon_y, ICON_SIZE, ICON_SIZE, COLOR_BLACK);

    /* 图标内部图案 (简化为十字) */
    gfx_draw_hline(icon_x + 4, icon_y + ICON_SIZE / 2, ICON_SIZE - 8, COLOR_WHITE);
    gfx_draw_vline(icon_x + ICON_SIZE / 2, icon_y + 4, ICON_SIZE - 8, COLOR_WHITE);

    /* 图标文字 (居中在图标下方) */
    text_x = icon_x + (ICON_SIZE - (int)strlen(icon->name) * FONT_WIDTH) / 2;
    text_y = icon_y + ICON_SIZE + 2;
    if (text_x < icon_x) text_x = icon_x;
    gfx_draw_string_transparent(text_x, text_y, icon->name, COLOR_WHITE);
}

/*
 * desktop_paint_taskbar — 绘制任务栏
 */
static void desktop_paint_taskbar(void)
{
    int taskbar_y = VGA12_HEIGHT - TASKBAR_HEIGHT;

    /* 任务栏背景 */
    gfx_fill_rect(0, taskbar_y, VGA12_WIDTH, TASKBAR_HEIGHT, GUI_COLOR_MENU_BG);

    /* 顶部边框线 */
    gfx_draw_hline(0, taskbar_y, VGA12_WIDTH, COLOR_WHITE);

    /* 开始按钮 */
    gfx_fill_rect(2, taskbar_y + 2, 60, TASKBAR_HEIGHT - 4, GUI_COLOR_BUTTON_BG);
    gfx_draw_rect(2, taskbar_y + 2, 60, TASKBAR_HEIGHT - 4, COLOR_BLACK);
    gfx_draw_string(8, taskbar_y + (TASKBAR_HEIGHT - FONT_HEIGHT) / 2,
                    "Start", COLOR_BLACK, GUI_COLOR_BUTTON_BG);

    /* 任务栏按钮区域 (显示已打开的窗口) */
    {
        int btn_x = 68;
        int i;
        for (i = 0; i < WM_MAX_WINDOWS; i++) {
            struct window *win = wm_get_window(i);
            if (win && (win->flags & WM_FLAG_VISIBLE)) {
                int btn_w = (int)strlen(win->title) * FONT_WIDTH + 12;
                if (btn_w > 120) btn_w = 120;
                if (btn_w < 60) btn_w = 60;

                /* 焦点窗口按下效果 */
                if (win->flags & WM_FLAG_FOCUSED) {
                    gfx_fill_rect(btn_x, taskbar_y + 2, btn_w, TASKBAR_HEIGHT - 4,
                                  COLOR_WHITE);
                } else {
                    gfx_fill_rect(btn_x, taskbar_y + 2, btn_w, TASKBAR_HEIGHT - 4,
                                  GUI_COLOR_BUTTON_BG);
                }
                gfx_draw_rect(btn_x, taskbar_y + 2, btn_w, TASKBAR_HEIGHT - 4,
                              COLOR_BLACK);
                gfx_draw_string(btn_x + 4, taskbar_y + (TASKBAR_HEIGHT - FONT_HEIGHT) / 2,
                                win->title, COLOR_BLACK, COLOR_WHITE);
                btn_x += btn_w + 2;
                if (btn_x > VGA12_WIDTH - 80) break;
            }
        }
    }

    /* 时钟 (简化显示) */
    gfx_draw_string(VGA12_WIDTH - 60, taskbar_y + (TASKBAR_HEIGHT - FONT_HEIGHT) / 2,
                    "12:00", COLOR_BLACK, GUI_COLOR_MENU_BG);
}

/*
 * desktop_paint_start_menu — 绘制开始菜单
 */
static void desktop_paint_start_menu(void)
{
    int x = 2;
    int y = VGA12_HEIGHT - TASKBAR_HEIGHT - START_MENU_ITEM_HEIGHT * START_MENU_MAX_ITEMS - 4;
    int i;

    /* 菜单背景 */
    gfx_fill_rect(x, y, START_MENU_WIDTH,
                  START_MENU_ITEM_HEIGHT * START_MENU_MAX_ITEMS + 4,
                  GUI_COLOR_MENU_BG);

    /* 边框 */
    gfx_draw_rect(x, y, START_MENU_WIDTH,
                  START_MENU_ITEM_HEIGHT * START_MENU_MAX_ITEMS + 4,
                  COLOR_BLACK);

    /* 3D 效果 */
    gfx_draw_hline(x + 1, y + 1, START_MENU_WIDTH - 2, COLOR_WHITE);
    gfx_draw_vline(x + 1, y + 1, START_MENU_ITEM_HEIGHT * START_MENU_MAX_ITEMS + 2, COLOR_WHITE);

    /* 菜单项 */
    {
        static const char *menu_items[] = {
            "Program Manager",
            "File Manager",
            "Notepad",
            "Calculator",
            "About PlexsDOS",
            "----------------",
            "Exit to DOS"
        };
        static void (*menu_callbacks[])(void) = {
            app_program_manager,
            app_file_manager,
            app_notepad,
            app_calculator,
            app_about,
            NULL,
            NULL
        };

        for (i = 0; i < START_MENU_MAX_ITEMS && i < 7; i++) {
            int item_y = y + 2 + i * START_MENU_ITEM_HEIGHT;

            /* 分隔线 */
            if (i == 5) {
                gfx_draw_hline(x + 4, item_y + START_MENU_ITEM_HEIGHT / 2,
                               START_MENU_WIDTH - 8, COLOR_DARK_GRAY);
                continue;
            }

            /* 菜单项文字 */
            gfx_draw_string(x + 8, item_y + (START_MENU_ITEM_HEIGHT - FONT_HEIGHT) / 2,
                            menu_items[i], GUI_COLOR_MENU_FG, GUI_COLOR_MENU_BG);
        }
    }
}

/*
 * desktop_handle_start_click — 处理开始菜单点击
 * @x, @y: 点击坐标
 */
static void desktop_handle_start_click(int x, int y)
{
    int menu_x = 2;
    int menu_y = VGA12_HEIGHT - TASKBAR_HEIGHT - START_MENU_ITEM_HEIGHT * START_MENU_MAX_ITEMS - 4;
    int item;

    if (x < menu_x || x >= menu_x + START_MENU_WIDTH)
        return;
    if (y < menu_y || y >= menu_y + START_MENU_ITEM_HEIGHT * START_MENU_MAX_ITEMS)
        return;

    item = (y - menu_y - 2) / START_MENU_ITEM_HEIGHT;

    switch (item) {
    case 0: /* Program Manager */
        g_start_menu_visible = false;
        app_program_manager();
        break;
    case 1: /* File Manager */
        g_start_menu_visible = false;
        app_file_manager();
        break;
    case 2: /* Notepad */
        g_start_menu_visible = false;
        app_notepad();
        break;
    case 3: /* Calculator */
        g_start_menu_visible = false;
        app_calculator();
        break;
    case 4: /* About */
        g_start_menu_visible = false;
        app_about();
        break;
    case 6: /* Exit */
        g_start_menu_visible = false;
        g_desktop_running = false;
        break;
    }
}

/*
 * desktop_handle_icon_click — 处理桌面图标点击
 * @x, @y: 点击坐标
 */
static void desktop_handle_icon_click(int x, int y)
{
    int i;
    for (i = 0; i < g_icon_count; i++) {
        struct desktop_icon *icon = &g_icons[i];
        if (x >= icon->x && x < icon->x + ICON_SIZE &&
            y >= icon->y && y < icon->y + ICON_SIZE + ICON_TEXT_HEIGHT) {
            if (icon->on_open)
                icon->on_open();
            return;
        }
    }
}

/* ==================== 公共 API 实现 ==================== */

/*
 * desktop_init — 初始化桌面环境
 * 返回: true = 成功
 */
bool desktop_init(void)
{
    /* 初始化窗口管理器 */
    if (!wm_init())
        return false;

    /* 初始化控件系统 */
    widgets_init();

    /* 清空图标 */
    g_icon_count = 0;
    g_next_icon_id = 1;
    g_start_menu_visible = false;

    /* 添加默认桌面图标 */
    desktop_add_icon("My Computer", 20, 20, COLOR_BLUE, app_program_manager);
    desktop_add_icon("Files", 20, 100, COLOR_GREEN, app_file_manager);
    desktop_add_icon("Notepad", 20, 180, COLOR_WHITE, app_notepad);
    desktop_add_icon("Calculator", 20, 260, COLOR_LIGHT_GRAY, app_calculator);

    return true;
}

/*
 * desktop_shutdown — 关闭桌面环境
 */
void desktop_shutdown(void)
{
    g_desktop_running = false;
    wm_shutdown();
}

/*
 * desktop_run — 运行桌面主循环
 * 处理事件、重绘、直到用户退出
 */
void desktop_run(void)
{
    g_desktop_running = true;

    /* 初始绘制 */
    desktop_paint();

    /* 主循环 */
    while (g_desktop_running) {
        /* 处理键盘事件 */
        wm_process_events();

        /* 检查退出条件 (Alt+X) */
        if (keyboard_available()) {
            char c = keyboard_getchar();
            /* ESC 关闭开始菜单 */
            if (c == 0x1B && g_start_menu_visible) {
                g_start_menu_visible = false;
                desktop_paint();
            }
        }

        /* 重绘 */
        desktop_paint();
    }
}

/*
 * desktop_add_icon — 添加桌面图标
 * @name:     图标名称
 * @x, @y:    位置
 * @color:    图标颜色
 * @on_open:  双击打开回调
 * 返回: 图标 ID
 */
int desktop_add_icon(const char *name, int x, int y, uint8_t color,
                     void (*on_open)(void))
{
    struct desktop_icon *icon;

    if (g_icon_count >= DESKTOP_MAX_ICONS)
        return -1;

    icon = &g_icons[g_icon_count++];
    icon->id = g_next_icon_id++;
    icon->x = x;
    icon->y = y;
    icon->color = color;
    icon->on_open = on_open;

    if (name) {
        strncpy(icon->name, name, sizeof(icon->name) - 1);
        icon->name[sizeof(icon->name) - 1] = '\0';
    }

    return icon->id;
}

/*
 * desktop_paint — 重绘桌面
 */
void desktop_paint(void)
{
    int i;

    /* 桌面背景 */
    wm_paint_desktop();

    /* 绘制图标 */
    for (i = 0; i < g_icon_count; i++) {
        desktop_paint_icon(&g_icons[i]);
    }

    /* 绘制窗口 */
    wm_repaint();

    /* 绘制任务栏 */
    desktop_paint_taskbar();

    /* 绘制开始菜单 */
    if (g_start_menu_visible) {
        desktop_paint_start_menu();
    }
}

/*
 * desktop_show_start_menu — 显示开始菜单
 */
void desktop_show_start_menu(void)
{
    g_start_menu_visible = !g_start_menu_visible;
    desktop_paint();
}

/* ==================== 应用程序实现 ==================== */

/* Program Manager 窗口回调 */
static int g_pm_win_id = -1;

/*
 * pm_paint — 程序管理器绘制回调
 * @win_id: 窗口 ID
 * @x, @y: 客户区位置
 * @width, @height: 客户区大小
 */
static void pm_paint(int win_id, int x, int y, int width, int height)
{
    (void)win_id;
    int icon_x = x + 10;
    int icon_y = y + 10;

    /* 标题 */
    gfx_draw_string(x + 10, y + 5, "Program Manager", COLOR_BLACK, GUI_COLOR_WINDOW_BG);

    /* 程序图标 */
    gfx_fill_rect(icon_x, icon_y + 20, 32, 32, COLOR_BLUE);
    gfx_draw_rect(icon_x, icon_y + 20, 32, 32, COLOR_BLACK);
    gfx_draw_string(icon_x, icon_y + 56, "Notepad", COLOR_BLACK, GUI_COLOR_WINDOW_BG);

    icon_x += 80;
    gfx_fill_rect(icon_x, icon_y + 20, 32, 32, COLOR_GREEN);
    gfx_draw_rect(icon_x, icon_y + 20, 32, 32, COLOR_BLACK);
    gfx_draw_string(icon_x, icon_y + 56, "Files", COLOR_BLACK, GUI_COLOR_WINDOW_BG);

    icon_x += 80;
    gfx_fill_rect(icon_x, icon_y + 20, 32, 32, COLOR_LIGHT_GRAY);
    gfx_draw_rect(icon_x, icon_y + 20, 32, 32, COLOR_BLACK);
    gfx_draw_string(icon_x, icon_y + 56, "Calc", COLOR_BLACK, GUI_COLOR_WINDOW_BG);

    /* 系统信息 */
    gfx_draw_string(x + 10, y + height - 30,
                    "PlexsDOS v0.2 - Nexsteaduser",
                    COLOR_DARK_GRAY, GUI_COLOR_WINDOW_BG);
}

/*
 * pm_event — 程序管理器事件回调
 */
static void pm_event(int win_id, int event, int param1, int param2)
{
    (void)param2;
    if (event == WM_EVENT_CLOSE) {
        wm_destroy_window(win_id);
        g_pm_win_id = -1;
    }
}

/*
 * app_program_manager — 启动程序管理器
 */
void app_program_manager(void)
{
    if (g_pm_win_id >= 0) {
        wm_focus_window(g_pm_win_id);
        return;
    }

    g_pm_win_id = wm_create_window("Program Manager", 50, 30, 400, 300,
                                   WM_FLAG_VISIBLE | WM_FLAG_MOVABLE | WM_FLAG_RESIZABLE,
                                   pm_paint, pm_event);
    if (g_pm_win_id >= 0)
        wm_show_window(g_pm_win_id);
}

/* File Manager 窗口回调 */
static int g_fm_win_id = -1;

/*
 * fm_paint — 文件管理器绘制回调
 */
static void fm_paint(int win_id, int x, int y, int width, int height)
{
    (void)win_id;
    int line_y = y + 5;

    /* 标题 */
    gfx_draw_string(x + 5, line_y, "File Manager - A:\\", COLOR_BLACK, GUI_COLOR_WINDOW_BG);
    line_y += FONT_HEIGHT + 4;

    /* 分隔线 */
    gfx_draw_hline(x + 5, line_y, width - 10, COLOR_DARK_GRAY);
    line_y += 4;

    /* 文件列表 (示例) */
    gfx_draw_string(x + 10, line_y, "[DIR]  .", COLOR_BLACK, GUI_COLOR_WINDOW_BG);
    line_y += FONT_HEIGHT;
    gfx_draw_string(x + 10, line_y, "[DIR]  ..", COLOR_BLACK, GUI_COLOR_WINDOW_BG);
    line_y += FONT_HEIGHT;
    gfx_draw_string(x + 10, line_y, "[DIR]  SYSTEM", COLOR_BLACK, GUI_COLOR_WINDOW_BG);
    line_y += FONT_HEIGHT;
    gfx_draw_string(x + 10, line_y, "[DIR]  PROGRAMS", COLOR_BLACK, GUI_COLOR_WINDOW_BG);
    line_y += FONT_HEIGHT;
    gfx_draw_string(x + 10, line_y, "       KERNEL BIN  123456 bytes", COLOR_BLACK, GUI_COLOR_WINDOW_BG);
    line_y += FONT_HEIGHT;
    gfx_draw_string(x + 10, line_y, "       README TXT  1024 bytes", COLOR_BLACK, GUI_COLOR_WINDOW_BG);
    line_y += FONT_HEIGHT;
    gfx_draw_string(x + 10, line_y, "       CONFIG SYS  256 bytes", COLOR_BLACK, GUI_COLOR_WINDOW_BG);

    /* 状态栏 */
    gfx_draw_hline(x + 5, y + height - FONT_HEIGHT - 6, width - 10, COLOR_DARK_GRAY);
    gfx_draw_string(x + 5, y + height - FONT_HEIGHT - 4,
                    "5 items  125,741 bytes free",
                    COLOR_DARK_GRAY, GUI_COLOR_WINDOW_BG);
}

/*
 * fm_event — 文件管理器事件回调
 */
static void fm_event(int win_id, int event, int param1, int param2)
{
    (void)param1; (void)param2;
    if (event == WM_EVENT_CLOSE) {
        wm_destroy_window(win_id);
        g_fm_win_id = -1;
    }
}

/*
 * app_file_manager — 启动文件管理器
 */
void app_file_manager(void)
{
    if (g_fm_win_id >= 0) {
        wm_focus_window(g_fm_win_id);
        return;
    }

    g_fm_win_id = wm_create_window("File Manager", 80, 40, 450, 350,
                                   WM_FLAG_VISIBLE | WM_FLAG_MOVABLE | WM_FLAG_RESIZABLE,
                                   fm_paint, fm_event);
    if (g_fm_win_id >= 0)
        wm_show_window(g_fm_win_id);
}

/* Notepad 窗口回调 */
static int g_np_win_id = -1;
static char g_np_buffer[2048];
static int g_np_cursor = 0;

/*
 * np_paint — 记事本绘制回调
 */
static void np_paint(int win_id, int x, int y, int width, int height)
{
    (void)win_id;
    /* 文本区域背景 */
    gfx_fill_rect(x + 2, y + 2, width - 4, height - 4, COLOR_WHITE);

    /* 凹陷边框 */
    gfx_draw_hline(x + 2, y + 2, width - 4, COLOR_DARK_GRAY);
    gfx_draw_vline(x + 2, y + 2, height - 4, COLOR_DARK_GRAY);
    gfx_draw_hline(x + 2, y + height - 3, width - 4, COLOR_WHITE);
    gfx_draw_vline(x + width - 3, y + 2, height - 4, COLOR_WHITE);

    /* 显示文本内容 */
    if (g_np_buffer[0] != '\0') {
        int line_x = x + 6;
        int line_y = y + 6;
        int i = 0;

        while (g_np_buffer[i] != '\0' && line_y < y + height - FONT_HEIGHT) {
            if (g_np_buffer[i] == '\n') {
                line_x = x + 6;
                line_y += FONT_HEIGHT;
            } else {
                gfx_draw_char(line_x, line_y, g_np_buffer[i], COLOR_BLACK, COLOR_WHITE);
                line_x += FONT_WIDTH;
                if (line_x > x + width - FONT_WIDTH) {
                    line_x = x + 6;
                    line_y += FONT_HEIGHT;
                }
            }
            i++;
        }
    } else {
        gfx_draw_string(x + 6, y + 6, "Type here...", COLOR_DARK_GRAY, COLOR_WHITE);
    }
}

/*
 * np_event — 记事本事件回调
 */
static void np_event(int win_id, int event, int param1, int param2)
{
    (void)param2;
    if (event == WM_EVENT_CLOSE) {
        wm_destroy_window(win_id);
        g_np_win_id = -1;
        g_np_buffer[0] = '\0';
        g_np_cursor = 0;
    } else if (event == WM_EVENT_KEY) {
        char c = (char)param1;
        if (c == '\b') {
            /* 退格 */
            if (g_np_cursor > 0) {
                g_np_cursor--;
                g_np_buffer[g_np_cursor] = '\0';
            }
        } else if (c >= 32 && c < 127) {
            /* 可打印字符 */
            if (g_np_cursor < (int)sizeof(g_np_buffer) - 1) {
                g_np_buffer[g_np_cursor++] = c;
                g_np_buffer[g_np_cursor] = '\0';
            }
        }
        wm_invalidate(win_id);
    }
}

/*
 * app_notepad — 启动记事本
 */
void app_notepad(void)
{
    if (g_np_win_id >= 0) {
        wm_focus_window(g_np_win_id);
        return;
    }

    g_np_buffer[0] = '\0';
    g_np_cursor = 0;

    g_np_win_id = wm_create_window("Notepad", 100, 50, 400, 300,
                                   WM_FLAG_VISIBLE | WM_FLAG_MOVABLE | WM_FLAG_RESIZABLE,
                                   np_paint, np_event);
    if (g_np_win_id >= 0)
        wm_show_window(g_np_win_id);
}

/* Calculator 窗口回调 */
static int g_calc_win_id = -1;
static char g_calc_display[32] = "0";
static int g_calc_value = 0;
static int g_calc_operand = 0;
static char g_calc_op = 0;
static bool g_calc_new_input = true;

/*
 * calc_update_display — 更新计算器显示
 */
static void calc_update_display(int value)
{
    int i = 0;
    int neg = 0;
    char tmp[16];

    if (value < 0) {
        neg = 1;
        value = -value;
    }

    if (value == 0) {
        g_calc_display[0] = '0';
        g_calc_display[1] = '\0';
        return;
    }

    /* 数字转字符串 */
    while (value > 0 && i < 15) {
        tmp[i++] = '0' + (char)(value % 10);
        value /= 10;
    }
    if (neg) tmp[i++] = '-';

    /* 反转 */
    {
        int j;
        for (j = 0; j < i; j++)
            g_calc_display[j] = tmp[i - 1 - j];
        g_calc_display[i] = '\0';
    }
}

/*
 * calc_button_click — 计算器按钮点击回调
 * @widget_id: 按钮控件 ID
 */
static void calc_button_click(int widget_id)
{
    struct widget *w = widget_get(widget_id);
    if (!w) return;

    char c = w->text[0];

    if (c >= '0' && c <= '9') {
        if (g_calc_new_input) {
            g_calc_value = c - '0';
            g_calc_new_input = false;
        } else {
            g_calc_value = g_calc_value * 10 + (c - '0');
        }
        calc_update_display(g_calc_value);
    } else if (c == '+' || c == '-' || c == '*' || c == '/') {
        g_calc_operand = g_calc_value;
        g_calc_op = c;
        g_calc_new_input = true;
    } else if (c == '=') {
        switch (g_calc_op) {
        case '+': g_calc_value = g_calc_operand + g_calc_value; break;
        case '-': g_calc_value = g_calc_operand - g_calc_value; break;
        case '*': g_calc_value = g_calc_operand * g_calc_value; break;
        case '/':
            if (g_calc_value != 0)
                g_calc_value = g_calc_operand / g_calc_value;
            else
                g_calc_value = 0;
            break;
        }
        g_calc_op = 0;
        g_calc_new_input = true;
        calc_update_display(g_calc_value);
    } else if (c == 'C') {
        g_calc_value = 0;
        g_calc_operand = 0;
        g_calc_op = 0;
        g_calc_new_input = true;
        calc_update_display(0);
    }

    if (g_calc_win_id >= 0)
        wm_invalidate(g_calc_win_id);
}

/*
 * calc_paint — 计算器绘制回调
 */
static void calc_paint(int win_id, int x, int y, int width, int height)
{
    (void)win_id;
    int disp_x = x + 10;
    int disp_y = y + 10;
    int disp_w = width - 20;
    int btn_y = disp_y + FONT_HEIGHT + 10;
    int btn_w = 40;
    int btn_h = 24;
    int gap = 4;
    int bx, by;
    int i, j;

    /* 显示屏 */
    gfx_fill_rect(disp_x, disp_y, disp_w, FONT_HEIGHT + 4, COLOR_WHITE);
    gfx_draw_rect(disp_x, disp_y, disp_w, FONT_HEIGHT + 4, COLOR_BLACK);

    /* 显示数字 (右对齐) */
    {
        int text_len = (int)strlen(g_calc_display);
        int text_x = disp_x + disp_w - text_len * FONT_WIDTH - 4;
        gfx_draw_string(text_x, disp_y + 2, g_calc_display, COLOR_BLACK, COLOR_WHITE);
    }

    /* 按钮布局 */
    {
        static const char *btn_labels[] = {
            "7", "8", "9", "/",
            "4", "5", "6", "*",
            "1", "2", "3", "-",
            "0", "C", "=", "+"
        };

        for (i = 0; i < 4; i++) {
            by = btn_y + i * (btn_h + gap);
            for (j = 0; j < 4; j++) {
                bx = disp_x + j * (btn_w + gap);

                /* 按钮背景 */
                gfx_fill_rect(bx, by, btn_w, btn_h, GUI_COLOR_BUTTON_BG);
                gfx_draw_rect(bx, by, btn_w, btn_h, COLOR_BLACK);

                /* 3D 效果 */
                gfx_draw_hline(bx + 1, by + 1, btn_w - 2, COLOR_WHITE);
                gfx_draw_vline(bx + 1, by + 1, btn_h - 2, COLOR_WHITE);

                /* 文字居中 */
                gfx_draw_string(bx + (btn_w - FONT_WIDTH) / 2,
                                by + (btn_h - FONT_HEIGHT) / 2,
                                btn_labels[i * 4 + j],
                                COLOR_BLACK, GUI_COLOR_BUTTON_BG);
            }
        }
    }
}

/*
 * calc_event — 计算器事件回调
 */
static void calc_event(int win_id, int event, int param1, int param2)
{
    (void)param2;
    if (event == WM_EVENT_CLOSE) {
        wm_destroy_window(win_id);
        g_calc_win_id = -1;
        g_calc_value = 0;
        g_calc_operand = 0;
        g_calc_op = 0;
        g_calc_new_input = true;
        g_calc_display[0] = '0';
        g_calc_display[1] = '\0';
    } else if (event == WM_EVENT_KEY) {
        char c = (char)param1;
        /* 数字键 */
        if (c >= '0' && c <= '9') {
            if (g_calc_new_input) {
                g_calc_value = c - '0';
                g_calc_new_input = false;
            } else {
                g_calc_value = g_calc_value * 10 + (c - '0');
            }
            calc_update_display(g_calc_value);
            wm_invalidate(win_id);
        }
        /* 运算符 */
        else if (c == '+' || c == '-' || c == '*' || c == '/') {
            g_calc_operand = g_calc_value;
            g_calc_op = c;
            g_calc_new_input = true;
        }
        /* 等号/回车 */
        else if (c == '=' || c == '\r') {
            calc_button_click(0);  /* 触发计算 */
        }
        /* 清除 */
        else if (c == 'c' || c == 'C') {
            g_calc_value = 0;
            g_calc_operand = 0;
            g_calc_op = 0;
            g_calc_new_input = true;
            calc_update_display(0);
            wm_invalidate(win_id);
        }
    }
}

/*
 * app_calculator — 启动计算器
 */
void app_calculator(void)
{
    if (g_calc_win_id >= 0) {
        wm_focus_window(g_calc_win_id);
        return;
    }

    g_calc_value = 0;
    g_calc_operand = 0;
    g_calc_op = 0;
    g_calc_new_input = true;
    g_calc_display[0] = '0';
    g_calc_display[1] = '\0';

    g_calc_win_id = wm_create_window("Calculator", 200, 80, 200, 220,
                                     WM_FLAG_VISIBLE | WM_FLAG_MOVABLE,
                                     calc_paint, calc_event);
    if (g_calc_win_id >= 0)
        wm_show_window(g_calc_win_id);
}

/* About 对话框 */
static int g_about_win_id = -1;

/*
 * about_paint — 关于对话框绘制回调
 */
static void about_paint(int win_id, int x, int y, int width, int height)
{
    (void)win_id;
    int cx = x + width / 2;

    /* 图标 */
    gfx_fill_rect(cx - 16, y + 15, 32, 32, COLOR_BLUE);
    gfx_draw_rect(cx - 16, y + 15, 32, 32, COLOR_BLACK);

    /* 文字 */
    gfx_draw_string(cx - 70, y + 55, "PlexsDOS v0.2", COLOR_BLACK, GUI_COLOR_WINDOW_BG);
    gfx_draw_string(cx - 80, y + 55 + FONT_HEIGHT + 4,
                    "Nexsteaduser OS Project", COLOR_DARK_GRAY, GUI_COLOR_WINDOW_BG);
    gfx_draw_string(cx - 60, y + 55 + 2 * (FONT_HEIGHT + 4),
                    "Author: Tinmc189623", COLOR_DARK_GRAY, GUI_COLOR_WINDOW_BG);
    gfx_draw_string(cx - 50, y + 55 + 3 * (FONT_HEIGHT + 4),
                    "Team: Nexlyh", COLOR_DARK_GRAY, GUI_COLOR_WINDOW_BG);

    /* OK 按钮 */
    {
        int btn_x = cx - 30;
        int btn_y = y + height - 35;
        gfx_fill_rect(btn_x, btn_y, 60, 24, GUI_COLOR_BUTTON_BG);
        gfx_draw_rect(btn_x, btn_y, 60, 24, COLOR_BLACK);
        gfx_draw_hline(btn_x + 1, btn_y + 1, 58, COLOR_WHITE);
        gfx_draw_vline(btn_x + 1, btn_y + 1, 22, COLOR_WHITE);
        gfx_draw_string(btn_x + 14, btn_y + 4, "OK", COLOR_BLACK, GUI_COLOR_BUTTON_BG);
    }
}

/*
 * about_event — 关于对话框事件回调
 */
static void about_event(int win_id, int event, int param1, int param2)
{
    (void)param1; (void)param2;
    if (event == WM_EVENT_CLOSE) {
        wm_destroy_window(win_id);
        g_about_win_id = -1;
    }
}

/*
 * app_about — 显示关于对话框
 */
void app_about(void)
{
    if (g_about_win_id >= 0) {
        wm_focus_window(g_about_win_id);
        return;
    }

    g_about_win_id = wm_create_window("About PlexsDOS",
                                      VGA12_WIDTH / 2 - 100,
                                      VGA12_HEIGHT / 2 - 80,
                                      200, 160,
                                      WM_FLAG_VISIBLE | WM_FLAG_MOVABLE,
                                      about_paint, about_event);
    if (g_about_win_id >= 0)
        wm_show_window(g_about_win_id);
}
