/*
 * Nexsteaduser — PlexsDOS
 * FAT32 文件系统驱动 (32-bit 保护模式)
 * 作者: Tinmc189623 | 团队: Nexlyh
 *
 * 通过 ATA 磁盘驱动读取 FAT32 格式分区。
 * 支持: 文件查找、根目录列表、文件加载到内存。
 * FAT32 特性: 32位簇号、最大 2TB 分区、长文件名 (可选)。
 *
 * FAT32 布局:
 *   保留扇区 (含 BPB) | FAT1 | FAT2 | 数据区
 *   根目录在数据区的簇 2 (与 FAT12/16 不同, 无独立根目录区)
 */

#include <plexsdos/types.h>
#include <plexsdos/config.h>
#include <plexsdos/fat32.h>
#include <plexsdos/disk.h>
#include <plexsdos/screen.h>
#include <plexsdos/string.h>

/* FAT32 BPB 参数 (从引导扇区 0x7C00 复制) */
static uint16_t bpb_bytes_per_sec;    /* 每扇区字节数 */
static uint8_t  bpb_sec_per_clust;    /* 每簇扇区数 */
static uint16_t bpb_reserved_secs;    /* 保留扇区数 */
static uint8_t  bpb_num_fats;         /* FAT 表数量 */
static uint32_t bpb_total_secs;       /* 总扇区数 */
static uint32_t bpb_fat_size;         /* 每个 FAT 占用扇区数 (FAT32) */
static uint32_t bpb_root_cluster;     /* 根目录起始簇号 (FAT32) */
static uint16_t bpb_fs_info;          /* FSINFO 扇区号 */

/* 计算得到的布局参数 */
static uint32_t fat_start_lba;        /* FAT 起始 LBA */
static uint32_t data_area_lba;        /* 数据区起始 LBA */
static uint32_t total_clusters;       /* 总簇数 */

/* FAT 表缓冲区 (动态分配, 当前使用静态缓冲区) */
#define FAT_BUF_SIZE  (16 * 1024)     /* 16KB 缓冲区 */
static uint8_t fat_buf[FAT_BUF_SIZE];

/* 目录项缓冲区 */
#define DIR_BUF_SIZE  (8 * 1024)      /* 8KB 缓冲区 */
static uint8_t dir_buf[DIR_BUF_SIZE];

/* 扇区缓冲区 */
static uint8_t sector_buf[512];

/* ===== 辅助函数 ===== */

/*
 * 将文件名转为 FAT32 8.3 格式 (空格填充, 大写)。
 * "test.bin" -> "TEST    BIN"
 */
static void convert_to_83(const char *name, char *out)
{
    int i;

    /* 填充空格 */
    for (i = 0; i < 11; i++)
        out[i] = ' ';

    /* 复制主文件名 (最多 8 字符) */
    i = 0;
    while (*name && *name != '.' && i < 8) {
        char c = *name;
        if (c >= 'a' && c <= 'z')
            c -= 32;
        out[i++] = c;
        name++;
    }

    /* 查找扩展名 */
    while (*name && *name != '.')
        name++;
    if (*name == '.') {
        name++;
        i = 8;
        while (*name && i < 11) {
            char c = *name;
            if (c >= 'a' && c <= 'z')
                c -= 32;
            out[i++] = c;
            name++;
        }
    }
}

/*
 * 比较两个 FAT32 文件名 (11 字节, 大小写不敏感)。
 * 返回: 0 = 匹配, 非 0 = 不匹配。
 */
static int fat32_name_cmp(const uint8_t *a, const uint8_t *b)
{
    for (int i = 0; i < 11; i++) {
        char ca = a[i];
        char cb = b[i];
        if (ca >= 'a' && ca <= 'z') ca -= 32;
        if (cb >= 'a' && cb <= 'z') cb -= 32;
        if (ca != cb)
            return (int)ca - (int)cb;
    }
    return 0;
}

/*
 * 读取一个扇区到缓冲区
 * 返回: true = 成功, false = 失败。
 */
static bool read_sector(uint32_t lba)
{
    return disk_read_sectors(lba, 1, sector_buf);
}

/* ===== FAT32 初始化 ===== */

/*
 * fat32_init — 初始化 FAT32 文件系统
 *
 * 1. 从 0x7C00 复制 BPB 参数
 * 2. 验证 FAT32 签名
 * 3. 读取 FAT1 表到内存
 * 返回: true = 成功, false = 失败。
 */
