/*
 * Nexsteaduser — PlexsDOS
 * FAT12 文件系统驱动 (32-bit 保护模式)
 * 作者: Tinmc189623 | 团队: Nexlyh
 *
 * 支持通过 ATA 或 FDC 读取 FAT12 格式分区。
 * 支持: 文件查找、根目录列表、文件加载到内存。
 * 注意: 当前仅支持根目录, 不支持子目录。
 */

#include <plexsdos/types.h>
#include <plexsdos/config.h>
#include <plexsdos/fat12.h>
#include <plexsdos/disk.h>
#include <plexsdos/screen.h>

/* 当前使用的扇区读函数 (默认为 ATA) */
static fat12_read_fn fat12_read = NULL;

/* 当前使用的扇区写函数 (默认 NULL = 只读) */
static fat12_write_fn fat12_write = NULL;

/*
 * fat12_set_write_fn — 设置扇区写函数
 * @fn: 写函数指针, NULL = 只读模式
 */
void fat12_set_write_fn(fat12_write_fn fn)
{
    fat12_write = fn;
}

/* BPB 数据来源地址 (默认为引导扇区 0x7C00) */
static uint32_t fat12_bpb_addr = 0x7C00;

/* FAT12 BPB 参数 (从引导扇区 0x7C00 复制) */
static uint16_t bpb_bytes_per_sec;   /* 每扇区字节数 */
static uint8_t  bpb_sec_per_clust;   /* 每簇扇区数 */
static uint16_t bpb_reserved_secs;   /* 保留扇区数 */
static uint8_t  bpb_num_fats;        /* FAT 表数量 */
static uint16_t bpb_root_entries;    /* 根目录最大项数 */
static uint16_t bpb_total_secs;      /* 总扇区数 */
static uint16_t bpb_fat_size;        /* 每个 FAT 占用扇区数 */

/* 内存中的 FAT 表和根目录 (静态分配) */
static uint8_t fat_table[9 * 512];         /* FAT1: 最多 9 扇区 = 4608 字节 */
static uint8_t root_dir_buf[224 * 32];     /* 根目录: 224 项 × 32 字节 */

/* 计算得到的布局参数 */
static uint32_t fat_start_lba;       /* FAT 起始 LBA */
static uint32_t root_dir_lba;        /* 根目录起始 LBA */
static uint32_t data_area_lba;       /* 数据区起始 LBA */

/* ===== 辅助函数 ===== */

/*
 * 将文件名转为 FAT12 8.3 格式 (空格填充, 大写)。
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
 * 比较两个 FAT12 文件名 (11 字节, 大小写不敏感)。
 * 返回: 0 = 匹配, 非 0 = 不匹配。
 */
static int fat12_name_cmp(const uint8_t *a, const uint8_t *b)
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
 * 检查 BPB 引导签名是否有效 (0xAA55)。
 * 从 0x7C00 引导扇区读取。
 * 返回: true = 有效 FAT12 BPB, false = 无效。
 */
static bool check_bpb_valid(void)
{
    uint16_t *sig = (uint16_t *)0x7DFE;
    if (*sig != 0xAA55)
        return false;
    return true;
}

/* ===== FAT12 初始化 ===== */

/*
 * fat12_init_common — FAT12 初始化公共逻辑
 * 从指定地址读取 BPB, 加载 FAT 表和根目录。
 * 返回: true = 成功, false = 失败。
 */
