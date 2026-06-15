/*
 * Nexsteaduser — PlexsDOS
 * config_sys.c — CONFIG.SYS 解析器实现
 * 作者: Tinmc189623 | 团队: Nexlyh
 *
 * 解析启动配置文件, 设置系统参数。
 * 语法: COMMAND=VALUE (大小写不敏感, 空格自动修剪)
 * 注释: ; 或 # 开头
 */

#include <plexsdos/config_sys.h>
#include <plexsdos/string.h>
#include <plexsdos/screen.h>
#include <plexsdos/serial.h>

/* 全局配置实例 */
static struct config_sys g_config;
static struct config_sys_entry *g_devices_head = NULL;
static struct config_sys_entry *g_devices_tail = NULL;
static int g_device_count = 0;

/* 正向引用 */
static void parse_line(char *line);
static void add_device(const char *path);

/*
 * config_sys_init — 默认配置
 */
void config_sys_init(void)
{
    g_config.files      = 8;
    g_config.buffers    = 4;
    g_config.last_drive = 'E';
    g_config.break_on   = true;
    g_config.echo_on    = true;
    g_config.dos_high   = false;
    g_config.shell_path[0] = '\0';  /* 空 = 使用内置 Shell */
    g_config.devices    = NULL;
    g_config.device_count = 0;

    g_devices_head = NULL;
    g_devices_tail = NULL;
    g_device_count = 0;
}

/*
 * skip_whitespace — 跳过前导空白
 */
static char *skip_whitespace(char *s)
{
    while (*s == ' ' || *s == '\t') s++;
    return s;
}

/*
 * trim_trailing — 去除尾部空白和回车
 */
static void trim_trailing(char *s)
{
    char *end = s;
    while (*end) end++;
    end--;
    while (end >= s && (*end == ' ' || *end == '\t' || *end == '\r' || *end == '\n'))
        *end-- = '\0';
}

/*
 * str_to_upper — 原地转为大写 (用于命令匹配)
 */
static void str_to_upper(char *s)
{
    while (*s) {
        if (*s >= 'a' && *s <= 'z') *s -= 32;
        s++;
    }
}

/*
 * parse_int — 解析整数
 */
static int parse_int(const char *s)
{
    int val = 0;
    while (*s >= '0' && *s <= '9') {
        val = val * 10 + (*s - '0');
        s++;
    }
    return val;
}

/*
 * add_device — 追加 DEVICE= 链表节点
 */
static void add_device(const char *path)
{
    struct config_sys_entry *entry;
    int i;

    /* 分配内存 (简化: 从静态池分配, freestanding 无 malloc) */
    static struct config_sys_entry pool[8];
    static int pool_idx = 0;

    if (pool_idx >= 8)
        return;

    entry = &pool[pool_idx++];
    for (i = 0; path[i] && i < CONFIG_VAL_SIZE - 1; i++)
        entry->value[i] = path[i];
    entry->value[i] = '\0';
    entry->next = NULL;

    if (!g_devices_head) {
        g_devices_head = entry;
        g_devices_tail = entry;
    } else {
        g_devices_tail->next = entry;
        g_devices_tail = entry;
    }
    g_device_count++;
}

/*
 * config_sys_parse — 按行解析
 */
void config_sys_parse(const char *data)
{
    char line[CONFIG_LINE_SIZE];
    int li = 0;
    const char *p = data;

    if (!data || !*data)
        return;

    while (*p) {
        if (*p == '\n' || *p == '\r') {
            if (li > 0) {
                line[li] = '\0';
                parse_line(line);
                li = 0;
            }
            /* 跳过 \r\n */
            if (*p == '\r' && *(p + 1) == '\n') p++;
            p++;
            continue;
        }
        if (li < CONFIG_LINE_SIZE - 1)
            line[li++] = *p;
        p++;
    }
    /* 最后一行 (可能无换行) */
    if (li > 0) {
        line[li] = '\0';
        parse_line(line);
    }

    g_config.devices = g_devices_head;
    g_config.device_count = g_device_count;
}

/*
 * parse_line — 解析单行
 */
