/*
 * Nexsteaduser — PlexsDOS
 * plxdm_compat.c — GLib 兼容层实现 (PlexsDOS 原生实现)
 * 作者: Tinmc189623 | 团队: Nexlyh
 *
 * 实现 plxdm_compat.h 中声明的所有 GLib 兼容函数。
 * 使用静态内存池 (PlexsDOS 无堆环境)。
 */

#include "plxdm_compat.h"
#include <plexsdos/string.h>
#include <stdarg.h>

/* ==================== 内存池 ==================== */

/* 字符串分配现在直接使用堆 (malloc/free) */

/* GList/GSList 节点池 */
#define NODE_POOL_SIZE 256
static struct {
    struct _GList node;  /* GList 和 GSList 共用, GList 更大 */
    int used;
} node_pool[NODE_POOL_SIZE];
static int node_pool_count = 0;

/* GString 池 */
#define GSTRING_POOL_SIZE 16
#define GSTRING_BUF_SIZE 512
static struct {
    GString str;
    char buf[GSTRING_BUF_SIZE];
    int used;
} gstring_pool[GSTRING_POOL_SIZE];

/* GHashTable 池 */
#define HASH_TABLE_POOL_SIZE 8
#define HASH_ENTRY_POOL_SIZE 32
static struct {
    GHashTable ht;
    struct GHashEntry entries[HASH_ENTRY_POOL_SIZE];
    int used;
} hashtable_pool[HASH_TABLE_POOL_SIZE];

/* GPtrArray 池 */
#define PTR_ARRAY_POOL_SIZE 8
#define PTR_ARRAY_DATA_SIZE 64
static struct {
    GPtrArray arr;
    gpointer data[PTR_ARRAY_DATA_SIZE];
    int used;
} ptrarray_pool[PTR_ARRAY_POOL_SIZE];

/* GTimer 池 */
#define TIMER_POOL_SIZE 4
static struct {
    GTimer timer;
    int used;
} timer_pool[TIMER_POOL_SIZE];

/* GError 槽 */
#define ERROR_SLOTS 4
static GError error_slots[ERROR_SLOTS];
static int error_used[ERROR_SLOTS];

/* 信号连接池 */
#define SIGNAL_CONN_POOL_SIZE 64
struct signal_conn {
    gpointer obj;
    char signal_name[48];
    GCallback handler;
    gpointer user_data;
    int used;
};
static struct signal_conn signal_conns[SIGNAL_CONN_POOL_SIZE];

/* 超时/空闲源 */
#define SOURCE_POOL_SIZE 16
struct source_entry {
    guint id;
    gboolean is_idle;
    guint interval_ms;       /* 仅超时 */
    guint64 expire_tick;     /* 仅超时 */
    GSourceFunc function;
    gpointer data;
    int used;
};
static struct source_entry sources[SOURCE_POOL_SIZE];
static guint next_source_id = 1;

/* 日志处理 */
static GLogFunc default_log_handler = NULL;
static gpointer default_log_userdata = NULL;

/* Tick 计数 (由 PlexsDOS 定时器中断更新) */
static guint64 system_ticks = 0;

/*
 * plxdm_tick — 由定时器中断调用, 更新系统 tick
 */
void plxdm_tick(void)
{
    system_ticks++;
}

/* ==================== 内部辅助 ==================== */

static guint64 get_ticks(void)
{
    return system_ticks;
}

/* arena_alloc 已弃用 — 改用 malloc */

/* ==================== 字符串工具 ==================== */

gchar *g_strdup(const gchar *str)
{
    if (!str)
        return NULL;

    int len = 0;
    while (str[len])
        len++;

    gchar *copy = (gchar *)malloc(len + 1);
    if (!copy)
        return NULL;

    for (int i = 0; i <= len; i++)
        copy[i] = str[i];
    return copy;
}

static int vsnprintf_impl(char *buf, unsigned int size, const char *fmt, va_list ap)
{
    /* Simple vsnprintf for freestanding */
    unsigned int pos = 0;
    char c;

    while ((c = *fmt) && pos + 1 < size) {
        fmt++;
        if (c != '%') {
            buf[pos++] = c;
            continue;
        }

        c = *fmt++;
        switch (c) {
        case 's': {
            const char *s = va_arg(ap, const char *);
            if (!s) s = "(null)";
            while (*s && pos + 1 < size)
                buf[pos++] = *s++;
            break;
        }
        case 'd': {
            int val = va_arg(ap, int);
            char tmp[16];
            int tpos = 0;
            int neg = 0;
            if (val < 0) { neg = 1; val = -val; }
            if (val == 0) tmp[tpos++] = '0';
            while (val > 0 && tpos < 16) {
                tmp[tpos++] = '0' + (val % 10);
                val /= 10;
            }
            if (neg) tmp[tpos++] = '-';
            while (tpos > 0 && pos + 1 < size)
                buf[pos++] = tmp[--tpos];
            break;
        }
        case 'u': {
            unsigned int val = va_arg(ap, unsigned int);
            char tmp[16];
            int tpos = 0;
            if (val == 0) tmp[tpos++] = '0';
            while (val > 0 && tpos < 16) {
                tmp[tpos++] = '0' + (val % 10);
                val /= 10;
            }
            while (tpos > 0 && pos + 1 < size)
                buf[pos++] = tmp[--tpos];
            break;
        }
        case 'x':
        case 'X': {
            unsigned int val = va_arg(ap, unsigned int);
            char tmp[16];
            int tpos = 0;
            if (val == 0) tmp[tpos++] = '0';
            while (val > 0 && tpos < 16) {
                int d = val & 0xF;
                tmp[tpos++] = d < 10 ? '0' + d : 'a' + d - 10;
                val >>= 4;
            }
            while (tpos > 0 && pos + 1 < size)
                buf[pos++] = tmp[--tpos];
            break;
        }
        case 'c': {
            char ch = (char)va_arg(ap, int);
            buf[pos++] = ch;
            break;
        }
        case 'f': {
            /* 简单 float 处理, LightDM 中的 float 仅用于 timer */
            double val = va_arg(ap, double);
            int intpart = (int)val;
            int frac = (int)((val - intpart) * 100 + 0.5);
            if (frac >= 100) { intpart++; frac = 0; }
            char tmp[16];
            int tpos = 0;
            if (intpart == 0) tmp[tpos++] = '0';
            while (intpart > 0 && tpos < 12) {
                tmp[tpos++] = '0' + (intpart % 10);
                intpart /= 10;
            }
            while (tpos > 0 && pos + 1 < size)
                buf[pos++] = tmp[--tpos];
            if (pos + 2 < size) {
                buf[pos++] = '.';
                buf[pos++] = '0' + (frac / 10);
                buf[pos++] = '0' + (frac % 10);
            }
            break;
        }
        case '%':
            buf[pos++] = '%';
            break;
        default:
            buf[pos++] = c;
            break;
        }
    }
    buf[pos] = '\0';
    return pos;
}

gchar *g_strdup_printf(const gchar *format, ...)
{
    va_list ap;
    va_start(ap, format);
    gchar *result = g_strdup_vprintf(format, ap);
    va_end(ap);
    return result;
}

gchar *g_strdup_vprintf(const gchar *format, va_list args)
{
    if (!format)
        return NULL;

    /* 先在栈上尝试 */
    char stack_buf[256];
    int len = vsnprintf_impl(stack_buf, 256, format, args);

    gchar *result = (gchar *)malloc(len + 1);
    if (!result)
        return NULL;

    for (int i = 0; i <= len; i++)
        result[i] = stack_buf[i];
    return result;
}

gchar **g_strsplit(const gchar *str, const gchar *delimiter, gint max_tokens)
{
    if (!str || !delimiter || max_tokens < 1)
        return NULL;

    int delim_len = 0;
    while (delimiter[delim_len])
        delim_len++;
    if (delim_len == 0)
        return NULL;

    /* 计算 token 数 */
    int tokens = 1;
    const gchar *p = str;
    while (*p) {
        int match = 1;
        for (int i = 0; i < delim_len; i++)
            if (p[i] != delimiter[i]) { match = 0; break; }
        if (match && (!max_tokens || tokens < max_tokens)) {
            tokens++;
            p += delim_len;
        } else {
            p++;
        }
    }
    if (max_tokens > 0 && tokens > max_tokens)
        tokens = max_tokens;

    /* 分配数组 (tokens + 1 以 NULL 结尾) */
    gchar **result = (gchar **)malloc(sizeof(gchar *) * (tokens + 1));
    if (!result) return NULL;

    int ti = 0;
    p = str;
    while (*p && ti < tokens - 1) {
        /* 找分隔符 */
        const gchar *end = p;
        while (*end) {
            int match = 1;
            for (int i = 0; i < delim_len; i++)
                if (end[i] != delimiter[i]) { match = 0; break; }
            if (match) break;
            end++;
        }

        int seg_len = (int)(end - p);
        result[ti] = (gchar *)malloc(seg_len + 1);
        if (result[ti]) {
            for (int i = 0; i < seg_len; i++)
                result[ti][i] = p[i];
            result[ti][seg_len] = '\0';
        }
        ti++;
        p = end + (end[0] ? delim_len : 0);
    }

    /* 最后一段 */
    int last_len = 0;
    while (p[last_len]) last_len++;
    result[ti] = (gchar *)malloc(last_len + 1);
    if (result[ti]) {
        for (int i = 0; i < last_len; i++)
            result[ti][i] = p[i];
        result[ti][last_len] = '\0';
    }
    ti++;
    result[ti] = NULL;

    return result;
}

gchar *g_strjoinv(const gchar *separator, gchar **str_array)
{
    if (!str_array)
        return NULL;

    int sep_len = separator ? 0 : 0;
    if (separator)
        while (separator[sep_len]) sep_len++;

    int total = 0;
    for (gchar **s = str_array; *s; s++) {
        int sl = 0;
        while ((*s)[sl]) sl++;
        total += sl + sep_len;
    }
    if (total > 0) total -= sep_len; /* 去掉最后的分隔符 */
    if (total < 0) total = 0;

    gchar *result = (gchar *)malloc(total + 1);
    if (!result) return NULL;

    int pos = 0;
    for (gchar **s = str_array; *s; s++) {
        if (separator && s != str_array) {
            for (int i = 0; separator[i]; i++)
                result[pos++] = separator[i];
        }
        for (int i = 0; (*s)[i]; i++)
            result[pos++] = (*s)[i];
    }
    result[pos] = '\0';
    return result;
}

gchar *g_strconcat(const gchar *string1, ...)
{
    if (!string1)
        return NULL;

    /* 先计算总长度 */
    int total = 0;
    {
        const gchar *s = string1;
        while (*s) { total++; s++; }
    }

    va_list ap;
    va_start(ap, string1);
    const gchar *arg;
    while ((arg = va_arg(ap, const gchar *))) {
        const gchar *p = arg;
        while (*p) { total++; p++; }
    }
    va_end(ap);

    gchar *result = (gchar *)malloc(total + 1);
    if (!result) return NULL;

    int pos = 0;
    const gchar *s = string1;
    while (*s) result[pos++] = *s++;

    va_start(ap, string1);
    while ((arg = va_arg(ap, const gchar *))) {
        s = arg;
        while (*s) result[pos++] = *s++;
    }
    va_end(ap);
    result[pos] = '\0';
    return result;
}

void g_strfreev(gchar **str_array)
{
    if (!str_array) return;
    for (int i = 0; str_array[i]; i++)
        free(str_array[i]);
    free(str_array);
}

gboolean g_str_has_prefix(const gchar *str, const gchar *prefix)
{
    if (!str || !prefix)
        return FALSE;

    int i = 0;
    while (prefix[i]) {
        if (str[i] != prefix[i])
            return FALSE;
        if (!str[i])
            return FALSE;
        i++;
    }
    return TRUE;
}

gboolean g_str_has_suffix(const gchar *str, const gchar *suffix)
{
    if (!str || !suffix)
        return FALSE;

    int sl = 0, sufl = 0;
    while (str[sl]) sl++;
    while (suffix[sufl]) sufl++;

    if (sufl > sl)
        return FALSE;

    int offset = sl - sufl;
    for (int i = 0; i < sufl; i++)
        if (str[offset + i] != suffix[i])
            return FALSE;
    return TRUE;
}

