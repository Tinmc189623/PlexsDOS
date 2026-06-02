/*
 * Nexsteaduser — PlexsDOS
 * editor.c — 全屏文本编辑器
 * 作者: Tinmc189623 | 团队: Nexlyh
 *
 * 类 DOS EDIT.COM 的全屏文本编辑器。
 * 功能:
 *   - 光标移动 (方向键, Home/End, PgUp/PgDn)
 *   - 插入/删除字符和行
 *   - 查找文本 (Ctrl+F)
 *   - 保存文件 (Ctrl+S)
 *   - 行号显示
 *   - 状态栏 (文件名, 行号, 修改标记)
 *
 * 按键:
 *   方向键     — 移动光标
 *   Home/End   — 行首/行尾
 *   PgUp/PgDn  — 翻页
 *   Ctrl+S     — 保存
 *   Ctrl+F     — 查找
 *   Ctrl+G     — 跳转到行
 *   ESC        — 退出
 */

#include <plexsdos/editor.h>
#include <plexsdos/types.h>
#include <plexsdos/screen.h>
#include <plexsdos/keyboard.h>
#include <plexsdos/fat32.h>
#include <plexsdos/string.h>

/* 编辑器常量 */
#define EDIT_MAX_LINES     128       /* 最大行数 */
#define EDIT_MAX_COLS      128       /* 每行最大字符数 */
#define EDIT_TAB_STOP      4         /* Tab 宽度 */
#define EDIT_LINE_NUM_W    5         /* 行号区域宽度 */
#define EDIT_SCREEN_LINES  23        /* 可编辑区域行数 (25 - 状态栏 - 命令行) */
#define EDIT_SCREEN_COLS   80        /* 屏幕宽度 */

/* VGA 文本模式显存 */
static volatile uint16_t *vga = (volatile uint16_t *)0xB8000;

/* 编辑缓冲区: 每行一个字符串 */
static char edit_lines[EDIT_MAX_LINES][EDIT_MAX_COLS];
static int edit_line_count;          /* 文件总行数 */

/* 光标位置 */
static int cursor_row;               /* 光标所在行 (文件内) */
static int cursor_col;               /* 光标所在列 (文件内) */

/* 视口偏移 (左上角在文件中的位置) */
static int view_top;                 /* 视口第一行的行号 */
static int view_left;                /* 视口第一列的列号 */

/* 状态 */
static char edit_filename[64];
static bool edit_modified;
static bool edit_running;

/* 查找缓冲区 */
static char search_str[64];

/* 颜色属性 */
#define COLOR_STATUS    0x1F   /* 蓝底白字 — 状态栏 */
#define COLOR_LINENUM   0x03   /* 黑底青色 — 行号 */
#define COLOR_TEXT      0x07   /* 黑底白字 — 正文 */
#define COLOR_TEXT_HI    0x0F   /* 黑底亮白 — 高亮正文 */
#define COLOR_CMD       0x0E   /* 黑底黄色 — 命令行 */
#define COLOR_CUR_LINE  0x07   /* 当前行 */

/*
 * vga_putchar_at — 在指定位置写入字符和颜色
 * @row: 屏幕行 (0-24)
 * @col: 屏幕列 (0-79)
 * @ch:  字符
 * @attr: 颜色属性
 */
static void vga_putchar_at(int row, int col, char ch, uint8_t attr)
{
    if (row >= 0 && row < 25 && col >= 0 && col < 80)
        vga[row * 80 + col] = (uint16_t)((attr << 8) | (uint8_t)ch);
}

/*
 * vga_puts_at — 在指定位置写入字符串
 * @row:  屏幕行
 * @col:  屏幕列
 * @str:  字符串
 * @attr: 颜色属性
 * 返回: 结束列位置。
 */
static int vga_puts_at(int row, int col, const char *str, uint8_t attr)
{
    while (*str) {
        vga_putchar_at(row, col, *str, attr);
        col++;
        str++;
    }
    return col;
}

/*
 * vga_fill_at — 在指定位置填充字符
 * @row:  屏幕行
 * @col:  起始列
 * @count: 字符数
 * @ch:   字符
 * @attr: 颜色属性
 */