bool fat32_init(void)
{
    uint8_t *bpb = (uint8_t *)0x7C00;

    /* 验证引导签名 */
    uint16_t *sig = (uint16_t *)0x7DFE;
    if (*sig != 0xAA55) {
        screen_puts("[fat32] Invalid boot signature\n");
        return false;
    }

    /* 从引导扇区复制 BPB 参数 */
    bpb_bytes_per_sec  = *(uint16_t *)(bpb + 11);
    bpb_sec_per_clust  = *(uint8_t  *)(bpb + 13);
    bpb_reserved_secs  = *(uint16_t *)(bpb + 14);
    bpb_num_fats       = *(uint8_t  *)(bpb + 16);

    /* FAT32 特有字段 (偏移 32+) */
    bpb_total_secs     = *(uint32_t *)(bpb + 32);
    bpb_fat_size       = *(uint32_t *)(bpb + 36);
    bpb_root_cluster   = *(uint32_t *)(bpb + 44);
    bpb_fs_info        = *(uint16_t *)(bpb + 48);

    /* 计算文件系统布局 */
    fat_start_lba  = bpb_reserved_secs;
    data_area_lba  = fat_start_lba + (bpb_num_fats * bpb_fat_size);

    /* 计算总簇数 */
    uint32_t data_sectors = bpb_total_secs - data_area_lba;
    total_clusters = data_sectors / bpb_sec_per_clust;

    /* 验证 FAT32 特征 */
    if (bpb_fat_size == 0) {
        /* 不是 FAT32 (FAT12/16 的 bpb_fat_size 在不同偏移) */
        screen_puts("[fat32] Not a FAT32 volume\n");
        return false;
    }

    /* 读取 FAT1 表 (前 64KB) */
    uint32_t fat_bytes = bpb_fat_size * bpb_bytes_per_sec;
    if (fat_bytes > FAT_BUF_SIZE)
        fat_bytes = FAT_BUF_SIZE;
    uint8_t fat_sectors = (uint8_t)(fat_bytes / bpb_bytes_per_sec);
    if (!disk_read_sectors(fat_start_lba, fat_sectors, fat_buf)) {
        screen_puts("[fat32] Failed to read FAT\n");
        return false;
    }

    /* 显示文件系统信息 */
    screen_puts("[fat32] Cluster size: ");
    screen_put_dec((uint32_t)bpb_sec_per_clust * bpb_bytes_per_sec);
    screen_puts(" bytes\n");

    screen_puts("[fat32] Total clusters: ");
    screen_put_dec(total_clusters);
    screen_putchar('\n');

    screen_puts("[fat32] Root cluster: ");
    screen_put_dec(bpb_root_cluster);
    screen_putchar('\n');

    screen_puts("[fat32] Filesystem ready\n");
    return true;
}

/*
 * fat32_init_drive — 从指定分区 LBA 初始化 FAT32
 * @partition_lba: 分区起始 LBA 扇区号
 *
 * 从磁盘读取引导扇区到 sector_buf, 解析 BPB 参数,
 * 读取 FAT1 表, 初始化文件系统。
 * 用于驱动器切换时重新初始化 FAT32。
 */
bool fat32_init_drive(uint32_t partition_lba)
{
    uint8_t *bpb;

    /* 读取分区引导扇区 */
    if (!disk_read_sectors(partition_lba, 1, sector_buf)) {
        screen_puts("[fat32] Failed to read boot sector\n");
        return false;
    }

    bpb = sector_buf;

    /* 验证引导签名 */
    uint16_t *sig = (uint16_t *)(bpb + 510);
    if (*sig != 0xAA55) {
        screen_puts("[fat32] Invalid boot signature\n");
        return false;
    }

    /* 从引导扇区复制 BPB 参数 */
    bpb_bytes_per_sec  = *(uint16_t *)(bpb + 11);
    bpb_sec_per_clust  = *(uint8_t  *)(bpb + 13);
    bpb_reserved_secs  = *(uint16_t *)(bpb + 14);
    bpb_num_fats       = *(uint8_t  *)(bpb + 16);

    /* FAT32 特有字段 (偏移 32+) */
    bpb_total_secs     = *(uint32_t *)(bpb + 32);
    bpb_fat_size       = *(uint32_t *)(bpb + 36);
    bpb_root_cluster   = *(uint32_t *)(bpb + 44);
    bpb_fs_info        = *(uint16_t *)(bpb + 48);

    /* 计算文件系统布局 (相对于分区起始) */
    fat_start_lba  = partition_lba + bpb_reserved_secs;
    data_area_lba  = fat_start_lba + (bpb_num_fats * bpb_fat_size);

    /* 计算总簇数 */
    uint32_t data_sectors = bpb_total_secs - bpb_reserved_secs
                           - (bpb_num_fats * bpb_fat_size);
    total_clusters = data_sectors / bpb_sec_per_clust;

    /* 验证 FAT32 特征 */
    if (bpb_fat_size == 0) {
        screen_puts("[fat32] Not a FAT32 volume\n");
        return false;
    }

    /* 读取 FAT1 表 (前 64KB) */
    uint32_t fat_bytes = bpb_fat_size * bpb_bytes_per_sec;
    if (fat_bytes > FAT_BUF_SIZE)
        fat_bytes = FAT_BUF_SIZE;
    uint8_t fat_sectors = (uint8_t)(fat_bytes / bpb_bytes_per_sec);
    if (!disk_read_sectors(fat_start_lba, fat_sectors, fat_buf)) {
        screen_puts("[fat32] Failed to read FAT\n");
        return false;
    }

    screen_puts("[fat32] Filesystem ready (LBA ");
    screen_put_dec(partition_lba);
    screen_puts(")\n");
    return true;
}

