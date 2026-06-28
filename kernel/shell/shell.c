/*
 * Nexsteaduser — PlexsDOS
 * 命令行 Shell (32-bit 保护模式)
 * 作者: Tinmc189623 | 团队: Nexlyh
 *
 * 完整的类 DOS 命令行界面, 同时支持 Unix 命令别名。
 * 通过 fs 抽象层统一访问 FAT12 (A:) / FAT32 (C:)。
 * 支持方向键 (VT100 转义序列) 编辑输入行。
 * 日期/时间通过 CMOS 端口 0x70/0x71 读取 MC146818 RTC。
 */

#include <plexsdos/types.h>
#include <plexsdos/config.h>
#include <plexsdos/screen.h>
#include <plexsdos/keyboard.h>
#include <plexsdos/shell.h>
#include <plexsdos/fs.h>
#include <plexsdos/loader.h>
#include <plexsdos/installer.h>
#include <plexsdos/cdrom.h>
#ifndef MINIMAL_KERNEL
#include <plexsdos/desktop.h>
#include <plexsdos/debug.h>
#include <plexsdos/editor.h>
#endif
#include <plexsdos/drive.h>
#include <plexsdos/fat32.h>
#include <plexsdos/scheduler.h>

__attribute__((weak)) extern int plxdm_lightdm_start(void);

/* 命令缓冲区 */
static char cmd_buf[128];

/* 命令历史 */
static char last_cmd[128];

/* 环境变量表 */
#define MAX_ENV_VARS 16
static char env_names[MAX_ENV_VARS][16];
static char env_values[MAX_ENV_VARS][64];
static int  env_count = 0;

/* 自定义提示符 */
static char custom_prompt[32];
static bool use_custom_prompt = false;

/* 搜索路径 */
static char search_path[256] = "";

/* 校验标志 */
static bool verify_flag = false;

/* Ctrl+C 检查标志 */
static bool break_flag = true;

/* MORE 分页状态 */
static bool more_active = false;
static int  more_lines = 0;
#define MORE_PAGE_SIZE 23

/* 当前目录 (始终为根) */
static const char *current_dir = "\\";

/* ===== 字符串辅助 ===== */

static int shell_strcmp(const char *s1, const char *s2)
{
    while (*s1 && *s2) {
        if (*s1 != *s2)
            return (int)*s1 - (int)*s2;
        s1++;
        s2++;
    }
    return (int)*s1 - (int)*s2;
}

static int shell_strlen(const char *s)
{
    int len = 0;
    while (*s++)
        len++;
    return len;
}

static void shell_strcpy(char *dst, const char *src)
{
    while ((*dst++ = *src++) != '\0')
        ;
}

static const char *shell_skip_spaces(const char *s)
{
    while (*s == ' ' || *s == '\t')
        s++;
    return s;
}

static bool shell_strchr(const char *s, char c)
{
    while (*s) {
        if (*s == c) return true;
        s++;
    }
    return false;
}

static int shell_strncmp(const char *s1, const char *s2, int n)
{
    for (int i = 0; i < n; i++) {
        if (s1[i] != s2[i])
            return (int)s1[i] - (int)s2[i];
        if (s1[i] == '\0')
            break;
    }
    return 0;
}

static int shell_stricmp(const char *s1, const char *s2)
{
    while (*s1 && *s2) {
        char c1 = *s1, c2 = *s2;
        if (c1 >= 'a' && c1 <= 'z') c1 -= 32;
        if (c2 >= 'a' && c2 <= 'z') c2 -= 32;
        if (c1 != c2)
            return (int)c1 - (int)c2;
        s1++;
        s2++;
    }
    return (int)*s1 - (int)*s2;
}

/* ===== CMOS/RTC ===== */

static uint8_t cmos_read(uint8_t reg)
{
    uint8_t addr = reg & 0x7F;
    uint8_t val;
    __asm__ __volatile__("outb %0, %1" : : "a"(addr), "Nd"((uint16_t)0x70));
    __asm__ __volatile__("inb %1, %0" : "=a"(val) : "Nd"((uint16_t)0x71));
    return val;
}

static uint8_t bcd_to_bin(uint8_t val)
{
    return (val >> 4) * 10 + (val & 0x0F);
}

/* ===== MORE 分页 ===== */

static void more_check(void)
{
    if (!more_active)
        return;
    more_lines++;
    if (more_lines >= MORE_PAGE_SIZE) {
        screen_set_color(0x08, 0x00);
        screen_puts("-- More -- (Press any key)");
        screen_reset_color();
        keyboard_getchar();
        screen_puts("\r            \r");
        more_lines = 0;
    }
}

static void screen_putchar_more(char c)
{
    screen_putchar(c);
    if (c == '\n')
        more_check();
}

/* ===== DOS / Unix 命令实现 ===== */

/* ---------- 帮助 ---------- */

static void cmd_help(void)
{
    screen_set_color(0x0B, 0x00);
    screen_puts("Nexsteaduser PlexsDOS v0.1 - Command Reference\n\n");
    screen_reset_color();

    screen_set_color(0x0F, 0x00);
    screen_puts("=== DOS Commands ===\n");
    screen_set_color(0x07, 0x00);
    screen_puts("  DIR             List directory contents\n");
    screen_puts("  TYPE <file>     Display file contents\n");
    screen_puts("  COPY <s> <d>    Copy file(s)\n");
    screen_puts("  DEL/ERASE <f>   Delete file\n");
    screen_puts("  REN/RENAME <o> <n>  Rename file\n");
    screen_puts("  MOVE <s> <d>    Move file\n");
    screen_puts("  ATTRIB <file>   Show file attributes\n");
    screen_puts("  CLS             Clear screen\n");
    screen_puts("  VER             Show version\n");
    screen_puts("  VOL             Show volume label\n");
    screen_puts("  LABEL [label]   Change volume label\n");
    screen_puts("  DATE [date]     Show/set date\n");
    screen_puts("  TIME [time]     Show/set time\n");
    screen_puts("  MEM             Show memory info\n");
    screen_puts("  ECHO <text>     Display text\n");
    screen_puts("  PROMPT <text>   Change prompt\n");
    screen_puts("  PATH [path]     Set search path\n");
    screen_puts("  SET [var=val]   Env variables\n");
    screen_puts("  VERIFY [ON|OFF] Verify writes\n");
    screen_puts("  BREAK [ON|OFF]  Ctrl+C check\n");
    screen_puts("  CD/CHDIR [dir]  Show/change dir\n");
    screen_puts("  MD/MKDIR <dir>  Create dir\n");
    screen_puts("  RD/RMDIR <dir>  Remove dir\n");
    screen_puts("  TREE            Directory tree\n");
    screen_puts("  MORE <file>     Paginated output\n");
    screen_puts("  FIND \"str\" <f>  Find string\n");
    screen_puts("  SORT <file>     Sort lines\n");
    screen_puts("  PAUSE           Pause execution\n");
    screen_puts("  REM [comment]   Remark\n");
    screen_puts("  HELP/?          This help\n");
    screen_puts("  EXIT/QUIT       Exit shell\n");
    screen_puts("  REBOOT/RESET    Reboot\n");

    screen_set_color(0x0F, 0x00);
    screen_puts("\n=== Unix Aliases ===\n");
    screen_set_color(0x07, 0x00);
    screen_puts("  ls [dir]        List (=DIR)\n");
    screen_puts("  cat <file>      Display (=TYPE)\n");
    screen_puts("  cp <s> <d>      Copy (=COPY)\n");
    screen_puts("  rm <file>       Delete (=DEL)\n");
    screen_puts("  mv <s> <d>      Move (=MOVE)\n");
    screen_puts("  clear           Clear (=CLS)\n");
    screen_puts("  pwd             Print working dir (=CD)\n");
    screen_puts("  touch <file>    Create empty file\n");
    screen_puts("  head <file>     First lines\n");
    screen_puts("  tail <file>     Last lines\n");
    screen_puts("  wc <file>       Count lines/words\n");
    screen_puts("  grep <p> <f>    Search (=FIND)\n");
    screen_puts("  which <cmd>     Locate command\n");
    screen_puts("  uname -a        System info (=VER)\n");
    screen_puts("  whoami          Show user name\n");
    screen_puts("  hostname        Show host name\n");
    screen_puts("  env             Show env (=SET)\n");
    screen_puts("  sort <file>     Sort lines\n");
    screen_puts("  cmp <f1> <f2>   Compare files\n");
    screen_puts("  diff <f1> <f2>  Diff files\n");
    screen_puts("  chmod <m> <f>   Change attributes\n");
    screen_puts("  df              Disk free space\n");
    screen_puts("  du [dir]        Disk usage\n");
    screen_puts("  ps              Process list\n");
    screen_puts("  kill <pid>      Kill process\n");
    screen_puts("  ln <s> <d>      Link (unsupported)\n");

    screen_set_color(0x0F, 0x00);
    screen_puts("\n=== PlexsDOS Extended ===\n");
    screen_set_color(0x07, 0x00);
    screen_puts("  RUN <file>      Execute .COMX/.EXE/.COM\n");
    screen_puts("  <file>.COM      Auto-execute .COM\n");
    screen_puts("  <file>.EXE      Auto-execute .EXE\n");
    screen_puts("  DRIVE           Show drives\n");
    screen_puts("  X:              Switch drive\n");
    screen_puts("  CDIR            CD-ROM list\n");
    screen_puts("  CCAT <file>     CD-ROM cat\n");
    screen_puts("  CDMOUNT         Mount CD\n");
    screen_puts("  CDEJECT         Eject CD\n");
    screen_puts("  CDINFO          CD info\n");
    screen_puts("  DM              Display Manager\n");
#ifndef MINIMAL_KERNEL
    screen_puts("  EDIT <file>     Text editor\n");
    screen_puts("  GUI             Desktop GUI\n");
    screen_puts("  DEBUG           Debugger\n");
#endif
    screen_puts("  INSTALL         Install to disk\n");
    screen_reset_color();
    screen_putchar('\n');
}

