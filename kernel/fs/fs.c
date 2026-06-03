/*
 * Nexsteaduser — PlexsDOS
 * fs.c — 统一文件系统抽象层实现
 * 作者: Tinmc189623 | 团队: Nexlyh
 *
 * 根据当前驱动器自动路由到 FAT12、FAT32 或 ISO 9660 文件系统。
 * 支持 A:-Z: 所有 26 个驱动器字母:
 *   A:/B: — FAT12 软盘 (FDC)
 *   C:/E:-Z: — FAT32 硬盘分区 (ATA)
 *   D: — CD-ROM (ISO 9660)
 */

#include <plexsdos/types.h>
#include <plexsdos/config.h>
#include <plexsdos/fs.h>
#include <plexsdos/fat12.h>
#include <plexsdos/fat32.h>
#include <plexsdos/disk.h>
#include <plexsdos/fdc.h>
#include <plexsdos/drive.h>
#include <plexsdos/screen.h>
#include <plexsdos/cdrom.h>

/* 当前驱动器 */
static char current_drive = 'A';

/* 当前驱动器的文件系统类型 */
static int current_fs_type = FS_NONE;

/* 标记 FAT12 是否已初始化 */
static bool fat12_initialized = false;

/* FAT12 软盘读取函数 (通过 FDC, 支持 drive 0/1) */
static bool floppy_read_sectors(uint8_t fdc_drive, uint32_t lba, uint8_t count, void *buf)
{
    return fdc_read_lba(fdc_drive, lba, count, buf);
}

/* FAT12 软盘写入函数 (通过 FDC, 支持 drive 0/1) */
static bool floppy_write_sectors(uint8_t fdc_drive, uint32_t lba, uint8_t count, const void *buf)
{
    uint8_t heads, spt;
    uint8_t head, cylinder, sector;
    uint32_t temp;
    const uint8_t *data = (const uint8_t *)buf;

    if (!fdc_get_geometry(fdc_drive, &heads, &spt))
        return false;

    for (uint8_t i = 0; i < count; i++) {
        uint32_t cur_lba = lba + i;
        temp = cur_lba / spt;
        head = (uint8_t)(temp % heads);
        cylinder = (uint8_t)(temp / heads);
        sector = (uint8_t)(cur_lba % spt + 1);

        if (!fdc_write_sectors(fdc_drive, head, cylinder, sector, 1, data + i * 512))
            return false;
    }
    return true;
}

/* 封装: floppy read (drive 0) 给 fat12_init_ex */
static bool floppy_read_a(uint32_t lba, uint8_t count, void *buf)
{
    return floppy_read_sectors(0, lba, count, buf);
}

/* 封装: floppy write (drive 0) 给 fat12_set_write_fn */
static bool floppy_write_a(uint32_t lba, uint8_t count, const void *buf)
{
    return floppy_write_sectors(0, lba, count, buf);
}

/* 封装: floppy read (drive 1) 给 fat12_init_ex */
static bool floppy_read_b(uint32_t lba, uint8_t count, void *buf)
{
    return floppy_read_sectors(1, lba, count, buf);
}

/* 封装: floppy write (drive 1) 给 fat12_set_write_fn */
static bool floppy_write_b(uint32_t lba, uint8_t count, const void *buf)
{
    return floppy_write_sectors(1, lba, count, buf);
}

/*
 * fs_init_floppy — 初始化软盘驱动器 (FAT12)
 * @fdc_drive: FDC 驱动器号 (0=A, 1=B)
 * 返回: true = 成功。
 */
static bool fs_init_floppy(uint8_t fdc_drive)
{
    fat12_read_fn rfn = (fdc_drive == 0) ? floppy_read_a : floppy_read_b;
    fat12_write_fn wfn = (fdc_drive == 0) ? floppy_write_a : floppy_write_b;

    if (fat12_init_ex(rfn, 0x7C00, 0)) {
        fat12_set_write_fn(wfn);
        fat12_initialized = true;
        current_fs_type = FS_FAT12;
        return true;
    }
    current_fs_type = FS_NONE;
    return false;
}

/*
 * fs_init_hdd — 初始化硬盘分区 (FAT32)
 * @partition_lba: 分区起始 LBA
 * 返回: true = 成功。
 */
static bool fs_init_hdd(uint32_t partition_lba)
{
    if (fat32_init_drive(partition_lba)) {
        current_fs_type = FS_FAT32;
        return true;
    }
    return false;
}