/* ===== FAT32 簇操作 ===== */

/*
 * fat32_get_next_cluster — 从 FAT 表获取下一个簇号
 * @cluster: 当前簇号
 * 返回: 下一个簇号 (0x00000000 - 0x0FFFFFFF)
 *
 * FAT32 每个簇号占 32 位 (实际使用 28 位)。
 */
static uint32_t fat32_get_next_cluster(uint32_t cluster)
{
    uint32_t offset = cluster * 4;
    uint32_t value;

    /* 检查是否在缓冲区内 */
    if (offset + 4 <= FAT_BUF_SIZE) {
        value = *(uint32_t *)(fat_buf + offset);
    } else {
        /* 需要读取 FAT 扇区 */
        uint32_t fat_sector = fat_start_lba + (offset / 512);
        if (!read_sector(fat_sector)) {
            return FAT32_CLUSTER_END_MAX;
        }
        uint32_t sector_offset = offset % 512;
        value = *(uint32_t *)(sector_buf + sector_offset);
    }

    /* 只使用低 28 位 */
    return value & 0x0FFFFFFF;
}

/*
 * fat32_get_cluster_lba — 计算簇号对应的 LBA 地址
 * @cluster: 簇号
 * 返回: LBA 扇区地址。
 */
static uint32_t fat32_get_cluster_lba(uint32_t cluster)
{
    return data_area_lba + (uint32_t)((cluster - 2) * bpb_sec_per_clust);
}

/* ===== FAT32 操作 ===== */

/*
 * fat32_list_root — 列出根目录下所有有效文件
 *
 * FAT32 根目录是一个簇链 (从 bpb_root_cluster 开始)。
 * 遍历簇链中的所有目录项, 跳过 LFN 和已删除条目。
 */
void fat32_list_root(void)
{
    uint32_t cluster = bpb_root_cluster;
    int count = 0;

    screen_set_color(0x0B, 0x00);
    screen_puts("Name           Size\n");
    screen_puts("----------- --------\n");
    screen_reset_color();

    /* 遍历根目录簇链 */
    while (cluster >= 2 && cluster < FAT32_MAX_CLUSTER) {
        uint32_t lba = fat32_get_cluster_lba(cluster);

        /* 读取整个簇 */
        for (uint8_t s = 0; s < bpb_sec_per_clust; s++) {
            if (!read_sector(lba + s)) {
                screen_puts("[fat32] Read error\n");
                return;
            }

            /* 解析目录项 (每项 32 字节, 每扇区 16 项) */
            struct fat32_dir_entry *entries = (struct fat32_dir_entry *)sector_buf;
            for (int i = 0; i < 16; i++) {
                uint8_t first = entries[i].name[0];

                /* 空条目 = 目录结束 */
                if (first == 0x00)
                    goto done;

                /* 已删除条目 */
                if (first == 0xE5)
                    continue;

                /* LFN 条目 */
                if (entries[i].attr == FAT32_ATTR_LFN)
                    continue;

                /* 卷标 */
                if (entries[i].attr & FAT32_ATTR_VOLUME_ID)
                    continue;

                /* 输出文件名 (8.3 格式) */
                for (int j = 0; j < 8; j++) {
                    if (entries[i].name[j] != ' ')
                        screen_putchar(entries[i].name[j]);
                }
                if (entries[i].name[8] != ' ') {
                    screen_putchar('.');
                    for (int j = 8; j < 11; j++) {
                        if (entries[i].name[j] != ' ')
                            screen_putchar(entries[i].name[j]);
                    }
                }

                /* 输出文件大小 */
                screen_puts("    ");
                screen_put_dec(entries[i].file_size);
                screen_puts(" bytes\n");

                count++;
            }
        }

        /* 获取下一个簇 */
        cluster = fat32_get_next_cluster(cluster);
    }

done:
    if (count == 0) {
        screen_puts("(empty)\n");
    }
}

/*
 * fat32_find_file — 按文件名在根目录中查找文件
 * @name: 普通文件名 (如 "test.bin"), 自动转为 8.3 格式。
 * 返回: 目录项指针 (静态缓冲区), 未找到返回 NULL。
 */
struct fat32_dir_entry *fat32_find_file(const char *name)
{
    char name83[11];
    uint32_t cluster = bpb_root_cluster;