/* ---------- DIR / ls ---------- */

static void cmd_dir(const char *args)
{
    (void)args;
    screen_set_color(0x0B, 0x00);
    screen_putchar(' ');
    screen_putchar(fs_get_current_drive());
    screen_puts(":\\> Directory listing\n\n");
    screen_reset_color();
    fs_list_root();
}

static void cmd_ls(const char *args)
{
    (void)args;
    fs_list_root();
}

/* ---------- TYPE / cat ---------- */

static void cmd_type(const char *filename)
{
    struct fs_entry *entry;
    uint8_t buf[512];

    if (*filename == '\0') {
        screen_puts("Required parameter missing\n");
        return;
    }

    entry = fs_find_file(filename);
    if (!entry) {
        screen_puts("File not found - ");
        screen_puts(filename);
        screen_putchar('\n');
        return;
    }

    uint32_t size = fs_load_file(entry, (uint32_t)buf);
    if (size == 0) {
        screen_puts("(empty)\n");
        return;
    }

    for (uint32_t i = 0; i < size && i < sizeof(buf); i++) {
        char c = (char)buf[i];
        if (c >= 32 && c <= 126)
            screen_putchar(c);
        else if (c == '\n')
            screen_putchar('\n');
        else if (c == '\t')
            screen_putchar('\t');
    }
    screen_putchar('\n');
}

/* ---------- COPY / cp ---------- */

static void cmd_copy(const char *args)
{
    char src[64], dst[64];
    const char *p = shell_skip_spaces(args);
    int i = 0;

    if (*p == '\0') { screen_puts("COPY: Missing source\n"); return; }
    while (*p && *p != ' ' && *p != '\t' && i < 63) src[i++] = *p++;
    src[i] = '\0';

    p = shell_skip_spaces(p);
    i = 0;
    if (*p == '\0') { screen_puts("COPY: Missing destination\n"); return; }
    while (*p && *p != ' ' && *p != '\t' && i < 63) dst[i++] = *p++;
    dst[i] = '\0';

    struct fs_entry *entry = fs_find_file(src);
    if (!entry) { screen_puts("COPY: Source not found\n"); return; }

    uint32_t size = entry->file_size;
    if (size > 65536) { screen_puts("COPY: File too large\n"); return; }

    if (size == 0) {
        if (fs_write_file(dst, NULL, 0))
            screen_puts("COPY: 1 file(s) copied.\n");
        else
            screen_puts("COPY: Write error\n");
        return;
    }

    static uint8_t buf[65536];
    if (fs_load_file(entry, (uint32_t)buf) != size) {
        screen_puts("COPY: Read error\n");
        return;
    }

    if (!fs_write_file(dst, buf, size)) {
        screen_puts("COPY: Write error\n");
        return;
    }

    screen_puts("COPY: 1 file(s) copied.\n");
}

/* ---------- DEL/ERASE / rm ---------- */

static void cmd_del(const char *args)
{
    const char *name = shell_skip_spaces(args);
    if (*name == '\0') { screen_puts("DEL: Missing filename\n"); return; }

    if (fs_delete_file(name))
        screen_puts("DEL: File deleted.\n");
    else
        screen_puts("DEL: File not found\n");
}

/* ---------- REN/RENAME ---------- */

static void cmd_rename(const char *args)
{
    char old[64], newn[64];
    const char *p = shell_skip_spaces(args);
    int i = 0;

    if (*p == '\0') { screen_puts("RENAME: Missing name\n"); return; }
    while (*p && *p != ' ' && *p != '\t' && i < 63) old[i++] = *p++;
    old[i] = '\0';

    p = shell_skip_spaces(p);
    i = 0;
    if (*p == '\0') { screen_puts("RENAME: Missing new name\n"); return; }
    while (*p && *p != ' ' && *p != '\t' && i < 63) newn[i++] = *p++;
    newn[i] = '\0';

    if (fs_rename_file(old, newn))
        screen_puts("RENAME: File renamed.\n");
    else
        screen_puts("RENAME: File not found\n");
}

/* ---------- MOVE / mv ---------- */

static void cmd_move(const char *args)
{
    char src[64], dst[64];
    const char *p = shell_skip_spaces(args);
    int i = 0;

    if (*p == '\0') { screen_puts("MOVE: Missing source\n"); return; }
    while (*p && *p != ' ' && *p != '\t' && i < 63) src[i++] = *p++;
    src[i] = '\0';

    p = shell_skip_spaces(p);
    i = 0;
    if (*p == '\0') { screen_puts("MOVE: Missing destination\n"); return; }
    while (*p && *p != ' ' && *p != '\t' && i < 63) dst[i++] = *p++;
    dst[i] = '\0';

    struct fs_entry *entry = fs_find_file(src);
    if (!entry) { screen_puts("MOVE: Source not found\n"); return; }

    uint32_t size = entry->file_size;
    if (size > 65536) { screen_puts("MOVE: File too large\n"); return; }

    if (size > 0) {
        static uint8_t buf[65536];
        if (fs_load_file(entry, (uint32_t)buf) != size) {
            screen_puts("MOVE: Read error\n");
            return;
        }
        if (!fs_write_file(dst, buf, size)) {
            screen_puts("MOVE: Write error\n");
            return;
        }
    } else {
        if (!fs_write_file(dst, NULL, 0)) {
            screen_puts("MOVE: Write error\n");
            return;
        }
    }

    fs_delete_file(src);
    screen_puts("MOVE: 1 file(s) moved.\n");
}

