/*
 * Nexsteaduser — PlexsDOS
 * FAT12 文件系统接口
 * 作者: Tinmc189623 | 团队: Nexlyh
 */

#ifndef _PLXSDOS_FAT12_H
#define _PLXSDOS_FAT12_H

#include <plexsdos/types.h>

#ifdef __cplusplus
extern "C" {
#endif

/* FAT12 目录项结构 (32 字节) */
struct fat12_dir_entry {
    uint8_t  name[11];        /* 8.3 格式文件名 (空格填充) */
    uint8_t  attr;            /* 文件属性 */
    uint8_t  reserved;        /* 保留 */
    uint8_t  create_time_ms;  /* 创建时间 (10ms 单位) */
    uint16_t create_time;     /* 创建时间 */
    uint16_t create_date;     /* 创建日期 */
    uint16_t access_date;     /* 最后访问日期 */
    uint16_t cluster_hi;      /* 起始簇号高 16 位 (FAT12/16 为 0) */
    uint16_t modify_time;     /* 最后修改时间 */
    uint16_t modify_date;     /* 最后修改日期 */
    uint16_t cluster_lo;      /* 起始簇号低 16 位 */
    uint32_t file_size;       /* 文件大小 (字节) */
} __attribute__((packed));

/* 目录项属性标志 */
#define FAT12_ATTR_READ_ONLY  0x01
#define FAT12_ATTR_HIDDEN     0x02
#define FAT12_ATTR_SYSTEM     0x04
#define FAT12_ATTR_VOLUME_ID  0x08
#define FAT12_ATTR_DIRECTORY  0x10
#define FAT12_ATTR_ARCHIVE    0x20

/* FAT12 特殊簇值 */
#define FAT12_CLUSTER_FREE       0x000
#define FAT12_CLUSTER_RESERVED   0x001
#define FAT12_CLUSTER_BAD        0xFF7
#define FAT12_CLUSTER_END_MIN    0xFF8
#define FAT12_CLUSTER_END_MAX    0xFFF

/* 扇区读函数类型: 读取 count 个扇区从 LBA 到 buf */
typedef bool (*fat12_read_fn)(uint32_t lba, uint8_t count, void *buf);

/* 扇区写函数类型: 将 buf 的 count 个扇区写入 LBA 起始位置 */
typedef bool (*fat12_write_fn)(uint32_t lba, uint8_t count, const void *buf);

/*
 * fat12_init — 使用 ATA 磁盘初始化 FAT12
 * 从引导扇区 0x7C00 读取 BPB, 加载 FAT 表和根目录。
 * 返回: TRUE = 成功, FALSE = 失败。
 */
bool fat12_init(void);

/*
 * fat12_init_ex — 使用自定义读函数初始化 FAT12
 * @read_fn:   扇区读函数 (可使用 ATA 或 FDC)
 * @bpb_addr:  BPB 数据地址 (引导扇区在内存中的位置)
 * @bpb_lba:   引导扇区在磁盘上的 LBA (FDC 场景需要)
 *
 * 用于从软盘 (FDC) 或其他设备加载 FAT12 文件系统。
 * 返回: TRUE = 成功, FALSE = 失败。
 */
bool fat12_init_ex(fat12_read_fn read_fn, uint32_t bpb_addr, uint32_t bpb_lba);

/*
 * fat12_list_root — 列出根目录下的所有文件
 * 输出文件名、大小到屏幕。
 */
void fat12_list_root(void);

/*
 * fat12_find_file — 按文件名在根目录中查找文件
 * @name: 8.3 格式文件名 (如 "TEST    BIN" 或 "test.bin")
 * 返回: 目录项指针, 未找到返回 NULL。
 */
struct fat12_dir_entry *fat12_find_file(const char *name);

/*
 * fat12_load_file — 将文件内容加载到指定内存地址
 * @entry:     目录项指针
 * @load_addr: 目标内存地址
 * 返回: 加载的字节数, 失败返回 0。
 */
uint32_t fat12_load_file(struct fat12_dir_entry *entry, uint32_t load_addr);

/*
 * fat12_set_write_fn — 设置扇区写函数
 * @fn: 写函数指针 (NULL = 禁用写入)
 */
void fat12_set_write_fn(fat12_write_fn fn);

/*
 * fat12_write_file — 将数据写入 FAT12 文件系统 (新建/覆盖)
 * @name: 文件名
 * @data: 数据缓冲区
 * @size: 数据大小
 * 返回: true = 成功。
 */
bool fat12_write_file(const char *name, const uint8_t *data, uint32_t size);

/*
 * fat12_delete_file — 删除文件
 * @name: 文件名
 * 返回: true = 成功。
 */
bool fat12_delete_file(const char *name);

/*
 * fat12_rename_file — 重命名文件
 * @old_name: 原文件名
 * @new_name: 新文件名
 * 返回: true = 成功。
 */
bool fat12_rename_file(const char *old_name, const char *new_name);

#ifdef __cplusplus
}
#endif

#endif /* _PLXSDOS_FAT12_H */