    convert_to_83(name, name83);

    /* 遍历根目录簇链 */
    while (cluster >= 2 && cluster < FAT32_MAX_CLUSTER) {
        uint32_t lba = fat32_get_cluster_lba(cluster);

        for (uint8_t s = 0; s < bpb_sec_per_clust; s++) {
            if (!read_sector(lba + s))
                return NULL;

            struct fat32_dir_entry *entries = (struct fat32_dir_entry *)sector_buf;
            for (int i = 0; i < 16; i++) {
                uint8_t first = entries[i].name[0];

                if (first == 0x00)
                    return NULL;
                if (first == 0xE5)
                    continue;
                if (entries[i].attr == FAT32_ATTR_LFN)
                    continue;
                if (entries[i].attr & FAT32_ATTR_VOLUME_ID)
                    continue;

                if (fat32_name_cmp(entries[i].name, (uint8_t *)name83) == 0) {
                    /* 复制到静态缓冲区 (sector_buf 会被覆盖) */
                    static struct fat32_dir_entry found_entry;
                    found_entry = entries[i];
                    return &found_entry;
                }
            }
        }

        cluster = fat32_get_next_cluster(cluster);
    }

    return NULL;
}

/*
 * fat32_load_file — 将文件内容加载到指定内存地址
 * @entry: 目录项指针
 * @load_addr: 目标内存地址
 *
 * 沿 FAT32 簇链逐簇读取, 直到遇到结束标记。
 * 返回: 加载的字节数, 失败返回 0。
 */
uint32_t fat32_load_file(struct fat32_dir_entry *entry, uint32_t load_addr)
{
    uint32_t cluster = ((uint32_t)entry->cluster_hi << 16) | entry->cluster_lo;
    uint32_t file_size = entry->file_size;
    uint32_t bytes_loaded = 0;
    uint8_t *dest = (uint8_t *)load_addr;

    if (cluster < 2 || cluster >= FAT32_CLUSTER_BAD) {
        screen_puts("[fat32] Invalid start cluster\n");
        return 0;
    }

    /* 遍历簇链 */
    while (cluster >= 2 && cluster < FAT32_MAX_CLUSTER) {
        uint32_t lba = fat32_get_cluster_lba(cluster);

        /* 读取整个簇 */
        for (uint8_t s = 0; s < bpb_sec_per_clust; s++) {
            if (!read_sector(lba + s)) {
                screen_puts("[fat32] Read error\n");
                return bytes_loaded;
            }

            /* 复制到目标地址 */
            uint32_t copy_size = 512;
            if (bytes_loaded + copy_size > file_size)
                copy_size = file_size - bytes_loaded;
            if (copy_size == 0)
                break;

            for (uint32_t i = 0; i < copy_size; i++)
                dest[bytes_loaded + i] = sector_buf[i];

            bytes_loaded += copy_size;
        }

        /* 获取下一个簇 */
        cluster = fat32_get_next_cluster(cluster);
    }

    return bytes_loaded;
}

/* ===== FAT32 写操作 ===== */

/*
 * fat32_alloc_cluster — 在 FAT 表中分配一个空闲簇
 * @hint: 从该簇号开始搜索 (0 = 从头搜索)
 * 返回: 分配的簇号, 0 = 无空闲簇。
 *
 * 扫描 FAT 表寻找值为 0 的条目,
 * 标记为结束标记 (0x0FFFFFF8) 并返回簇号。
 */
static uint32_t fat32_alloc_cluster(uint32_t hint)
{
    uint32_t start = (hint >= 2) ? hint : 2;

    for (uint32_t cluster = start; cluster < total_clusters + 2; cluster++) {
        uint32_t offset = cluster * 4;

        /* 检查是否在 FAT 缓冲区内 */
        if (offset + 4 <= FAT_BUF_SIZE) {
            uint32_t value = *(uint32_t *)(fat_buf + offset);
            if ((value & 0x0FFFFFFF) == 0) {
                /* 标记为结束 */
                *(uint32_t *)(fat_buf + offset) = FAT32_CLUSTER_END_MIN;

                /* 写回 FAT 扇区 */
                uint32_t fat_sector = fat_start_lba + (offset / 512);
                if (!disk_write_sectors(fat_sector, 1,
                    fat_buf + (offset & ~0x1FF))) {
                    return 0;
                }
                return cluster;
            }
        }
    }

    return 0;  /* 无空闲簇 */
}

/*
 * fat32_update_fat — 更新 FAT 表中指定簇的值
 * @cluster: 簇号
 * @value:   新值 (下一个簇号或结束标记)
 *
 * 同时更新内存缓冲区和磁盘上的 FAT 表。
 */