/* ---------- ATTRIB ---------- */

static void cmd_attrib(const char *args)
{
    const char *p = shell_skip_spaces(args);
    if (*p == '\0') {
        screen_puts("ATTRIB: Missing filename\n");
        return;
    }

    struct fs_entry *entry = fs_find_file(p);
    if (!entry) { screen_puts("ATTRIB: File not found\n"); return; }

    screen_putchar((entry->attr & 0x01) ? 'R' : '-');
    screen_putchar((entry->attr & 0x02) ? 'H' : '-');
    screen_putchar((entry->attr & 0x04) ? 'S' : '-');
    screen_putchar((entry->attr & 0x20) ? 'A' : '-');
    screen_puts("  ");
    screen_puts(p);
    screen_putchar('\n');
}

/* ---------- CLS / clear ---------- */

static void cmd_cls(void)
{
    screen_clear();
}

/* ---------- VER / uname ---------- */

static void cmd_ver(void)
{
    screen_puts("Nexsteaduser PlexsDOS v0.1\n");
    screen_puts("Author: Tinmc189623 | Team: Nexlyh\n");
    screen_puts("Kernel: 32-bit protected mode\n");
    screen_puts("x86 i686, self-written from scratch\n\n");
}

static void cmd_uname(const char *args)
{
    (void)args;
    screen_puts("PlexsDOS x86_32 0.1\n");
}

/* ---------- VOL ---------- */

static void cmd_vol(void)
{
    char label[12];
    screen_putchar(' ');
    screen_putchar(fs_get_current_drive());
    screen_puts(": is ");
    if (fs_get_volume_label(label))
        screen_puts(label);
    else
        screen_puts("NO LABEL");
    screen_puts("\n File system: ");
    screen_puts(fs_get_type());
    screen_putchar('\n');
}

/* ---------- LABEL ---------- */

static void cmd_label(const char *args)
{
    const char *p = shell_skip_spaces(args);
    if (*p == '\0') {
        char label[12];
        if (fs_get_volume_label(label)) {
            screen_puts("Volume label: ");
            screen_puts(label);
            screen_putchar('\n');
        }
        screen_puts("Usage: LABEL <new_label>\n");
        return;
    }

    if (fs_set_volume_label(p))
        screen_puts("Volume label changed.\n");
    else
        screen_puts("Volume label change not supported.\n");
}

/* ---------- ECHO ---------- */

static void cmd_echo(const char *args)
{
    screen_puts(args);
    screen_putchar('\n');
}

/* ---------- DATE ---------- */

static void cmd_date(const char *args)
{
    (void)args;
    uint8_t d = bcd_to_bin(cmos_read(0x07));
    uint8_t m = bcd_to_bin(cmos_read(0x08));
    uint8_t y = bcd_to_bin(cmos_read(0x09));
    screen_puts("Current date: 20");
    screen_put_dec(y);
    screen_putchar('-');
    screen_put_dec(m);
    screen_putchar('-');
    screen_put_dec(d);
    screen_putchar('\n');
}

/* ---------- TIME ---------- */

static void cmd_time(const char *args)
{
    (void)args;
    uint8_t s = bcd_to_bin(cmos_read(0x00));
    uint8_t m = bcd_to_bin(cmos_read(0x02));
    uint8_t h = bcd_to_bin(cmos_read(0x04));
    screen_puts("Current time: ");
    screen_put_dec(h);
    screen_putchar(':');
    screen_put_dec(m);
    screen_putchar(':');
    screen_put_dec(s);
    screen_putchar('\n');
}

/* ---------- MEM ---------- */

static void cmd_mem(void)
{
    screen_puts("\nMemory:\n");
    screen_puts("  Kernel code:  0x30000 - 0x60000\n");
    screen_puts("  BSS/Stack:    0x62000 - 0x90000\n");
    screen_puts("  Heap:         ~0x90000 - 0x110000\n");
    screen_puts("  Page frames:  0x100000 - 0x800000 (128 MB)\n\n");
}

/* ---------- MORE ---------- */

static void cmd_more(const char *args)
{
    const char *f = shell_skip_spaces(args);
    if (*f == '\0') { screen_puts("MORE: Missing filename\n"); return; }

    more_active = true;
    more_lines = 0;
    cmd_type(f);
    more_active = false;
}

/* ---------- FIND / grep ---------- */

static void cmd_find(const char *args)
{
    const char *p = shell_skip_spaces(args);
    char str[64], fname[64];
    int i;

    if (*p != '"') { screen_puts("FIND: Quoted string required\n"); return; }
    p++;
    for (i = 0; *p && *p != '"' && i < 63;) str[i++] = *p++;
    str[i] = '\0';
    if (*p == '"') p++;

    p = shell_skip_spaces(p);
    if (*p == '\0') { screen_puts("FIND: Missing filename\n"); return; }
    for (i = 0; *p && *p != ' ' && i < 63;) fname[i++] = *p++;
    fname[i] = '\0';

    struct fs_entry *e = fs_find_file(fname);
    if (!e) { screen_puts("FIND: File not found\n"); return; }

    static uint8_t buf[16384];
    if (e->file_size == 0 || e->file_size > sizeof(buf))
        { screen_puts("FIND: Empty or too large\n"); return; }

    if (fs_load_file(e, (uint32_t)buf) != e->file_size)
        { screen_puts("FIND: Read error\n"); return; }

    int sl = shell_strlen(str);
    int match = 0;
    uint32_t ls = 0;
    int ln = 0;

    for (uint32_t j = 0; j <= e->file_size; j++) {
        if (j == e->file_size || buf[j] == '\n') {
            ln++;
            int ll = (int)(j - ls);
            if (j < e->file_size && buf[j] == '\n') ll--;

            for (int k = 0; k <= ll - sl; k++) {
                bool found = true;
                for (int m = 0; m < sl; m++) {
                    char ca = (char)buf[ls + k + m];
                    char cb = str[m];
                    if (ca >= 'a' && ca <= 'z') ca -= 32;
                    if (cb >= 'a' && cb <= 'z') cb -= 32;
                    if (ca != cb) { found = false; break; }
                }
                if (found) {
                    screen_put_dec(ln); screen_puts(": ");
                    for (uint32_t c = ls; c < j && c < ls + 256; c++) {
                        char ch = (char)buf[c];
                        if (ch >= 32 && ch <= 126) screen_putchar(ch);
                    }
                    screen_putchar('\n');
                    match++;
                    break;
                }
            }
            ls = j + 1;
        }
    }

    screen_puts("\nFIND: ");
    screen_put_dec(match);
    screen_puts(" match(es)\n");
}

/* ---------- SORT ---------- */

