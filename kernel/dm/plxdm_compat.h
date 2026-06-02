/*
 * Nexsteaduser — PlexsDOS
 * plxdm_compat.h — GLib 兼容层 (PlexsDOS 原生实现)
 * 作者: Tinmc189623 | 团队: Nexlyh
 *
 * 提供 GLib 核心类型的 PlexsDOS 原生实现:
 *   GMainLoop, GList, GHashTable, GString, GKeyFile, GError
 *   以及 GObject 的轻量化替代。
 *
 * 使用方法:
 *   #include "plxdm_compat.h"   // 替代 #include <glib.h>
 *   // 所有 GLib API 保持原名, 实现为 PlexsDOS 原生代码
 */

#ifndef _PLXDM_COMPAT_H
#define _PLXDM_COMPAT_H

#include <plexsdos/types.h>
#include <plexsdos/string.h>
#include <stdarg.h>
#include "posix_stubs.h"

/* ==================== PAM 类型占位 (由 DFAN 替代) ==================== */
/* LightDM 使用 PAM conversation 与用户交互 (提示输入密码等).
 * DFAN 简化了认证: dfan_authenticate(handle, flags, password) 直接验证.
 * 这些 stub 保留 struct 定义使原有 conversation 代码能通过编译,
 * 但 conversation 路径实际不会被 DFAN 调用。 */
struct pam_message {
    int msg_style;
    const char *msg;
};
struct pam_response {
    char *resp;
    int resp_retcode;
};

#ifdef __cplusplus
#define G_BEGIN_DECLS  extern "C" {
#define G_END_DECLS    }
#else
#define G_BEGIN_DECLS
#define G_END_DECLS
#endif