gboolean g_pattern_match_simple(const gchar *pattern, const gchar *str)
{
    if (!pattern || !str)
        return FALSE;

    /* 简单 glob 匹配: '*' = 任意序列, '?' = 单个字符 */
    const gchar *p = pattern;
    const gchar *s = str;
    const gchar *star_p = NULL;
    const gchar *star_s = NULL;

    while (*s) {
        if (*p == '*') {
            star_p = ++p;
            star_s = s;
        } else if (*p == '?' || *p == *s) {
            p++;
            s++;
        } else if (star_p) {
            p = star_p;
            s = ++star_s;
        } else {
            return FALSE;
        }
    }
    while (*p == '*') p++;
    return *p == '\0';
}

gchar *g_strstrip(gchar *str)
{
    if (!str) return NULL;
    str = g_strchug(str);
    if (!*str) return str;

    int len = 0;
    while (str[len]) len++;
    while (len > 0 && (str[len - 1] == ' ' || str[len - 1] == '\t' || str[len - 1] == '\n' || str[len - 1] == '\r'))
        str[--len] = '\0';
    return str;
}

gchar *g_strchug(gchar *str)
{
    if (!str) return NULL;
    int i = 0;
    while (str[i] == ' ' || str[i] == '\t' || str[i] == '\n' || str[i] == '\r')
        i++;
    if (i > 0) {
        int j = 0;
        while (str[i + j]) {
            str[j] = str[i + j];
            j++;
        }
        str[j] = '\0';
    }
    return str;
}

void g_strdelimit(gchar *str, const gchar *delimiters, gchar new_delimiter)
{
    if (!str || !delimiters) return;

    for (int i = 0; str[i]; i++) {
        for (int j = 0; delimiters[j]; j++) {
            if (str[i] == delimiters[j]) {
                str[i] = new_delimiter;
                break;
            }
        }
    }
}

gint g_strcmp0(const gchar *str1, const gchar *str2)
{
    if (!str1 && !str2) return 0;
    if (!str1) return -1;
    if (!str2) return 1;

    int i = 0;
    while (str1[i] && str2[i] && str1[i] == str2[i])
        i++;
    return (unsigned char)str1[i] - (unsigned char)str2[i];
}

gint g_ascii_strcasecmp(const gchar *s1, const gchar *s2)
{
    if (!s1 && !s2) return 0;
    if (!s1) return -1;
    if (!s2) return 1;

    int i = 0;
    while (s1[i] && s2[i]) {
        char c1 = s1[i], c2 = s2[i];
        if (c1 >= 'A' && c1 <= 'Z') c1 += 0x20;
        if (c2 >= 'A' && c2 <= 'Z') c2 += 0x20;
        if (c1 != c2) return (unsigned char)c1 - (unsigned char)c2;
        i++;
    }
    if (!s1[i] && !s2[i]) return 0;
    return s1[i] ? 1 : -1;
}

gint g_ascii_strncasecmp(const gchar *s1, const gchar *s2, gsize n)
{
    if (n == 0) return 0;
    if (!s1 && !s2) return 0;
    if (!s1) return -1;
    if (!s2) return 1;

    for (gsize i = 0; i < n; i++) {
        if (!s1[i] && !s2[i]) return 0;
        char c1 = s1[i], c2 = s2[i];
        if (c1 >= 'A' && c1 <= 'Z') c1 += 0x20;
        if (c2 >= 'A' && c2 <= 'Z') c2 += 0x20;
        if (c1 != c2) return (unsigned char)c1 - (unsigned char)c2;
        if (!s1[i] || !s2[i]) return s1[i] ? 1 : -1;
    }
    return 0;
}

gboolean g_ascii_isspace(gchar c)
{
    return c == ' ' || c == '\t' || c == '\n' || c == '\r' || c == '\f' || c == '\v';
}

gchar g_ascii_tolower(gchar c)
{
    if (c >= 'A' && c <= 'Z') return c + 0x20;
    return c;
}

gchar g_ascii_toupper(gchar c)
{
    if (c >= 'a' && c <= 'z') return c - 0x20;
    return c;
}

/* ==================== GString ==================== */

GString *g_string_new(const gchar *init)
{
    for (int i = 0; i < GSTRING_POOL_SIZE; i++) {
        if (!gstring_pool[i].used) {
            gstring_pool[i].used = 1;
            GString *s = &gstring_pool[i].str;
            s->str = gstring_pool[i].buf;
            s->allocated_len = GSTRING_BUF_SIZE;
            s->str[0] = '\0';
            s->len = 0;

            if (init) {
                int j = 0;
                while (init[j] && j < (int)GSTRING_BUF_SIZE - 1) {
                    s->str[j] = init[j];
                    j++;
                }
                s->str[j] = '\0';
                s->len = j;
            }
            return s;
        }
    }
    return NULL;
}

GString *g_string_new_len(const gchar *init, gssize len)
{
    GString *s = g_string_new(NULL);
    if (!s) return NULL;

    if (init && len > 0) {
        gsize copy_len = (len < (gssize)(GSTRING_BUF_SIZE - 1)) ? (gsize)len : (GSTRING_BUF_SIZE - 1);
        for (gsize i = 0; i < copy_len; i++)
            s->str[i] = init[i];
        s->str[copy_len] = '\0';
        s->len = copy_len;
    }
    return s;
}

void g_string_free(GString *str, gboolean free_segment)
{
    if (!str) return;
    (void)free_segment;

    for (int i = 0; i < GSTRING_POOL_SIZE; i++) {
        if (&gstring_pool[i].str == str) {
            gstring_pool[i].used = 0;
            return;
        }
    }
}

GString *g_string_append(GString *str, const gchar *val)
{
    if (!str || !val) return str;

    int vl = 0;
    while (val[vl]) vl++;

    if (str->len + vl >= str->allocated_len)
        vl = (int)(str->allocated_len - str->len - 1);
    if (vl < 0) vl = 0;

    for (int i = 0; i < vl; i++)
        str->str[str->len + i] = val[i];
    str->len += vl;
    str->str[str->len] = '\0';
    return str;
}

GString *g_string_append_c(GString *str, gchar c)
{
    if (!str) return NULL;
    if (str->len + 1 < str->allocated_len) {
        str->str[str->len++] = c;
        str->str[str->len] = '\0';
    }
    return str;
}

GString *g_string_append_printf(GString *str, const gchar *format, ...)
{
    if (!str || !format) return str;

    va_list ap;
    va_start(ap, format);
    char buf[256];
    vsnprintf_impl(buf, 256, format, ap);
    va_end(ap);

    return g_string_append(str, buf);
}

GString *g_string_assign(GString *str, const gchar *val)
{
    if (!str) return NULL;
    str->len = 0;
    if (val) {
        int j = 0;
        while (val[j] && j < (int)str->allocated_len - 1) {
            str->str[j] = val[j];
            j++;
        }
        str->str[j] = '\0';
        str->len = j;
    } else {
        str->str[0] = '\0';
    }
    return str;
}

gchar *g_string_free_and_steal(GString *str)
{
    if (!str) return NULL;
    gchar *ret = str->str;

    for (int i = 0; i < GSTRING_POOL_SIZE; i++) {
        if (&gstring_pool[i].str == str) {
            gstring_pool[i].used = 0;
            return ret;
        }
    }
    return ret;
}

void g_string_set_size(GString *str, gsize len)
{
    if (!str) return;
    if (len < str->allocated_len) {
        str->len = len;
        str->str[len] = '\0';
    }
}

GString *g_string_truncate(GString *str, gsize len)
{
    if (!str) return NULL;
    if (len < str->len) {
        str->len = len;
        str->str[len] = '\0';
    }
    return str;
}

/* ==================== GList (双向链表) ==================== */

static GList *alloc_list_node(void)
{
    if (node_pool_count >= NODE_POOL_SIZE)
        return NULL;

    node_pool[node_pool_count].used = 1;
    /* 清零 */
    node_pool[node_pool_count].node.data = NULL;
    node_pool[node_pool_count].node.next = NULL;
    node_pool[node_pool_count].node.prev = NULL;
    return &node_pool[node_pool_count++].node;
}

static void free_list_node(GList *node)
{
    for (int i = 0; i < node_pool_count; i++) {
        if (&node_pool[i].node == node) {
            node_pool[i].used = 0;
            /* 移动最后一个元素到此位置以压缩 */
            if (i < node_pool_count - 1) {
                node_pool[i] = node_pool[node_pool_count - 1];
                node_pool[node_pool_count - 1].used = 0;
            }
            node_pool_count--;
            return;
        }
    }
}

GList *g_list_append(GList *list, gpointer data)
{
    GList *node = alloc_list_node();
    if (!node) return list;
    node->data = data;

    if (!list) {
        return node;
    }

    GList *last = list;
    while (last->next)
        last = last->next;
    last->next = node;
    node->prev = last;
    return list;
}

GList *g_list_prepend(GList *list, gpointer data)
{
    GList *node = alloc_list_node();
    if (!node) return list;
    node->data = data;
    node->next = list;
    if (list)
        list->prev = node;
    return node;
}

GList *g_list_insert(GList *list, gpointer data, gint position)
{
    if (position <= 0)
        return g_list_prepend(list, data);

    GList *prev = list;
    for (int i = 0; prev && i < position - 1; i++)
        prev = prev->next;

    if (!prev)
        return g_list_append(list, data);

    GList *node = alloc_list_node();
    if (!node) return list;
    node->data = data;
    node->next = prev->next;
    node->prev = prev;
    if (prev->next)
        prev->next->prev = node;
    prev->next = node;
    return list;
}

GList *g_list_remove(GList *list, gconstpointer data)
{
    GList *cur = list;
    while (cur) {
        if (cur->data == data) {
            if (cur->prev)
                cur->prev->next = cur->next;
            else
                list = cur->next;
            if (cur->next)
                cur->next->prev = cur->prev;
            GList *next = cur->next;
            free_list_node(cur);
            cur = next;
        } else {
            cur = cur->next;
        }
    }
    return list;
}

GList *g_list_remove_link(GList *list, GList *link)
{
    if (!link)
        return list;

    if (link->prev)
        link->prev->next = link->next;
    else
        list = link->next;
    if (link->next)
        link->next->prev = link->prev;
    link->next = NULL;
    link->prev = NULL;
    return list;
}

GList *g_list_delete_link(GList *list, GList *link)
{
    list = g_list_remove_link(list, link);
    free_list_node(link);
    return list;
}

GList *g_list_find(GList *list, gconstpointer data)
{
    while (list) {
        if (list->data == data)
            return list;
        list = list->next;
    }
    return NULL;
}

GList *g_list_last(GList *list)
{
    if (!list) return NULL;
    while (list->next)
        list = list->next;
    return list;
}

GList *g_list_first(GList *list)
{
    if (!list) return NULL;
    while (list->prev)
        list = list->prev;
    return list;
}

gint g_list_length(GList *list)
{
    int n = 0;
    while (list) {
        n++;
        list = list->next;
    }
    return n;
}

GList *g_list_nth(GList *list, guint n)
{
    while (list && n > 0) {
        list = list->next;
        n--;
    }
    return list;
}

gpointer g_list_nth_data(GList *list, guint n)
{
    GList *node = g_list_nth(list, n);
    return node ? node->data : NULL;
}

GList *g_list_copy(GList *list)
{
    GList *new_list = NULL;
    while (list) {
        new_list = g_list_append(new_list, list->data);
        list = list->next;
    }
    return new_list;
}

GList *g_list_sort(GList *list, int (*compare_func)(gconstpointer a, gconstpointer b))
{
    if (!list || !list->next)
        return list;

    /* 简单冒泡排序 (数据量小) */
    int swapped;
    do {
        swapped = 0;
        GList *cur = list;
        while (cur->next) {
            if (compare_func(cur->data, cur->next->data) > 0) {
                gpointer tmp = cur->data;
                cur->data = cur->next->data;
                cur->next->data = tmp;
                swapped = 1;
            }
            cur = cur->next;
        }
    } while (swapped);

    return list;
}

void g_list_free(GList *list)
{
    while (list) {
        GList *next = list->next;
        free_list_node(list);
        list = next;
    }
}

void g_list_free_full(GList *list, void (*free_func)(gpointer))
{
    while (list) {
        GList *next = list->next;
        if (free_func)
            free_func(list->data);
        free_list_node(list);
        list = next;
    }
}