static void vga_fill_at(int row, int col, int count, char ch, uint8_t attr)
{
    for (int i = 0; i < count; i++)
        vga_putchar_at(row, col + i, ch, attr);
}

/*
 * edit_strlen — 计算行字符串长度
 */
static int edit_strlen(const char *s)
{
    int len = 0;
    while (*s++) len++;
    return len;
}

/*
 * edit_strcpy — 复制字符串
 */
static void edit_strcpy(char *dst, const char *src)
{
    while ((*dst++ = *src++) != '\0')
        ;
}

/*
 * edit_print_dec — 输出十进制数到指定位置
 */
static int edit_print_dec_at(int row, int col, int val, uint8_t attr)
{
    char buf[12];
    int i = 0;
    if (val == 0) {
        buf[i++] = '0';
    } else {
        int tmp = val;
        while (tmp > 0) { buf[i++] = '0' + (char)(tmp % 10); tmp /= 10; }
    }
    /* 反转 */
    for (int j = 0; j < i / 2; j++) {
        char t = buf[j]; buf[j] = buf[i - 1 - j]; buf[i - 1 - j] = t;
    }
    buf[i] = '\0';
    return vga_puts_at(row, col, buf, attr);
}

/* ===== 屏幕渲染 ===== */

/*
 * draw_status_bar — 绘制状态栏 (第 0 行)
 *
 * 格式: " Nexsteaduser Editor | filename.txt | Ln 1 Col 1 | Modified"
 */
static void draw_status_bar(void)
{
    vga_fill_at(0, 0, 80, ' ', COLOR_STATUS);

    int col = 1;
    col = vga_puts_at(0, col, " Nexsteaduser Editor", COLOR_STATUS);
    col = vga_puts_at(0, col, " | ", COLOR_STATUS);
    col = vga_puts_at(0, col, edit_filename[0] ? edit_filename : "(untitled)", COLOR_STATUS);
    col = vga_puts_at(0, col, " | Ln ", COLOR_STATUS);
    col = edit_print_dec_at(0, col, cursor_row + 1, COLOR_STATUS);
    col = vga_puts_at(0, col, " Col ", COLOR_STATUS);
    col = edit_print_dec_at(0, col, cursor_col + 1, COLOR_STATUS);
    if (edit_modified)
        col = vga_puts_at(0, col, " | *", COLOR_STATUS);
}

/*
 * draw_command_line — 绘制命令行 (第 24 行)
 */
static void draw_command_line(void)
{
    vga_fill_at(24, 0, 80, ' ', 0x07);
    vga_puts_at(24, 0, " ^S:Save ^F:Find ^G:Goto  ESC:Quit", 0x07);
}

/*
 * draw_line — 绘制一行文本 (带行号)
 * @file_row: 文件行号 (0-based)
 * @screen_row: 屏幕行号 (1-based, 第 0 行是状态栏)
 */
static void draw_line(int file_row, int screen_row)
{
    /* 行号区域 */
    if (file_row < edit_line_count) {
        edit_print_dec_at(screen_row, 0, file_row + 1, COLOR_LINENUM);
        vga_putchar_at(screen_row, EDIT_LINE_NUM_W - 1, '|', COLOR_LINENUM);
    } else {
        vga_fill_at(screen_row, 0, EDIT_LINE_NUM_W, ' ', COLOR_LINENUM);
        vga_putchar_at(screen_row, 0, '~', COLOR_LINENUM);
    }

    /* 文本区域 */
    int start_col = view_left;
    int max_col = EDIT_SCREEN_COLS - EDIT_LINE_NUM_W;

    if (file_row < edit_line_count) {
        const char *line = edit_lines[file_row];
        int len = edit_strlen(line);

        for (int i = 0; i < max_col; i++) {
            int src_col = start_col + i;
            char ch;
            uint8_t attr;

            if (src_col < len) {
                ch = line[src_col];
                if (ch == '\t') ch = ' ';  /* Tab 显示为空格 */
                attr = COLOR_TEXT;
            } else {
                ch = ' ';
                attr = COLOR_TEXT;
            }

            /* 当前行高亮 */
            if (file_row == cursor_row)
                attr = COLOR_CUR_LINE;

            vga_putchar_at(screen_row, EDIT_LINE_NUM_W + i, ch, attr);
        }
    } else {
        /* 超出文件范围的空行 */
        vga_fill_at(screen_row, EDIT_LINE_NUM_W, max_col, ' ', COLOR_TEXT);
    }
}