static void fat32_update_fat(uint32_t cluster, uint32_t value)
{
    uint32_t offset = cluster * 4;

    /* 更新内存缓冲区 */
    if (offset + 4 <= FAT_BUF_SIZE) {
        *(uint32_t *)(fat_buf + offset) = value & 0x0FFFFFFF;
    }

    /* 写回磁盘 */
    uint32_t fat_sector = fat_start_lba + (offset / 512);
    if (offset + 4 <= FAT_BUF_SIZE) {
        disk_write_sectors(fat_sector, 1, fat_buf + (offset & ~0x1FF));
    }
}

/*
 * fat32_write_file — 将数据写入 FAT32 文件系统
 * @name: 文件名 (普通格式, 如 "test.bin")
 * @data: 数据缓冲区
 * @size: 数据大小 (字节)
 * 返回: true = 成功, false = 失败。
 *
 * 1. 查找根目录中的空闲目录项
 * 2. 分配簇并写入文件数据
 * 3. 创建目录项
 */
bool fat32_write_file(const char *name, const uint8_t *data, uint32_t size)
{
    char name83[11];
    uint32_t cluster = bpb_root_cluster;
    uint32_t data_offset = 0;
    uint32_t prev_cluster = 0;
    uint32_t first_cluster = 0;

    convert_to_83(name, name83);

    /* 计算需要的簇数 */
    uint32_t cluster_size = (uint32_t)bpb_sec_per_clust * bpb_bytes_per_sec;
    uint32_t clusters_needed = (size + cluster_size - 1) / cluster_size;
    if (clusters_needed == 0)
        clusters_needed = 1;  /* 空文件也需要一个簇 */

    /* 分配簇并写入数据 */
    for (uint32_t c = 0; c < clusters_needed; c++) {
        uint32_t new_cluster = fat32_alloc_cluster(0);
        if (new_cluster == 0) {
            screen_puts("[fat32] No free clusters\n");
            return false;
        }

        if (first_cluster == 0)
            first_cluster = new_cluster;

        /* 链接到前一个簇 */
        if (prev_cluster != 0) {
            fat32_update_fat(prev_cluster, new_cluster);
        }

        /* 写入簇数据 */
        uint32_t lba = fat32_get_cluster_lba(new_cluster);
        uint8_t write_buf[512];

        for (uint8_t s = 0; s < bpb_sec_per_clust; s++) {
            /* 清零缓冲区 */
            for (int i = 0; i < 512; i++)
                write_buf[i] = 0;

            /* 复制数据 */
            uint32_t remaining = size - data_offset;
            if (remaining > 0) {
                uint32_t copy = (remaining > 512) ? 512 : remaining;
                for (uint32_t i = 0; i < copy; i++)
                    write_buf[i] = data[data_offset + i];
                data_offset += copy;
            }

            if (!disk_write_sectors(lba + s, 1, write_buf)) {
                return false;
            }
        }

        prev_cluster = new_cluster;
    }

    /* 标记最后一个簇为结束 */
    if (prev_cluster != 0) {
        fat32_update_fat(prev_cluster, FAT32_CLUSTER_END_MIN);
    }

    /* 查找根目录中的空闲目录项并写入 */
    cluster = bpb_root_cluster;
    while (cluster >= 2 && cluster < FAT32_MAX_CLUSTER) {
        uint32_t lba = fat32_get_cluster_lba(cluster);

        for (uint8_t s = 0; s < bpb_sec_per_clust; s++) {
            if (!read_sector(lba + s))
                return false;

            struct fat32_dir_entry *entries =
                (struct fat32_dir_entry *)sector_buf;

            for (int i = 0; i < 16; i++) {
                uint8_t first = entries[i].name[0];

                /* 空闲或已删除的条目 */
                if (first == 0x00 || first == 0xE5) {
                    /* 写入新目录项 */
                    for (int j = 0; j < 11; j++)
                        entries[i].name[j] = name83[j];
                    entries[i].attr = FAT32_ATTR_ARCHIVE;
                    entries[i].reserved = 0;
                    entries[i].create_time_ms = 0;
                    entries[i].create_time = 0;
                    entries[i].create_date = 0x5621;  /* 2026-05-31 */
                    entries[i].access_date = 0x5621;
                    entries[i].cluster_hi = (uint16_t)(first_cluster >> 16);
                    entries[i].modify_time = 0;
                    entries[i].modify_date = 0x5621;
                    entries[i].cluster_lo = (uint16_t)(first_cluster & 0xFFFF);
                    entries[i].file_size = size;

                    /* 写回扇区 */
                    if (!disk_write_sectors(lba + s, 1, sector_buf)) {
                        return false;
                    }
                    return true;
                }
            }
        }

        cluster = fat32_get_next_cluster(cluster);
    }

    screen_puts("[fat32] Root directory full\n");
    return false;
}

/*
 * fat32_free_cluster_chain — 释放 FAT32 簇链
 * @cluster: 起始簇号
 *
 * 遍历簇链并将所有簇标记为空闲 (0)。
 */