#ifdef __cplusplus
extern "C" {
#endif

/* ==================== 基本类型映射 ==================== */

typedef char     gchar;
typedef int      gint;
typedef unsigned guint;
typedef short    gshort;
typedef long     glong;
typedef unsigned long gulong;
typedef int      gboolean;
typedef void    *gpointer;
typedef const void *gconstpointer;
typedef size_t   gsize;
typedef int      gssize;
typedef uint32_t guint32;
typedef int32_t  gint32;
typedef uint16_t guint16;
typedef uint8_t  guint8;
typedef uint64_t guint64;
typedef int64_t  gint64;

#ifndef TRUE
#define TRUE  1
#endif
#ifndef FALSE
#define FALSE 0
#endif
#ifndef NULL
#define NULL  ((void*)0)
#endif

/* GStrv = 字符串数组 */
typedef gchar **GStrv;

/* ==================== 断言/日志 ==================== */

#define g_return_if_fail(expr)          do { if (!(expr)) return; } while(0)
#define g_return_val_if_fail(expr, val) do { if (!(expr)) return (val); } while(0)
#define g_assert(expr)                  do { if (!(expr)) { /* assert fail */ } } while(0)

/* ==================== 内存管理 ==================== */

void *g_malloc(gsize size);  /* 函数声明 (用于函数指针场景), 宏定义在下方 */
#define g_malloc(size)        malloc(size)
#define g_malloc0(size)       calloc(1, size)
#define g_realloc(ptr, size)  realloc(ptr, size)
void g_free(gpointer ptr);
#define g_new(type, n)        ((type*)g_malloc(sizeof(type) * (n)))
#define g_new0(type, n)       ((type*)g_malloc0(sizeof(type) * (n)))

/* 注意: PlexsDOS 当前无堆, 使用静态池 */
/* 这些宏会在链接时重定向到 plxdm_compat.c 中的池分配器 */

/* ==================== 字符串工具 ==================== */

gchar       *g_strdup(const gchar *str);
gchar       *g_strdup_printf(const gchar *format, ...);
gchar       *g_strdup_vprintf(const gchar *format, va_list args);
gchar      **g_strsplit(const gchar *str, const gchar *delimiter, gint max_tokens);
gchar       *g_strjoinv(const gchar *separator, gchar **str_array);
gchar       *g_strconcat(const gchar *string1, ...);
void         g_strfreev(gchar **str_array);
gboolean     g_str_has_prefix(const gchar *str, const gchar *prefix);
gboolean     g_str_has_suffix(const gchar *str, const gchar *suffix);
gboolean     g_pattern_match_simple(const gchar *pattern, const gchar *str);
gchar       *g_strstrip(gchar *str);
gchar       *g_strchug(gchar *str);
void         g_strdelimit(gchar *str, const gchar *delimiters, gchar new_delimiter);
gchar       *g_strchomp(gchar *str);
#define      g_clear_pointer(pp, destroy) do { if (*(pp)) { destroy(*(pp)); *(pp) = NULL; } } while(0)
gint         g_strcmp0(const gchar *str1, const gchar *str2);
gint         g_ascii_strcasecmp(const gchar *s1, const gchar *s2);
gint         g_ascii_strncasecmp(const gchar *s1, const gchar *s2, gsize n);
gboolean     g_ascii_isspace(gchar c);
gchar        g_ascii_tolower(gchar c);
gchar        g_ascii_toupper(gchar c);

/* ==================== GString ==================== */

typedef struct {
    gchar  *str;
    gsize   len;
    gsize   allocated_len;
} GString;

GString     *g_string_new(const gchar *init);
GString     *g_string_new_len(const gchar *init, gssize len);
void         g_string_free(GString *str, gboolean free_segment);
GString     *g_string_append(GString *str, const gchar *val);
GString     *g_string_append_c(GString *str, gchar c);
GString     *g_string_append_printf(GString *str, const gchar *format, ...);
GString     *g_string_assign(GString *str, const gchar *val);
gchar       *g_string_free_and_steal(GString *str);
void         g_string_set_size(GString *str, gsize len);
GString     *g_string_truncate(GString *str, gsize len);

/* ==================== GList (双向链表) ==================== */

typedef struct _GList {
    gpointer data;
    struct _GList *next;
    struct _GList *prev;
} GList;

GList       *g_list_append(GList *list, gpointer data);
GList       *g_list_prepend(GList *list, gpointer data);
GList       *g_list_insert(GList *list, gpointer data, gint position);
GList       *g_list_remove(GList *list, gconstpointer data);
GList       *g_list_remove_link(GList *list, GList *link);
GList       *g_list_delete_link(GList *list, GList *link);
GList       *g_list_find(GList *list, gconstpointer data);
GList       *g_list_last(GList *list);
GList       *g_list_first(GList *list);
gint         g_list_length(GList *list);
GList       *g_list_nth(GList *list, guint n);
gpointer     g_list_nth_data(GList *list, guint n);
GList       *g_list_copy(GList *list);
GList       *g_list_sort(GList *list, int (*compare_func)(gconstpointer a, gconstpointer b));
void         g_list_free(GList *list);
void         g_list_free_full(GList *list, void (*free_func)(gpointer));
GList       *g_list_reverse(GList *list);
GList       *g_list_insert_sorted(GList *list, gpointer data, int (*compare_func)(gconstpointer a, gconstpointer b));
GList       *g_list_concat(GList *list1, GList *list2);
void         g_list_foreach(GList *list, void (*func)(gpointer data, gpointer user_data), gpointer user_data);

/* ==================== GSList (单向链表) ==================== */

typedef struct _GSList {
    gpointer data;
    struct _GSList *next;
} GSList;

GSList      *g_slist_append(GSList *list, gpointer data);
GSList      *g_slist_prepend(GSList *list, gpointer data);
GSList      *g_slist_remove(GSList *list, gconstpointer data);
GSList      *g_slist_delete_link(GSList *list, GSList *link);
guint        g_slist_length(GSList *list);
void         g_slist_free(GSList *list);
void         g_slist_free_full(GSList *list, void (*free_func)(gpointer));

/* ==================== GHashTable ==================== */

typedef struct _GHashTable {
    struct GHashEntry {
        gpointer key;
        gpointer value;
        gboolean used;
    } *entries;
    guint size;
    guint count;
    guint (*hash_func)(gconstpointer key);
    gboolean (*equal_func)(gconstpointer a, gconstpointer b);
    void (*key_destroy_func)(gpointer data);
    void (*value_destroy_func)(gpointer data);
} GHashTable;

guint        g_str_hash(gconstpointer v);
gboolean     g_str_equal(gconstpointer a, gconstpointer b);
guint        g_int_hash(gconstpointer v);
gboolean     g_int_equal(gconstpointer a, gconstpointer b);

GHashTable  *g_hash_table_new(guint (*hash_func)(gconstpointer key),
                              gboolean (*equal_func)(gconstpointer a, gconstpointer b));
GHashTable  *g_hash_table_new_full(guint (*hash_func)(gconstpointer key),
                                   gboolean (*equal_func)(gconstpointer a, gconstpointer b),
                                   void (*key_destroy_func)(gpointer data),
                                   void (*value_destroy_func)(gpointer data));
void         g_hash_table_insert(GHashTable *ht, gpointer key, gpointer value);
gpointer     g_hash_table_lookup(GHashTable *ht, gconstpointer key);
gboolean     g_hash_table_remove(GHashTable *ht, gconstpointer key);
void         g_hash_table_destroy(GHashTable *ht);
void         g_hash_table_unref(GHashTable *ht);
guint        g_hash_table_size(GHashTable *ht);
gboolean     g_hash_table_contains(GHashTable *ht, gconstpointer key);
void         g_hash_table_foreach(GHashTable *ht, void (*func)(gpointer key, gpointer value, gpointer user_data), gpointer user_data);
GList       *g_hash_table_get_keys(GHashTable *ht);
GList       *g_hash_table_get_values(GHashTable *ht);
void         g_hash_table_replace(GHashTable *ht, gpointer key, gpointer value);

/* ==================== GError ==================== */

typedef struct {
    guint     domain;
    gint      code;
    gchar    *message;
} GError;

#define g_autoptr_cleanup_GError (void)0

void         g_clear_error(GError **error);
void         g_propagate_error(GError **dest, GError *src);
void         g_set_error(GError **err, guint domain, gint code,
                         const gchar *format, ...);
gboolean     g_error_matches(GError *err, guint domain, gint code);

/* 文件错误码 */
#define G_FILE_ERROR       g_file_error_quark()
#define G_FILE_ERROR_NOENT 2
#define G_FILE_ERROR_ACCES 13
#define G_FILE_ERROR_EXIST 17
guint        g_file_error_quark(void);
gint         g_strv_length(gchar **str_array);
const gchar * const *g_get_system_data_dirs(void);
const gchar * const *g_get_system_config_dirs(void);
gchar       *g_path_get_basename(const gchar *path);

/* 目录操作 (仅存根) */
typedef struct { int dummy; } GDir;
GDir        *g_dir_open(const gchar *path, guint flags, GError **error);
const gchar *g_dir_read_name(GDir *dir);
void         g_dir_close(GDir *dir);

/* 路径操作 */
gboolean     g_path_is_absolute(const gchar *path);

/* 指针 / 资源管理 */
void        *g_steal_pointer_impl(void *pp);
#define g_steal_pointer(pp) g_steal_pointer_impl((gpointer *)(pp))

/* 输出 */
void         g_printerr(const gchar *format, ...);

/* ==================== 日志 ==================== */

#define G_LOG_LEVEL_ERROR   1
#define G_LOG_LEVEL_CRITICAL 2
#define G_LOG_LEVEL_WARNING 3
#define G_LOG_LEVEL_MESSAGE 4
#define G_LOG_LEVEL_INFO    5
#define G_LOG_LEVEL_DEBUG   6
#define G_LOG_LEVEL_MASK    0x07

/* GLogLevelFlags — 必须先于 GLogFunc 定义 */
typedef guint GLogLevelFlags;

typedef void (*GLogFunc)(const gchar *log_domain,
                         GLogLevelFlags log_level,
                         const gchar *message,
                         gpointer user_data);

void         g_log(const gchar *log_domain, int log_level,
                   const gchar *format, ...);
void         g_log_default_handler(const gchar *log_domain,
                                   int log_level,
                                   const gchar *message,
                                   gpointer unused_data);
void         g_log_set_default_handler(GLogFunc log_func, gpointer user_data);
void         g_log_set_handler(const gchar *log_domain, int log_levels,
                               GLogFunc log_func, gpointer user_data);

#define g_debug(...)    g_log("PLXDM", G_LOG_LEVEL_DEBUG, __VA_ARGS__)
#define g_warning(...)  g_log("PLXDM", G_LOG_LEVEL_WARNING, __VA_ARGS__)
#define g_message(...)  g_log("PLXDM", G_LOG_LEVEL_MESSAGE, __VA_ARGS__)
#define g_critical(...) g_log("PLXDM", G_LOG_LEVEL_CRITICAL, __VA_ARGS__)
#define g_error(...)    g_log("PLXDM", G_LOG_LEVEL_ERROR, __VA_ARGS__)
#define g_info(...)     g_log("PLXDM", G_LOG_LEVEL_INFO, __VA_ARGS__)

/* ==================== GMainLoop ==================== */

typedef struct _GMainLoop {
    gboolean running;
    gboolean is_running;
} GMainLoop;

typedef gboolean (*GSourceFunc)(gpointer user_data);
typedef guint    GSourceId;

GMainLoop   *g_main_loop_new(gpointer context, gboolean is_running);
void         g_main_loop_run(GMainLoop *loop);
void         g_main_loop_quit(GMainLoop *loop);
gboolean     g_main_loop_is_running(GMainLoop *loop);
gboolean     g_main_context_iteration(gpointer context, gboolean may_block);
guint        g_timeout_add(guint interval_ms, GSourceFunc function, gpointer data);
guint        g_idle_add(GSourceFunc function, gpointer data);
gboolean     g_source_remove(guint source_id);

/* ==================== GKeyFile (配置解析) ==================== */

#define G_KEY_FILE_NONE              0
#define G_KEY_FILE_KEEP_COMMENTS     1
#define G_KEY_FILE_KEEP_TRANSLATIONS 2

typedef struct {
    struct {
        gchar group[64];
        struct {
            gchar key[64];
            gchar value[256];
        } entries[64];
        int entry_count;
    } groups[32];
    int group_count;
} GKeyFile;

GKeyFile    *g_key_file_new(void);
void         g_key_file_free(GKeyFile *key_file);
gboolean     g_key_file_load_from_file(GKeyFile *key_file,
                                       const gchar *file,
                                       guint flags,
                                       GError **error);
gboolean     g_key_file_load_from_data(GKeyFile *key_file,
                                       const gchar *data,
                                       gsize length,
                                       guint flags,
                                       GError **error);
gchar       *g_key_file_get_string(GKeyFile *key_file,
                                   const gchar *group_name,
                                   const gchar *key,
                                   GError **error);
gboolean     g_key_file_has_key(GKeyFile *key_file,
                                const gchar *group_name,
                                const gchar *key,
                                GError **error);
gboolean     g_key_file_has_group(GKeyFile *key_file,
                                  const gchar *group_name);
gint         g_key_file_get_integer(GKeyFile *key_file,
                                    const gchar *group_name,
                                    const gchar *key,
                                    GError **error);
gboolean     g_key_file_get_boolean(GKeyFile *key_file,                                    const gchar *group_name,                                    const gchar *key,                                    GError **error);
gchar       *g_key_file_get_value(GKeyFile *key_file,
                                  const gchar *group_name,
                                  const gchar *key,
                                  GError **error);
gboolean     g_key_file_set_value(GKeyFile *key_file,
                                  const gchar *group_name,
                                  const gchar *key,
                                  const gchar *value);
gboolean     g_key_file_set_string(GKeyFile *key_file,
                                   const gchar *group_name,
                                   const gchar *key,
                                   const gchar *value);
gboolean     g_key_file_set_string_list(GKeyFile *key_file,
                                        const gchar *group_name,
                                        const gchar *key,
                                        const gchar * const *list,
                                        gsize length);
gboolean     g_key_file_set_integer(GKeyFile *key_file,
                                    const gchar *group_name,
                                    const gchar *key,
                                    gint value);
gboolean     g_key_file_set_boolean(GKeyFile *key_file,
                                    const gchar *group_name,
                                    const gchar *key,
                                    gboolean value);

#define GPOINTER_TO_INT(p)   ((gint)(gsize)(p))
#define GINT_TO_POINTER(i)   ((gpointer)(gsize)(i))
gchar      **g_key_file_get_string_list(GKeyFile *key_file,
                                        const gchar *group_name,
                                        const gchar *key,
                                        gsize *length,
                                        GError **error);
gchar      **g_key_file_get_keys(GKeyFile *key_file,
                                 const gchar *group_name,
                                 gsize *length,
                                 GError **error);
gchar      **g_key_file_get_groups(GKeyFile *key_file, gsize *length);

/* GKeyFile Desktop Entry 标准键名 */
#define G_KEY_FILE_DESKTOP_GROUP         "Desktop Entry"
#define G_KEY_FILE_DESKTOP_KEY_EXEC      "Exec"
#define G_KEY_FILE_DESKTOP_KEY_TYPE      "Type"
#define G_KEY_FILE_DESKTOP_KEY_NAME      "Name"
#define G_KEY_FILE_DESKTOP_KEY_COMMENT   "Comment"
#define G_KEY_FILE_DESKTOP_KEY_ICON      "Icon"
#define G_KEY_FILE_DESKTOP_KEY_CATEGORIES "Categories"
#define G_KEY_FILE_DESKTOP_KEY_NO_DISPLAY "NoDisplay"
#define G_KEY_FILE_DESKTOP_KEY_HIDDEN    "Hidden"
#define G_KEY_FILE_DESKTOP_KEY_ONLY_SHOW_IN "OnlyShowIn"
#define G_KEY_FILE_DESKTOP_KEY_NOT_SHOW_IN "NotShowIn"
#define G_KEY_FILE_DESKTOP_KEY_TRY_EXEC  "TryExec"
#define G_KEY_FILE_DESKTOP_KEY_TERMINAL  "Terminal"
#define G_KEY_FILE_DESKTOP_KEY_MIME_TYPE "MimeType"
#define G_KEY_FILE_DESKTOP_KEY_STARTUP_NOTIFY "StartupNotify"
#define G_KEY_FILE_DESKTOP_KEY_STARTUP_WM_CLASS "StartupWMClass"
#define G_KEY_FILE_DESKTOP_KEY_URL       "URL"
#define G_KEY_FILE_DESKTOP_KEY_DBUS_ACTIVATABLE "DBusActivatable"
#define G_KEY_FILE_DESKTOP_KEY_ACTIONS   "Actions"

/* GKeyFile 错误域 */
#define G_KEY_FILE_ERROR g_key_file_error_quark()
#define G_KEY_FILE_ERROR_KEY_NOT_FOUND 0
#define G_KEY_FILE_ERROR_GROUP_NOT_FOUND 1
#define G_KEY_FILE_ERROR_INVALID_VALUE 2
guint g_key_file_error_quark(void);

/* ==================== GObject 轻量化替代 ==================== */

/* LightDM 使用 GObject 的类型系统, 但我们用轻量化 C 对象模型替代 */

/* 对象基类 — 扩展: 添加 priv 指针支持私有数据 */
typedef struct {
    const char *class_name;
    int ref_count;
    void (*finalize)(gpointer obj);
    gpointer priv;          /* 私有数据指针, 供 G_DEFINE_TYPE_WITH_PRIVATE 使用 */
    gpointer klass;         /* 指向 PlxdmClass 静态结构, 供 G_TYPE_INSTANCE_GET_CLASS 使用 */
} PlxdmObject;

#define PLXDM_OBJECT(obj)            ((PlxdmObject*)(obj))
#define plxdm_object_ref(obj)        (PLXDM_OBJECT(obj)->ref_count++, (obj))
#define plxdm_object_unref(obj)      do { \
    PlxdmObject *_o = PLXDM_OBJECT(obj); \
    if (--_o->ref_count <= 0) { \
        if (_o->finalize) _o->finalize(obj); \
    } \
} while(0)