/*
 * fs_ensure_init — 确保当前驱动器已初始化
 * 通过 drive_get_info 查询驱动器类型, 初始化为对应的文件系统。
 * 返回: true = 就绪。
 */
static bool fs_ensure_init(void)
{
    const struct drive_info *info;

    if (current_fs_type != FS_NONE)
        return true;

    /* 查询驱动器信息 */
    int idx = current_drive - 'A';
    info = drive_get_info(idx);
    if (!info || info->type == DRIVE_TYPE_NONE)
        return false;

    switch (info->type) {
    case DRIVE_TYPE_FLOPPY:
        return fs_init_floppy((uint8_t)info->device_id);

    case DRIVE_TYPE_HDD:
        return fs_init_hdd(info->partition_lba);

    case DRIVE_TYPE_CDROM:
        /* CD-ROM: 确保 ISO 9660 已挂载 */
        if (iso9660_get_fs() && iso9660_get_fs()->mounted) {
            current_fs_type = FS_NONE; /* CD-ROM 用特殊接口 */
            return true;
        }
        return false;

    default:
        return false;
    }
}

/* ===== 公共接口 ===== */

bool fs_init(int drive_type, uint32_t param)
{
    (void)drive_type;
    (void)param;
    /* 延迟初始化: shell 在访问文件系统时自动初始化 */
    current_fs_type = FS_NONE;
    current_drive = 'A';
    return true;
}

void fs_list_root(void)
{
    if (!fs_ensure_init()) {
        screen_puts("No filesystem available.\n");
        return;
    }

    switch (current_fs_type) {
    case FS_FAT12:
        fat12_list_root();
        break;
    case FS_FAT32:
        fat32_list_root();
        break;
    default:
        screen_puts("No filesystem available.\n");
        break;
    }
}

void fs_list_dir(const char *path)
{
    (void)path;
    /* 当前仅支持根目录 */
    fs_list_root();
}

struct fs_entry *fs_find_file(const char *name)
{
    static struct fs_entry entry;
    struct fat12_dir_entry *f12e;
    struct fat32_dir_entry *f32e;
    int i, j;

    if (!fs_ensure_init())
        return NULL;

    switch (current_fs_type) {
    case FS_FAT12:
        f12e = fat12_find_file(name);
        if (!f12e) return NULL;
        /* 转换 FAT12 目录项到通用结构 */
        for (i = 0, j = 0; i < 8; i++) {
            if (f12e->name[i] != ' ')
                entry.name[j++] = f12e->name[i];
        }
        if (f12e->name[8] != ' ') {
            entry.name[j++] = '.';
            for (i = 8; i < 11; i++) {
                if (f12e->name[i] != ' ')
                    entry.name[j++] = f12e->name[i];
            }
        }
        entry.name[j] = '\0';
        entry.attr = f12e->attr;
        entry.file_size = f12e->file_size;
        entry.is_directory = (f12e->attr & FAT12_ATTR_DIRECTORY) != 0;
        return &entry;

    case FS_FAT32:
        f32e = fat32_find_file(name);
        if (!f32e) return NULL;
        for (i = 0, j = 0; i < 8; i++) {
            if (f32e->name[i] != ' ')
                entry.name[j++] = f32e->name[i];
        }
        if (f32e->name[8] != ' ') {
            entry.name[j++] = '.';
            for (i = 8; i < 11; i++) {
                if (f32e->name[i] != ' ')
                    entry.name[j++] = f32e->name[i];
            }
        }
        entry.name[j] = '\0';
        entry.attr = f32e->attr;
        entry.file_size = f32e->file_size;
        entry.is_directory = (f32e->attr & FAT32_ATTR_DIRECTORY) != 0;
        return &entry;

    default:
        return NULL;
    }
}

uint32_t fs_load_file(struct fs_entry *entry, uint32_t load_addr)
{
    if (!fs_ensure_init())
        return 0;

    switch (current_fs_type) {
    case FS_FAT12: {
        struct fat12_dir_entry *f12ep = fat12_find_file(entry->name);
        if (!f12ep) return 0;
        return fat12_load_file(f12ep, load_addr);
    }

    case FS_FAT32: {
        struct fat32_dir_entry *f32ep = fat32_find_file(entry->name);
        if (!f32ep) return 0;
        return fat32_load_file(f32ep, load_addr);
    }

    default:
        return 0;
    }
}