static void fat32_free_cluster_chain(uint32_t cluster)
{
    while (cluster >= 2 && cluster < FAT32_MAX_CLUSTER) {
        uint32_t next = fat32_get_next_cluster(cluster);
        fat32_update_fat(cluster, 0);
        cluster = next;
    }
}

/*
 * fat32_delete_file — 删除 FAT32 文件
 * @name: 文件名
 *
 * 遍历根目录簇链查找文件, 释放簇链, 标记目录项已删除。
 * 返回: true = 成功。
 */
bool fat32_delete_file(const char *name)
{
    char name83[11];
    uint32_t cluster = bpb_root_cluster;

    convert_to_83(name, name83);

    while (cluster >= 2 && cluster < FAT32_MAX_CLUSTER) {
        uint32_t lba = fat32_get_cluster_lba(cluster);

        for (uint8_t s = 0; s < bpb_sec_per_clust; s++) {
            if (!read_sector(lba + s))
                return false;

            struct fat32_dir_entry *entries =
                (struct fat32_dir_entry *)sector_buf;

            for (int i = 0; i < 16; i++) {
                uint8_t first = entries[i].name[0];

                if (first == 0x00)
                    return false;  /* 到达目录末尾, 文件不存在 */
                if (first == 0xE5)
                    continue;
                if (entries[i].attr == FAT32_ATTR_LFN)
                    continue;
                if (entries[i].attr & FAT32_ATTR_VOLUME_ID)
                    continue;

                if (fat32_name_cmp(entries[i].name,
                                   (uint8_t *)name83) == 0) {
                    /* 找到文件, 释放簇链 */
                    uint32_t file_cluster =
                        ((uint32_t)entries[i].cluster_hi << 16)
                        | entries[i].cluster_lo;
                    if (file_cluster >= 2
                        && file_cluster < FAT32_CLUSTER_BAD) {
                        fat32_free_cluster_chain(file_cluster);
                    }

                    /* 标记目录项为已删除 */
                    entries[i].name[0] = 0xE5;

                    /* 写回扇区 */
                    if (!disk_write_sectors(lba + s, 1, sector_buf))
                        return false;

                    screen_puts("[fat32] File deleted: ");
                    screen_puts(name);
                    screen_putchar('\n');
                    return true;
                }
            }
        }

        cluster = fat32_get_next_cluster(cluster);
    }

    return false;
}

/*
 * fat32_rename_file — 重命名 FAT32 文件
 * @old_name: 原文件名
 * @new_name: 新文件名
 *
 * 遍历根目录簇链, 更新目录项中的文件名。
 * 返回: true = 成功。
 */
bool fat32_rename_file(const char *old_name, const char *new_name)
{
    char old_name83[11];
    char new_name83[11];
    uint32_t cluster = bpb_root_cluster;

    convert_to_83(old_name, old_name83);
    convert_to_83(new_name, new_name83);

    while (cluster >= 2 && cluster < FAT32_MAX_CLUSTER) {
        uint32_t lba = fat32_get_cluster_lba(cluster);

        for (uint8_t s = 0; s < bpb_sec_per_clust; s++) {
            if (!read_sector(lba + s))
                return false;

            struct fat32_dir_entry *entries =
                (struct fat32_dir_entry *)sector_buf;

            for (int i = 0; i < 16; i++) {
                uint8_t first = entries[i].name[0];

                if (first == 0x00)
                    return false;
                if (first == 0xE5)
                    continue;
                if (entries[i].attr == FAT32_ATTR_LFN)
                    continue;
                if (entries[i].attr & FAT32_ATTR_VOLUME_ID)
                    continue;

                if (fat32_name_cmp(entries[i].name,
                                   (uint8_t *)old_name83) == 0) {
                    /* 更新文件名 */
                    for (int j = 0; j < 11; j++)
                        entries[i].name[j] = new_name83[j];

                    /* 写回扇区 */
                }
            }
        }

        cluster = fat32_get_next_cluster(cluster);
    }

    return false;
}

/*
 * fat32_format — 格式化 FAT32 分区
 * @partition_lba: 分区起始 LBA 扇区号
 *
 * 读取当前 BPB 参数, 重新初始化 FAT 表、
 * FSINFO 和根目录簇, 清空所有文件数据。
 * 返回: true = 成功, false = 失败。
 */