GList *g_list_reverse(GList *list)
{
    GList *cur = list;
    GList *tmp = NULL;
    while (cur) {
        tmp = cur->prev;
        cur->prev = cur->next;
        cur->next = tmp;
        cur = cur->prev;
    }
    if (tmp)
        list = tmp->prev;
    return list;
}

GList *g_list_insert_sorted(GList *list, gpointer data,
                             int (*compare_func)(gconstpointer a, gconstpointer b))
{
    if (!compare_func) return g_list_append(list, data);

    GList *cur = list;
    int pos = 0;
    while (cur) {
        if (compare_func(data, cur->data) < 0)
            return g_list_insert(list, data, pos);
        cur = cur->next;
        pos++;
    }
    return g_list_append(list, data);
}

GList *g_list_concat(GList *list1, GList *list2)
{
    if (!list1) return list2;
    if (!list2) return list1;

    GList *last = g_list_last(list1);
    last->next = list2;
    list2->prev = last;
    return list1;
}

void g_list_foreach(GList *list, void (*func)(gpointer data, gpointer user_data),
                    gpointer user_data)
{
    while (list) {
        func(list->data, user_data);
        list = list->next;
    }
}

/* ==================== GSList (单向链表) ==================== */

/* GSList 复用 GList 节点 (指向同一池) */
GSList *g_slist_append(GSList *list, gpointer data)
{
    GList *node = alloc_list_node();
    if (!node) return list;
    node->data = data;

    if (!list)
        return (GSList *)node;

    GSList *last = list;
    while (last->next)
        last = last->next;
    last->next = (GSList *)node;
    return list;
}

GSList *g_slist_prepend(GSList *list, gpointer data)
{
    GList *node = alloc_list_node();
    if (!node) return list;
    node->data = data;
    ((GSList*)node)->next = (GSList *)list;
    return (GSList *)node;
}

GSList *g_slist_remove(GSList *list, gconstpointer data)
{
    GSList *cur = list;
    GSList *prev = NULL;
    while (cur) {
        if (cur->data == data) {
            if (prev)
                prev->next = cur->next;
            else
                list = cur->next;
            free_list_node((GList *)cur);
            break;
        }
        prev = cur;
        cur = cur->next;
    }
    return list;
}

GSList *g_slist_delete_link(GSList *list, GSList *link)
{
    GSList *cur = list;
    GSList *prev = NULL;
    while (cur) {
        if (cur == link) {
            if (prev)
                prev->next = cur->next;
            else
                list = cur->next;
            free_list_node((GList *)cur);
            break;
        }
        prev = cur;
        cur = cur->next;
    }
    return list;
}

guint g_slist_length(GSList *list)
{
    int n = 0;
    while (list) {
        n++;
        list = list->next;
    }
    return n;
}

void g_slist_free(GSList *list)
{
    while (list) {
        GSList *next = list->next;
        free_list_node((GList *)list);
        list = next;
    }
}

void g_slist_free_full(GSList *list, void (*free_func)(gpointer))
{
    while (list) {
        GSList *next = list->next;
        if (free_func)
            free_func(list->data);
        free_list_node((GList *)list);
        list = next;
    }
}

/* ==================== GHashTable ==================== */

guint g_str_hash(gconstpointer v)
{
    const unsigned char *p = (const unsigned char *)v;
    guint h = 5381;
    while (*p)
        h = (h * 33) ^ *p++;
    return h;
}

gboolean g_str_equal(gconstpointer a, gconstpointer b)
{
    return g_strcmp0((const gchar *)a, (const gchar *)b) == 0;
}

guint g_int_hash(gconstpointer v)
{
    return *(const guint *)v;
}

gboolean g_int_equal(gconstpointer a, gconstpointer b)
{
    return *(const guint *)a == *(const guint *)b;
}

/* g_direct_hash / g_direct_equal 函数实现 (用于函数指针场景) */
#undef g_direct_hash
#undef g_direct_equal
guint g_direct_hash(gconstpointer p)
{
    return (guint)(gsize)p;
}
gboolean g_direct_equal(gconstpointer a, gconstpointer b)
{
    return a == b;
}

static GHashTable *hash_table_alloc(void)
{
    for (int i = 0; i < HASH_TABLE_POOL_SIZE; i++) {
        if (!hashtable_pool[i].used) {
            hashtable_pool[i].used = 1;
            hashtable_pool[i].ht.entries = hashtable_pool[i].entries;
            hashtable_pool[i].ht.size = HASH_ENTRY_POOL_SIZE;
            hashtable_pool[i].ht.count = 0;
            for (int j = 0; j < HASH_ENTRY_POOL_SIZE; j++)
                hashtable_pool[i].entries[j].used = FALSE;
            hashtable_pool[i].ht.hash_func = NULL;
            hashtable_pool[i].ht.equal_func = NULL;
            hashtable_pool[i].ht.key_destroy_func = NULL;
            hashtable_pool[i].ht.value_destroy_func = NULL;
            return &hashtable_pool[i].ht;
        }
    }
    return NULL;
}

GHashTable *g_hash_table_new(guint (*hash_func)(gconstpointer key),
                              gboolean (*equal_func)(gconstpointer a, gconstpointer b))
{
    return g_hash_table_new_full(hash_func, equal_func, NULL, NULL);
}

GHashTable *g_hash_table_new_full(guint (*hash_func)(gconstpointer key),
                                   gboolean (*equal_func)(gconstpointer a, gconstpointer b),
                                   void (*key_destroy_func)(gpointer data),
                                   void (*value_destroy_func)(gpointer data))
{
    GHashTable *ht = hash_table_alloc();
    if (!ht) return NULL;

    ht->hash_func = hash_func ? hash_func : g_str_hash;
    ht->equal_func = equal_func ? equal_func : g_str_equal;
    ht->key_destroy_func = key_destroy_func;
    ht->value_destroy_func = value_destroy_func;
    return ht;
}

void g_hash_table_insert(GHashTable *ht, gpointer key, gpointer value)
{
    if (!ht) return;

    /* 查找现有键 */
    guint h = ht->hash_func(key);
    for (guint i = 0; i < ht->size; i++) {
        guint idx = (h + i) % ht->size;
        if (!ht->entries[idx].used) {
            ht->entries[idx].key = key;
            ht->entries[idx].value = value;
            ht->entries[idx].used = TRUE;
            ht->count++;
            return;
        }
        if (ht->equal_func(ht->entries[idx].key, key)) {
            /* 更新 */
            if (ht->value_destroy_func && ht->entries[idx].value)
                ht->value_destroy_func(ht->entries[idx].value);
            ht->entries[idx].value = value;
            return;
        }
    }
}

gpointer g_hash_table_lookup(GHashTable *ht, gconstpointer key)
{
    if (!ht || ht->count == 0) return NULL;

    guint h = ht->hash_func(key);
    for (guint i = 0; i < ht->size; i++) {
        guint idx = (h + i) % ht->size;
        if (!ht->entries[idx].used)
            return NULL;
        if (ht->equal_func(ht->entries[idx].key, key))
            return ht->entries[idx].value;
    }
    return NULL;
}

gboolean g_hash_table_remove(GHashTable *ht, gconstpointer key)
{
    if (!ht || ht->count == 0) return FALSE;

    guint h = ht->hash_func(key);
    for (guint i = 0; i < ht->size; i++) {
        guint idx = (h + i) % ht->size;
        if (!ht->entries[idx].used)
            return FALSE;
        if (ht->equal_func(ht->entries[idx].key, key)) {
            if (ht->key_destroy_func)
                ht->key_destroy_func(ht->entries[idx].key);
            if (ht->value_destroy_func)
                ht->value_destroy_func(ht->entries[idx].value);
            ht->entries[idx].used = FALSE;
            ht->count--;
            return TRUE;
        }
    }
    return FALSE;
}

void g_hash_table_destroy(GHashTable *ht)
{
    if (!ht) return;

    for (guint i = 0; i < ht->size; i++) {
        if (ht->entries[i].used) {
            if (ht->key_destroy_func)
                ht->key_destroy_func(ht->entries[i].key);
            if (ht->value_destroy_func)
                ht->value_destroy_func(ht->entries[i].value);
        }
    }
    ht->count = 0;

    for (int i = 0; i < HASH_TABLE_POOL_SIZE; i++) {
        if (&hashtable_pool[i].ht == ht) {
            hashtable_pool[i].used = 0;
            return;
        }
    }
}

void g_hash_table_unref(GHashTable *ht)
{
    /* 在无引用计数的环境中等同于 destroy */
    g_hash_table_destroy(ht);
}

guint g_hash_table_size(GHashTable *ht)
{
    return ht ? ht->count : 0;
}

gboolean g_hash_table_contains(GHashTable *ht, gconstpointer key)
{
    return g_hash_table_lookup(ht, key) != NULL;
}

void g_hash_table_foreach(GHashTable *ht,
                           void (*func)(gpointer key, gpointer value, gpointer user_data),
                           gpointer user_data)
{
    if (!ht || !func) return;

    for (guint i = 0; i < ht->size; i++) {
        if (ht->entries[i].used)
            func(ht->entries[i].key, ht->entries[i].value, user_data);
    }
}

GList *g_hash_table_get_keys(GHashTable *ht)
{
    GList *list = NULL;
    if (!ht) return NULL;

    for (guint i = 0; i < ht->size; i++) {
        if (ht->entries[i].used)
            list = g_list_append(list, ht->entries[i].key);
    }
    return list;
}

GList *g_hash_table_get_values(GHashTable *ht)
{
    GList *list = NULL;
    if (!ht) return NULL;

    for (guint i = 0; i < ht->size; i++) {
        if (ht->entries[i].used)
            list = g_list_append(list, ht->entries[i].value);
    }
    return list;
}

void g_hash_table_replace(GHashTable *ht, gpointer key, gpointer value)
{
    if (!ht) return;

    /* 同 insert, 但先移除旧键 */
    guint h = ht->hash_func(key);
    for (guint i = 0; i < ht->size; i++) {
        guint idx = (h + i) % ht->size;
        if (!ht->entries[idx].used)
            break;
        if (ht->equal_func(ht->entries[idx].key, key)) {
            if (ht->key_destroy_func)
                ht->key_destroy_func(ht->entries[idx].key);
            if (ht->value_destroy_func)
                ht->value_destroy_func(ht->entries[idx].value);
            ht->entries[idx].used = FALSE;
            ht->count--;
            break;
        }
    }
    g_hash_table_insert(ht, key, value);
}

/* ==================== GError ==================== */

void g_clear_error(GError **error)
{
    if (!error || !*error) return;

    for (int i = 0; i < ERROR_SLOTS; i++) {
        if (&error_slots[i] == *error) {
            error_used[i] = 0;
            break;
        }
    }
    *error = NULL;
}

void g_propagate_error(GError **dest, GError *src)
{
    if (!dest) {
        g_clear_error(&src);
        return;
    }
    g_clear_error(dest);
    *dest = src;
}

void g_set_error(GError **err, guint domain, gint code, const gchar *format, ...)
{
    if (!err) return;

    g_clear_error(err);

    /* 找空闲槽 */
    GError *slot = NULL;
    for (int i = 0; i < ERROR_SLOTS; i++) {
        if (!error_used[i]) {
            error_used[i] = 1;
            slot = &error_slots[i];
            break;
        }
    }
    if (!slot) return;

    slot->domain = domain;
    slot->code = code;

    va_list ap;
    va_start(ap, format);
    char buf[256];
    vsnprintf_impl(buf, 256, format, ap);
    va_end(ap);

    slot->message = g_strdup(buf);
    *err = slot;
}

/* ==================== 日志 ==================== */

void g_log(const gchar *log_domain, int log_level, const gchar *format, ...)
{
    char buf[512];
    va_list ap;
    va_start(ap, format);
    vsnprintf_impl(buf, 512, format, ap);
    va_end(ap);

    if (default_log_handler) {
        default_log_handler(log_domain, log_level, buf, default_log_userdata);
    } else {
        g_log_default_handler(log_domain, log_level, buf, NULL);
    }
}