/* GObject 兼容宏 */
#define g_object_new(type, first, ...)     plxdm_object_new(type, first, ##__VA_ARGS__)
/* g_object_unref: 函数 (用于函数指针场景如 g_list_free_full) + 宏 (用于直接调用) */
void g_object_unref(gpointer obj);
#define g_object_unref(obj)                plxdm_object_unref(obj)
#define g_object_ref(obj)                  plxdm_object_ref(obj)
#define g_clear_object(obj)                do { if (*(obj)) { g_object_unref(*(obj)); *(obj) = NULL; } } while(0)

/* GObject 类型别名 */
#define GObject                            PlxdmObject
#define G_OBJECT(obj)                      ((GObject*)(obj))

#define G_TYPE_CHECK_INSTANCE_CAST(obj, type, cast_to) ((cast_to*)(obj))

#define G_DEFINE_AUTOPTR_CLEANUP_FUNC(type, func)
typedef void (*GDestroyNotify)(gpointer);

/* GObject 类型系统模拟 */
typedef struct {
    gsize instance_size;
    const char *class_name;
    void (*init)(gpointer obj);
    void (*finalize)(gpointer obj);
} PlxdmClass;

/* GObjectClass — 用于 class_init 中设置 finalize/get_property/set_property */
struct _GValue;
struct _GParamSpec;
typedef struct {
    void (*finalize)(GObject *object);
    void (*get_property)(GObject *object, guint prop_id, struct _GValue *value, struct _GParamSpec *pspec);
    void (*set_property)(GObject *object, guint prop_id, const struct _GValue *value, struct _GParamSpec *pspec);
} GObjectClass;