static bool fat12_init_common(void)
{
    /* 从 BPB 来源地址复制参数 */
    uint8_t *bpb = (uint8_t *)fat12_bpb_addr;
    bpb_bytes_per_sec = *(uint16_t *)(bpb + 11);
    bpb_sec_per_clust = *(uint8_t  *)(bpb + 13);
    bpb_reserved_secs = *(uint16_t *)(bpb + 14);
    bpb_num_fats      = *(uint8_t  *)(bpb + 16);
    bpb_root_entries  = *(uint16_t *)(bpb + 17);
    bpb_total_secs    = *(uint16_t *)(bpb + 19);
    bpb_fat_size      = *(uint16_t *)(bpb + 22);

    /* 计算文件系统布局 */
    fat_start_lba  = bpb_reserved_secs;
    root_dir_lba   = fat_start_lba + (bpb_num_fats * bpb_fat_size);
    uint32_t root_dir_size = (bpb_root_entries * 32 + bpb_bytes_per_sec - 1) / bpb_bytes_per_sec;
    data_area_lba  = root_dir_lba + root_dir_size;

    /* 读取 FAT1 表 */
    uint32_t fat_bytes = bpb_fat_size * bpb_bytes_per_sec;
    if (fat_bytes > sizeof(fat_table))
        fat_bytes = sizeof(fat_table);
    if (!fat12_read(fat_start_lba, bpb_fat_size, fat_table)) {
        screen_puts("[fat12] Failed to read FAT\n");
        return false;
    }

    /* 读取根目录 */
    uint8_t root_sectors = (uint8_t)root_dir_size;
    if (!fat12_read(root_dir_lba, root_sectors, root_dir_buf)) {
        screen_puts("[fat12] Failed to read root dir\n");
        return false;
    }

    screen_puts("[fat12] Filesystem ready\n");
    return true;
}

/*
 * fat12_init — 使用 ATA 磁盘初始化 FAT12
 * 从引导扇区 0x7C00 读取 BPB, 加载 FAT 表和根目录。
 * 返回: true = 成功, false = 失败。
 */
bool fat12_init(void)
{
    if (!check_bpb_valid()) {
        screen_puts("[fat12] Invalid BPB signature\n");
        return false;
    }

    fat12_read = disk_read_sectors;
    fat12_bpb_addr = 0x7C00;
    return fat12_init_common();
}

/*
 * fat12_init_ex — 使用自定义读函数初始化 FAT12
 * @read_fn:   扇区读函数 (可使用 ATA 或 FDC)
 * @bpb_addr:  BPB 数据地址 (引导扇区在内存中的位置)
 * @bpb_lba:   引导扇区在磁盘上的 LBA (未使用, 保留)
 *
 * 用于从软盘 (FDC) 或其他设备加载 FAT12 文件系统。
 * 返回: true = 成功, false = 失败。
 */
bool fat12_init_ex(fat12_read_fn read_fn, uint32_t bpb_addr, uint32_t bpb_lba)
{
    (void)bpb_lba;

    if (read_fn == NULL)
        return false;

    fat12_read = read_fn;
    fat12_bpb_addr = bpb_addr;
    return fat12_init_common();
}

/* ===== FAT12 操作 ===== */

/*
 * 从 FAT 表中获取指定簇号的下一个簇号。
 * FAT12 每个簇号占 12 位, 两个簇号打包在 3 字节中。
 *
 * 参数: cluster - 当前簇号
 * 返回: 下一个簇号 (0x000-0xFFF)
 */
static uint16_t fat12_get_next_cluster(uint16_t cluster)
{
    uint32_t offset = cluster + (cluster / 2);
    uint16_t value;

    if (cluster & 1) {
        /* 奇数簇: 取高 12 位 */
        value = (fat_table[offset] >> 4) | ((uint16_t)fat_table[offset + 1] << 4);
    } else {
        /* 偶数簇: 取低 12 位 */
        value = fat_table[offset] | ((uint16_t)(fat_table[offset + 1] & 0x0F) << 8);
    }

    return value & 0xFFF;
}

/*
 * 列出根目录下所有有效文件。
 * 跳过已删除 (0xE5) 和卷标 (0x08) 条目。
 * 输出: 文件名(8.3) + 文件大小。
 */
void fat12_list_root(void)
{
    struct fat12_dir_entry *entries = (struct fat12_dir_entry *)root_dir_buf;
    int count = 0;

    screen_set_color(0x0B, 0x00);
    screen_puts("Name           Size\n");
    screen_puts("----------- --------\n");
    screen_reset_color();

    for (uint16_t i = 0; i < bpb_root_entries; i++) {
        uint8_t first = entries[i].name[0];

        /* 跳过空条目和已删除条目 */
        if (first == 0x00)
            break;
        if (first == 0xE5)
            continue;

        /* 跳过卷标和长文件名条目 */
        if (entries[i].attr & FAT12_ATTR_VOLUME_ID)
            continue;
        if (entries[i].attr == 0x0F)
            continue;

        /* 输出文件名 (8.3 格式, 去除填充空格) */
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

        /* 对齐并输出文件大小 */
        screen_puts("    ");
        screen_put_dec(entries[i].file_size);
        screen_puts(" bytes\n");

        count++;
    }

    if (count == 0) {
        screen_puts("(empty)\n");
    }
}