void g_log_default_handler(const gchar *log_domain, int log_level,
                            const gchar *message, gpointer unused_data)
{
    (void)unused_data;
    const char *prefix = "LOG";
    switch (log_level) {
    case G_LOG_LEVEL_ERROR:    prefix = "ERROR"; break;
    case G_LOG_LEVEL_CRITICAL: prefix = "CRIT";  break;
    case G_LOG_LEVEL_WARNING:  prefix = "WARN";  break;
    case G_LOG_LEVEL_MESSAGE:  prefix = "MSG";   break;
    case G_LOG_LEVEL_INFO:     prefix = "INFO";  break;
    case G_LOG_LEVEL_DEBUG:    prefix = "DEBUG"; break;
    }

    /* PlexsDOS 串口输出 */
    const char *dom = log_domain ? log_domain : "PLXDM";
    /* 简易输出 — 在实际环境中会通过串口写入 */
    volatile int unused = 0;
    (void)unused;
    /* stubbed out for freestanding */
}

void g_log_set_default_handler(GLogFunc log_func, gpointer user_data)
{
    default_log_handler = log_func;
    default_log_userdata = user_data;
}

void g_log_set_handler(const gchar *log_domain, int log_levels,
                        GLogFunc log_func, gpointer user_data)
{
    (void)log_domain;
    (void)log_levels;
    (void)log_func;
    (void)user_data;
    /* 简化为设置默认 handler */
    if (log_func)
        g_log_set_default_handler(log_func, user_data);
}

/* ==================== GMainLoop ==================== */

GMainLoop *g_main_loop_new(gpointer context, gboolean is_running)
{
    (void)context;
    static GMainLoop loop_storage;
    loop_storage.running = is_running;
    loop_storage.is_running = is_running;
    return &loop_storage;
}

void g_main_loop_run(GMainLoop *loop)
{
    if (!loop) return;
    loop->running = TRUE;
    loop->is_running = TRUE;

    while (loop->running) {
        /* 检查超时/空闲源 */
        guint64 now = get_ticks();
        for (int i = 0; i < SOURCE_POOL_SIZE; i++) {
            if (!sources[i].used) continue;

            gboolean fire = FALSE;
            if (sources[i].is_idle) {
                fire = TRUE;  /* idle 每次都触发 */
            } else if (sources[i].expire_tick <= now) {
                fire = TRUE;
            }

            if (fire) {
                gboolean keep = sources[i].function(sources[i].data);
                if (sources[i].is_idle) {
                    if (!keep) {
                        sources[i].used = 0;
                    }
                } else {
                    if (keep) {
                        sources[i].expire_tick = now + sources[i].interval_ms;
                    } else {
                        sources[i].used = 0;
                    }
                }
            }
        }
    }
}

void g_main_loop_quit(GMainLoop *loop)
{
    if (loop) {
        loop->running = FALSE;
        loop->is_running = FALSE;
    }
}

gboolean g_main_loop_is_running(GMainLoop *loop)
{
    return loop ? loop->is_running : FALSE;
}

gboolean g_main_context_iteration(gpointer context, gboolean may_block)
{
    (void)context;
    (void)may_block;
    return FALSE;
}

guint g_timeout_add(guint interval_ms, GSourceFunc function, gpointer data)
{
    if (!function) return 0;

    for (int i = 0; i < SOURCE_POOL_SIZE; i++) {
        if (!sources[i].used) {
            sources[i].used = 1;
            sources[i].id = next_source_id++;
            sources[i].is_idle = FALSE;
            sources[i].interval_ms = interval_ms;
            sources[i].expire_tick = get_ticks() + interval_ms;
            sources[i].function = function;
            sources[i].data = data;
            return sources[i].id;
        }
    }
    return 0;
}

guint g_idle_add(GSourceFunc function, gpointer data)
{
    if (!function) return 0;

    for (int i = 0; i < SOURCE_POOL_SIZE; i++) {
        if (!sources[i].used) {
            sources[i].used = 1;
            sources[i].id = next_source_id++;
            sources[i].is_idle = TRUE;
            sources[i].function = function;
            sources[i].data = data;
            return sources[i].id;
        }
    }
    return 0;
}

gboolean g_source_remove(guint source_id)
{
    for (int i = 0; i < SOURCE_POOL_SIZE; i++) {
        if (sources[i].used && sources[i].id == source_id) {
            sources[i].used = 0;
            return TRUE;
        }
    }
    return FALSE;
}

/* ==================== GKeyFile ==================== */

GKeyFile *g_key_file_new(void)
{
    static GKeyFile keyfile_storage;
    keyfile_storage.group_count = 0;
    for (int i = 0; i < 32; i++) {
        keyfile_storage.groups[i].group[0] = '\0';
        keyfile_storage.groups[i].entry_count = 0;
    }
    return &keyfile_storage;
}

void g_key_file_free(GKeyFile *key_file)
{
    if (!key_file) return;
    key_file->group_count = 0;
    for (int i = 0; i < 32; i++) {
        key_file->groups[i].group[0] = '\0';
        key_file->groups[i].entry_count = 0;
    }
}

static int key_file_find_group(GKeyFile *key_file, const gchar *group_name)
{
    for (int i = 0; i < key_file->group_count; i++) {
        int j = 0;
        while (key_file->groups[i].group[j] && group_name[j]
               && key_file->groups[i].group[j] == group_name[j])
            j++;
        if (key_file->groups[i].group[j] == '\0' && group_name[j] == '\0')
            return i;
    }
    return -1;
}

static int key_file_add_group(GKeyFile *key_file, const gchar *group_name)
{
    if (key_file->group_count >= 32) return -1;
    int idx = key_file->group_count++;
    int j = 0;
    while (group_name[j] && j < 63) {
        key_file->groups[idx].group[j] = group_name[j];
        j++;
    }
    key_file->groups[idx].group[j] = '\0';
    key_file->groups[idx].entry_count = 0;
    return idx;
}

static int key_file_find_entry(GKeyFile *key_file, int group_idx, const gchar *key)
{
    for (int i = 0; i < key_file->groups[group_idx].entry_count; i++) {
        int j = 0;
        while (key_file->groups[group_idx].entries[i].key[j] && key[j]
               && key_file->groups[group_idx].entries[i].key[j] == key[j])
            j++;
        if (key_file->groups[group_idx].entries[i].key[j] == '\0' && key[j] == '\0')
            return i;
    }
    return -1;
}

gboolean g_key_file_load_from_file(GKeyFile *key_file, const gchar *file,
                                    guint flags, GError **error)
{
    (void)flags;
    if (!key_file || !file) {
        if (error)
            g_set_error(error, 1, 1, "Invalid arguments");
        return FALSE;
    }

    /* PlexsDOS: file I/O through HAL layer */
    /* 打开文件 */
    int fd = open(file, 0); /* O_RDONLY = 0 */
    if (fd < 0) {
        if (error)
            g_set_error(error, 1, 2, "Cannot open file %s", file);
        return FALSE;
    }

    char buf[4096];
    int total = 0;
    int n;
    while ((n = read(fd, buf + total, 4096 - total)) > 0)
        total += n;
    close(fd);

    if (total == 0) {
        return TRUE; /* 空文件 */
    }
    buf[total] = '\0';

    return g_key_file_load_from_data(key_file, buf, total, flags, error);
}

gboolean g_key_file_load_from_data(GKeyFile *key_file, const gchar *data,
                                    gsize length, guint flags, GError **error)
{
    (void)flags;
    if (!key_file || !data) {
        if (error)
            g_set_error(error, 1, 1, "Invalid arguments");
        return FALSE;
    }

    g_key_file_free(key_file);

    int pos = 0;
    char current_group[64] = "";
    int line = 1;

    while (pos < (int)length) {
        /* 读取一行 */
        char line_buf[512];
        int lp = 0;
        while (pos < (int)length && data[pos] != '\n' && lp < 510) {
            if (data[pos] != '\r')
                line_buf[lp++] = data[pos];
            pos++;
        }
        line_buf[lp] = '\0';
        if (data[pos] == '\n') pos++;
        if (data[pos] == '\r') pos++;

        /* 跳过空行和注释 */
        char *trimmed = line_buf;
        while (*trimmed == ' ' || *trimmed == '\t') trimmed++;
        if (*trimmed == '\0' || *trimmed == '#' || *trimmed == ';') {
            line++;
            continue;
        }

        /* 组名行 */
        if (*trimmed == '[') {
            trimmed++;
            int gn = 0;
            while (*trimmed && *trimmed != ']' && gn < 63) {
                current_group[gn++] = *trimmed;
                trimmed++;
            }
            current_group[gn] = '\0';
            if (key_file_find_group(key_file, current_group) < 0)
                key_file_add_group(key_file, current_group);
            line++;
            continue;
        }

        /* 键值对行 */
        if (current_group[0] == '\0') continue;
        int group_idx = key_file_find_group(key_file, current_group);
        if (group_idx < 0) continue;
        if (key_file->groups[group_idx].entry_count >= 64) continue;

        char key[64], value[256];
        int ki = 0, vi = 0;
        int eq_found = 0;

        while (*trimmed && *trimmed != '=' && ki < 63)
            key[ki++] = *trimmed++;
        if (*trimmed == '=') { eq_found = 1; trimmed++; }

        /* 去除 key 尾部空格 */
        while (ki > 0 && (key[ki - 1] == ' ' || key[ki - 1] == '\t')) ki--;
        key[ki] = '\0';
        if (!eq_found) continue;

        /* 去除 value 前导空格 */
        while (*trimmed == ' ' || *trimmed == '\t') trimmed++;

        /* 去除 value 尾部引号/空格 */
        while (*trimmed && vi < 255)
            value[vi++] = *trimmed++;
        while (vi > 0 && (value[vi - 1] == ' ' || value[vi - 1] == '\t'))
            vi--;
        value[vi] = '\0';

        int entry_idx = key_file->groups[group_idx].entry_count++;
        int j;
        for (j = 0; key[j] && j < 63; j++)
            key_file->groups[group_idx].entries[entry_idx].key[j] = key[j];
        key_file->groups[group_idx].entries[entry_idx].key[j] = '\0';
        for (j = 0; value[j] && j < 255; j++)
            key_file->groups[group_idx].entries[entry_idx].value[j] = value[j];
        key_file->groups[group_idx].entries[entry_idx].value[j] = '\0';
    }

    return TRUE;
}

gchar *g_key_file_get_string(GKeyFile *key_file, const gchar *group_name,
                              const gchar *key, GError **error)
{
    if (!key_file || !group_name || !key) {
        if (error) g_set_error(error, 1, 1, "Invalid arguments");
        return NULL;
    }

    int g = key_file_find_group(key_file, group_name);
    if (g < 0) {
        if (error) g_set_error(error, 1, 2, "Group '%s' not found", group_name);
        return NULL;
    }

    int e = key_file_find_entry(key_file, g, key);
    if (e < 0) {
        if (error) g_set_error(error, 1, 3, "Key '%s' not found in group '%s'", key, group_name);
        return NULL;
    }

    return g_strdup(key_file->groups[g].entries[e].value);
}

gboolean g_key_file_has_key(GKeyFile *key_file, const gchar *group_name,
                             const gchar *key, GError **error)
{
    (void)error;
    if (!key_file || !group_name || !key) return FALSE;

    int g = key_file_find_group(key_file, group_name);
    if (g < 0) return FALSE;

    return key_file_find_entry(key_file, g, key) >= 0;
}

gboolean g_key_file_has_group(GKeyFile *key_file, const gchar *group_name)
{
    if (!key_file || !group_name) return FALSE;
    return key_file_find_group(key_file, group_name) >= 0;
}

gint g_key_file_get_integer(GKeyFile *key_file, const gchar *group_name,
                             const gchar *key, GError **error)
{
    gchar *val = g_key_file_get_string(key_file, group_name, key, error);
    if (!val) return 0;

    int result = 0;
    int sign = 1;
    const char *p = val;
    if (*p == '-') { sign = -1; p++; }
    while (*p >= '0' && *p <= '9') {
        result = result * 10 + (*p - '0');
        p++;
    }
    g_free(val);
    return result * sign;
}

gboolean g_key_file_get_boolean(GKeyFile *key_file, const gchar *group_name,
                                 const gchar *key, GError **error)
{
    gchar *val = g_key_file_get_string(key_file, group_name, key, error);
    if (!val) return FALSE;

    gboolean ret = FALSE;
    if (g_ascii_strcasecmp(val, "true") == 0 ||
        g_ascii_strcasecmp(val, "1") == 0 ||
        g_ascii_strcasecmp(val, "yes") == 0)
        ret = TRUE;
    g_free(val);
    return ret;
}