#define G_OBJECT_CLASS(klass) ((GObjectClass*)(klass))

#define G_TYPE_INVALID ((GType)0)
#define G_TYPE_OBJECT ((GType)1)
typedef PlxdmClass *GType;

/* GValue — 属性值容器 (简化存根) */
typedef struct _GValue {
    guint g_type;
    gpointer data[2];
} GValue;

#define g_value_set_string(v, s) ((void)((v)->data[0] = (gpointer)(s)))
#define g_value_set_int(v, i)     ((void)((v)->data[0] = (gpointer)(gsize)(i)))
#define g_value_set_boolean(v, b) ((void)((v)->data[0] = (gpointer)(gsize)(b)))
#define g_value_set_uint64(v, u)  ((void)((v)->data[0] = (gpointer)(gsize)(u)))
#define g_value_set_boxed(v, b)   ((void)((v)->data[0] = (b)))

/* G_DEFINE_INTERFACE — 接口类型注册 */
#define G_DEFINE_INTERFACE(tn, name, parent) \
    GType name##_get_type(void); \
    static void name##_default_init(void *g_class);

/* G_LOG_DOMAIN 默认值 */
#ifndef G_LOG_DOMAIN
#define G_LOG_DOMAIN "PLXDM"
#endif

/* GTypeInterface — 接口类型定义, 用于 LoggerInterface 等 */
typedef struct { GType parent_type; } GTypeInterface;

/* GSpawnFlags */
typedef int GSpawnFlags;

/* GOption — 命令行选项解析 */
typedef enum {
    G_OPTION_ARG_NONE,
    G_OPTION_ARG_STRING,
    G_OPTION_ARG_INT,
    G_OPTION_ARG_CALLBACK,
    G_OPTION_ARG_FILENAME,
    G_OPTION_ARG_STRING_ARRAY,
    G_OPTION_ARG_FILENAME_ARRAY,
    G_OPTION_ARG_DOUBLE,
    G_OPTION_ARG_INT64
} GOptionArg;

typedef struct {
    const gchar *long_name;
    gchar short_name;
    gint flags;
    GOptionArg arg;
    gpointer arg_data;
    const gchar *description;
    const gchar *arg_description;
} GOptionEntry;

typedef struct _GOptionContext { int dummy; } GOptionContext;

GOptionContext *g_option_context_new(const gchar *parameter_string);
void           g_option_context_add_main_entries(GOptionContext *context, const GOptionEntry *entries, const gchar *translation_domain);
gboolean       g_option_context_parse(GOptionContext *context, gint *argc, gchar ***argv, GError **error);
void           g_option_context_free(GOptionContext *context);

/* G_TYPE_CHECK_INSTANCE_TYPE */
#define G_TYPE_CHECK_INSTANCE_TYPE(obj, type) (1)

/* G_SOURCE_* 常量 */
#define G_SOURCE_REMOVE   0
#define G_SOURCE_CONTINUE 1

/* GPOINTER_TO_UINT / GUINT_TO_POINTER */
#define GPOINTER_TO_UINT(p)  ((guint)(gsize)(p))
#define GUINT_TO_POINTER(u)  ((gpointer)(gsize)(u))

/* g_snprintf, g_vsnprintf */
int g_snprintf(char *buf, unsigned int size, const char *fmt, ...);
int g_vsnprintf(char *buf, unsigned int size, const char *fmt, va_list ap);

/* g_strsignal */
const char *g_strsignal(int sig);

/* g_file_test */
#define G_FILE_TEST_EXISTS    (1 << 0)
#define G_FILE_TEST_IS_REG    (1 << 1)
#define G_FILE_TEST_IS_DIR    (1 << 2)
gboolean g_file_test(const char *filename, guint test);

/* GPid = 进程 ID 类型 */
typedef int GPid;
typedef unsigned char guchar;

/* G_TYPE_INSTANCE_GET_INTERFACE — 从实例获取接口指针 */
#define G_TYPE_INSTANCE_GET_INTERFACE(obj, type, cast_to) ((cast_to*)(obj))

/* G_TYPE_CHECK_CLASS_CAST — 类指针转换 */
#define G_TYPE_CHECK_CLASS_CAST(klass, type, cast_to) ((cast_to*)(klass))

/* G_TYPE_INSTANCE_GET_CLASS — 从实例获取类结构 (通过 klass 指针) */
#define G_TYPE_INSTANCE_GET_CLASS(obj, type, cast_to) ((cast_to*)(((PlxdmObject*)(obj))->klass))

#define G_DEFINE_TYPE(tn, name, parent) \
    static void name##_init(tn *obj); \
    static void name##_class_init(tn##Class *klass); \
    GType name##_get_type(void) { \
        static int _inited = 0; \
        static PlxdmClass klass = { sizeof(tn), #tn, NULL, NULL }; \
        if (!_inited) { _inited = 1; \
            tn##Class _c; memset(&_c, 0, sizeof(_c)); \
            name##_class_init(&_c); \
            klass.init = (void(*)(gpointer))name##_init; \
            klass.finalize = (void(*)(gpointer))((GObjectClass*)(&_c))->finalize;\
        } \
        return (GType)(&klass); \
    }

/* G_DEFINE_TYPE_WITH_PRIVATE: 前向声明 get_type() + 生成 get_instance_private.
 * 需要文件末尾调用 PLXDM_DEFINE_TYPE_GET_TYPE(tn, name, parent) 以完成定义. */
#define G_DEFINE_TYPE_WITH_PRIVATE(tn, name, parent) \
    GType name##_get_type(void); \
    static inline tn##Private *name##_get_instance_private(tn *obj) { \
        return (tn##Private *)(((PlxdmObject*)(obj))->priv); \
    }