/*
 * draw_screen — 重绘整个编辑区域
 */
static void draw_screen(void)
{
    draw_status_bar();
    draw_command_line();

    for (int i = 0; i < EDIT_SCREEN_LINES; i++) {
        int file_row = view_top + i;
        draw_line(file_row, i + 1);
    }
}

/*
 * update_cursor — 更新硬件光标位置
 *
 * 将文件光标位置转换为屏幕位置并设置 VGA 硬件光标。
 * VGA 光标寄存器: 0x3D4=索引, 0x3D5=数据。
 * 寄存器 0x0E=光标位置高字节, 0x0F=低字节。
 */
static void update_cursor(void)
{
    uint16_t screen_row = (uint16_t)((cursor_row - view_top) + 1);
    uint16_t screen_col = (uint16_t)((cursor_col - view_left) + EDIT_LINE_NUM_W);

    if (screen_row < 1) screen_row = 1;
    if (screen_row > EDIT_SCREEN_LINES) screen_row = EDIT_SCREEN_LINES;
    if (screen_col < EDIT_LINE_NUM_W) screen_col = EDIT_LINE_NUM_W;
    if (screen_col >= 80) screen_col = 79;

    uint16_t pos = screen_row * 80 + screen_col;
    uint8_t hi = (uint8_t)((pos >> 8) & 0xFF);
    uint8_t lo = (uint8_t)(pos & 0xFF);

    /* 写入光标位置高字节 (寄存器 0x0E) */
    __asm__ __volatile__("outb %0, %1" : : "a"((uint8_t)0x0E), "Nd"((uint16_t)0x3D4));
    __asm__ __volatile__("outb %0, %1" : : "a"(hi), "Nd"((uint16_t)0x3D5));

    /* 写入光标位置低字节 (寄存器 0x0F) */
    __asm__ __volatile__("outb %0, %1" : : "a"((uint8_t)0x0F), "Nd"((uint16_t)0x3D4));
    __asm__ __volatile__("outb %0, %1" : : "a"(lo), "Nd"((uint16_t)0x3D5));
}

/* ===== 编辑操作 ===== */

/*
 * ensure_line — 确保行号有效, 必要时扩展文件
 * @line: 行号
 */
static void ensure_line(int line)
{
    while (edit_line_count <= line) {
        edit_lines[edit_line_count][0] = '\0';
        edit_line_count++;
    }
}

/*
 * insert_char — 在光标位置插入字符
 * @ch: 要插入的字符
 */
static void insert_char(char ch)
{
    ensure_line(cursor_row);

    char *line = edit_lines[cursor_row];
    int len = edit_strlen(line);

    if (len >= EDIT_MAX_COLS - 1)
        return;

    /* 右移字符 */
    for (int i = len; i > cursor_col; i--)
        line[i] = line[i - 1];

    line[cursor_col] = ch;
    line[len + 1] = '\0';

    cursor_col++;
    edit_modified = true;
}

/*
 * delete_char — 删除光标位置的字符
 */
static void delete_char(void)
{
    if (cursor_row >= edit_line_count)
        return;

    char *line = edit_lines[cursor_row];
    int len = edit_strlen(line);

    if (cursor_col >= len)
        return;

    /* 左移字符 */
    for (int i = cursor_col; i < len; i++)
        line[i] = line[i + 1];

    edit_modified = true;
}

/*
 * backspace_char — 退格删除
 */
