/*
 * Nexsteaduser — PlexsDOS
 * config_sys.h — CONFIG.SYS 启动配置接口
 * 作者: Tinmc189623 | 团队: Nexlyh
 *
 * 经典 DOS 风格 CONFIG.SYS 解析器。
 * 支持: SHELL, DEVICE, FILES, BUFFERS, LASTDRIVE, BREAK, ECHO, DOS
 */

#ifndef _PLXSDOS_CONFIG_SYS_H
#define _PLXSDOS_CONFIG_SYS_H

#include <plexsdos/types.h>

#ifdef __cplusplus
extern "C" {
#endif

/* 最大配置行数 */
#define CONFIG_MAX_LINES      64
/* 每行最大字符数 (含 null) */
#define CONFIG_LINE_SIZE      128
/* 命令名/值最大长度 */
#define CONFIG_CMD_SIZE       32
#define CONFIG_VAL_SIZE       128

/* CONFIG.SYS 解析结果 */
struct config_sys {
    int   files;              /* FILES=nn 最大打开文件数 */
    int   buffers;            /* BUFFERS=nn 磁盘缓冲区数 */
    char  last_drive;         /* LASTDRIVE=X */
    bool  break_on;           /* BREAK=ON|OFF */
    bool  echo_on;            /* ECHO=ON|OFF, 启动时逐行显示 */
    bool  dos_high;           /* DOS=HIGH */
    char  shell_path[64];     /* SHELL=path */
    /* DEVICE= 链表 (可加载驱动) */
    struct config_sys_entry *devices;
    int   device_count;
};

/* 单条 DEVICE= 条目 (链表节点) */
struct config_sys_entry {
    char value[CONFIG_VAL_SIZE];
    struct config_sys_entry *next;
};

/*
 * config_sys_init — 初始化默认配置
 *
 * 在所有字段填入默认值。
 */
void config_sys_init(void);

/*
 * config_sys_parse — 解析 CONFIG.SYS 内容
 * @data: 文件内容字符串 (null 结尾)
 *
 * 按行解析, 跳过注释 (;) 和空行。
 * 将解析结果写入全局 config 结构。
 */
void config_sys_parse(const char *data);

/*
 * config_sys_get — 获取解析后的配置
 * 返回: 只读配置指针。
 */
const struct config_sys *config_sys_get(void);

/*
 * config_sys_print — 在屏幕上输出当前配置摘要
 */
void config_sys_print(void);

/*
 * config_sys_find_device — 查找 DEVICE= 条目
 * @index: 0-based 索引
 * 返回: 设备路径, NULL 表示超出范围。
 */
const char *config_sys_find_device(int index);

#ifdef __cplusplus
}
#endif

#endif /* _PLXSDOS_CONFIG_SYS_H */