/* PLXDM_DEFINE_INTERFACE_GET_TYPE: 接口 get_type, 放在文件末尾 */
#define PLXDM_DEFINE_INTERFACE_GET_TYPE(tn, name) \
    GType name##_get_type(void) { \
        static int _inited = 0; \
        static PlxdmClass klass = { 0, #tn, NULL, NULL }; \
        if (!_inited) { _inited = 1; \
            name##_default_init(NULL); \
        } \
        return (GType)(&klass); \
    }

/* PLXDM_DEFINE_TYPE_GET_TYPE: 在文件末尾 (所有函数定义之后) 调用此宏
 * 以生成 name##_get_type() 函数。替换 G_DEFINE_TYPE 的功能。 */
#define PLXDM_DEFINE_TYPE_GET_TYPE(tn, name, parent) \
    GType name##_get_type(void) { \
        static int _inited = 0; \
        static PlxdmClass klass = { sizeof(tn), #tn, NULL, NULL }; \
        if (!_inited) { _inited = 1; \
            klass.init = (void(*)(gpointer))name##_init; \
            { tn##Class _c; memset(&_c, 0, sizeof(_c)); \
              name##_class_init(&_c); \
              klass.finalize = (void(*)(gpointer))((GObjectClass*)(&_c))->finalize;} \
        } \
        return (GType)(&klass); \
    }

#define G_DEFINE_TYPE_WITH_CODE(tn, name, parent, code) \
    static void name##_init(tn *obj); \
    static inline tn##Private *name##_get_instance_private(tn *obj) { \
        return (tn##Private *)(((PlxdmObject*)(obj))->priv); \
    }

#define G_ADD_PRIVATE(tn)               /* 忽略, 手动管理私有数据 */
#define G_IMPLEMENT_INTERFACE(iface, fn) /* 忽略接口实现 */

typedef guint GSignalId;

/* 信号回调类型 */
typedef void (*GCallback)(void);

#define G_CALLBACK(fn) ((GCallback)(fn))

/* 信号连接 */
#define g_signal_connect(instance, detailed_signal, handler, data) \
    plxdm_signal_connect((gpointer)(instance), (detailed_signal), (GCallback)(handler), (gpointer)(data))

#define g_signal_connect_data(instance, detailed_signal, handler, data, destroy_data, flags) \
    plxdm_signal_connect((gpointer)(instance), (detailed_signal), (GCallback)(handler), (gpointer)(data))

#define g_signal_handlers_disconnect_matched(instance, mask, signal_id, detail, closure, func, data) \
    plxdm_signal_disconnect_by_func((gpointer)(instance), (GCallback)(func))

#define g_signal_handlers_disconnect_by_func(instance, func, data) \
    plxdm_signal_disconnect_by_func((gpointer)(instance), (GCallback)(func))

/* 信号系统内部 */
void plxdm_signal_connect(gpointer obj, const char *signal_name,
                          GCallback handler, gpointer user_data);
void plxdm_signal_emit(gpointer obj, const char *signal_name);
void plxdm_signal_disconnect_by_func(gpointer obj, GCallback handler);

/* GObject 替代创建函数 */
gpointer plxdm_object_new(PlxdmClass *class_info, ...);
gpointer plxdm_object_new0(gsize size, const char *class_name,
                           void (*init)(gpointer),
                           void (*finalize)(gpointer));

#define g_type_init()

/* GObject 数据附加 */
void g_object_set_data_full(GObject *obj, const char *key, gpointer data, void (*destroy)(gpointer));
gpointer g_object_get_data(GObject *obj, const char *key);

/* ==================== 文件/路径操作 ==================== */

#define g_build_filename(first, ...)        plxdm_build_filename(first, ##__VA_ARGS__)
#define g_get_current_dir()                 "."  /* PlexsDOS 无当前目录概念 */
#define g_get_home_dir()                    "/system"
#define g_get_user_cache_dir()              "/system/cache"
#define g_get_host_name()                   "plexsdos"
#define g_get_user_name()                   "root"
#define g_getenv(name)                      NULL  /* 无环境变量 */
#define g_setenv(name, val, overwrite)      (void)0
#define g_find_program_in_path(prog)        NULL  /* 无 PATH 搜索 */
#define g_mkdir_with_parents(path, mode)    0     /* 无文件系统 */

gchar       *plxdm_build_filename(const gchar *first, ...);

/* 文件内容操作 */
gchar       *g_key_file_to_data(GKeyFile *key_file, gsize *length, GError **error);
gboolean     g_file_set_contents(const gchar *filename, const gchar *contents, gssize length, GError **error);
gboolean     g_file_get_contents(const gchar *filename, gchar **contents, gsize *length, GError **error);
const gchar *g_strerror(gint errnum);
int          g_open(const gchar *path, int flags, ...);
guint        g_file_error_from_errno(gint err_no);

/* gstdio */
#define g_access(path, mode)                access(path, mode)
#define g_unlink(path)                      unlink(path)
#define g_remove(path)                      remove(path)
#define g_rmdir(path)                       rmdir(path)
#define g_rename(old, new)                  rename(old, new)

/* ==================== D-Bus 存根类型 (1-Bus 替代) ==================== */

/* 根据架构: D-Bus → 1-Bus, 这里提供最小类型定义使编译通过 */
typedef struct { int dummy; } GDBusConnection;
typedef struct { int dummy; } GDBusProxy;
typedef struct { int dummy; } GVariant;
typedef struct { int dummy; } GVariantBuilder;
typedef struct { int dummy; } GInetAddress;
typedef struct { int dummy; } GSocket;
typedef int GIOCondition;

typedef enum { G_BUS_TYPE_SYSTEM = 0, G_BUS_TYPE_SESSION = 1 } GBusType;
typedef enum { G_BUS_NAME_OWNER_FLAGS_NONE = 0 } GBusNameOwnerFlags;
typedef enum { G_DBUS_CALL_FLAGS_NONE = 0 } GDBusCallFlags;
typedef enum { G_SOCKET_FAMILY_INVALID = 0, G_SOCKET_FAMILY_UNKNOWN = 1 } GSocketFamily;

#define G_VARIANT_TYPE(typetag) ((void*)0)

GDBusConnection *g_bus_get_sync(GBusType bus_type, void *cancellable, GError **error);
guint g_bus_own_name(GBusType bus_type, const char *name, GBusNameOwnerFlags flags,
                     void (*bus_acquired)(GDBusConnection *, const char *, gpointer),
                     void (*name_acquired)(GDBusConnection *, const char *, gpointer),
                     void (*name_lost)(GDBusConnection *, const char *, gpointer),
                     gpointer user_data, void (*destroy)(gpointer));
void g_bus_unown_name(guint id);
GVariant *g_dbus_connection_call_sync(GDBusConnection *conn, const char *bus_name, const char *object_path, const char *interface_name, const char *method_name, GVariant *parameters, GVariant *reply_type, GDBusCallFlags flags, int timeout_msec, void *cancellable, GError **error);
GVariant *g_variant_new(const char *format_string, ...);
void g_variant_get(GVariant *value, const char *format_string, ...);
void g_variant_unref(GVariant *value);