static void backspace_char(void)
{
    if (cursor_col > 0) {
        cursor_col--;
        delete_char();
    } else if (cursor_row > 0) {
        /* 合并到上一行 */
        int prev_len = edit_strlen(edit_lines[cursor_row - 1]);
        int cur_len = edit_strlen(edit_lines[cursor_row]);

        if (prev_len + cur_len < EDIT_MAX_COLS) {
            /* 拼接 */
            for (int i = 0; i <= cur_len; i++)
                edit_lines[cursor_row - 1][prev_len + i] = edit_lines[cursor_row][i];

            /* 删除当前行 */
            for (int i = cursor_row; i < edit_line_count - 1; i++)
                edit_strcpy(edit_lines[i], edit_lines[i + 1]);
            edit_line_count--;

            cursor_row--;
            cursor_col = prev_len;
            edit_modified = true;
        }
    }
}

/*
 * insert_newline — 在光标位置插入新行 (Enter)
 */
static void insert_newline(void)
{
    ensure_line(cursor_row);

    char *line = edit_lines[cursor_row];
    int len = edit_strlen(line);

    /* 将光标后的部分移到新行 */
    char new_line[EDIT_MAX_COLS];
    int right_len = len - cursor_col;
    for (int i = 0; i <= right_len; i++)
        new_line[i] = line[cursor_col + i];

    /* 截断当前行 */
    line[cursor_col] = '\0';

    /* 插入新行 */
    if (edit_line_count < EDIT_MAX_LINES - 1) {
        for (int i = edit_line_count; i > cursor_row; i--)
            edit_strcpy(edit_lines[i + 1], edit_lines[i]);
        edit_strcpy(edit_lines[cursor_row + 1], new_line);
        edit_line_count++;
    }

    cursor_row++;
    cursor_col = 0;
    edit_modified = true;
}

/* ===== 文件操作 ===== */

/*
 * file_save — 保存文件到 FAT32
 * @filename: 文件名
 *
 * 将编辑缓冲区内容写入文件。
 */
static bool file_save(const char *filename)
{
    /* 构建数据缓冲区 (使用编辑缓冲区下方的内存) */
    static uint8_t save_buf[32768];
    uint32_t offset = 0;

    for (int i = 0; i < edit_line_count && offset < sizeof(save_buf) - 2; i++) {
        int len = edit_strlen(edit_lines[i]);
        for (int j = 0; j < len && offset < sizeof(save_buf) - 2; j++)
            save_buf[offset++] = (uint8_t)edit_lines[i][j];
        save_buf[offset++] = '\n';
    }

    if (fat32_write_file(filename, save_buf, offset)) {
        edit_modified = false;
        return true;
    }
    return false;
}

/*
 * file_load — 从 FAT32 加载文件
 * @filename: 文件名
 */
static bool file_load(const char *filename)
{
    struct fat32_dir_entry *entry = fat32_find_file(filename);
    if (!entry)
        return false;

    static uint8_t load_buf[32768];
    uint32_t size = fat32_load_file(entry, (uint32_t)load_buf);
    if (size == 0)
        return false;

    /* 解析行 */
    edit_line_count = 0;
    int col = 0;

    for (uint32_t i = 0; i < size && edit_line_count < EDIT_MAX_LINES; i++) {
        char c = (char)load_buf[i];

        if (c == '\n') {
            edit_lines[edit_line_count][col] = '\0';
            edit_line_count++;
            col = 0;
        } else if (c == '\r') {
            /* 跳过 */
        } else if (col < EDIT_MAX_COLS - 1) {
            edit_lines[edit_line_count][col++] = c;
        }
    }

    /* 最后一行如果没有换行 */
    if (col > 0 && edit_line_count < EDIT_MAX_LINES) {
        edit_lines[edit_line_count][col] = '\0';
        edit_line_count++;
    }

    if (edit_line_count == 0) {
        edit_lines[0][0] = '\0';
        edit_line_count = 1;
    }

    edit_modified = false;
    return true;
}

/* ===== 命令处理 ===== */

/*
 * editor_show_message — 在命令行显示消息
 * @msg: 消息字符串
 */
static void editor_show_message(const char *msg)
{
    vga_fill_at(24, 0, 80, ' ', COLOR_CMD);
    vga_puts_at(24, 1, msg, COLOR_CMD);
}