bool fat32_format(uint32_t partition_lba)
{
    /* 读取当前引导扇区以获得 BPB 参数 */
    if (!disk_read_sectors(partition_lba, 1, sector_buf)) {
        screen_puts("[fat32] Cannot read boot sector\n");
        return false;
    }

    /* 验证引导签名 */
    if (*(uint16_t *)(sector_buf + 510) != 0xAA55) {
        screen_puts("[fat32] Invalid boot signature\n");
        return false;
    }

    /* 提取 BPB 参数 */
    uint16_t bytes_per_sec = *(uint16_t *)(sector_buf + 11);
    uint8_t  sec_per_clust = *(uint8_t  *)(sector_buf + 13);
    uint16_t reserved_secs = *(uint16_t *)(sector_buf + 14);
    uint8_t  num_fats      = *(uint8_t  *)(sector_buf + 16);
    uint32_t total_secs    = *(uint32_t *)(sector_buf + 32);
    uint32_t fat_size      = *(uint32_t *)(sector_buf + 36);
    uint32_t root_cluster  = *(uint32_t *)(sector_buf + 44);
    uint16_t fs_info_sec   = *(uint16_t *)(sector_buf + 48);
    (void)total_secs;

    if (bytes_per_sec != 512) {
        screen_puts("[fat32] Unsupported bytes per sector\n");
        return false;
    }

    uint32_t fat1_lba = partition_lba + reserved_secs;
    uint32_t fat2_lba = fat1_lba + fat_size;
    uint32_t data_lba = partition_lba + reserved_secs
                        + ((uint32_t)num_fats * fat_size);

    screen_puts("[fat32] Formatting...\n");

    /* === Step 1: Write FSINFO === */
    if (fs_info_sec > 0 && fs_info_sec < reserved_secs) {
        memset(sector_buf, 0, 512);
        *(uint32_t *)(sector_buf + 0)   = 0x41615252;  /* "RRaA" */
        *(uint32_t *)(sector_buf + 484) = 0x61417272;  /* "rrAa" */
        *(uint32_t *)(sector_buf + 488) = 0xFFFFFFFF;  /* 空闲簇数未知 */
        *(uint32_t *)(sector_buf + 492) = 0xFFFFFFFF;  /* 下一个空闲簇未知 */
        *(uint16_t *)(sector_buf + 510) = 0xAA55;

        if (!disk_write_sectors(partition_lba + fs_info_sec, 1, sector_buf)) {
            screen_puts("[fat32] Failed to write FSINFO\n");
            return false;
        }
    }

    /* === Step 2: Write FAT1 === */
    memset(sector_buf, 0, 512);
    *(uint32_t *)(sector_buf + 0) = 0x0FFFFFF8;  /* 条目 0: 介质描述 */
    *(uint32_t *)(sector_buf + 4) = 0x0FFFFFFF;  /* 条目 1: 保留 */
    *(uint32_t *)(sector_buf + 8) = 0x0FFFFFFF;  /* 条目 2: 根目录 EOC */

    if (!disk_write_sectors(fat1_lba, 1, sector_buf)) {
        screen_puts("[fat32] Failed to write FAT1 (first sector)\n");
        return false;
    }

    memset(sector_buf, 0, 512);
    for (uint32_t i = 1; i < fat_size; i++) {
        if (!disk_write_sectors(fat1_lba + i, 1, sector_buf)) {
            screen_puts("[fat32] Failed to write FAT1\n");
            return false;
        }
    }

    /* === Step 3: Write FAT2 (镜像) === */
    memset(sector_buf, 0, 512);
    *(uint32_t *)(sector_buf + 0) = 0x0FFFFFF8;
    *(uint32_t *)(sector_buf + 4) = 0x0FFFFFFF;
    *(uint32_t *)(sector_buf + 8) = 0x0FFFFFFF;

    if (!disk_write_sectors(fat2_lba, 1, sector_buf)) {
        screen_puts("[fat32] Failed to write FAT2 (first sector)\n");
        return false;
    }

    memset(sector_buf, 0, 512);
    for (uint32_t i = 1; i < fat_size; i++) {
        if (!disk_write_sectors(fat2_lba + i, 1, sector_buf)) {
            screen_puts("[fat32] Failed to write FAT2\n");
            return false;
        }
    }

    /* === Step 4: 清空根目录簇 === */
    uint32_t root_lba = data_lba + (root_cluster - 2) * sec_per_clust;
    memset(sector_buf, 0, 512);
    for (uint8_t i = 0; i < sec_per_clust; i++) {
        if (!disk_write_sectors(root_lba + i, 1, sector_buf)) {
            screen_puts("[fat32] Failed to clear root directory\n");
            return false;
        }
    }

    screen_puts("[fat32] Format complete.\n");
    return true;
}

/*
 * fat32_create_dir — 在根目录下创建子目录
 * @name: 目录名 (普通格式, 如 "SUBDIR")
 *
 * 流程:
 * 1. 检查是否已存在同名文件/目录
 * 2. 分配一个新簇用于存储目录内容
 * 3. 在新簇中初始化 "." (自身) 和 ".." (父目录) 条目
 * 4. 在根目录中创建对应的目录项 (ATTR_DIRECTORY)
 * 返回: true = 成功, false = 失败。
 */