/* ==================== GSubprocess / 进程衍生 ==================== */
#define G_SPAWN_DEFAULT                0
#define G_SPAWN_LEAVE_DESCRIPTORS_OPEN (1 << 0)
#define G_SPAWN_DO_NOT_REAP_CHILD      (1 << 1)
#define G_SPAWN_SEARCH_PATH            (1 << 2)
#define G_SPAWN_STDOUT_TO_DEV_NULL     (1 << 3)
#define G_SPAWN_STDERR_TO_DEV_NULL     (1 << 4)
#define G_SPAWN_CHILD_INHERITS_STDIN   (1 << 5)
#define G_SPAWN_FILE_AND_ARGV_ZERO     (1 << 6)

gboolean g_spawn_sync(const char *working_directory, char **argv, char **envp, GSpawnFlags flags, void (*child_setup)(gpointer), gpointer user_data, gchar **standard_output, gchar **standard_error, int *exit_status, GError **error);
gboolean g_spawn_command_line_sync(const char *command_line, gchar **standard_output, gchar **standard_error, int *exit_status, GError **error);
gboolean g_spawn_command_line_async(const char *command_line, GError **error);
gboolean g_spawn_async(const char *working_directory, char **argv, char **envp,
                       GSpawnFlags flags,
                       void (*child_setup)(gpointer), gpointer user_data,
                       GPid *child_pid, GError **error);
gboolean g_shell_parse_argv(const char *command_line, int *argcp, char ***argvp, GError **error);
gchar *g_shell_quote(const char *unquoted_string);

/* 最小/最大值 */
#define MIN(a, b) (((a) < (b)) ? (a) : (b))
#define MAX(a, b) (((a) > (b)) ? (a) : (b))

/* 自动释放 (使用 GCC cleanup 属性) */
#define _PLXDM_AUTO_CLEANUP(cleanup_fn) __attribute__((cleanup(cleanup_fn)))

static inline void _plxdm_free_cleanup(void *ptr) { void **p = (void **)ptr; g_free(*p); }
#define g_autofree _PLXDM_AUTO_CLEANUP(_plxdm_free_cleanup)

/* g_auto / g_autoptr */
#define g_auto(type)   type __attribute__((cleanup(_plxdm_free_cleanup)))
#define g_autoptr(type) type * __attribute__((cleanup(_plxdm_free_cleanup)))

/* 消息队列 */
typedef struct _GPtrArray {
    gpointer *pdata;
    guint     len;
} GPtrArray;

GPtrArray   *g_ptr_array_new(void);
GPtrArray   *g_ptr_array_new_with_free_func(void (*free_func)(gpointer));
void         g_ptr_array_add(GPtrArray *array, gpointer data);
void         g_ptr_array_free(GPtrArray *array, gboolean free_seg);
void         g_ptr_array_set_free_func(GPtrArray *array, void (*free_func)(gpointer));

/* 定时器 */
typedef struct { double start; } GTimer;
GTimer      *g_timer_new(void);
double       g_timer_elapsed(GTimer *timer, gulong *microseconds);
void         g_timer_destroy(GTimer *timer);

/* 退出 */
#define EXIT_SUCCESS 0
#define EXIT_FAILURE 1
void exit(int status);

/* 随机 */
#define g_random_int() 42  /* PlexsDOS 伪随机 */

/* ==================== GObject 信号系统增强 ==================== */

#define g_signal_emit(instance, signal_id, detail, ...) \
    plxdm_signal_emit((gpointer)(instance), "signal")
#define g_signal_new(signal_name, type, flags, struct_offset, accumulator, accu_data, marshaller, return_type, n_params, ...) (0)

#define G_SIGNAL_RUN_LAST 1
#define G_SIGNAL_MATCH_DATA (1 << 1)
#define g_signal_accumulator_first_wins NULL
#define G_TYPE_NONE 0
#define G_STRUCT_OFFSET(struct_type, member) ((long)(unsigned long)&((struct_type*)0)->member)

/* g_object_unref 宏 (已在上面定义, 这里补全) */
#ifndef g_object_unref
#define g_object_unref(obj) plxdm_object_unref(obj)
#endif

/* GObject 属性系统存根 */
void g_object_notify(GObject *obj, const char *property_name);
#define g_object_class_install_property(klass, id, pspec) ((void)0)
#define g_param_spec_string(name, nick, blurb, default_val, flags) ((GParamSpec*)NULL)
#define G_OBJECT_WARN_INVALID_PROPERTY_ID(obj, id, pspec) ((void)0)
#define G_PARAM_READABLE (1 << 0)
#define G_PARAM_WRITABLE (1 << 1)

/* ==================== GIO 存根 ==================== */

/* GIOChannel */
typedef struct { int fd; } GIOChannel;
GIOChannel *g_io_channel_unix_new(int fd);
void g_io_channel_unref(GIOChannel *channel);
void g_io_channel_set_encoding(GIOChannel *channel, const char *encoding, GError **error);
void g_io_channel_set_buffered(GIOChannel *channel, gboolean buffered);
int g_io_channel_write_chars(GIOChannel *channel, const char *buf, unsigned int count, unsigned int *bytes_written, GError **error);
int g_io_channel_read_chars(GIOChannel *channel, char *buf, unsigned int count, unsigned int *bytes_read, GError **error);
int g_io_channel_flush(GIOChannel *channel, GError **error);
guint g_io_add_watch(GIOChannel *channel, int condition, void *function, gpointer data);

/* GSource */
typedef struct { int dummy; } GSource;
#define G_IO_IN 1
#define G_IO_OUT 2
#define G_IO_HUP 4

/* GSocketAddress */
typedef GInetAddress GSocketAddress;

/* GInetSocketAddress */
typedef struct { int dummy; } GInetSocketAddress;
#define G_INET_SOCKET_ADDRESS(obj) ((GInetSocketAddress*)(obj))
GInetAddress *g_inet_socket_address_get_address(GInetSocketAddress *address);

/* GSocket 函数 */
GSocket *g_socket_new(int family, int type, int protocol, GError **error);
gboolean g_socket_bind(GSocket *socket, GSocketAddress *address, gboolean allow_reuse, GError **error);
gboolean g_socket_listen(GSocket *socket, GError **error);
GSocket *g_socket_accept(GSocket *socket, void *cancellable, GError **error);
void g_socket_close(GSocket *socket, GError **error);
int g_socket_get_fd(GSocket *socket);
GSocketAddress *g_socket_get_remote_address(GSocket *socket, GError **error);
GSource *g_socket_create_source(GSocket *socket, int condition, void *cancellable);

/* GInetAddress 函数 */
GInetAddress *g_inet_address_new_from_bytes(const void *bytes, int family);
GInetAddress *g_inet_address_new_any(int family);
gchar *g_inet_address_to_string(GInetAddress *address);
gboolean g_inet_address_get_is_link_local(GInetAddress *address);