static void cmd_sort(const char *args)
{
    const char *f = shell_skip_spaces(args);
    if (*f == '\0') { screen_puts("SORT: Missing filename\n"); return; }

    struct fs_entry *e = fs_find_file(f);
    if (!e) { screen_puts("SORT: File not found\n"); return; }

    static uint8_t buf[16384];
    if (e->file_size == 0 || e->file_size > sizeof(buf))
        { screen_puts("SORT: Empty or too large\n"); return; }
    if (fs_load_file(e, (uint32_t)buf) != e->file_size)
        { screen_puts("SORT: Read error\n"); return; }

    #define MAX_L 512
    const char *lines[MAX_L];
    int cnt = 0;
    uint32_t ls = 0;

    for (uint32_t j = 0; j <= e->file_size && cnt < MAX_L; j++) {
        if (j == e->file_size || buf[j] == '\n') {
            buf[j] = '\0';
            lines[cnt++] = (const char *)(buf + ls);
            ls = j + 1;
        }
    }

    for (int i = 0; i < cnt - 1; i++) {
        for (int j = 0; j < cnt - 1 - i; j++) {
            int cmp = 0;
            const char *a = lines[j], *b = lines[j+1];
            while (*a && *b && cmp == 0) {
                char ca = *a, cb = *b;
                if (ca >= 'a' && ca <= 'z') ca -= 32;
                if (cb >= 'a' && cb <= 'z') cb -= 32;
                cmp = (int)ca - (int)cb; a++; b++;
            }
            if (cmp > 0 || (cmp == 0 && *a && !*b)) {
                const char *t = lines[j]; lines[j] = lines[j+1]; lines[j+1] = t;
            }
        }
    }

    for (int i = 0; i < cnt; i++) {
        screen_puts(lines[i]);
        screen_putchar('\n');
    }
}

/* ---------- PROMPT ---------- */

static void cmd_prompt(const char *args)
{
    const char *p = shell_skip_spaces(args);
    if (*p == '\0') {
        use_custom_prompt = false;
        screen_puts("PROMPT: Reset to default\n");
        return;
    }

    int out = 0;
    while (*p && out < 30) {
        if (*p == '$' && *(p+1)) {
            p++;
            switch (*p) {
            case 'P': case 'p':
                custom_prompt[out++] = fs_get_current_drive();
                custom_prompt[out++] = ':';
                custom_prompt[out++] = '\\';
                break;
            case 'N': case 'n':
                custom_prompt[out++] = fs_get_current_drive();
                break;
            case 'G': case 'g': custom_prompt[out++] = '>'; break;
            case 'L': case 'l': custom_prompt[out++] = '<'; break;
            case 'Q': case 'q': custom_prompt[out++] = '='; break;
            case '_': custom_prompt[out++] = '\n'; break;
            case 'D': case 'd':
                custom_prompt[out++] = 'D'; /* date placeholder */
                break;
            case 'T': case 't':
                custom_prompt[out++] = 'T'; /* time placeholder */
                break;
            default: custom_prompt[out++] = *p; break;
            }
        } else {
            custom_prompt[out++] = *p;
        }
        p++;
    }
    custom_prompt[out] = '\0';
    use_custom_prompt = true;
}

/* ---------- PATH ---------- */

static void cmd_path(const char *args)
{
    const char *p = shell_skip_spaces(args);
    if (*p == '\0') {
        if (search_path[0])
            { screen_puts("PATH: "); screen_puts(search_path); screen_putchar('\n'); }
        else
            screen_puts("PATH: No path\n");
        return;
    }
    shell_strcpy(search_path, p);
    screen_puts("PATH set.\n");
}

/* ---------- SET / env ---------- */

static void cmd_set(const char *args)
{
    const char *p = shell_skip_spaces(args);
    if (*p == '\0') {
        if (env_count == 0)
            screen_puts("(no variables)\n");
        else for (int i = 0; i < env_count; i++) {
            screen_puts(env_names[i]);
            screen_putchar('=');
            screen_puts(env_values[i]);
            screen_putchar('\n');
        }
        return;
    }

    char name[16];
    int i = 0;
    while (*p && *p != '=' && i < 15) name[i++] = *p++;
    name[i] = '\0';
    if (*p != '=') { screen_puts("SET: Use VAR=value\n"); return; }
    p++;

    int idx = -1;
    for (int j = 0; j < env_count; j++)
        if (shell_strcmp(env_names[j], name) == 0) { idx = j; break; }

    if (idx < 0) {
        if (env_count >= MAX_ENV_VARS) { screen_puts("SET: Environment full\n"); return; }
        idx = env_count++;
        shell_strcpy(env_names[idx], name);
    }
    shell_strcpy(env_values[idx], p);
}

/* ---------- VERIFY ---------- */

static void cmd_verify(const char *args)
{
    const char *p = shell_skip_spaces(args);
    if (*p == '\0') {
        screen_puts("VERIFY is ");
        screen_puts(verify_flag ? "ON" : "OFF");
        screen_putchar('\n');
        return;
    }
    if (shell_stricmp(p, "ON") == 0) { verify_flag = true; screen_puts("VERIFY ON\n"); }
    else if (shell_stricmp(p, "OFF") == 0) { verify_flag = false; screen_puts("VERIFY OFF\n"); }
    else screen_puts("VERIFY: Use ON or OFF\n");
}

/* ---------- BREAK ---------- */

static void cmd_break(const char *args)
{
    const char *p = shell_skip_spaces(args);
    if (*p == '\0') {
        screen_puts("BREAK is ");
        screen_puts(break_flag ? "ON" : "OFF");
        screen_putchar('\n');
        return;
    }
    if (shell_stricmp(p, "ON") == 0) { break_flag = true; screen_puts("BREAK ON\n"); }
    else if (shell_stricmp(p, "OFF") == 0) { break_flag = false; screen_puts("BREAK OFF\n"); }
    else screen_puts("BREAK: Use ON or OFF\n");
}

/* ---------- PAUSE ---------- */

static void cmd_pause(void)
{
    screen_puts("Press any key to continue...");
    keyboard_getchar();
    screen_putchar('\n');
}

/* ---------- REM ---------- */

static void cmd_rem(const char *args)
{
    (void)args;
}

/* ---------- CD/CHDIR / pwd ---------- */

static void cmd_cd(const char *args)
{
    const char *p = shell_skip_spaces(args);
    if (*p == '\0' || *p == '\\') {
        screen_putchar(fs_get_current_drive());
        screen_puts(":\\\n");
        return;
    }
    screen_puts("CD: Subdirs not supported (root only)\n");
}

static void cmd_pwd(void)
{
    screen_putchar(fs_get_current_drive());
    screen_puts(":\\\n");
}

/* ---------- MD/MKDIR ---------- */

static void cmd_mkdir(const char *args)
{
    const char *p = shell_skip_spaces(args);
    if (*p == '\0') {
        screen_puts("MKDIR: Missing directory name\n");
        return;
    }
    char dirname[13];
    int i = 0;
    while (*p && *p != ' ' && i < 12)
        dirname[i++] = *p++;
    dirname[i] = '\0';

    if (fat32_create_dir(dirname)) {
        screen_puts("Directory created\n");
    } else {
        screen_puts("MKDIR: Failed to create directory\n");
    }
}

/* ---------- RD/RMDIR ---------- */

static void cmd_rmdir(const char *args)
{
    (void)args;
    screen_puts("RMDIR: Subdirs not supported (root only)\n");
}

/* ---------- TREE ---------- */

static void cmd_tree(void)
{
    screen_putchar(fs_get_current_drive());
    screen_puts(":\\\n");
    screen_puts("  (root - no subdirectories)\n");
}

/* ---------- TOUCH ---------- */

static void cmd_touch(const char *args)
{
    const char *f = shell_skip_spaces(args);
    if (*f == '\0') { screen_puts("TOUCH: Missing filename\n"); return; }
    if (fs_write_file(f, NULL, 0))
        screen_puts("TOUCH: File created.\n");
    else
        screen_puts("TOUCH: Failed.\n");
}

/* ---------- HEAD / TAIL ---------- */