static void parse_line(char *line)
{
    char *s;
    char *eq;
    char command[CONFIG_CMD_SIZE];
    char value[CONFIG_VAL_SIZE];

    s = skip_whitespace(line);

    /* 空行或注释 */
    if (!*s || *s == ';' || *s == '#')
        return;

    /* 查找 = */
    eq = s;
    while (*eq && *eq != '=') eq++;
    if (!*eq) return;  /* 无等号, 忽略 */

    /* 提取命令名 */
    {
        int len = (int)(eq - s);
        if (len >= CONFIG_CMD_SIZE) len = CONFIG_CMD_SIZE - 1;
        {
            int i;
            for (i = 0; i < len; i++) command[i] = s[i];
            command[len] = '\0';
        }
    }
    trim_trailing(command);
    str_to_upper(command);

    /* 提取值 */
    {
        char *v = eq + 1;
        int i;
        v = skip_whitespace(v);
        for (i = 0; v[i] && i < CONFIG_VAL_SIZE - 1; i++)
            value[i] = v[i];
        value[i] = '\0';
    }
    trim_trailing(value);

    if (g_config.echo_on) {
        screen_set_color(0x08, 0x00);  /* 灰 */
        screen_puts("  [CFG] ");
        screen_puts(command);
        screen_puts("=");
        screen_puts(value);
        screen_putchar('\n');
        screen_set_color(0x07, 0x00);
    }

    /* 命令匹配 */
    if (strcmp(command, "FILES") == 0) {
        g_config.files = parse_int(value);
        if (g_config.files < 5) g_config.files = 5;
    } else if (strcmp(command, "BUFFERS") == 0) {
        g_config.buffers = parse_int(value);
        if (g_config.buffers < 2) g_config.buffers = 2;
    } else if (strcmp(command, "LASTDRIVE") == 0 || strcmp(command, "LASTDRV") == 0) {
        if (*value >= 'A' && *value <= 'Z')
            g_config.last_drive = *value;
    } else if (strcmp(command, "BREAK") == 0) {
        str_to_upper(value);
        g_config.break_on = (value[0] == 'O' && value[1] == 'N');
    } else if (strcmp(command, "ECHO") == 0) {
        str_to_upper(value);
        g_config.echo_on = (value[0] == 'O' && value[1] == 'N');
    } else if (strcmp(command, "DOS") == 0) {
        str_to_upper(value);
        g_config.dos_high = (value[0] == 'H');
    } else if (strcmp(command, "SHELL") == 0) {
        int i;
        for (i = 0; value[i] && i < 63; i++)
            g_config.shell_path[i] = value[i];
        g_config.shell_path[i] = '\0';
    } else if (strcmp(command, "DEVICE") == 0) {
        add_device(value);
    }
}

/*
 * config_sys_get — 获取配置
 */
const struct config_sys *config_sys_get(void)
{
    return &g_config;
}

/*
 * config_sys_find_device — 查找 DEVICE 条目
 */
const char *config_sys_find_device(int index)
{
    struct config_sys_entry *e = g_devices_head;
    while (e && index > 0) {
        e = e->next;
        index--;
    }
    return e ? e->value : NULL;
}

/*
 * config_sys_print — 输出配置摘要
 */
void config_sys_print(void)
{
    const struct config_sys *cfg = &g_config;

    screen_set_color(0x0B, 0x00);
    screen_puts("\n  --- CONFIG.SYS Summary ---\n");
    screen_set_color(0x07, 0x00);

    screen_puts("  FILES=");     screen_put_dec(cfg->files);     screen_putchar('\n');
    screen_puts("  BUFFERS=");   screen_put_dec(cfg->buffers);   screen_putchar('\n');
    screen_puts("  LASTDRIVE="); screen_putchar(cfg->last_drive); screen_putchar('\n');
    screen_puts("  BREAK=");     screen_puts(cfg->break_on ? "ON" : "OFF"); screen_putchar('\n');
    screen_puts("  DOS=");       screen_puts(cfg->dos_high ? "HIGH" : "LOW"); screen_putchar('\n');

    if (cfg->shell_path[0])
        { screen_puts("  SHELL="); screen_puts(cfg->shell_path); screen_putchar('\n'); }
    else
        screen_puts("  SHELL=(built-in)\n");

    if (cfg->device_count > 0) {
        int i;
        for (i = 0; i < cfg->device_count; i++) {
            screen_puts("  DEVICE=");
            screen_puts(config_sys_find_device(i));
            screen_putchar('\n');
        }
    }

    screen_set_color(0x0B, 0x00);
    screen_puts("  ---------------------------\n");
    screen_set_color(0x07, 0x00);
}