/*
 * editor_read_cmdline — 在命令行读取输入
 * @prompt: 提示字符串
 * @buf:    目标缓冲区
 * @max:    最大长度
 * 返回: 输入长度, 0 = 取消。
 */
static int editor_read_cmdline(const char *prompt, char *buf, int max)
{
    vga_fill_at(24, 0, 80, ' ', COLOR_CMD);
    vga_puts_at(24, 1, prompt, COLOR_CMD);

    int col = 1;
    while (prompt[col - 1]) col++;

    int len = 0;
    char c;

    while (1) {
        c = keyboard_getchar();

        if (c == 0x1B) {  /* ESC 取消 */
            buf[0] = '\0';
            return 0;
        }
        if (c == '\r' || c == '\n') {
            buf[len] = '\0';
            return len;
        }
        if ((c == '\b' || c == 0x7F) && len > 0) {
            len--;
            vga_putchar_at(24, col + len, ' ', COLOR_CMD);
            continue;
        }
        if (c >= 32 && c < 127 && len < max - 1) {
            buf[len++] = c;
            vga_putchar_at(24, col + len - 1, c, COLOR_CMD);
        }
    }
}

/*
 * cmd_find — Ctrl+F: 查找文本
 */
static void cmd_find(void)
{
    if (!editor_read_cmdline("Find: ", search_str, sizeof(search_str)))
        return;

    /* 从光标位置开始搜索 */
    int start_row = cursor_row;
    int start_col = cursor_col + 1;

    for (int r = start_row; r < edit_line_count; r++) {
        char *line = edit_lines[r];
        int len = edit_strlen(line);
        int sc = (r == start_row) ? start_col : 0;

        for (int c = sc; c <= len - (int)edit_strlen(search_str); c++) {
            bool match = true;
            for (int k = 0; search_str[k]; k++) {
                if (line[c + k] != search_str[k]) {
                    match = false;
                    break;
                }
            }
            if (match) {
                cursor_row = r;
                cursor_col = c;
                editor_show_message("Found.");
                return;
            }
        }
    }

    editor_show_message("Not found.");
}

/*
 * cmd_goto — Ctrl+G: 跳转到行
 */
static void cmd_goto(void)
{
    char buf[16];
    if (!editor_read_cmdline("Go to line: ", buf, sizeof(buf)))
        return;

    int line = 0;
    for (int i = 0; buf[i]; i++) {
        if (buf[i] >= '0' && buf[i] <= '9')
            line = line * 10 + (buf[i] - '0');
    }

    if (line > 0 && line <= edit_line_count) {
        cursor_row = line - 1;
        cursor_col = 0;
    } else {
        editor_show_message("Invalid line number.");
    }
}

/* ===== 主循环 ===== */

/*
 * editor_main — 文本编辑器主入口
 * @filename: 要打开的文件名 (NULL = 新文件)
 *
 * 初始化编辑器, 加载文件, 进入全屏编辑循环。
 */