static void cmd_head(const char *args)
{
    const char *f = shell_skip_spaces(args);
    if (*f == '\0') { screen_puts("HEAD: Missing filename\n"); return; }

    struct fs_entry *e = fs_find_file(f);
    if (!e) { screen_puts("HEAD: File not found\n"); return; }

    static uint8_t buf[16384];
    if (e->file_size > sizeof(buf))
        { screen_puts("HEAD: File too large\n"); return; }
    if (fs_load_file(e, (uint32_t)buf) != e->file_size)
        { screen_puts("HEAD: Read error\n"); return; }

    int lines = 0;
    for (uint32_t j = 0; j < e->file_size && lines < 10; j++) {
        screen_putchar((char)buf[j]);
        if (buf[j] == '\n') lines++;
    }
    if (lines < 10 && e->file_size > 0) screen_putchar('\n');
}

static void cmd_tail(const char *args)
{
    const char *f = shell_skip_spaces(args);
    if (*f == '\0') { screen_puts("TAIL: Missing filename\n"); return; }

    struct fs_entry *e = fs_find_file(f);
    if (!e) { screen_puts("TAIL: File not found\n"); return; }

    static uint8_t buf[16384];
    if (e->file_size > sizeof(buf))
        { screen_puts("TAIL: File too large\n"); return; }
    if (fs_load_file(e, (uint32_t)buf) != e->file_size)
        { screen_puts("TAIL: Read error\n"); return; }

    /* 找到倒数第 10 行的起始位置 */
    int lines = 0;
    uint32_t start = 0;
    if (e->file_size > 0) {
        for (uint32_t j = e->file_size - 1; j > 0; j--) {
            if (buf[j] == '\n') {
                lines++;
                if (lines == 10) { start = j + 1; break; }
            }
        }
    }

    for (uint32_t j = start; j < e->file_size; j++)
        screen_putchar((char)buf[j]);
    if (e->file_size > 0) screen_putchar('\n');
}

/* ---------- WC ---------- */

static void cmd_wc(const char *args)
{
    const char *f = shell_skip_spaces(args);
    if (*f == '\0') { screen_puts("WC: Missing filename\n"); return; }

    struct fs_entry *e = fs_find_file(f);
    if (!e) { screen_puts("WC: File not found\n"); return; }

    static uint8_t buf[16384];
    if (e->file_size > sizeof(buf))
        { screen_puts("WC: File too large\n"); return; }
    if (fs_load_file(e, (uint32_t)buf) != e->file_size)
        { screen_puts("WC: Read error\n"); return; }

    int lines = 0, words = 0, chars = (int)e->file_size;
    bool in_word = false;

    for (uint32_t j = 0; j < e->file_size; j++) {
        char c = (char)buf[j];
        if (c == '\n') lines++;
        if (c == ' ' || c == '\t' || c == '\n') {
            in_word = false;
        } else if (!in_word) {
            words++;
            in_word = true;
        }
    }

    screen_put_dec(lines);
    screen_putchar(' ');
    screen_put_dec(words);
    screen_putchar(' ');
    screen_put_dec(chars);
    screen_puts(" ");
    screen_puts(f);
    screen_putchar('\n');
}

/* ---------- CMP ---------- */

static void cmd_cmp(const char *args)
{
    char f1[64], f2[64];
    const char *p = shell_skip_spaces(args);
    int i = 0;

    if (*p == '\0') { screen_puts("CMP: Missing file1\n"); return; }
    while (*p && *p != ' ' && i < 63) f1[i++] = *p++;
    f1[i] = '\0';

    p = shell_skip_spaces(p);
    i = 0;
    if (*p == '\0') { screen_puts("CMP: Missing file2\n"); return; }
    while (*p && *p != ' ' && i < 63) f2[i++] = *p++;
    f2[i] = '\0';

    struct fs_entry *e1 = fs_find_file(f1);
    struct fs_entry *e2 = fs_find_file(f2);
    if (!e1 || !e2) { screen_puts("CMP: File not found\n"); return; }
    if (e1->file_size != e2->file_size)
        { screen_puts("CMP: Files differ (size)\n"); return; }

    static uint8_t b1[8192], b2[8192];
    uint32_t s = e1->file_size;
    if (s > sizeof(b1)) { screen_puts("CMP: File too large\n"); return; }

    fs_load_file(e1, (uint32_t)b1);
    fs_load_file(e2, (uint32_t)b2);

    bool same = true;
    for (uint32_t j = 0; j < s; j++) {
        if (b1[j] != b2[j]) {
            screen_puts("CMP: Files differ at byte ");
            screen_put_dec(j);
            screen_putchar('\n');
            same = false;
            break;
        }
    }
    if (same) screen_puts("CMP: Files are identical.\n");
}

/* ---------- WHICH ---------- */

static void cmd_which(const char *args)
{
    const char *name = shell_skip_spaces(args);
    if (*name == '\0') { screen_puts("WHICH: Missing command\n"); return; }

    /* 检查是否为内置命令 */
    struct { const char *n; } builtins[] = {
        {"HELP"}, {"DIR"}, {"TYPE"}, {"COPY"}, {"DEL"}, {"ERASE"},
        {"REN"}, {"RENAME"}, {"MOVE"}, {"ATTRIB"}, {"CLS"}, {"CLEAR"},
        {"VER"}, {"VOL"}, {"LABEL"}, {"ECHO"}, {"DATE"}, {"TIME"},
        {"MEM"}, {"MORE"}, {"FIND"}, {"SORT"}, {"PROMPT"}, {"PATH"},
        {"SET"}, {"VERIFY"}, {"BREAK"}, {"PAUSE"}, {"REM"}, {"CD"},
        {"CHDIR"}, {"MD"}, {"MKDIR"}, {"RD"}, {"RMDIR"}, {"TREE"},
        {"LS"}, {"CAT"}, {"CP"}, {"RM"}, {"MV"}, {"CLEAR"}, {"PWD"},
        {"TOUCH"}, {"HEAD"}, {"TAIL"}, {"WC"}, {"GREP"}, {"WHICH"},
        {"UNAME"}, {"WHOAMI"}, {"HOSTNAME"}, {"ENV"}, {"CMP"}, {"DIFF"},
        {"RUN"}, {"DRIVE"}, {"INSTALL"}, {"CDIR"}, {"CCAT"},
        {"CDMOUNT"}, {"CDEJECT"}, {"CDINFO"}, {"DM"}, {"GUI"},
        {"DEBUG"}, {"DBG"}, {"EDIT"}, {"ED"}, {"REBOOT"}, {"RESET"},
        {"EXIT"}, {"QUIT"}, {NULL}
    };

    /* 将命令名转大写 */
    char upper[16];
    int i = 0;
    while (name[i] && i < 15) {
        upper[i] = name[i];
        if (upper[i] >= 'a' && upper[i] <= 'z') upper[i] -= 32;
        i++;
    }
    upper[i] = '\0';

    for (int j = 0; builtins[j].n; j++) {
        if (shell_strcmp(upper, builtins[j].n) == 0) {
            screen_puts("WHICH: ");
            screen_puts(name);
            screen_puts(" is a built-in command\n");
            return;
        }
    }

    screen_puts("WHICH: ");
    screen_puts(name);
    screen_puts(" not found\n");
}

/* ---------- WHOAMI ---------- */

static void cmd_whoami(void)
{
    screen_puts("user\n");
}

/* ---------- HOSTNAME ---------- */

static void cmd_hostname(void)
{
    screen_puts("PlexsDOS\n");
}

/* ---------- DIFF ---------- */

static void cmd_diff(const char *args)
{
    cmd_cmp(args);
}

