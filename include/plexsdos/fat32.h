/*
 * Nexsteaduser — PlexsDOS
 * FAT32 文件系统接口
 * 作者: Tinmc189623 | 团队: Nexlyh
 */

#ifndef _PLXSDOS_FAT32_H
#define _PLXSDOS_FAT32_H

#include <plexsdos/types.h>

#ifdef __cplusplus
extern "C" {
#endif

/* FAT32 目录项结构 (32 字节) */
struct fat32_dir_entry {
    uint8_t  name[11];        /* 8.3 格式文件名 (空格填充) */
    uint8_t  attr;            /* 文件属性 */
    uint8_t  reserved;        /* 保留 */
    uint8_t  create_time_ms;  /* 创建时间 (10ms 单位) */
    uint16_t create_time;     /* 创建时间 */
    uint16_t create_date;     /* 创建日期 */
    uint16_t access_date;     /* 最后访问日期 */
    uint16_t cluster_hi;      /* 起始簇号高 16 位 */
    uint16_t modify_time;     /* 最后修改时间 */
    uint16_t modify_date;     /* 最后修改日期 */
    uint16_t cluster_lo;      /* 起始簇号低 16 位 */
    uint32_t file_size;       /* 文件大小 (字节) */
} __attribute__((packed));

/* FAT32 目录项属性标志 */
#define FAT32_ATTR_READ_ONLY  0x01
#define FAT32_ATTR_HIDDEN     0x02
#define FAT32_ATTR_SYSTEM     0x04
#define FAT32_ATTR_VOLUME_ID  0x08
#define FAT32_ATTR_DIRECTORY  0x10
#define FAT32_ATTR_ARCHIVE    0x20
#define FAT32_ATTR_LFN        0x0F  /* 长文件名条目 */

/* FAT32 特殊簇值 */
#define FAT32_CLUSTER_FREE       0x00000000
#define FAT32_CLUSTER_RESERVED   0x0FFFFFF0
#define FAT32_CLUSTER_BAD        0x0FFFFFF7
#define FAT32_CLUSTER_END_MIN    0x0FFFFFF8
#define FAT32_CLUSTER_END_MAX    0x0FFFFFFF

/* FAT32 最大簇号 */
#define FAT32_MAX_CLUSTER        0x0FFFFFF8

/*
 * fat32_init — 初始化 FAT32 文件系统 (从 0x7C00 引导扇区)
 * 读取 BPB、FAT 和根目录到内存。
 * 成功返回 TRUE, 失败返回 FALSE。
 */
bool fat32_init(void);

/*
 * fat32_init_drive — 从指定分区 LBA 初始化 FAT32
 * @partition_lba: 分区起始 LBA 扇区号
 *
 * 从磁盘读取引导扇区, 解析 BPB, 初始化 FAT32 文件系统。
 * 用于多驱动器场景 (驱动器切换时调用)。
 * 返回: true = 成功, false = 失败。
 */
bool fat32_init_drive(uint32_t partition_lba);

/*
 * fat32_list_root — 列出根目录下的所有文件
 * 输出文件名、大小到屏幕。
 */
void fat32_list_root(void);

/*
 * fat32_find_file — 按文件名在根目录中查找文件
 * @name: 普通文件名 (如 "test.bin"), 自动转为 8.3 格式。
 * 返回: 目录项指针, 未找到返回 NULL。
 */
struct fat32_dir_entry *fat32_find_file(const char *name);

/*
 * fat32_load_file — 将文件内容加载到指定内存地址
 * @entry: 目录项指针
 * @load_addr: 目标内存地址
 * 返回: 加载的字节数, 失败返回 0。
 */
uint32_t fat32_load_file(struct fat32_dir_entry *entry, uint32_t load_addr);

/*
 * fat32_write_file — 将数据写入 FAT32 文件系统
 * @name:     文件名 (8.3 格式或普通格式)
 * @data:     数据缓冲区
 * @size:     数据大小 (字节)
 * 返回: TRUE = 成功, FALSE = 失败。
 *
 * 在根目录中创建新文件, 分配簇, 写入数据。
 */
bool fat32_write_file(const char *name, const uint8_t *data, uint32_t size);

/*
 * fat32_delete_file — 删除 FAT32 文件
 * @name: 文件名
 * 返回: true = 成功。
 */
bool fat32_delete_file(const char *name);

/*
 * fat32_rename_file — 重命名 FAT32 文件
 * @old_name: 原文件名
 * @new_name: 新文件名
 * 返回: true = 成功。
 */
bool fat32_rename_file(const char *old_name, const char *new_name);

/*
 * fat32_create_dir — 在根目录下创建子目录
 * @name: 目录名
 * 返回: true = 成功, false = 失败。
 */
bool fat32_create_dir(const char *name);

/*
 * fat32_format — 格式化 FAT32 分区
 * @partition_lba: 分区起始 LBA 扇区号
 *
 * 重新初始化 FAT 表、FSINFO 和根目录簇。
 * 返回: true = 成功, false = 失败。
 */
bool fat32_format(uint32_t partition_lba);

#ifdef __cplusplus
}
#endif

#endif /* _PLXSDOS_FAT32_H */