gchar **g_key_file_get_string_list(GKeyFile *key_file, const gchar *group_name,
                                    const gchar *key, gsize *length,
                                    GError **error)
{
    gchar *val = g_key_file_get_string(key_file, group_name, key, error);
    if (!val) {
        if (length) *length = 0;
        return NULL;
    }

    /* 以分号分隔 */
    gchar **result = g_strsplit(val, ";", 0);
    g_free(val);
    if (length) {
        *length = 0;
        if (result) {
            while (result[*length]) (*length)++;
        }
    }
    return result;
}

gchar **g_key_file_get_keys(GKeyFile *key_file, const gchar *group_name,
                             gsize *length, GError **error)
{
    if (!key_file || !group_name) {
        if (error) g_set_error(error, 1, 1, "Invalid arguments");
        if (length) *length = 0;
        return NULL;
    }

    int g = key_file_find_group(key_file, group_name);
    if (g < 0) {
        if (error) g_set_error(error, 1, 2, "Group '%s' not found", group_name);
        if (length) *length = 0;
        return NULL;
    }

    int count = key_file->groups[g].entry_count;
    gchar **result = (gchar **)malloc(sizeof(gchar *) * (count + 1));
    if (!result) {
        if (length) *length = 0;
        return NULL;
    }

    for (int i = 0; i < count; i++) {
        result[i] = g_strdup(key_file->groups[g].entries[i].key);
    }
    result[count] = NULL;

    if (length) *length = count;
    return result;
}

gchar **g_key_file_get_groups(GKeyFile *key_file, gsize *length)
{
    if (!key_file) {
        if (length) *length = 0;
        return NULL;
    }

    gchar **result = (gchar **)malloc(sizeof(gchar *) * (key_file->group_count + 1));
    if (!result) {
        if (length) *length = 0;
        return NULL;
    }

    for (int i = 0; i < key_file->group_count; i++)
        result[i] = g_strdup(key_file->groups[i].group);
    result[key_file->group_count] = NULL;

    if (length) *length = key_file->group_count;
    return result;
}

/* ==================== 信号系统 ==================== */

void plxdm_signal_connect(gpointer obj, const char *signal_name,
                           GCallback handler, gpointer user_data)
{
    if (!obj || !signal_name || !handler) return;

    for (int i = 0; i < SIGNAL_CONN_POOL_SIZE; i++) {
        if (!signal_conns[i].used) {
            signal_conns[i].used = 1;
            signal_conns[i].obj = obj;

            int j = 0;
            while (signal_name[j] && j < 47) {
                signal_conns[i].signal_name[j] = signal_name[j];
                j++;
            }
            signal_conns[i].signal_name[j] = '\0';

            signal_conns[i].handler = handler;
            signal_conns[i].user_data = user_data;
            return;
        }
    }
}

void plxdm_signal_emit(gpointer obj, const char *signal_name)
{
    if (!obj || !signal_name) return;

    for (int i = 0; i < SIGNAL_CONN_POOL_SIZE; i++) {
        if (signal_conns[i].used && signal_conns[i].obj == obj) {
            /* 匹配信号名 */
            int match = 1;
            int j = 0;
            while (signal_conns[i].signal_name[j] && signal_name[j]) {
                if (signal_conns[i].signal_name[j] != signal_name[j]) {
                    match = 0;
                    break;
                }
                j++;
            }
            if (match && signal_conns[i].signal_name[j] == '\0'
                      && signal_name[j] == '\0') {
                /* 调信号处理函数 */
                void (*handler)(gpointer, gpointer) = (void (*)(gpointer, gpointer))signal_conns[i].handler;
                handler(signal_conns[i].obj, signal_conns[i].user_data);
            }
        }
    }
}

void plxdm_signal_disconnect_by_func(gpointer obj, GCallback handler)
{
    if (!obj || !handler) return;

    for (int i = 0; i < SIGNAL_CONN_POOL_SIZE; i++) {
        if (signal_conns[i].used && signal_conns[i].obj == obj
            && signal_conns[i].handler == handler) {
            signal_conns[i].used = 0;
        }
    }
}

/* ==================== GObject 轻量化替代 ==================== */

gpointer plxdm_object_new(PlxdmClass *class_info, ...)
{
    if (!class_info) return NULL;

    /* 使用静态池分配对象内存 */
    static char obj_pool[2048];
    static int obj_used = 0;

    gsize size = class_info->instance_size;
    if (obj_used + (int)size > 2048) return NULL;

    gpointer obj = &obj_pool[obj_used];
    obj_used += size;

    /* 清零 */
    char *p = (char *)obj;
    for (gsize i = 0; i < size; i++)
        p[i] = 0;

    /* 设置对象头 */
    PlxdmObject *hdr = (PlxdmObject *)obj;
    hdr->class_name = class_info->class_name;
    hdr->ref_count = 1;
    hdr->finalize = class_info->finalize;

    /* 调用 init */
    if (class_info->init)
        class_info->init(obj);

    return obj;
}

gpointer plxdm_object_new0(gsize size, const char *class_name,
                            void (*init)(gpointer),
                            void (*finalize)(gpointer))
{
    static char obj_pool2[2048];
    static int obj_used2 = 0;

    if (obj_used2 + (int)size > 2048) return NULL;

    gpointer obj = &obj_pool2[obj_used2];
    obj_used2 += size;

    char *p = (char *)obj;
    for (gsize i = 0; i < size; i++)
        p[i] = 0;

    PlxdmObject *hdr = (PlxdmObject *)obj;
    hdr->class_name = class_name;
    hdr->ref_count = 1;
    hdr->finalize = finalize;

    if (init)
        init(obj);

    return obj;
}

/* ==================== GPtrArray ==================== */

GPtrArray *g_ptr_array_new(void)
{
    for (int i = 0; i < PTR_ARRAY_POOL_SIZE; i++) {
        if (!ptrarray_pool[i].used) {
            ptrarray_pool[i].used = 1;
            ptrarray_pool[i].arr.pdata = ptrarray_pool[i].data;
            ptrarray_pool[i].arr.len = 0;
            return &ptrarray_pool[i].arr;
        }
    }
    return NULL;
}

GPtrArray *g_ptr_array_new_with_free_func(void (*free_func)(gpointer))
{
    GPtrArray *arr = g_ptr_array_new();
    if (arr) {
        (void)free_func;
        /* 在无堆环境中，我们将 free_func 存储在另一个位置 */
    }
    return arr;
}

void g_ptr_array_add(GPtrArray *array, gpointer data)
{
    if (!array) return;

    /* 查找所属池 */
    for (int i = 0; i < PTR_ARRAY_POOL_SIZE; i++) {
        if (ptrarray_pool[i].used && &ptrarray_pool[i].arr == array) {
            if (array->len < PTR_ARRAY_DATA_SIZE) {
                array->pdata[array->len++] = data;
            }
            return;
        }
    }
}

void g_ptr_array_free(GPtrArray *array, gboolean free_seg)
{
    if (!array) return;
    (void)free_seg;

    for (int i = 0; i < PTR_ARRAY_POOL_SIZE; i++) {
        if (ptrarray_pool[i].used && &ptrarray_pool[i].arr == array) {
            ptrarray_pool[i].used = 0;
            return;
        }
    }
}

void g_ptr_array_set_free_func(GPtrArray *array, void (*free_func)(gpointer))
{
    (void)array;
    (void)free_func;
}

/* ==================== GTimer ==================== */

GTimer *g_timer_new(void)
{
    for (int i = 0; i < TIMER_POOL_SIZE; i++) {
        if (!timer_pool[i].used) {
            timer_pool[i].used = 1;
            timer_pool[i].timer.start = 0.0;
            return &timer_pool[i].timer;
        }
    }
    return NULL;
}

double g_timer_elapsed(GTimer *timer, gulong *microseconds)
{
    if (!timer) return 0.0;

    /* 使用系统 tick 计算经过时间 (假设 1 tick ≈ 1ms) */
    double secs = (get_ticks() - (guint64)timer->start) / 1000.0;
    if (microseconds)
        *microseconds = (gulong)(secs * 1000000.0);
    return secs;
}

void g_timer_destroy(GTimer *timer)
{
    if (!timer) return;

    for (int i = 0; i < TIMER_POOL_SIZE; i++) {
        if (&timer_pool[i].timer == timer) {
            timer_pool[i].used = 0;
            return;
        }
    }
}

/* ==================== 路径工具 ==================== */

gchar *plxdm_build_filename(const gchar *first, ...)
{
    if (!first) return NULL;

    /* 计算总长度 */
    int total = 0;
    {
        const gchar *s = first;
        while (*s) { total++; s++; }
    }

    va_list ap;
    va_start(ap, first);
    const gchar *arg;
    while ((arg = va_arg(ap, const gchar *))) {
        total++; /* 分隔符 '/' */
        const gchar *p = arg;
        while (*p) { total++; p++; }
    }
    va_end(ap);

    gchar *result = (gchar *)malloc(total + 1);
    if (!result) return NULL;

    int pos = 0;
    const gchar *s = first;
    while (*s) result[pos++] = *s++;

    va_start(ap, first);
    while ((arg = va_arg(ap, const gchar *))) {
        result[pos++] = '/';
        s = arg;
        while (*s) result[pos++] = *s++;
    }
    va_end(ap);
    result[pos] = '\0';

    return result;
}

/* ==================== GDateTime ==================== */

GDateTime *g_date_time_new_now_local(void)
{
    /* 返回编译时的固定日期 (PlexsDOS 无 RTC) */
    static GDateTime dt;
    dt.year = 2026;
    dt.month = 6;
    dt.day = 2;
    dt.hour = 12;
    dt.minute = 0;
    dt.second = 0;
    return &dt;
}

void g_date_time_unref(GDateTime *dt)
{
    (void)dt;
}

/* ==================== GRegex (简化) ==================== */

GRegex *g_regex_new(const gchar *pattern, guint compile_flags,
                     guint match_flags, GError **error)
{
    (void)compile_flags;
    (void)match_flags;

    if (!pattern) {
        if (error) g_set_error(error, 1, 1, "NULL pattern");
        return NULL;
    }

    static GRegex regex_storage;
    int j = 0;
    while (pattern[j] && j < 63) {
        regex_storage.pattern[j] = pattern[j];
        j++;
    }
    regex_storage.pattern[j] = '\0';
    return &regex_storage;
}

gboolean g_regex_match(GRegex *regex, const gchar *string,
                        guint match_flags, GMatchInfo **match_info)
{
    (void)match_flags;
    (void)match_info;

    if (!regex || !string) return FALSE;

    /* 简化: 使用 glob 风格的匹配 */
    return g_pattern_match_simple(regex->pattern, string);
}

void g_regex_unref(GRegex *regex)
{
    (void)regex;
}

void g_match_info_unref(GMatchInfo *match_info)
{
    (void)match_info;
}

/* ==================== GMarkup (简化) ==================== */

GMarkupParseContext *g_markup_parse_context_new(const GMarkupParser *parser,
                                                 guint flags,
                                                 gpointer user_data,
                                                 void (*user_data_dnotify)(gpointer))
{
    (void)parser;
    (void)flags;
    (void)user_data;
    (void)user_data_dnotify;
    return NULL; /* 不支持 */
}

void g_markup_parse_context_free(GMarkupParseContext *context)
{
    (void)context;
}

/* ==================== 标准 C 函数 (用于 freestanding) ==================== */

/* 注意: atoi/strtol/strtod 在 lib/kstdlib.c 中实现, 此处不再重复定义 */

void exit(int status)
{
    (void)status;
    /* PlexsDOS: 陷入死循环 (由内核重启) */
    while (1) {
        __asm__ volatile ("cli; hlt");
    }
}

/* ==================== 补充存根 ==================== */

gboolean g_error_matches(GError *err, guint domain, gint code)
{
    if (!err) return FALSE;
    return (err->domain == domain && err->code == code);
}

guint g_file_error_quark(void)
{
    return 0x46494C45; /* 'FILE' */
}

GDir *g_dir_open(const gchar *path, guint flags, GError **error)
{
    (void)path; (void)flags; (void)error;
    return NULL; /* PlexsDOS: 无目录支持 */
}