/* ---------- PlexsDOS 扩展 ---------- */

static void cmd_run(const char *f)
{
    if (*f == '\0') { screen_puts("RUN: Missing filename\n"); return; }
    loader_run(f);
}

static void cmd_install(void)
{
    installer_run();
}

static void cmd_cdir(void)
{
    const struct iso9660_fs *fs = iso9660_get_fs();
    if (!fs || !fs->mounted) {
        screen_puts("No CD-ROM mounted. Use CDMOUNT first.\n");
        return;
    }
    const struct iso9660_entry *root = iso9660_get_root();
    if (!root) { screen_puts("Error reading root dir.\n"); return; }

    struct iso9660_entry entries[64];
    int cnt = iso9660_read_dir(root->lba, root->size, entries, 64);

    screen_set_color(0x0B, 0x00);
    screen_puts("Volume: "); screen_puts(fs->volume_id);
    screen_puts("\nCD-ROM directory:\n\n");
    screen_reset_color();

    for (int i = 0; i < cnt; i++) {
        if (entries[i].is_directory) {
            screen_set_color(0x0E, 0x00);
            screen_puts("[DIR]  ");
        } else {
            screen_set_color(0x07, 0x00);
            screen_puts("       ");
        }
        screen_puts(entries[i].name);
        if (!entries[i].is_directory) {
            screen_puts("  ");
            screen_put_dec(entries[i].size);
            screen_puts(" bytes");
        }
        screen_putchar('\n');
    }
    screen_set_color(0x07, 0x00);
    screen_putchar('\n');
    screen_put_dec(cnt);
    screen_puts(" file(s)\n");
}

static void cmd_ccat(const char *f)
{
    if (*f == '\0') { screen_puts("CCAT: Missing filename\n"); return; }

    const struct iso9660_fs *fs = iso9660_get_fs();
    if (!fs || !fs->mounted) { screen_puts("No CD-ROM mounted.\n"); return; }

    const struct iso9660_entry *root = iso9660_get_root();
    struct iso9660_entry entries[64];
    int cnt = iso9660_read_dir(root->lba, root->size, entries, 64);

    struct iso9660_entry *found = NULL;
    for (int i = 0; i < cnt; i++) {
        const char *a = entries[i].name;
        const char *b = f;
        bool match = true;
        while (*a && *b) {
            char ca = *a, cb = *b;
            if (ca >= 'A' && ca <= 'Z') ca += 32;
            if (cb >= 'A' && cb <= 'Z') cb += 32;
            if (ca != cb) { match = false; break; }
            a++; b++;
        }
        if (match && *a == *b && !entries[i].is_directory) {
            found = &entries[i];
            break;
        }
    }

    if (!found) { screen_puts("CCAT: File not found\n"); return; }

    static uint8_t buf[4096];
    uint32_t sz = iso9660_read_file(found->lba, found->size, buf, sizeof(buf));
    for (uint32_t i = 0; i < sz; i++) {
        char c = (char)buf[i];
        if (c >= 32 && c <= 126) screen_putchar(c);
        else if (c == '\n') screen_putchar('\n');
        else if (c == '\t') screen_putchar('\t');
    }
    screen_putchar('\n');
}

static void cmd_cdmount(void)
{
    const struct cdrom_info *info = cdrom_get_info();
    if (!info->present) { screen_puts("No CD-ROM device.\n"); return; }
    if (iso9660_mount()) screen_puts("CD-ROM mounted.\n");
    else screen_puts("Mount failed.\n");
}

static void cmd_cdeject(void)
{
    cdrom_eject();
}

static void cmd_cdinfo(void)
{
    const struct cdrom_info *info = cdrom_get_info();
    if (!info->present) { screen_puts("No CD-ROM.\n"); return; }

    screen_set_color(0x0B, 0x00);
    screen_puts("CD-ROM Info:\n\n");
    screen_set_color(0x0F, 0x00);
    screen_puts("  Vendor:  "); screen_puts(info->vendor); screen_putchar('\n');
    screen_puts("  Product: "); screen_puts(info->product); screen_putchar('\n');
    screen_puts("  Rev:     "); screen_puts(info->revision); screen_putchar('\n');
    screen_puts("  Size:    "); screen_put_dec(info->total_sectors); screen_puts(" sectors\n");

    const struct iso9660_fs *fs = iso9660_get_fs();
    if (fs && fs->mounted) {
        screen_set_color(0x0A, 0x00);
        screen_puts("\n  ISO 9660 mounted\n");
        screen_puts("  Volume:  "); screen_puts(fs->volume_id); screen_putchar('\n');
    }
    screen_reset_color();
    screen_putchar('\n');
}

/*
 * cmd_format — FORMAT: 格式化当前硬盘分区 (FAT32)
 *
 * 警告用户后将擦除所有数据, 重新初始化 FAT32 文件系统。
 * 仅支持 C: (硬盘), 不适用于软盘或 CD-ROM。
 */
static void cmd_format(void)
{
    char cur_letter = fs_get_current_drive();
    int cur_idx = cur_letter - 'A';
    const struct drive_info *info = drive_get_info(cur_idx);

    if (!info || info->type != DRIVE_TYPE_HDD) {
        screen_puts("FORMAT only supports hard disk (C:).\n");
        return;
    }

    screen_set_color(0x0C, 0x00);
    screen_puts("WARNING: Formatting will ERASE ALL DATA on this drive!\n");
    screen_reset_color();

    screen_puts("Continue? (Y/N): ");
    char c = keyboard_getchar();
    screen_putchar(c);
    screen_putchar('\n');

    if (c != 'Y' && c != 'y') {
        screen_puts("Format cancelled.\n");
        return;
    }

    screen_puts("Formatting drive ");
    screen_putchar(drive_letter_to_char(cur_idx));
    screen_puts(":...\n");

    if (fat32_format(info->partition_lba)) {
        screen_puts("\nFormat complete. Re-mounting filesystem...\n");
        if (!fat32_init_drive(info->partition_lba)) {
            screen_puts("ERROR: Failed to re-mount filesystem.\n");
        }
    } else {
        screen_puts("\nFormat failed!\n");
    }
}

static void cmd_dm(void)
{
    screen_puts("Starting Display Manager...\n");
    plxdm_lightdm_start();
    screen_init();
    screen_puts("Display Manager exited.\n");
}

#ifndef MINIMAL_KERNEL
static void cmd_gui(void)
{
    screen_puts("Starting GUI...\n");
    if (desktop_init()) {
        desktop_run();
        desktop_shutdown();
        screen_init();
        screen_puts("GUI exited.\n");
    } else {
        screen_puts("GUI init failed.\n");
    }
}

static void cmd_debug(void)
{
    debug_main();
    screen_init();
    screen_puts("Debug exited.\n");
}

static void cmd_edit(const char *f)
{
    editor_main(f);
}
#endif

static void cmd_drive(void)
{
    int cur = drive_get_current();
    screen_set_color(0x0B, 0x00);
    screen_puts("Drive  Type        Status\n");
    screen_puts("-----  ----------  ------\n");
    screen_reset_color();

    for (int i = 0; i < DRIVE_MAX; i++) {
        const struct drive_info *info = drive_get_info(i);
        if (!info || info->type == DRIVE_TYPE_NONE) continue;
        screen_putchar(' ');
        screen_putchar(drive_letter_to_char(i));
        screen_puts(":    ");
        const char *tn = drive_get_type_name(info->type);
        screen_puts(tn);
        for (int j = shell_strlen(tn); j < 10; j++) screen_putchar(' ');
        if (i == cur) { screen_set_color(0x0A, 0x00); screen_puts("(current)"); screen_reset_color(); }
        screen_putchar('\n');
    }
    screen_putchar('\n');
}

