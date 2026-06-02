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