const gchar *g_dir_read_name(GDir *dir)
{
    (void)dir;
    return NULL;
}

void g_dir_close(GDir *dir)
{
    (void)dir;
}

gboolean g_path_is_absolute(const gchar *path)
{
    return path && path[0] == '/';
}

void *g_steal_pointer_impl(void *pp)
{
    gpointer *real_pp = (gpointer *)pp;
    if (!real_pp) return NULL;
    gpointer val = *real_pp;
    *real_pp = NULL;
    return val;
}

void g_printerr(const gchar *format, ...)
{
    va_list args;
    va_start(args, format);
    /* PlexsDOS: 输出到串口或控制台 */
    va_end(args);
}

void g_free(gpointer ptr)
{
    free(ptr);
}

gint g_strv_length(gchar **str_array)
{
    if (!str_array) return 0;
    gint n = 0;
    while (str_array[n]) n++;
    return n;
}

const gchar * const *g_get_system_data_dirs(void)
{
    static const gchar *dirs[] = {"/system/share", NULL};
    return dirs;
}

const gchar * const *g_get_system_config_dirs(void)
{
    static const gchar *dirs[] = {"/system/config", NULL};
    return dirs;
}

gchar *g_path_get_basename(const gchar *path)
{
    if (!path || !*path) return g_strdup(".");
    const char *p = path;
    const char *last = path;
    while (*p) {
        if (*p == '/') last = p + 1;
        p++;
    }
    return g_strdup(last);
}

gboolean g_key_file_set_string(GKeyFile *key_file,
                               const gchar *group_name,
                               const gchar *key,
                               const gchar *value)
{
    return g_key_file_set_value(key_file, group_name, key, value);
}

gboolean g_key_file_set_string_list(GKeyFile *key_file,
                                    const gchar *group_name,
                                    const gchar *key,
                                    const gchar * const *list,
                                    gsize length)
{
    /* 简化: 只存第一个元素 */
    if (length > 0 && list && list[0])
        return g_key_file_set_value(key_file, group_name, key, list[0]);
    return FALSE;
}

gboolean g_key_file_set_integer(GKeyFile *key_file,
                                const gchar *group_name,
                                const gchar *key,
                                gint value)
{
    char buf[32];
    int pos = 0;
    gint v = value;
    /* Handle negative */
    if (v < 0) { buf[pos++] = '-'; v = -v; }
    /* Write digits in reverse then reverse */
    char rev[16];
    int rpos = 0;
    if (v == 0) rev[rpos++] = '0';
    while (v > 0 && rpos < 16) { rev[rpos++] = '0' + (v % 10); v /= 10; }
    while (rpos > 0 && pos < (int)sizeof(buf) - 1) buf[pos++] = rev[--rpos];
    buf[pos] = '\0';
    return g_key_file_set_value(key_file, group_name, key, buf);
}

gboolean g_key_file_set_boolean(GKeyFile *key_file,
                                const gchar *group_name,
                                const gchar *key,
                                gboolean value)
{
    return g_key_file_set_value(key_file, group_name, key, value ? "true" : "false");
}

gchar *g_strchomp(gchar *str)
{
    if (!str) return NULL;
    gsize len = 0;
    while (str[len]) len++;
    while (len > 0 && (str[len-1] == ' ' || str[len-1] == '\t' || str[len-1] == '\n' || str[len-1] == '\r'))
        len--;
    str[len] = '\0';
    return str;
}

/* ==================== 新增存根实现 ==================== */

gchar *g_key_file_to_data(GKeyFile *key_file, gsize *length, GError **error)
{
    (void)key_file;
    if (length) *length = 0;
    if (error) *error = NULL;
    return g_strdup("");
}

gboolean g_file_set_contents(const gchar *filename, const gchar *contents,
                              gssize length, GError **error)
{
    (void)filename; (void)contents; (void)length; (void)error;
    return TRUE;
}

gboolean g_file_get_contents(const gchar *filename, gchar **contents,
                              gsize *length, GError **error)
{
    (void)filename; (void)length; (void)error;
    if (contents) *contents = g_strdup("");
    return TRUE;
}

const gchar *g_strerror(gint errnum)
{
    (void)errnum;
    return "Unknown error";
}

int g_open(const gchar *path, int flags, ...)
{
    (void)path; (void)flags;
    return -1;
}

guint g_file_error_from_errno(gint err_no)
{
    (void)err_no;
    return G_FILE_ERROR_NOENT;
}

guint g_key_file_error_quark(void)
{
    return 0x4B464559; /* "KEY" */
}

gboolean g_spawn_sync(const char *working_directory, char **argv, char **envp,
                       GSpawnFlags flags,
                       void (*child_setup)(gpointer), gpointer user_data,
                       gchar **standard_output, gchar **standard_error,
                       int *exit_status, GError **error)
{
    (void)working_directory; (void)argv; (void)envp; (void)flags;
    (void)child_setup; (void)user_data; (void)standard_output; (void)standard_error;
    if (exit_status) *exit_status = 0;
    if (error) *error = NULL;
    return TRUE;
}

gboolean g_spawn_command_line_sync(const char *command_line,
                                    gchar **standard_output,
                                    gchar **standard_error,
                                    int *exit_status, GError **error)
{
    (void)command_line; (void)standard_output; (void)standard_error;
    if (exit_status) *exit_status = 0;
    if (error) *error = NULL;
    return TRUE;
}

gboolean g_spawn_command_line_async(const char *command_line, GError **error)
{
    (void)command_line; (void)error;
    return TRUE;
}

gboolean g_shell_parse_argv(const char *command_line, int *argcp,
                             char ***argvp, GError **error)
{
    (void)command_line; (void)error;
    if (argcp) *argcp = 0;
    if (argvp) *argvp = NULL;
    return TRUE;
}

gchar *g_shell_quote(const char *unquoted_string)
{
    return g_strdup(unquoted_string);
}

gboolean g_file_test(const char *filename, guint test)
{
    (void)filename; (void)test;
    return FALSE;
}

int g_snprintf(char *buf, unsigned int size, const char *fmt, ...)
{
    va_list ap;
    va_start(ap, fmt);
    int r = vsnprintf_impl(buf, size, fmt, ap);
    va_end(ap);
    return r;
}

int g_vsnprintf(char *buf, unsigned int size, const char *fmt, va_list ap)
{
    return vsnprintf_impl(buf, size, fmt, ap);
}

const char *g_strsignal(int sig)
{
    return strsignal(sig);
}

/* ==================== D-Bus 存根 ==================== */

GDBusConnection *g_bus_get_sync(GBusType bus_type, void *cancellable, GError **error)
{
    (void)bus_type; (void)cancellable;
    if (error) *error = NULL;
    return NULL;
}

GVariant *g_dbus_connection_call_sync(GDBusConnection *conn, const char *bus_name,
                                       const char *object_path,
                                       const char *interface_name,
                                       const char *method_name,
                                       GVariant *parameters, GVariant *reply_type,
                                       GDBusCallFlags flags, int timeout_msec,
                                       void *cancellable, GError **error)
{
    (void)conn; (void)bus_name; (void)object_path; (void)interface_name;
    (void)method_name; (void)parameters; (void)reply_type;
    (void)flags; (void)timeout_msec; (void)cancellable;
    if (error) *error = NULL;
    return NULL;
}

GVariant *g_variant_new(const char *format_string, ...)
{
    (void)format_string;
    return NULL;
}

void g_variant_get(GVariant *value, const char *format_string, ...)
{
    (void)value; (void)format_string;
}

void g_variant_unref(GVariant *value)
{
    (void)value;
}

gboolean g_variant_is_of_type(GVariant *value, const void *type)
{
    (void)value; (void)type;
    return FALSE;
}

gboolean g_variant_get_boolean(GVariant *value)
{
    (void)value;
    return FALSE;
}

const char *g_variant_get_type_string(GVariant *value)
{
    (void)value;
    return "s";
}

void g_variant_builder_init(void *builder, const void *type)
{
    (void)builder; (void)type;
}

void g_variant_builder_open(void *builder, const void *type)
{
    (void)builder; (void)type;
}

void g_variant_builder_add(void *builder, const char *format, ...)
{
    (void)builder; (void)format;
}

gboolean g_variant_iter_loop(void *iter, const char *format, ...)
{
    (void)iter; (void)format;
    return FALSE;
}

/* ==================== GHashTable 迭代器存根 ==================== */

void g_hash_table_iter_init(GHashTableIter *iter, GHashTable *ht)
{
    iter->pos = 0;
    (void)ht;
}

gboolean g_hash_table_iter_next(GHashTableIter *iter, gpointer *key, gpointer *value)
{
    (void)key; (void)value;
    iter->pos++;
    return FALSE;
}

/* ==================== D-Bus Proxy 存根 ==================== */

GDBusProxy *g_dbus_proxy_new_for_bus_sync(GBusType bus_type, GDBusProxyFlags flags,
                                           void *info, const char *name,
                                           const char *object_path,
                                           const char *interface_name,
                                           void *cancellable, GError **error)
{
    (void)bus_type; (void)flags; (void)info; (void)name;
    (void)object_path; (void)interface_name; (void)cancellable;
    if (error) *error = NULL;
    return NULL;
}

GVariant *g_dbus_proxy_call_sync(GDBusProxy *proxy, const char *method,
                                  GVariant *parameters, GDBusCallFlags flags,
                                  int timeout_msec, void *cancellable,
                                  GError **error)
{
    (void)proxy; (void)method; (void)parameters; (void)flags;
    (void)timeout_msec; (void)cancellable;
    if (error) *error = NULL;
    return NULL;
}

/* ==================== 更多存根实现 ==================== */

gchar **g_strdupv(gchar **str_array)
{
    if (!str_array) return NULL;
    int len = 0;
    while (str_array[len]) len++;
    gchar **copy = (gchar**)g_malloc(sizeof(gchar*) * (len + 1));
    for (int i = 0; i < len; i++)
        copy[i] = g_strdup(str_array[i]);
    copy[len] = NULL;
    return copy;
}

guint g_child_watch_add(guint pid, void *function, gpointer data)
{
    (void)pid; (void)function; (void)data;
    return 0;
}

gchar *g_strjoin(const char *separator, ...)
{
    (void)separator;
    return g_strdup("");
}

GArray *g_array_sized_new(gboolean zero_terminated, gboolean clear_,
                           guint element_size, guint reserved_size)
{
    (void)zero_terminated; (void)clear_; (void)element_size; (void)reserved_size;
    return (GArray*)g_malloc0(sizeof(GArray));
}

void g_array_free(GArray *array, gboolean free_segment)
{
    (void)free_segment;
    g_free(array);
}

void g_array_unref(GArray *array)
{
    g_array_free(array, TRUE);
}

gboolean g_dbus_connection_emit_signal(GDBusConnection *conn, const char *bus_name,
                                        const char *object_path,
                                        const char *interface_name,
                                        const char *signal_name,
                                        GVariant *parameters, GError **error)
{
    (void)conn; (void)bus_name; (void)object_path; (void)interface_name;
    (void)signal_name; (void)parameters;
    if (error) *error = NULL;
    return TRUE;
}

GVariant *g_variant_new_object_path(const char *object_path)
{
    (void)object_path;
    return NULL;
}

GVariant *g_variant_builder_end(void *builder)
{
    (void)builder;
    return NULL;
}

void g_variant_builder_add_value(void *builder, GVariant *value)
{
    (void)builder; (void)value;
}

guint g_dbus_connection_signal_subscribe(GDBusConnection *conn, const char *bus_name,
                                          const char *interface_name,
                                          const char *member, const char *object_path,
                                          const char *arg0, GDBusSignalFlags flags,
                                          void *callback,
                                          gpointer user_data, void *destroy_notify)
{
    (void)conn; (void)bus_name; (void)interface_name; (void)member;
    (void)object_path; (void)arg0; (void)flags; (void)callback; (void)user_data; (void)destroy_notify;
    return 0;
}

void g_dbus_connection_signal_unsubscribe(GDBusConnection *conn, guint subscription_id)
{
    (void)conn; (void)subscription_id;
}

GSocketAddress *g_unix_socket_address_new(const char *path)
{
    (void)path;
    return NULL;
}

void g_source_set_callback(GSource *source, void *function, gpointer data,
                            void *destroy_notify)
{
    (void)source; (void)function; (void)data; (void)destroy_notify;
}