void editor_main(const char *filename)
{
    /* 初始化状态 */
    cursor_row = 0;
    cursor_col = 0;
    view_top = 0;
    view_left = 0;
    edit_modified = false;
    edit_running = true;
    edit_line_count = 1;
    edit_lines[0][0] = '\0';
    edit_filename[0] = '\0';

    /* 加载文件 */
    if (filename && filename[0]) {
        int i = 0;
        while (filename[i] && i < 63) {
            edit_filename[i] = filename[i];
            i++;
        }
        edit_filename[i] = '\0';

        if (!file_load(edit_filename)) {
            editor_show_message("New file.");
        }
    }

    /* 清屏 */
    screen_clear();

    /* 初始绘制 */
    draw_screen();
    update_cursor();

    /* 编辑循环 */
    while (edit_running) {
        char c = keyboard_getchar();

        /* VT100 转义序列处理 */
        if (c == 0x1B) {
            /* ESC — 检查是否是转义序列或退出 */
            if (keyboard_available()) {
                c = keyboard_getchar();
                if (c == '[') {
                    c = keyboard_getchar();
                    switch (c) {
                    case 'A': /* 上 */
                        if (cursor_row > 0) {
                            cursor_row--;
                            int len = edit_strlen(edit_lines[cursor_row]);
                            if (cursor_col > len) cursor_col = len;
                        }
                        break;
                    case 'B': /* 下 */
                        if (cursor_row < edit_line_count - 1) {
                            cursor_row++;
                            int len = edit_strlen(edit_lines[cursor_row]);
                            if (cursor_col > len) cursor_col = len;
                        }
                        break;
                    case 'C': /* 右 */
                        {
                            int len = (cursor_row < edit_line_count) ?
                                      edit_strlen(edit_lines[cursor_row]) : 0;
                            if (cursor_col < len) cursor_col++;
                        }
                        break;
                    case 'D': /* 左 */
                        if (cursor_col > 0) cursor_col--;
                        break;
                    case 'H': /* Home */
                        cursor_col = 0;
                        break;
                    case 'F': /* End */
                        cursor_col = edit_strlen(edit_lines[cursor_row]);
                        break;
                    case '5': /* PgUp */
                        keyboard_getchar(); /* 消耗 ~ */
                        cursor_row -= EDIT_SCREEN_LINES;
                        if (cursor_row < 0) cursor_row = 0;
                        {
                            int len = edit_strlen(edit_lines[cursor_row]);
                            if (cursor_col > len) cursor_col = len;
                        }
                        break;
                    case '6': /* PgDn */
                        keyboard_getchar(); /* 消耗 ~ */
                        cursor_row += EDIT_SCREEN_LINES;
                        if (cursor_row >= edit_line_count)
                            cursor_row = edit_line_count - 1;
                        if (cursor_row < 0) cursor_row = 0;
                        {
                            int len = edit_strlen(edit_lines[cursor_row]);
                            if (cursor_col > len) cursor_col = len;
                        }
                        break;
                    }
                }
            } else {
                /* 纯 ESC — 退出 */
                if (edit_modified) {
                    editor_show_message("Unsaved changes! ESC again to quit.");
                    c = keyboard_getchar();
                    if (c != 0x1B) continue;
                }
                edit_running = false;
                continue;
            }
        }
        /* Ctrl 键处理 */
        else if (c == 0x13) {  /* Ctrl+S */
            if (edit_filename[0]) {
                if (file_save(edit_filename))
                    editor_show_message("Saved.");
                else
                    editor_show_message("Save failed!");
            } else {
                if (editor_read_cmdline("Save as: ", edit_filename, 63)) {
                    if (file_save(edit_filename))
                        editor_show_message("Saved.");
                    else
                        editor_show_message("Save failed!");
                }
            }
        }
        else if (c == 0x06) {  /* Ctrl+F */
            cmd_find();
        }
        else if (c == 0x07) {  /* Ctrl+G */
            cmd_goto();
        }
        /* 普通编辑键 */
        else if (c == '\r' || c == '\n') {
            insert_newline();
        }
        else if (c == '\b' || c == 0x7F) {
            backspace_char();
        }
        else if (c == '\t') {
            /* 插入 Tab (空格) */
            for (int i = 0; i < EDIT_TAB_STOP; i++)
                insert_char(' ');
        }
        else if (c >= 32 && c < 127) {
            insert_char(c);
        }

        /* 更新视口 */
        if (cursor_row < view_top)
            view_top = cursor_row;
        if (cursor_row >= view_top + EDIT_SCREEN_LINES)
            view_top = cursor_row - EDIT_SCREEN_LINES + 1;
        if (cursor_col < view_left)
            view_left = cursor_col;
        if (cursor_col >= view_left + (EDIT_SCREEN_COLS - EDIT_LINE_NUM_W))
            view_left = cursor_col - (EDIT_SCREEN_COLS - EDIT_LINE_NUM_W) + 1;

        /* 重绘 */
        draw_screen();
        update_cursor();
    }

    /* 退出: 恢复文本模式 */
    screen_init();
    screen_puts("Editor exited.\n");
}