/* GSocket 常量 */
#define G_SOCKET_FAMILY_UNIX    0
#define G_SOCKET_FAMILY_IPV4    2
#define G_SOCKET_FAMILY_IPV6    10
#define G_SOCKET_TYPE_STREAM    1
#define G_SOCKET_TYPE_DGRAM     2
#define G_SOCKET_TYPE_DATAGRAM  G_SOCKET_TYPE_DGRAM
#define G_SOCKET_PROTOCOL_DEFAULT 0

/* g_dbus_proxy 存根 */
typedef enum { G_DBUS_PROXY_FLAGS_NONE = 0 } GDBusProxyFlags;
GVariant *g_dbus_proxy_call_sync(GDBusProxy *proxy, const char *method, GVariant *parameters, GDBusCallFlags flags, int timeout_msec, void *cancellable, GError **error);
GDBusProxy *g_dbus_proxy_new_for_bus_sync(GBusType bus_type, GDBusProxyFlags flags, void *info, const char *name, const char *object_path, const char *interface_name, void *cancellable, GError **error);

/* g_variant 增强 */
gboolean g_variant_is_of_type(GVariant *value, const void *type);
gboolean g_variant_get_boolean(GVariant *value);
guint64  g_variant_get_uint64(GVariant *value);
gchar  **g_variant_dup_strv(GVariant *value, gsize *length);
const char *g_variant_get_type_string(GVariant *value);
void g_variant_builder_init(void *builder, const void *type);
void g_variant_builder_open(void *builder, const void *type);
void g_variant_builder_add(void *builder, const char *format, ...);

/* G_VARIANT_TYPE_* 宏 */
#define G_VARIANT_TYPE_BOOLEAN  ((void*)0)
#define G_VARIANT_TYPE_ARRAY    ((void*)0)
#define G_VARIANT_TYPE_STRING       ((void*)0)
#define G_VARIANT_TYPE_UINT64       ((void*)0)
#define G_VARIANT_TYPE_STRING_ARRAY ((void*)0)
#define GVariantIter            GVariant  /* 简化迭代器为 GVariant 本身 */
gboolean g_variant_iter_loop(void *iter, const char *format, ...);
gchar *g_variant_dup_string(GVariant *value, gsize *length);

/* g_hash_table_iter 存根 */
typedef struct { int pos; } GHashTableIter;
void g_hash_table_iter_init(GHashTableIter *iter, GHashTable *ht);
gboolean g_hash_table_iter_next(GHashTableIter *iter, gpointer *key, gpointer *value);

/* g_print */
void g_print(const char *format, ...);

/* kill() */
int kill(int pid, int sig);

/* GIOStatus */
typedef int GIOStatus;

#define G_MAXUINT32 ((guint32)0xFFFFFFFF)
#define G_IO_STATUS_NORMAL 0
#define G_IO_STATUS_EOF    1

/* g_hash_table_remove_all */
#define g_hash_table_remove_all(ht) g_hash_table_remove(ht, NULL)

/* g_strdupv — 字符串数组复制 */
gchar **g_strdupv(gchar **str_array);

/* g_direct_hash / g_direct_equal (函数声明用于函数指针, 宏用于内联) */
guint g_direct_hash(gconstpointer p);
gboolean g_direct_equal(gconstpointer a, gconstpointer b);
#define g_direct_hash(p) ((guint)(gsize)(p))
#define g_direct_equal(a, b) ((a) == (b))

/* g_child_watch_add */
guint g_child_watch_add(guint pid, void *function, gpointer data);

/* g_strjoin — 字符串连接 */
gchar *g_strjoin(const char *separator, ...);

/* GSubprocess 存根 */
typedef struct { int dummy; } GSubprocess;
#define G_SUBPROCESS_FLAGS_STDERR_SILENCE 0
#define GSubprocessFlags int
GSubprocess *g_subprocess_new(GSubprocessFlags flags, GError **error, const char *arg0, ...);

/* GArray */
typedef struct { gchar *data; guint len; } GArray;
GArray *g_array_sized_new(gboolean zero_terminated, gboolean clear_, guint element_size, guint reserved_size);
void g_array_free(GArray *array, gboolean free_segment);
#define g_array_append_val(a, v)   (a)
#define g_array_index(a, t, i)     (((t*)(a)->data)[i])
#define g_array_sort_with_data(a, c, d) ((void)0)

/* g_dbus_connection_emit_signal 等 */
gboolean g_dbus_connection_emit_signal(GDBusConnection *conn, const char *bus_name, const char *object_path, const char *interface_name, const char *signal_name, GVariant *parameters, GError **error);
#define G_DBUS_SIGNAL_FLAGS_NONE 0
GVariant *g_variant_new_object_path(const char *object_path);
GVariant *g_variant_new_boolean(gboolean value);
GVariant *g_variant_new_int32(gint32 value);
GVariant *g_variant_new_string(const char *str);
GVariant *g_variant_builder_end(void *builder);
void g_variant_builder_add_value(void *builder, GVariant *value);
gchar *g_variant_print(GVariant *value, gboolean type_annotate);
GVariantBuilder *g_variant_builder_new(const void *type);
typedef int GDBusSignalFlags;
#define G_DBUS_SIGNAL_FLAGS_NONE 0
guint g_dbus_connection_signal_subscribe(GDBusConnection *conn, const char *bus_name, const char *interface_name, const char *member, const char *object_path, const char *arg0, GDBusSignalFlags flags, void *callback, gpointer user_data, void *destroy_notify);
void  g_dbus_connection_signal_unsubscribe(GDBusConnection *conn, guint subscription_id);

/* g_unix_socket_address_new */
GSocketAddress *g_unix_socket_address_new(const char *path);

/* g_source_set_callback / g_source_attach */
void g_source_set_callback(GSource *source, void *function, gpointer data, void *destroy_notify);
guint g_source_attach(GSource *source, void *context);

/* g_inet_socket_address_new / g_inet_socket_address_get_port */
GSocketAddress *g_inet_socket_address_new(GInetAddress *address, guint port);
guint g_inet_socket_address_get_port(GInetSocketAddress *address);

/* g_socket_get_family */
int g_socket_get_family(GSocket *socket);

/* G_SOCKET_PROTOCOL_TCP */
#define G_SOCKET_PROTOCOL_TCP 0
#define G_SOCKET_PROTOCOL_UDP 1

/* g_resolver */
typedef struct { int dummy; } GResolver;
GResolver *g_resolver_get_default(void);
GList *g_resolver_lookup_by_name(GResolver *resolver, const char *hostname, void *cancellable, GError **error);
void g_resolver_free_addresses(GList *addresses);

/* g_inet_address_get_family / g_inet_address_equal */
int g_inet_address_get_family(GInetAddress *address);
gboolean g_inet_address_equal(GInetAddress *a, GInetAddress *b);

/* g_socket_send_to */
gssize g_socket_send_to(GSocket *socket, GSocketAddress *address, const void *buffer, gsize size, void *cancellable, GError **error);
gssize g_socket_receive_from(GSocket *socket, GSocketAddress **address, void *buffer, gsize size, void *cancellable, GError **error);