guint g_source_attach(GSource *source, void *context)
{
    (void)source; (void)context;
    return 0;
}

GSocketAddress *g_inet_socket_address_new(GInetAddress *address, guint port)
{
    (void)address; (void)port;
    return NULL;
}

guint g_inet_socket_address_get_port(GInetSocketAddress *address)
{
    (void)address;
    return 0;
}

int g_socket_get_family(GSocket *socket)
{
    (void)socket;
    return G_SOCKET_FAMILY_IPV4;
}

GResolver *g_resolver_get_default(void)
{
    static GResolver r;
    return &r;
}

GList *g_resolver_lookup_by_name(GResolver *resolver, const char *hostname,
                                  void *cancellable, GError **error)
{
    (void)resolver; (void)hostname; (void)cancellable;
    if (error) *error = NULL;
    return NULL;
}

void g_resolver_free_addresses(GList *addresses)
{
    g_list_free(addresses);
}

int g_inet_address_get_family(GInetAddress *address)
{
    (void)address;
    return G_SOCKET_FAMILY_IPV4;
}

gboolean g_inet_address_equal(GInetAddress *a, GInetAddress *b)
{
    (void)a; (void)b;
    return FALSE;
}

gssize g_socket_send_to(GSocket *socket, GSocketAddress *address,
                         const void *buffer, gsize size,
                         void *cancellable, GError **error)
{
    (void)socket; (void)address; (void)buffer; (void)size;
    (void)cancellable;
    if (error) *error = NULL;
    return 0;
}

gssize g_socket_receive_from(GSocket *socket, GSocketAddress **address,
                              void *buffer, gsize size,
                              void *cancellable, GError **error)
{
    (void)socket; (void)address; (void)buffer; (void)size;
    (void)cancellable;
    if (error) *error = NULL;
    return 0;
}

gboolean g_inet_address_get_is_loopback(GInetAddress *address)
{
    (void)address;
    return FALSE;
}

/* ==================== GIOChannel 存根 ==================== */

GIOChannel *g_io_channel_unix_new(int fd)
{
    static GIOChannel ch;
    ch.fd = fd;
    return &ch;
}

void g_io_channel_unref(GIOChannel *channel) { (void)channel; }
void g_io_channel_set_encoding(GIOChannel *channel, const char *encoding, GError **error)
{
    (void)channel; (void)encoding; (void)error;
}
void g_io_channel_set_buffered(GIOChannel *channel, gboolean buffered)
{
    (void)channel; (void)buffered;
}
int g_io_channel_write_chars(GIOChannel *channel, const char *buf,
                              unsigned int count, unsigned int *bytes_written,
                              GError **error)
{
    (void)channel; (void)buf; (void)count; (void)error;
    if (bytes_written) *bytes_written = count;
    return 0;
}
int g_io_channel_read_chars(GIOChannel *channel, char *buf,
                             unsigned int count, unsigned int *bytes_read,
                             GError **error)
{
    (void)channel; (void)buf; (void)count; (void)error;
    if (bytes_read) *bytes_read = 0;
    return 0;
}
int g_io_channel_flush(GIOChannel *channel, GError **error)
{
    (void)channel; (void)error;
    return 0;
}
guint g_io_add_watch(GIOChannel *channel, int condition,
                      void *function, gpointer data)
{
    (void)channel; (void)condition; (void)function; (void)data;
    return 0;
}

/* ==================== GSocket 存根 ==================== */

GSocket *g_socket_new(int family, int type, int protocol, GError **error)
{
    (void)family; (void)type; (void)protocol; (void)error;
    return NULL;
}

gboolean g_socket_bind(GSocket *socket, GSocketAddress *address,
                        gboolean allow_reuse, GError **error)
{
    (void)socket; (void)address; (void)allow_reuse; (void)error;
    return FALSE;
}

gboolean g_socket_listen(GSocket *socket, GError **error)
{
    (void)socket; (void)error;
    return FALSE;
}

GSocket *g_socket_accept(GSocket *socket, void *cancellable, GError **error)
{
    (void)socket; (void)cancellable; (void)error;
    return NULL;
}

void g_socket_close(GSocket *socket, GError **error)
{
    (void)socket; (void)error;
}

int g_socket_get_fd(GSocket *socket)
{
    (void)socket;
    return -1;
}

GSocketAddress *g_socket_get_remote_address(GSocket *socket, GError **error)
{
    (void)socket; (void)error;
    return NULL;
}

GSource *g_socket_create_source(GSocket *socket, int condition, void *cancellable)
{
    (void)socket; (void)condition; (void)cancellable;
    return NULL;
}

/* ==================== GInetAddress 存根 ==================== */

GInetAddress *g_inet_address_new_from_bytes(const void *bytes, int family)
{
    (void)bytes; (void)family;
    return NULL;
}

gchar *g_inet_address_to_string(GInetAddress *address)
{
    (void)address;
    return g_strdup("0.0.0.0");
}

gboolean g_inet_address_get_is_link_local(GInetAddress *address)
{
    (void)address;
    return FALSE;
}

GInetAddress *g_inet_socket_address_get_address(GInetSocketAddress *address)
{
    (void)address;
    return NULL;
}

/* ==================== g_print / kill ==================== */

void g_print(const char *format, ...)
{
    (void)format;
}

int kill(int pid, int sig)
{
    (void)pid; (void)sig;
    return 0;
}

/* g_malloc 函数实现 (用于函数指针场景) */
#undef g_malloc
void *g_malloc(gsize size)
{
    return malloc(size);
}

/* g_object_unref 函数实现 (用于函数指针场景如 g_list_free_full) */
#undef g_object_unref
void g_object_unref(gpointer obj)
{
    if (!obj) return;
    PlxdmObject *o = (PlxdmObject*)obj;
    if (--o->ref_count <= 0) {
        if (o->finalize) o->finalize(obj);
    }
}

/* g_inet_address_new_any */
GInetAddress *g_inet_address_new_any(int family)
{
    (void)family;
    return NULL;
}

/* g_dbus_method_invocation_return_error */
void g_dbus_method_invocation_return_error(GDBusMethodInvocation *invocation, int domain, int code, const char *format, ...)
{
    (void)invocation; (void)domain; (void)code; (void)format;
}

/* g_dbus_proxy_get_name_owner */
const char *g_dbus_proxy_get_name_owner(GDBusProxy *proxy)
{
    (void)proxy;
    return NULL;
}

/* g_dbus_proxy_get_cached_property */
GVariant *g_dbus_proxy_get_cached_property(GDBusProxy *proxy, const char *name)
{
    (void)proxy; (void)name;
    return NULL;
}

/* g_dbus_proxy_new_sync */
GDBusProxy *g_dbus_proxy_new_sync(GDBusConnection *connection, GDBusProxyFlags flags, void *info, const char *name, const char *object_path, const char *interface_name, void *cancellable, GError **error)
{
    (void)connection; (void)flags; (void)info; (void)name; (void)object_path; (void)interface_name; (void)cancellable;
    if (error) *error = NULL;
    return NULL;
}

/* g_dbus_proxy_get_connection */
GDBusConnection *g_dbus_proxy_get_connection(GDBusProxy *proxy)
{
    (void)proxy;
    return NULL;
}

/* g_dbus_proxy_get_cached_property_names */
gchar **g_dbus_proxy_get_cached_property_names(GDBusProxy *proxy)
{
    (void)proxy;
    return NULL;
}

/* g_subprocess_new — stub */
GSubprocess *g_subprocess_new(GSubprocessFlags flags, GError **error, const char *arg0, ...)
{
    (void)flags; (void)arg0;
    if (error) *error = NULL;
    return NULL;
}

/* ==================== 新增 D-Bus 存根 ==================== */

/* g_dbus_node_info_new_for_xml — 简单解析 XML 节点信息 */
GDBusNodeInfo *g_dbus_node_info_new_for_xml(const char *xml, GError **error)
{
    (void)xml;
    if (error) *error = NULL;
    /* PlexsDOS: 返回最小存根, interfaces 数组为 NULL */
    GDBusNodeInfo *info = (GDBusNodeInfo*)g_malloc0(sizeof(GDBusNodeInfo));
    return info;
}

/* g_dbus_node_info_unref */
void g_dbus_node_info_unref(GDBusNodeInfo *info)
{
    g_free(info);
}

/* g_bus_own_name — 获取总线名称 (立即调用 bus_acquired) */
guint g_bus_own_name(GBusType bus_type, const char *name, GBusNameOwnerFlags flags,
                     void (*bus_acquired)(GDBusConnection *, const char *, gpointer),
                     void (*name_acquired)(GDBusConnection *, const char *, gpointer),
                     void (*name_lost)(GDBusConnection *, const char *, gpointer),
                     gpointer user_data, void (*destroy)(gpointer))
{
    (void)bus_type; (void)name; (void)flags; (void)name_acquired;
    (void)name_lost; (void)destroy;
    if (bus_acquired)
        bus_acquired(NULL, name, user_data);
    return 1;
}

/* g_bus_unown_name — 释放总线名称 */
void g_bus_unown_name(guint id)
{
    (void)id;
}

/* g_dbus_connection_register_object — 注册 D-Bus 对象 */
guint g_dbus_connection_register_object(GDBusConnection *connection,
                                         const char *path,
                                         GDBusInterfaceInfo *interface,
                                         const GDBusInterfaceVTable *vtable,
                                         gpointer user_data,
                                         void *destroy_notify,
                                         GError **error)
{
    (void)connection; (void)path; (void)interface; (void)vtable;
    (void)user_data; (void)destroy_notify;
    if (error) *error = NULL;
    return 1;
}

/* g_dbus_connection_unregister_object — 注销 D-Bus 对象 */
void g_dbus_connection_unregister_object(GDBusConnection *connection, guint id)
{
    (void)connection; (void)id;
}

/* g_variant_new_boolean */
GVariant *g_variant_new_boolean(gboolean value)
{
    (void)value;
    return NULL;
}

/* g_variant_new_string */
GVariant *g_variant_new_string(const char *str)
{
    (void)str;
    return NULL;
}

/* g_variant_dup_string */
gchar *g_variant_dup_string(GVariant *value, gsize *length)
{
    (void)value;
    if (length) *length = 0;
    return NULL;
}

/* g_variant_new_int32 */
GVariant *g_variant_new_int32(gint32 value)
{
    (void)value;
    return NULL;
}

/* g_dbus_method_invocation_return_value */
void g_dbus_method_invocation_return_value(GDBusMethodInvocation *invocation, GVariant *parameters)
{
    (void)invocation; (void)parameters;
}

/* g_object_set_data_full — 在对象上附加数据 */
void g_object_set_data_full(GObject *obj, const char *key, gpointer data, void (*destroy)(gpointer))
{
    (void)obj; (void)key; (void)data; (void)destroy;
}

/* g_object_get_data — 获取对象附加数据 */
gpointer g_object_get_data(GObject *obj, const char *key)
{
    (void)obj; (void)key;
    return NULL;
}

/* g_variant_print — 将 GVariant 打印为字符串 */
gchar *g_variant_print(GVariant *value, gboolean type_annotate)
{
    (void)value; (void)type_annotate;
    return g_strdup("<variant>");
}

/* g_variant_builder_new — 创建新的 GVariantBuilder */
GVariantBuilder *g_variant_builder_new(const void *type)
{
    (void)type;
    return (GVariantBuilder*)g_malloc0(sizeof(GVariantBuilder));
}

/* g_object_notify — 属性变更通知 */
void g_object_notify(GObject *obj, const char *property_name)
{
    (void)obj; (void)property_name;
}

/* g_spawn_async — 异步衍生进程 */
gboolean g_spawn_async(const char *working_directory, char **argv, char **envp,
                       GSpawnFlags flags,
                       void (*child_setup)(gpointer), gpointer user_data,
                       GPid *child_pid, GError **error)
{
    (void)working_directory; (void)argv; (void)envp; (void)flags;
    (void)child_setup; (void)user_data; (void)child_pid;
    if (error) *error = NULL;
    return FALSE;
}

/* ==================== GOption 存根 ==================== */

/* g_option_context_new — 创建选项上下文 */
GOptionContext *g_option_context_new(const gchar *parameter_string)
{
    (void)parameter_string;
    return g_new0(GOptionContext, 1);
}