/*
 * 按文件名在根目录中查找文件。
 * name: 普通文件名 (如 "test.bin"), 自动转为 8.3 格式。
 * 返回: 目录项指针, 未找到返回 NULL。
 */
struct fat12_dir_entry *fat12_find_file(const char *name)
{
    char name83[11];
    struct fat12_dir_entry *entries = (struct fat12_dir_entry *)root_dir_buf;

    convert_to_83(name, name83);

    for (uint16_t i = 0; i < bpb_root_entries; i++) {
        uint8_t first = entries[i].name[0];

        if (first == 0x00)
            break;
        if (first == 0xE5)
            continue;
        if (entries[i].attr & FAT12_ATTR_VOLUME_ID)
            continue;
        if (entries[i].attr == 0x0F)
            continue;

        if (fat12_name_cmp(entries[i].name, (uint8_t *)name83) == 0) {
            return &entries[i];
        }
    }

    return NULL;
}

/*
 * 将文件内容加载到指定内存地址。
 * 沿 FAT 链逐簇读取, 直到遇到结束标记。
 *
 * 参数:
 *   entry     - 目录项指针
 *   load_addr - 目标内存地址
 *
 * 返回: 加载的字节数, 失败返回 0。
 */
uint32_t fat12_load_file(struct fat12_dir_entry *entry, uint32_t load_addr)
{
    uint16_t cluster = entry->cluster_lo;
    uint32_t file_size = entry->file_size;
    uint32_t bytes_loaded = 0;
    uint8_t *dest = (uint8_t *)load_addr;
    uint8_t sector_buf[512];

    if (cluster < 2 || cluster >= FAT12_CLUSTER_BAD) {
        screen_puts("[fat12] Invalid start cluster\n");
        return 0;
    }

    while (cluster >= 2 && cluster < FAT12_CLUSTER_BAD) {
        /* 计算簇对应的 LBA 地址 */
        uint32_t lba = data_area_lba + (cluster - 2) * bpb_sec_per_clust;

        /* 读取整个簇 (通常 1 扇区) */
        for (uint8_t s = 0; s < bpb_sec_per_clust; s++) {
            if (!fat12_read(lba + s, 1, sector_buf)) {
                screen_puts("[fat12] Read error\n");
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
        cluster = fat12_get_next_cluster(cluster);
    }

    return bytes_loaded;
}

/* ===== 写入 / 删除 / 重命名 ===== */

/*
 * fat12_check_writable — 检查文件系统是否可写
 * 返回: true = 可写 (写函数已设置)。
 */
static bool fat12_check_writable(void)
{
    if (fat12_write == NULL) {
        screen_puts("[fat12] Write function not set (read-only mode)\n");
        return false;
    }
    return true;
}

/*
 * fat12_flush_fat — 将内存中的 FAT 表写回磁盘
 * 写入所有 FAT 副本。
 * 返回: true = 成功。
 */
static bool fat12_flush_fat(void)
{
    uint32_t fat_bytes = bpb_fat_size * bpb_bytes_per_sec;

    for (uint8_t i = 0; i < bpb_num_fats; i++) {
        uint32_t fat_lba = fat_start_lba + i * bpb_fat_size;
        if (!fat12_write(fat_lba, bpb_fat_size, fat_table)) {
            screen_puts("[fat12] Failed to write FAT\n");
            return false;
        }
    }
    return true;
}

/*
 * fat12_flush_root_dir — 将内存中的根目录写回磁盘
 * 返回: true = 成功。
 */
static bool fat12_flush_root_dir(void)
{
    uint32_t root_dir_size = (bpb_root_entries * 32 + bpb_bytes_per_sec - 1) / bpb_bytes_per_sec;
    if (!fat12_write(root_dir_lba, (uint8_t)root_dir_size, root_dir_buf)) {
        screen_puts("[fat12] Failed to write root directory\n");
        return false;
    }
    return true;
}

/*
 * fat12_flush_sector — 将 512 字节缓冲区写回指定 LBA
 * 用于写入目录项所在的扇区。
 */
static bool fat12_flush_sector(uint32_t lba, const uint8_t *data)
{
    return fat12_write(lba, 1, data);
}

/*
 * 在 FAT12 表中设置指定簇号的下一个簇值。
 * 同步更新内存中的 FAT 表并写回磁盘。
 */
static void fat12_set_next_cluster(uint16_t cluster, uint16_t value)
{
    uint32_t offset = cluster + (cluster / 2);
    value &= 0xFFF;

    if (cluster & 1) {
        /* 奇数簇: 设置高 12 位 */
        fat_table[offset] = (fat_table[offset] & 0x0F) | (uint8_t)(value << 4);
        fat_table[offset + 1] = (uint8_t)(value >> 4);
    } else {
        /* 偶数簇: 设置低 12 位 */
        fat_table[offset] = (uint8_t)(value & 0xFF);
        fat_table[offset + 1] = (fat_table[offset + 1] & 0xF0) | (uint8_t)((value >> 8) & 0x0F);
    }
}

/*
 * 在 FAT 表中查找一个空闲簇 (值为 0x000)。
 * 返回: 簇号 (2 ~ 最大簇), 0 = 无空闲簇。
 */
static uint16_t fat12_alloc_cluster(void)
{
    uint16_t total_clusters;
    uint16_t data_sectors;

    /* 计算数据区扇区数 */
    data_sectors = bpb_total_secs - data_area_lba;
    total_clusters = (uint16_t)(data_sectors / bpb_sec_per_clust);

    /* 从簇 2 开始搜索 */
    for (uint16_t c = 2; c < total_clusters + 2 && c < FAT12_CLUSTER_BAD; c++) {
        if (fat12_get_next_cluster(c) == FAT12_CLUSTER_FREE) {
            /* 标记为结束簇 */
            fat12_set_next_cluster(c, FAT12_CLUSTER_END_MIN);
            return c;
        }
    }

    screen_puts("[fat12] No free clusters\n");
    return 0;
}

/*
 * 释放从指定簇开始的整个簇链。
 * 将所有簇标记为空闲 (0x000)。
 */
static void fat12_free_cluster_chain(uint16_t cluster)
{
    while (cluster >= 2 && cluster < FAT12_CLUSTER_BAD) {
        uint16_t next = fat12_get_next_cluster(cluster);
        fat12_set_next_cluster(cluster, FAT12_CLUSTER_FREE);
        cluster = next;
    }
}

/*
 * 计算文件需要的簇数
 */
static uint16_t fat12_calc_needed_clusters(uint32_t file_size)
{
    uint32_t bytes_per_cluster = bpb_sec_per_clust * bpb_bytes_per_sec;
    return (uint16_t)((file_size + bytes_per_cluster - 1) / bytes_per_cluster);
}

/*
 * fat12_write_file — 将数据写入 FAT12 文件系统 (新建/覆盖)
 * @name: 文件名 (如 "test.bin")
 * @data: 数据缓冲区
 * @size: 数据大小
 *
 * 流程:
 *   1. 查找目录中是否已存在同名文件, 存在则释放旧簇链
 *   2. 搜索空闲簇, 分配新簇链
 *   3. 写入数据扇区
 *   4. 写入目录项 (新建或更新)
 *   5. 刷新 FAT
 *
 * 返回: true = 成功。
 */
bool fat12_write_file(const char *name, const uint8_t *data, uint32_t size)
{
    char name83[11];
    struct fat12_dir_entry *entries = (struct fat12_dir_entry *)root_dir_buf;
    struct fat12_dir_entry *existing = NULL;
    uint16_t free_entry = 0xFFFF;
    uint16_t cluster_chain[512];
    uint16_t num_clusters;
    uint16_t cluster_count = 0;
    bool found_free = false;
    uint16_t i;

    if (!fat12_check_writable())
        return false;

    convert_to_83(name, name83);

    /* 计算所需簇数 */
    num_clusters = fat12_calc_needed_clusters(size);
    if (num_clusters == 0) {
        /* 空文件也需要至少 1 个簇 */
        num_clusters = 1;
    }

    /* 遍历根目录: 查找同名文件 和 空闲条目 */
    for (i = 0; i < bpb_root_entries; i++) {
        uint8_t first = entries[i].name[0];

        if (first == 0x00) {
            /* 空条目 = 目录结束, 也是空闲位置 */
            if (free_entry == 0xFFFF)
                free_entry = i;
            break;  /* 遇到 0x00 意味着后面没有更多条目 */
        }

        if (first == 0xE5) {
            /* 已删除条目, 可复用 */
            if (free_entry == 0xFFFF)
                free_entry = i;
            continue;
        }

        if (entries[i].attr & FAT12_ATTR_VOLUME_ID)
            continue;
        if (entries[i].attr == 0x0F)
            continue;

        /* 检查文件名是否匹配 */
        if (fat12_name_cmp(entries[i].name, (uint8_t *)name83) == 0) {
            existing = &entries[i];
            break;
        }
    }

    /* 如果遍历到目录末尾还没找到空闲项, 则取下一个 */
    if (free_entry == 0xFFFF && i < bpb_root_entries)
        free_entry = i;

    /* 如果没有空闲目录项且没找到同名文件, 则目录满 */
    if (existing == NULL && free_entry >= bpb_root_entries) {
        screen_puts("[fat12] Root directory is full\n");
        return false;
    }

    /* 如果找到同名文件, 释放旧簇链 */
    if (existing != NULL) {
        if (existing->cluster_lo >= 2 && existing->cluster_lo < FAT12_CLUSTER_BAD) {
            fat12_free_cluster_chain(existing->cluster_lo);
        }
        free_entry = (uint16_t)(existing - entries);
    }

    /* 分配新簇链 */
    if (size > 0) {
        uint16_t prev_cluster = 0;

        for (uint16_t c = 0; c < num_clusters; c++) {
            uint16_t cl = fat12_alloc_cluster();
            if (cl == 0) {
                /* 分配失败, 释放已分配的簇 */
                if (cluster_count > 0)
                    fat12_free_cluster_chain(cluster_chain[0]);
                screen_puts("[fat12] Out of disk space\n");
                return false;
            }
            cluster_chain[cluster_count++] = cl;

            if (prev_cluster != 0)
                fat12_set_next_cluster(prev_cluster, cl);
            prev_cluster = cl;
        }

        /* 写入数据 */
        uint32_t bytes_written = 0;
        for (uint16_t c = 0; c < cluster_count; c++) {
            uint32_t lba = data_area_lba + (cluster_chain[c] - 2) * bpb_sec_per_clust;
            uint8_t sector_buf[512];

            for (uint8_t s = 0; s < bpb_sec_per_clust; s++) {
                uint32_t copy_size = bpb_bytes_per_sec;
                if (bytes_written + copy_size > size)
                    copy_size = size - bytes_written;

                /* 填充扇区缓冲区 */
                for (uint32_t j = 0; j < copy_size; j++)
                    sector_buf[j] = data[bytes_written + j];
                /* 剩余部分填 0 */
                for (uint32_t j = copy_size; j < bpb_bytes_per_sec; j++)
                    sector_buf[j] = 0;

                if (!fat12_write(lba + s, 1, sector_buf)) {
                    screen_puts("[fat12] Write data error\n");
                    fat12_free_cluster_chain(cluster_chain[0]);
                    fat12_flush_fat();
                    return false;
                }
                bytes_written += copy_size;
                if (bytes_written >= size)
                    break;
            }
        }
    }

    /* 写入目录项 */
    {
        struct fat12_dir_entry *target;

        /* 如果是新建, 需要先清空目标目录项 */
        if (existing == NULL) {
            target = &entries[free_entry];
            for (int j = 0; j < 11; j++)
                target->name[j] = name83[j];
            target->attr = FAT12_ATTR_ARCHIVE;
            target->reserved = 0;
            target->create_time_ms = 0;
            target->create_time = 0;
            target->create_date = 0;
            target->access_date = 0;
            target->cluster_hi = 0;
            target->modify_time = 0;
            target->modify_date = 0;
        } else {
            target = existing;
        }

        target->cluster_lo = (cluster_count > 0) ? cluster_chain[0] : 0;
        target->file_size = size;

        /* 写回目录项所在的扇区 */
        uint32_t entry_offset = (uint32_t)((uint8_t *)target - root_dir_buf);
        uint32_t sector_index = entry_offset / bpb_bytes_per_sec;
        uint32_t sector_lba = root_dir_lba + sector_index;
        uint8_t sector_buf[512];

        if (!fat12_read(sector_lba, 1, sector_buf)) {
            screen_puts("[fat12] Failed to read dir sector for write\n");
            return false;
        }

        uint32_t offset_in_sector = entry_offset % bpb_bytes_per_sec;
        for (uint32_t j = 0; j < sizeof(struct fat12_dir_entry); j++)
            sector_buf[offset_in_sector + j] = ((uint8_t *)target)[j];

        if (!fat12_write(sector_lba, 1, sector_buf)) {
            screen_puts("[fat12] Failed to write dir entry\n");
            return false;
        }
    }

    /* 同步 FAT 到磁盘 */
    if (!fat12_flush_fat())
        return false;

    return true;
}

/*
 * fat12_delete_file — 删除文件
 * @name: 文件名
 *
 * 流程:
 *   1. 查找文件
 *   2. 释放簇链
 *   3. 标记目录项首字节为 0xE5
 *   4. 同步 FAT
 *
 * 返回: true = 成功。
 */
bool fat12_delete_file(const char *name)
{
    struct fat12_dir_entry *entry = fat12_find_file(name);
    uint32_t entry_offset;
    uint32_t sector_lba;
    uint32_t offset_in_sector;
    uint8_t sector_buf[512];

    if (!fat12_check_writable())
        return false;

    if (entry == NULL) {
        screen_puts("[fat12] File not found\n");
        return false;
    }

    /* 释放簇链 */
    if (entry->cluster_lo >= 2 && entry->cluster_lo < FAT12_CLUSTER_BAD)
        fat12_free_cluster_chain(entry->cluster_lo);

    /* 标记目录项首字节为 0xE5 (已删除) */
    entry->name[0] = 0xE5;

    /* 写回目录项所在的扇区 */
    entry_offset = (uint32_t)((uint8_t *)entry - root_dir_buf);
    sector_lba = root_dir_lba + entry_offset / bpb_bytes_per_sec;
    offset_in_sector = entry_offset % bpb_bytes_per_sec;

    if (!fat12_read(sector_lba, 1, sector_buf))
        return false;

    sector_buf[offset_in_sector] = 0xE5;

    if (!fat12_write(sector_lba, 1, sector_buf))
        return false;

    /* 同步 FAT */
    if (!fat12_flush_fat())
        return false;

    screen_puts("[fat12] File deleted: ");
    screen_puts(name);
    screen_putchar('\n');
    return true;
}

/*
 * fat12_rename_file — 重命名文件
 * @old_name: 原文件名
 * @new_name: 新文件名
 *
 * 返回: true = 成功。
 */
bool fat12_rename_file(const char *old_name, const char *new_name)
{
    struct fat12_dir_entry *entry = fat12_find_file(old_name);
    char new_name83[11];
    uint32_t entry_offset;
    uint32_t sector_lba;
    uint32_t offset_in_sector;
    uint8_t sector_buf[512];
    int j;

    if (!fat12_check_writable())
        return false;

    if (entry == NULL) {
        screen_puts("[fat12] File not found\n");
        return false;
    }

    convert_to_83(new_name, new_name83);

    /* 更新目录项中的文件名 */
    for (j = 0; j < 11; j++)
        entry->name[j] = new_name83[j];

    /* 写回目录项所在的扇区 */
    entry_offset = (uint32_t)((uint8_t *)entry - root_dir_buf);
    sector_lba = root_dir_lba + entry_offset / bpb_bytes_per_sec;
    offset_in_sector = entry_offset % bpb_bytes_per_sec;

    if (!fat12_read(sector_lba, 1, sector_buf))
        return false;

    for (j = 0; j < 11; j++)
        sector_buf[offset_in_sector + j] = new_name83[j];

    if (!fat12_write(sector_lba, 1, sector_buf))
        return false;

    return true;
}