bool fat32_create_dir(const char *name)
{
    char name83[11];
    uint32_t new_cluster;

    convert_to_83(name, name83);

    /* 检查是否已存在同名文件或目录 */
    uint32_t cluster = bpb_root_cluster;
    while (cluster >= 2 && cluster < FAT32_MAX_CLUSTER) {
        uint32_t lba = fat32_get_cluster_lba(cluster);
        for (uint8_t s = 0; s < bpb_sec_per_clust; s++) {
            if (!read_sector(lba + s)) return false;
            struct fat32_dir_entry *entries =
                (struct fat32_dir_entry *)sector_buf;
            for (int i = 0; i < 16; i++) {
                uint8_t first = entries[i].name[0];
                if (first == 0x00) goto check_done;
                if (first == 0xE5) continue;
                if (entries[i].attr == FAT32_ATTR_LFN) continue;
                if (entries[i].attr & FAT32_ATTR_VOLUME_ID) continue;
                if (fat32_name_cmp(entries[i].name,
                                   (uint8_t *)name83) == 0) {
                    screen_puts("[fat32] Already exists\n");
                    return false;
                }
            }
        }
        cluster = fat32_get_next_cluster(cluster);
    }
check_done:

    /* 分配新簇 */
    new_cluster = fat32_alloc_cluster(0);
    if (new_cluster == 0) {
        screen_puts("[fat32] No free clusters\n");
        return false;
    }

    /* 初始化新目录: 写入 "." 和 ".." 条目 */
    uint32_t dir_lba = fat32_get_cluster_lba(new_cluster);
    memset(sector_buf, 0, 512);

    /* 第 0 项: "." = 指向自身 */
    struct fat32_dir_entry *de = (struct fat32_dir_entry *)sector_buf;
    memset(de->name, ' ', 11);
    de->name[0] = '.';
    de->attr = FAT32_ATTR_DIRECTORY;
    de->cluster_hi = (uint16_t)(new_cluster >> 16);
    de->cluster_lo = (uint16_t)(new_cluster & 0xFFFF);

    /* 第 1 项: ".." = 指向父目录 (根目录簇) */
    de = (struct fat32_dir_entry *)(sector_buf + 32);
    memset(de->name, ' ', 11);
    de->name[0] = '.';
    de->name[1] = '.';
    de->attr = FAT32_ATTR_DIRECTORY;
    de->cluster_hi = (uint16_t)(bpb_root_cluster >> 16);
    de->cluster_lo = (uint16_t)(bpb_root_cluster & 0xFFFF);

    /* 写入第一个扇区 */
    if (!disk_write_sectors(dir_lba, 1, sector_buf)) {
        fat32_free_cluster_chain(new_cluster);
        return false;
    }

    /* 清零剩余扇区 */
    memset(sector_buf, 0, 512);
    for (uint8_t i = 1; i < bpb_sec_per_clust; i++) {
        if (!disk_write_sectors(dir_lba + i, 1, sector_buf)) {
            fat32_free_cluster_chain(new_cluster);
            return false;
        }
    }

    /* 在根目录中创建目录项 */
    cluster = bpb_root_cluster;
    while (cluster >= 2 && cluster < FAT32_MAX_CLUSTER) {
        uint32_t lba = fat32_get_cluster_lba(cluster);
        for (uint8_t s = 0; s < bpb_sec_per_clust; s++) {
            if (!read_sector(lba + s)) {
                fat32_free_cluster_chain(new_cluster);
                return false;
            }
            struct fat32_dir_entry *entries =
                (struct fat32_dir_entry *)sector_buf;
            for (int i = 0; i < 16; i++) {
                uint8_t first = entries[i].name[0];
                if (first == 0x00 || first == 0xE5) {
                    /* 写入新目录项 */
                    for (int j = 0; j < 11; j++)
                        entries[i].name[j] = name83[j];
                    entries[i].attr = FAT32_ATTR_DIRECTORY;
                    entries[i].reserved = 0;
                    entries[i].create_time_ms = 0;
                    entries[i].create_time = 0;
                    entries[i].create_date = 0x5621;
                    entries[i].access_date = 0x5621;
                    entries[i].cluster_hi =
                        (uint16_t)(new_cluster >> 16);
                    entries[i].modify_time = 0;
                    entries[i].modify_date = 0x5621;
                    entries[i].cluster_lo =
                        (uint16_t)(new_cluster & 0xFFFF);
                    entries[i].file_size = 0;

                    if (!disk_write_sectors(lba + s, 1, sector_buf)) {
                        fat32_free_cluster_chain(new_cluster);
                        return false;
                    }
                    return true;
                }
            }
        }
        cluster = fat32_get_next_cluster(cluster);
    }

    /* 根目录已满 */
    fat32_free_cluster_chain(new_cluster);
    screen_puts("[fat32] Root directory full\n");
    return false;
}