static void cmd_switch_drive(char letter)
{
    if (letter >= 'a' && letter <= 'z') letter -= 32;
    if (fs_set_current_drive(letter)) {
        screen_putchar(letter);
        screen_puts(":\\>\n");
    } else {
        screen_putchar(letter);
        screen_puts(": Not available.\n");
    }
}

/*
 * cmd_chmod — 修改文件属性 (Unix 兼容)
 */
static void cmd_chmod(const char *args)
{
    if (*args == '\0') {
        screen_puts("Usage: CHMOD <mode> <file>\n");
        return;
    }
    cmd_attrib(args);
}

/*
 * cmd_df — 磁盘空间 (Unix 兼容)
 */
static void cmd_df(void)
{
    screen_puts("Drive  Type      Label      Mounted\n");
    screen_puts("------ --------- ---------- -------\n");
    for (int d = 0; d < 26; d++) {
        const struct drive_info *info = drive_get_info(d);
        if (info && info->type != 0) {
            screen_putchar(' ');
            screen_putchar((char)('A' + d));
            screen_puts(":      ");
            screen_puts(drive_get_type_name(info->type));
            screen_puts("  ");
            screen_puts(info->label[0] ? info->label : "(none)");
            screen_puts("     ");
            screen_puts(info->mounted ? "YES" : "NO");
            screen_putchar('\n');
        }
    }
}

/*
 * cmd_du — 磁盘占用 (Unix 兼容)
 */
static void cmd_du(const char *args)
{
    (void)args;
    screen_puts("DIR:\n");
    fat32_list_root();
    screen_puts("\nUse DIR for detailed file sizes.\n");
}

/*
 * cmd_ps — 进程列表 (Unix 兼容)
 */
static void cmd_ps(void)
{
    /* 扫描 PCB 池 (最多 32 个进程) */
    screen_puts("  PID  STATE    NAME\n");
    screen_puts("  ---- -------- ----\n");
    int shown = 0;
    for (int pid = 0; pid < 32 && shown < 16; pid++) {
        struct pcb *p = sched_get_pcb(pid);
        if (p && p->state != 0) {
            screen_puts("  ");
            screen_put_dec((int)p->pid);
            screen_puts("   ");
            switch (p->state) {
            case 1: screen_puts("READY   "); break;
            case 2: screen_puts("RUNNING "); break;
            case 3: screen_puts("BLOCKED "); break;
            default: screen_puts("UNKNOWN "); break;
            }
            screen_puts(" ");
            screen_puts(p->name);
            screen_putchar('\n');
            shown++;
        }
    }
    screen_put_dec(shown);
    screen_puts(" process(es)\n");
}

/*
 * cmd_kill — 终止进程 (Unix 兼容)
 */
static void cmd_kill(const char *args)
{
    if (*args == '\0') { screen_puts("Usage: KILL <pid>\n"); return; }
    int pid = 0;
    while (*args >= '0' && *args <= '9') { pid = pid * 10 + (*args - '0'); args++; }
    if (pid <= 0) { screen_puts("Invalid PID.\n"); return; }
    struct pcb *p = sched_get_pcb(pid);
    if (p && p->state != 0) {
        p->state = 0;  /* FREE */
        p->ticks_remaining = 0;
        screen_puts("Killed PID "); screen_put_dec(pid); screen_putchar('\n');
    } else {
        screen_puts("No such process.\n");
    }
}

/*
 * cmd_ln — 符号链接 (Unix 兼容, FAT 不支持)
 */
static void cmd_ln(const char *args)
{
    (void)args;
    screen_puts("ln: symbolic links not supported on FAT filesystem.\n");
    screen_puts("Use COPY command to duplicate files.\n");
}

static void cmd_reboot(void)
{
    screen_puts("Rebooting...\n");
    __asm__ __volatile__("mov $0xFE, %%al\n\tout %%al, $0x64" : : : "ax", "memory");
    __asm__ __volatile__("cli\n\tlidt (%%eax)\n\tint $0x00" : : "a"(0) : "memory");
    while (1) __asm__ __volatile__("hlt");
}

/* ===== 命令解析调度 ===== */