/* g_option_context_add_main_entries — 添加选项条目 */
void g_option_context_add_main_entries(GOptionContext *context, const GOptionEntry *entries, const gchar *translation_domain)
{
    (void)context; (void)entries; (void)translation_domain;
}

/* g_option_context_parse — 解析命令行参数 */
gboolean g_option_context_parse(GOptionContext *context, gint *argc, gchar ***argv, GError **error)
{
    (void)context; (void)argc; (void)argv;
    if (error) *error = NULL;
    return TRUE;
}

/* g_option_context_free — 释放选项上下文 */
void g_option_context_free(GOptionContext *context)
{
    g_free(context);
}

/* ==================== GFile 存根 ==================== */

GFile *g_file_new_for_path(const char *path)
{
    (void)path;
    return g_new0(GFile, 1);
}

gchar *g_file_get_path(GFile *file)
{
    (void)file;
    return NULL;
}

GFileMonitor *g_file_monitor(GFile *file, int flags, void *cancellable, void *error)
{
    (void)file; (void)flags; (void)cancellable; (void)error;
    return g_new0(GFileMonitor, 1);
}

void g_file_monitor_set_rate_limit(GFileMonitor *monitor, int limit)
{
    (void)monitor; (void)limit;
}

/* ==================== GVariant 补充存根 ==================== */

guint64 g_variant_get_uint64(GVariant *value)
{
    (void)value;
    return 0;
}

gchar **g_variant_dup_strv(GVariant *value, gsize *length)
{
    (void)value;
    if (length) *length = 0;
    return NULL;
}

/* ==================== 堆分配器 (边界标签 free-list) ==================== */
/* PlexsDOS 使用 512KB 静态池 + 边界标签 (boundary-tag) 算法。
 * free() 实际回收内存: 相邻空闲块合并, 插入空闲链表供后续 malloc 复用。
 *
 * 块布局:
 *   已分配块: [4字节 头部: 总大小|IN_USE] [...数据...]
 *   空闲块:   [4字节 头部: 总大小|0] [4字节 next] [4字节 prev] [...数据...] [4字节 尾部: 总大小|0]
 *
 * 头部 bit31 = IN_USE 标志, bits30-0 = 块总大小 (含头部/尾部/指针)。
 * 最小分配: 8 字节 (4 头部 + 4 数据)。最小空闲块: 16 字节。 */
#define HEAP_POOL_SIZE (512 * 1024)
#define BLK_IN_USE    0x80000000u
#define BLK_SIZE_MASK 0x7FFFFFFFu
#define MIN_ALLOC     8u
#define MIN_FREE      16u

static char heap_pool[HEAP_POOL_SIZE];
static void *heap_free_list = NULL;
static int heap_initialized = 0;

/* 读取/写入块头部 */
static inline uint32_t blk_get_hdr(void *blk)
    { return *(uint32_t *)blk; }
static inline void blk_set_hdr(void *blk, uint32_t v)
    { *(uint32_t *)blk = v; }
/* 块总大小 (含头部) */
static inline uint32_t blk_size(uint32_t hdr)
    { return hdr & BLK_SIZE_MASK; }
/* 是否已分配 */
static inline int blk_in_use(uint32_t hdr)
    { return (hdr & BLK_IN_USE) != 0; }

/* 空闲链表指针 (块内偏移 4 和 8) */
static inline void blk_set_next(void *blk, void *n)
    { *(void **)((char *)blk + 4) = n; }
static inline void *blk_get_next(void *blk)
    { return *(void **)((char *)blk + 4); }
static inline void blk_set_prev(void *blk, void *p)
    { *(void **)((char *)blk + 8) = p; }
static inline void *blk_get_prev(void *blk)
    { return *(void **)((char *)blk + 8); }

/* 从空闲链表中移除一个块 */
static void blk_remove_free(void *blk)
{
    void *n = blk_get_next(blk);
    void *p = blk_get_prev(blk);
    if (p)
        blk_set_next(p, n);
    else
        heap_free_list = n;
    if (n)
        blk_set_prev(n, p);
}

/* 向空闲链表头部插入一个块 */
static void blk_insert_free(void *blk)
{
    blk_set_next(blk, heap_free_list);
    blk_set_prev(blk, NULL);
    if (heap_free_list)
        blk_set_prev(heap_free_list, blk);
    heap_free_list = blk;
}

/* 写入尾部标签 (位于 blk + size - 4) */
static inline void blk_set_footer(void *blk, uint32_t v)
    { *(uint32_t *)((char *)blk + blk_size(v) - 4) = v; }

/* 初始化堆: 整个池子作为一个大空闲块 */
static void heap_init(void)
{
    if (heap_initialized) return;
    blk_set_hdr(heap_pool, HEAP_POOL_SIZE);      /* free, size=512KB */
    blk_set_next(heap_pool, NULL);
    blk_set_prev(heap_pool, NULL);
    blk_set_footer(heap_pool, HEAP_POOL_SIZE);
    heap_free_list = heap_pool;
    heap_initialized = 1;
}

void *malloc(size_t size)
{
    if (size == 0) return NULL;
    heap_init();

    uint32_t need = (uint32_t)size + 4;           /* 头部 + 数据 */
    if (need < MIN_ALLOC) need = MIN_ALLOC;
    need = (need + 3) & ~3u;                      /* 4 字节对齐 */

    /* first-fit 扫描空闲链表 */
    void *blk = heap_free_list;
    while (blk) {
        uint32_t hdr = blk_get_hdr(blk);
        uint32_t bsz = blk_size(hdr);
        if (bsz >= need) {
            uint32_t remain = bsz - need;
            if (remain >= MIN_FREE) {
                /* 分裂: 前部 [need] 字节用作已分配, 后部 [remain] 空闲 */
                blk_set_hdr(blk, need | BLK_IN_USE);
                blk_set_footer(blk, need | BLK_IN_USE);

                void *nxt = (char *)blk + need;
                blk_set_hdr(nxt, remain);
                blk_set_next(nxt, blk_get_next(blk));
                blk_set_prev(nxt, blk_get_prev(blk));
                blk_set_footer(nxt, remain);

                /* 更新 nxt 相邻块的 prev/next */
                if (blk_get_next(nxt))
                    blk_set_prev(blk_get_next(nxt), nxt);
                /* 替换空闲链表中的当前块为 nxt */
                if (blk_get_prev(blk))
                    blk_set_next(blk_get_prev(blk), nxt);
                else
                    heap_free_list = nxt;
            } else {
                /* 不分裂, 整个块分配出去 */
                blk_set_hdr(blk, bsz | BLK_IN_USE);
                blk_set_footer(blk, bsz | BLK_IN_USE);
                blk_remove_free(blk);
            }
            return (char *)blk + 4;               /* 返回数据区起始 */
        }
        blk = blk_get_next(blk);
    }
    return NULL;  /* 内存不足 */
}

void *calloc(size_t nmemb, size_t size)
{
    size_t total = nmemb * size;
    void *ptr = malloc(total);
    if (ptr) __builtin_memset(ptr, 0, total);
    return ptr;
}

void *realloc(void *ptr, size_t size)
{
    if (!ptr) return malloc(size);
    if (size == 0) { free(ptr); return NULL; }

    uint32_t *hdr = (uint32_t *)((char *)ptr - 4);
    uint32_t old_hdr = *hdr;
    uint32_t old_size = blk_size(old_hdr) - 4;    /* 可用数据大小 */

    if ((uint32_t)size <= old_size)
        return ptr;                                /* 缩小, 直接返回 */

    void *newptr = malloc(size);
    if (newptr) {
        __builtin_memcpy(newptr, ptr,
                         old_size < (uint32_t)size ? old_size : (uint32_t)size);
        free(ptr);
    }
    return newptr;
}

void free(void *ptr)
{
    if (!ptr || !heap_initialized) return;

    void *blk = (char *)ptr - 4;                  /* 定位到头部 */
    uint32_t hdr = blk_get_hdr(blk);
    uint32_t bsz = blk_size(hdr);

    /* 标记为空闲 (清除 IN_USE) */
    blk_set_hdr(blk, bsz);
    blk_set_footer(blk, bsz);

    /* 向前合并: 检查后方 (地址更高) 的相邻块是否空闲 */
    void *nxt = (char *)blk + bsz;
    if ((char *)nxt < heap_pool + HEAP_POOL_SIZE) {
        uint32_t nxt_hdr = blk_get_hdr(nxt);
        if (!blk_in_use(nxt_hdr)) {
            uint32_t nxt_sz = blk_size(nxt_hdr);
            blk_remove_free(nxt);
            bsz += nxt_sz;
            blk_set_hdr(blk, bsz);
        }
    }

    /* 向后合并: 检查前方 (地址更低) 的相邻块是否空闲
     * 通过读取 blk 前 4 字节 (上一块的尾部标签) 判断 */
    if ((char *)blk > heap_pool) {
        uint32_t prev_footer = *(uint32_t *)((char *)blk - 4);
        if (!blk_in_use(prev_footer)) {
            uint32_t prev_sz = blk_size(prev_footer);
            void *prev = (char *)blk - prev_sz;
            blk_remove_free(prev);
            bsz += prev_sz;
            blk = prev;
            blk_set_hdr(blk, bsz);
        }
    }

    /* 更新尾部标签并插入空闲链表头部 */
    blk_set_footer(blk, bsz);
    blk_insert_free(blk);
}

/* errno */
int errno = 0;

/* strerror */
char *strerror(int errnum)
{
    switch (errnum) {
        case 0:     return (char*)"Success";
        case 2:     return (char*)"No such file or directory";
        case 4:     return (char*)"Interrupted system call";
        case 5:     return (char*)"I/O error";
        case 9:     return (char*)"Bad file descriptor";
        case 11:    return (char*)"Resource temporarily unavailable";
        case 12:    return (char*)"Cannot allocate memory";
        case 13:    return (char*)"Permission denied";
        case 17:    return (char*)"File exists";
        default:    return (char*)"Unknown error";
    }
}

/* ==================== 函数指针版本 (绕过宏, 用于回调场景) ==================== */
#undef close
int close(int fd) { return plxdm_close(fd); }
#undef write
int write(int fd, const void *buf, unsigned int count) { return plxdm_write(fd, buf, count); }
#undef read
int read(int fd, void *buf, unsigned int count) { return plxdm_read(fd, buf, count); }
#undef open
int open(const char *path, int flags, ...) { (void)flags; return plxdm_open(path, flags); }

#undef access
int access(const char *path, int mode) { (void)path; (void)mode; return 0; }
#undef unlink
int unlink(const char *path) { (void)path; return 0; }
#undef rename
int rename(const char *old, const char *new_name) { (void)old; (void)new_name; return 0; }
int fcntl(int fd, int cmd, ...) { (void)fd; (void)cmd; return 0; }

/* g_key_file_get_value / set_value */
gchar *g_key_file_get_value(GKeyFile *key_file, const gchar *group_name, const gchar *key, GError **error)
{
    (void)key_file; (void)group_name; (void)key; (void)error;
    return NULL;
}
gboolean g_key_file_set_value(GKeyFile *key_file, const gchar *group, const gchar *key, const gchar *value)
{
    (void)key_file; (void)group; (void)key; (void)value;
    return FALSE;
}

/* g_dbus_error_quark */
int g_dbus_error_quark(void)
{
    return 0;
}

/* gcry_malloc_secure (用于函数指针场景) */
void *gcry_malloc_secure(size_t n)
{
    return malloc(n);
}

/* session_set_pam_service — DFAN 简化, PAM 服务名不区分 */
void session_set_pam_service(void *session, const gchar *pam_service)
{
    (void)session; (void)pam_service;
}

/* greeter_set_pam_services */
void greeter_set_pam_services(void *greeter, const gchar *pam_service, const gchar *autologin_pam_service)
{
    (void)greeter; (void)pam_service; (void)autologin_pam_service;
}

/* environ */
char **environ = NULL;

/* __main — MinGW 启动入口, freestanding 中为空 */
void __main(void) {}

/* strsignal — 信号描述 */
const char *strsignal(int sig)
{
    (void)sig;
    return "Signal";
}

/* plxdm_seteuid / plxdm_setegid — 权限提升存根 (单用户系统, 恒成功) */
int plxdm_seteuid(int euid) { (void)euid; return 0; }
int plxdm_setegid(int egid) { (void)egid; return 0; }