bool fs_write_file(const char *name, const uint8_t *data, uint32_t size)
{
    if (!fs_ensure_init())
        return false;

    switch (current_fs_type) {
    case FS_FAT12:
        return fat12_write_file(name, data, size);
    case FS_FAT32:
        return fat32_write_file(name, data, size);
    default:
        return false;
    }
}

bool fs_delete_file(const char *name)
{
    if (!fs_ensure_init())
        return false;

    switch (current_fs_type) {
    case FS_FAT12:
        return fat12_delete_file(name);
    case FS_FAT32:
        return fat32_delete_file(name);
    default:
        return false;
    }
}

bool fs_rename_file(const char *old_name, const char *new_name)
{
    if (!fs_ensure_init())
        return false;

    switch (current_fs_type) {
    case FS_FAT12:
        return fat12_rename_file(old_name, new_name);
    case FS_FAT32:
        return fat32_rename_file(old_name, new_name);
    default:
        return false;
    }
}

bool fs_get_free_space(uint32_t *total_bytes, uint32_t *free_bytes)
{
    /* 简化实现: FAT12 1.44MB 软盘 */
    if (!fs_ensure_init())
        return false;

    if (current_fs_type == FS_FAT12) {
        *total_bytes = 1440 * 1024;
        *free_bytes = 1440 * 1024;  /* 粗略估计 */
        return true;
    }

    *total_bytes = 0;
    *free_bytes = 0;
    return false;
}

bool fs_get_volume_label(char *label)
{
    uint8_t *bpb;

    if (!fs_ensure_init())
        return false;

    bpb = (uint8_t *)0x7C00;

    /* 根据文件系统类型选择正确的 BPB 偏移 */
    uint32_t vol_off = (current_fs_type == FS_FAT32) ? 0x47 : 0x2B;

    for (int i = 0; i < 11; i++) {
        char c = (char)bpb[vol_off + i];
        label[i] = (c >= 32 && c < 127) ? c : ' ';
    }
    label[11] = '\0';
    return true;
}

bool fs_set_volume_label(const char *label)
{
    uint8_t *bpb;

    if (!fs_ensure_init())
        return false;

    /* 仅 FAT32 硬盘支持写回卷标 */
    if (current_fs_type != FS_FAT32) {
        screen_puts("Volume label change not supported on this drive.\n");
        return false;
    }

    bpb = (uint8_t *)0x7C00;

    for (int i = 0; i < 11; i++) {
        char c = label[i];
        bpb[0x47 + i] = (c != '\0') ? (uint8_t)c : ' ';
    }

    /* 写回引导扇区 (LBA 0) */
    if (disk_write_sectors(0, 1, bpb))
        return true;

    screen_puts("Failed to write volume label.\n");
    return false;
}

const char *fs_get_type(void)
{
    switch (current_fs_type) {
    case FS_FAT12: return "FAT12";
    case FS_FAT32: return "FAT32";
    default:       return "NONE";
    }
}

char fs_get_current_drive(void)
{
    return current_drive;
}

bool fs_set_current_drive(char letter)
{
    if (letter >= 'a' && letter <= 'z')
        letter -= 32;

    if (letter < 'A' || letter > 'Z') {
        screen_puts("Invalid drive letter.\n");
        return false;
    }

    int idx = letter - 'A';

    /* 查询驱动器信息 */
    const struct drive_info *info = drive_get_info(idx);
    if (!info || info->type == DRIVE_TYPE_NONE) {
        screen_putchar(letter);
        screen_puts(": Drive not available.\n");
        return false;
    }

    /* 如果已经是当前驱动且已初始化 */
    if (letter == current_drive && current_fs_type != FS_NONE)
        return true;

    /* 切换驱动器 */
    current_drive = letter;
    current_fs_type = FS_NONE;

    switch (info->type) {
    case DRIVE_TYPE_FLOPPY:
        return fs_init_floppy((uint8_t)info->device_id);

    case DRIVE_TYPE_HDD:
        return fs_init_hdd(info->partition_lba);

    case DRIVE_TYPE_CDROM:
        /* CD-ROM 使用 ISO 9660 接口, 无需 FAT 初始化 */
        return true;

    default:
        screen_putchar(letter);
        screen_puts(": Unknown drive type.\n");
        return false;
    }
}