static void shell_exec(const char *cmd)
{
    char name[16];
    const char *args;
    int i;

    cmd = shell_skip_spaces(cmd);
    if (*cmd == '\0')
        return;

    shell_strcpy(last_cmd, cmd);

    /* 驱动器切换 A: C: */
    if (cmd[0] && cmd[1] == ':') {
        char c = cmd[0];
        if ((c >= 'A' && c <= 'Z') || (c >= 'a' && c <= 'z')) {
            if (*shell_skip_spaces(cmd + 2) == '\0') {
                cmd_switch_drive(c);
                return;
            }
        }
    }

    /* 提取命令名 (转大写) */
    i = 0;
    while (*cmd && *cmd != ' ' && *cmd != '\t' && i < 15) {
        char c = *cmd;
        if (c >= 'a' && c <= 'z') c -= 32;
        name[i++] = c;
        cmd++;
    }
    name[i] = '\0';
    args = shell_skip_spaces(cmd);

    /* === DOS 命令 === */
    if (shell_strcmp(name, "HELP") == 0 || shell_strcmp(name, "?") == 0)
        cmd_help();

    else if (shell_strcmp(name, "DIR") == 0 || shell_strcmp(name, "LS") == 0)
        cmd_dir(args);

    else if (shell_strcmp(name, "TYPE") == 0 || shell_strcmp(name, "CAT") == 0)
        cmd_type(args);

    else if (shell_strcmp(name, "COPY") == 0 || shell_strcmp(name, "CP") == 0)
        cmd_copy(args);

    else if (shell_strcmp(name, "DEL") == 0 || shell_strcmp(name, "ERASE") == 0
             || shell_strcmp(name, "RM") == 0)
        cmd_del(args);

    else if (shell_strcmp(name, "REN") == 0 || shell_strcmp(name, "RENAME") == 0)
        cmd_rename(args);

    else if (shell_strcmp(name, "MOVE") == 0 || shell_strcmp(name, "MV") == 0)
        cmd_move(args);

    else if (shell_strcmp(name, "ATTRIB") == 0)
        cmd_attrib(args);

    else if (shell_strcmp(name, "CLS") == 0 || shell_strcmp(name, "CLEAR") == 0)
        cmd_cls();

    else if (shell_strcmp(name, "VER") == 0)
        cmd_ver();

    else if (shell_strcmp(name, "UNAME") == 0)
        cmd_uname(args);

    else if (shell_strcmp(name, "VOL") == 0)
        cmd_vol();

    else if (shell_strcmp(name, "LABEL") == 0)
        cmd_label(args);

    else if (shell_strcmp(name, "ECHO") == 0)
        cmd_echo(args);

    else if (shell_strcmp(name, "DATE") == 0)
        cmd_date(args);

    else if (shell_strcmp(name, "TIME") == 0)
        cmd_time(args);

    else if (shell_strcmp(name, "MEM") == 0)
        cmd_mem();

    else if (shell_strcmp(name, "MORE") == 0)
        cmd_more(args);

    else if (shell_strcmp(name, "FIND") == 0 || shell_strcmp(name, "GREP") == 0)
        cmd_find(args);

    else if (shell_strcmp(name, "SORT") == 0)
        cmd_sort(args);

    else if (shell_strcmp(name, "PROMPT") == 0)
        cmd_prompt(args);

    else if (shell_strcmp(name, "PATH") == 0)
        cmd_path(args);

    else if (shell_strcmp(name, "SET") == 0 || shell_strcmp(name, "ENV") == 0)
        cmd_set(args);

    else if (shell_strcmp(name, "VERIFY") == 0)
        cmd_verify(args);

    else if (shell_strcmp(name, "BREAK") == 0)
        cmd_break(args);

    else if (shell_strcmp(name, "PAUSE") == 0)
        cmd_pause();

    else if (shell_strcmp(name, "REM") == 0)
        cmd_rem(args);

    else if (shell_strcmp(name, "CD") == 0 || shell_strcmp(name, "CHDIR") == 0)
        cmd_cd(args);

    else if (shell_strcmp(name, "PWD") == 0)
        cmd_pwd();

    else if (shell_strcmp(name, "MD") == 0 || shell_strcmp(name, "MKDIR") == 0)
        cmd_mkdir(args);

    else if (shell_strcmp(name, "RD") == 0 || shell_strcmp(name, "RMDIR") == 0)
        cmd_rmdir(args);

    else if (shell_strcmp(name, "TREE") == 0)
        cmd_tree();

    else if (shell_strcmp(name, "TOUCH") == 0)
        cmd_touch(args);

    else if (shell_strcmp(name, "HEAD") == 0)
        cmd_head(args);

    else if (shell_strcmp(name, "TAIL") == 0)
        cmd_tail(args);

    else if (shell_strcmp(name, "WC") == 0)
        cmd_wc(args);

    else if (shell_strcmp(name, "CMP") == 0 || shell_strcmp(name, "DIFF") == 0)
        cmd_cmp(args);

    else if (shell_strcmp(name, "WHICH") == 0)
        cmd_which(args);

    else if (shell_strcmp(name, "WHOAMI") == 0)
        cmd_whoami();

    else if (shell_strcmp(name, "HOSTNAME") == 0)
        cmd_hostname();

    /* === PlexsDOS 扩展 === */
    else if (shell_strcmp(name, "DRIVE") == 0 || shell_strcmp(name, "DRV") == 0)
        cmd_drive();

    else if (shell_strcmp(name, "RUN") == 0)
        cmd_run(args);

    else if (shell_strcmp(name, "INSTALL") == 0)
        cmd_install();

    else if (shell_strcmp(name, "CDIR") == 0)
        cmd_cdir();

    else if (shell_strcmp(name, "CCAT") == 0)
        cmd_ccat(args);

    else if (shell_strcmp(name, "CDMOUNT") == 0)
        cmd_cdmount();

    else if (shell_strcmp(name, "CDEJECT") == 0)
        cmd_cdeject();

    else if (shell_strcmp(name, "CDINFO") == 0)
        cmd_cdinfo();

    else if (shell_strcmp(name, "FORMAT") == 0)
        cmd_format();

    else if (shell_strcmp(name, "DM") == 0)
        cmd_dm();

#ifndef MINIMAL_KERNEL
    else if (shell_strcmp(name, "GUI") == 0)
        cmd_gui();

    else if (shell_strcmp(name, "DEBUG") == 0 || shell_strcmp(name, "DBG") == 0)
        cmd_debug();

    else if (shell_strcmp(name, "EDIT") == 0 || shell_strcmp(name, "ED") == 0)
        cmd_edit(args);
#endif

    else if (shell_strcmp(name, "REBOOT") == 0 || shell_strcmp(name, "RESET") == 0)
        cmd_reboot();

    else if (shell_strcmp(name, "EXIT") == 0 || shell_strcmp(name, "QUIT") == 0)
        cmd_reboot();

    /* === Unix 扩展命令 === */
    else if (shell_strcmp(name, "CHMOD") == 0)
        cmd_chmod(args);

    else if (shell_strcmp(name, "DF") == 0)
        cmd_df();

    else if (shell_strcmp(name, "DU") == 0)
        cmd_du(args);

    else if (shell_strcmp(name, "PS") == 0)
        cmd_ps();

    else if (shell_strcmp(name, "KILL") == 0)
        cmd_kill(args);

    else if (shell_strcmp(name, "LN") == 0)
        cmd_ln(args);

    /* === 自动执行: 尝试作为 .COMX/.EXE/.COM 运行 === */
    else
        loader_run(name);
}

/* ===== 行编辑 ===== */

static int shell_read_line(char *buf, int max_len)
{
    int len = 0, pos = 0;
    char c;
    int esc = 0;

    while (1) {
        c = keyboard_getchar();

        if (esc == 2) {
            esc = 0;
            switch (c) {
            case 'A': /* 上: 历史 */
                if (last_cmd[0]) {
                    int t;
                    while (pos > 0) { screen_putchar('\b'); pos--; }
                    t = len; while (t > 0) { screen_putchar(' '); t--; }
                    t = len; while (t > 0) { screen_putchar('\b'); t--; }
                    shell_strcpy(buf, last_cmd);
                    len = shell_strlen(buf);
                    pos = len;
                    screen_puts(buf);
                }
                continue;
            case 'B': continue;
            case 'C': /* 右 */
                if (pos < len) { screen_putchar(buf[pos]); pos++; }
                continue;
            case 'D': /* 左 */
                if (pos > 0) { screen_putchar('\b'); pos--; }
                continue;
            case 'H': while (pos > 0) { screen_putchar('\b'); pos--; } continue;
            case 'F': while (pos < len) { screen_putchar(buf[pos]); pos++; } continue;
            default: continue;
            }
        }

        if (esc == 1) {
            esc = 0;
            if (c == '[') { esc = 2; continue; }
            continue;
        }

        if (c == 0x1B) { esc = 1; continue; }

        if (c == '\r' || c == '\n') {
            while (pos < len) { screen_putchar(buf[pos]); pos++; }
            screen_putchar('\n');
            break;
        }

        if (c == '\b' || c == 0x7F) {
            if (pos > 0) {
                int j;
                pos--; len--;
                for (j = pos; j < len; j++) buf[j] = buf[j+1];
                buf[len] = '\0';
                screen_putchar('\b');
                for (j = pos; j < len; j++) screen_putchar(buf[j]);
                screen_putchar(' ');
                for (j = len; j >= pos; j--) screen_putchar('\b');
            }
            continue;
        }

        if (c >= 32 && c < 127 && len < max_len - 1) {
            int j;
            if (pos < len)
                for (j = len; j > pos; j--) buf[j] = buf[j-1];
            buf[pos] = c;
            len++; pos++;
            buf[len] = '\0';
            for (j = pos-1; j < len; j++) screen_putchar(buf[j]);
            for (j = len; j > pos; j--) screen_putchar('\b');
        }
    }

    buf[len] = '\0';
    return len;
}

/* ===== Shell 主循环 ===== */

void shell_main(void)
{
    screen_puts("Nexsteaduser PlexsDOS Shell.\n");
    screen_puts("Type HELP for available commands.\n\n");

    last_cmd[0] = '\0';

    while (1) {
        if (use_custom_prompt) {
            screen_set_color(0x0A, 0x00);
            screen_puts(custom_prompt);
            screen_reset_color();
        } else {
            char prompt[12];
            prompt[0] = fs_get_current_drive();
            prompt[1] = ':';
            prompt[2] = '\\';
            prompt[3] = '>';
            prompt[4] = ' ';
            prompt[5] = '\0';
            screen_set_color(0x0A, 0x00);
            screen_puts(prompt);
            screen_reset_color();
        }

        shell_read_line(cmd_buf, sizeof(cmd_buf));
        shell_exec(cmd_buf);
    }
}
