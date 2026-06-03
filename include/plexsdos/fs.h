/*
 * Nexsteaduser — PlexsDOS
 * fs.h — 统一文件系统抽象层
 * 作者: Tinmc189623 | 团队: Nexlyh
 *
 * 提供 FAT12 (软盘) 和 FAT32 (硬盘) 的统一操作接口。
 * 根据当前驱动器自动选择对应的文件系统实现。
 */

#ifndef _PLXSDOS_FS_H
#define _PLXSDOS_FS_H

#include <plexsdos/types.h>

#ifdef __cplusplus
extern "C" {
#endif

/* 文件系统类型 */
#define FS_FAT12   0
#define FS_FAT32   1
#define FS_NONE    0xFF

/* 目录项信息 (与驱动无关的通用结构) */
struct fs_entry {
    char     name[13];         /* 8.3 格式文件名 (含 null) */
    uint8_t  attr;             /* 文件属性 */
    uint32_t file_size;        /* 文件大小 */
    bool     is_directory;     /* 是否目录 */
};

/*
 * fs_init — 初始化文件系统
 * @drive_type: 驱动器类型 (FS_FAT12 或 FS_FAT32)
 * @param:      参数 (FAT12 需要 BPB 地址, FAT32 需要分区 LBA)
 *
 * 返回: true = 成功, false = 失败。
 */
bool fs_init(int drive_type, uint32_t param);

/*
 * fs_list_root — 列出根目录内容
 * 输出到屏幕。
 */
void fs_list_root(void);

/*
 * fs_list_dir — 列出指定路径的目录内容
 * @path: 目录路径 (如 "" 表示根目录)
 */
void fs_list_dir(const char *path);

/*
 * fs_find_file — 按文件名查找文件
 * @name: 文件名 (自动转为 8.3 格式)
 * 返回: 静态 fs_entry 指针, 未找到返回 NULL。
 */
struct fs_entry *fs_find_file(const char *name);

/*
 * fs_load_file — 将文件内容加载到指定内存地址
 * @entry: 目录项指针
 * @load_addr: 目标内存地址
 * 返回: 加载的字节数, 失败返回 0。
 */
uint32_t fs_load_file(struct fs_entry *entry, uint32_t load_addr);

/*
 * fs_write_file — 将数据写入文件 (创建新文件或覆盖)
 * @name: 文件名
 * @data: 数据缓冲区
 * @size: 数据大小
 * 返回: true = 成功, false = 失败。
 */
bool fs_write_file(const char *name, const uint8_t *data, uint32_t size);

/*
 * fs_delete_file — 删除文件
 * @name: 文件名
 * 返回: true = 成功, false = 失败。
 */
bool fs_delete_file(const char *name);

/*
 * fs_rename_file — 重命名文件
 * @old_name: 原文件名
 * @new_name: 新文件名
 * 返回: true = 成功, false = 失败。
 */
bool fs_rename_file(const char *old_name, const char *new_name);

/*
 * fs_get_free_space — 获取空闲空间信息
 * @total_bytes: 输出总字节数
 * @free_bytes:  输出空闲字节数
 * 返回: true = 成功, false = 失败。
 */
bool fs_get_free_space(uint32_t *total_bytes, uint32_t *free_bytes);

/*
 * fs_get_volume_label — 获取卷标
 * @label: 输出缓冲区 (至少 12 字节)
 * 返回: true = 成功, false = 失败。
 */
bool fs_get_volume_label(char *label);

/*
 * fs_set_volume_label — 设置卷标
 * @label: 新卷标 (最多 11 字符)
 * 返回: true = 成功, false = 失败。
 */
bool fs_set_volume_label(const char *label);

/*
 * fs_get_type — 获取当前文件系统类型名
 * 返回: "FAT12", "FAT32", "NONE" 之一的字符串指针。
 */
const char *fs_get_type(void);

/*
 * fs_get_current_drive — 获取当前驱动器字母
 * 返回: 'A' - 'Z'。
 */
char fs_get_current_drive(void);

/*
 * fs_set_current_drive — 设置当前驱动器
 * @letter: 驱动器字母 ('A' - 'Z')
 * 返回: true = 成功, false = 失败或不可用。
 */
bool fs_set_current_drive(char letter);

#ifdef __cplusplus
}
#endif

#endif /* _PLXSDOS_FS_H */