/* g_inet_address */
gboolean g_inet_address_get_is_loopback(GInetAddress *address);

/* XDMCP */
#define XDM_UDP_PORT 177

/* g_array_unref */
void g_array_unref(GArray *array);

/* GDBusInterfaceInfo — D-Bus 接口信息 */
typedef struct _GDBusInterfaceInfo {
    const char *name;
    struct _GDBusInterfaceInfo **interfaces; /* 子接口 (用于 node) */
} GDBusInterfaceInfo;

/* GDBusMethodInvocation (声明在 GDBusInterfaceVTable 之前) */
typedef struct { int dummy; } GDBusMethodInvocation;

/* GDBusNodeInfo — D-Bus 节点信息, 含 interfaces 数组 */
typedef struct _GDBusNodeInfo {
    GDBusInterfaceInfo **interfaces;
} GDBusNodeInfo;

/* GDBusInterfaceVTable — D-Bus 接口虚函数表 */
typedef struct {
    void (*method_call)(GDBusConnection *connection, const gchar *sender,
                        const gchar *object_path, const gchar *interface_name,
                        const gchar *method_name, GVariant *parameters,
                        GDBusMethodInvocation *invocation, gpointer user_data);
    GVariant *(*get_property)(GDBusConnection *connection, const gchar *sender,
                              const gchar *object_path, const gchar *interface_name,
                              const gchar *property_name, GError **error,
                              gpointer user_data);
    void (*set_property)(GDBusConnection *connection, const gchar *sender,
                         const gchar *object_path, const gchar *interface_name,
                         const gchar *property_name, GVariant *value,
                         GError **error, gpointer user_data);
} GDBusInterfaceVTable;

/* D-Bus 节点/接口操作 */
GDBusNodeInfo *g_dbus_node_info_new_for_xml(const char *xml, GError **error);
void g_dbus_node_info_unref(GDBusNodeInfo *info);
guint g_dbus_connection_register_object(GDBusConnection *connection,
                                        const char *path,
                                        GDBusInterfaceInfo *interface,
                                        const GDBusInterfaceVTable *vtable,
                                        gpointer user_data,
                                        void *destroy_notify,
                                        GError **error);
void g_dbus_connection_unregister_object(GDBusConnection *connection, guint id);
int g_dbus_error_quark(void);
#define G_DBUS_ERROR                  (g_dbus_error_quark())
#define G_DBUS_ERROR_INVALID_ARGS     (1)
#define G_DBUS_ERROR_FAILED           (2)
#define G_DBUS_ERROR_UNKNOWN_METHOD   (3)
void g_dbus_method_invocation_return_error(GDBusMethodInvocation *invocation, int domain, int code, const char *format, ...);
void g_dbus_method_invocation_return_value(GDBusMethodInvocation *invocation, GVariant *parameters);

/* g_dbus_proxy_get_name_owner / get_cached_property / new_sync / get_connection / get_cached_property_names */
const char *g_dbus_proxy_get_name_owner(GDBusProxy *proxy);
GVariant *g_dbus_proxy_get_cached_property(GDBusProxy *proxy, const char *name);
GDBusProxy *g_dbus_proxy_new_sync(GDBusConnection *connection, GDBusProxyFlags flags, void *info, const char *name, const char *object_path, const char *interface_name, void *cancellable, GError **error);
GDBusConnection *g_dbus_proxy_get_connection(GDBusProxy *proxy);
gchar **g_dbus_proxy_get_cached_property_names(GDBusProxy *proxy);

/* GFile / GFileMonitor */
typedef struct { int dummy; } GFile;
typedef struct { int dummy; } GFileMonitor;
#define G_FILE_TYPE_REGULAR       0
#define G_FILE_MONITOR_WATCH_MOVES 0
GFile *g_file_new_for_path(const char *path);
gchar *g_file_get_path(GFile *file);
GFileMonitor *g_file_monitor(GFile *file, int flags, void *cancellable, void *error);
void g_file_monitor_set_rate_limit(GFileMonitor *monitor, int limit);
typedef enum { G_FILE_MONITOR_EVENT_CHANGED, G_FILE_MONITOR_EVENT_CHANGES_DONE_HINT } GFileMonitorEvent;
#define G_FILE_MONITOR_NONE 0

#define G_PARAM_READWRITE        0
#define G_PARAM_STATIC_STRINGS   0

/* GObject 属性 (已简化, 不使用完整属性系统) */
typedef struct _GParamSpec {} GParamSpec;

/* ==================== GDateTime (简化) ==================== */

typedef struct {
    gint year;
    gint month;
    gint day;
    gint hour;
    gint minute;
    gint second;
} GDateTime;

GDateTime   *g_date_time_new_now_local(void);
void         g_date_time_unref(GDateTime *dt);

/* ==================== GRegex (简化) ==================== */

typedef struct {
    gchar pattern[64];
} GRegex;

typedef struct {} GMatchInfo;

GRegex      *g_regex_new(const gchar *pattern, guint compile_flags, guint match_flags, GError **error);
gboolean     g_regex_match(GRegex *regex, const gchar *string, guint match_flags, GMatchInfo **match_info);
void         g_regex_unref(GRegex *regex);
void         g_match_info_unref(GMatchInfo *match_info);

/* ==================== GMarkup (简化) ==================== */

typedef struct {} GMarkupParser;
typedef struct {} GMarkupParseContext;

GMarkupParseContext *g_markup_parse_context_new(const GMarkupParser *parser,
                                                 guint flags,
                                                 gpointer user_data,
                                                 void (*user_data_dnotify)(gpointer));
void g_markup_parse_context_free(GMarkupParseContext *context);

/* ==================== 系统头文件模拟 ==================== */

/* POSIX 文件 I/O 通过 PlexsDOS HAL 实现 */
int open(const char *path, int flags, ...);
int close(int fd);
ssize_t read(int fd, void *buf, size_t count);
ssize_t write(int fd, const void *buf, size_t count);
int fcntl(int fd, int cmd, ...);

/* 标准 C 模拟 */
void *malloc(size_t size);
void *calloc(size_t nmemb, size_t size);
void *realloc(void *ptr, size_t size);
void free(void *ptr);
int remove(const char *path);
int rename(const char *old, const char *new);
int unlink(const char *path);
int rmdir(const char *path);
int atoi(const char *nptr);
double strtod(const char *nptr, char **endptr);
long strtol(const char *nptr, char **endptr, int base);

/* ==================== PlexsDOS shim 映射 ==================== */

/* PAM → DFAN 映射在 session.c 中直接替换, 不通过此头文件 */
/* D-Bus → 1-Bus 映射在 display-manager-service.c 中直接替换 */
/* login1 → syslog 在 login1_*.c 中直接替换 */

#ifdef __cplusplus
}
#endif

#endif /* _PLXDM_COMPAT_H */
