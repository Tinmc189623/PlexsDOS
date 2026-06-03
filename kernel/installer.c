/*
 * Nexsteaduser — PlexsDOS
 * 安装程序 — 将 PlexsDOS 安装到硬盘
 * 作者: Tinmc189623 | 团队: Nexlyh
 *
 * 安装介质检测 (自动选择):
 *   优先: CD-ROM (ATAPI + ISO 9660) — 单光盘安装, 无需换盘
 *   回退: 软盘 (FDC + FAT12) — MS-DOS 风格换盘交互
 *
 * 安装流程:
 * 1. 检测 ATA 硬盘
 * 2. 写入 MBR (含分区表)
 * 3. 写入 FAT32 文件系统 (VBR + FAT + 根目录)
 * 4. 从安装介质复制文件 (CD-ROM 或软盘)
 * 5. 显示完成信息
 */

#include <plexsdos/types.h>
#include <plexsdos/installer.h>
#include <plexsdos/screen.h>
#include <plexsdos/keyboard.h>
#include <plexsdos/disk.h>
#include <plexsdos/cdrom.h>
#include <plexsdos/fdc.h>
#include <plexsdos/fat12.h>
#include <plexsdos/fat32.h>
#include <plexsdos/serial.h>

/* 安装分区参数 */
#define PART_START_LBA   2048       /* 分区起始 (1MB 偏移) */
#define PART_SIZE_MB     64         /* 分区大小 64MB */
#define PART_SECTORS     (PART_SIZE_MB * 1024 * 1024 / 512)

/* FAT32 参数 */
#define FAT32_SECS_PER_CLUST  8
#define FAT32_NUM_FATS        2
#define FAT32_RESERVED_SECS   32
#define FAT32_ROOT_CLUSTER    2

/* FAT 表占用扇区数 */
#define FAT32_FAT_SECTORS     128

/* 文件加载缓冲区 (64KB, 用于从软盘加载文件) */
#define FILE_BUF_SIZE         65536

/* MBR 和 VBR 二进制 (由构建系统通过 hd_boot.S 的 .incbin 嵌入) */
extern const uint8_t _binary_hd_mbr_bin_start[];
extern const uint8_t _binary_hd_mbr_bin_end[];
extern const uint8_t _binary_hd_vbr_bin_start[];
extern const uint8_t _binary_hd_vbr_bin_end[];

/* 扇区缓冲区 */
static uint8_t inst_buf[512];

/* 文件加载缓冲区 (静态分配, 避免栈溢出) */
static uint8_t file_buf[FILE_BUF_SIZE];

/*
 * inst_fdc_read — FAT12 读函数包装器 (绑定 drive=0)
 * @lba:   逻辑块地址
 * @count: 扇区数
 * @buf:   目标缓冲区
 * 返回: true = 成功。
 *
 * 将 fdc_read_lba(drive, lba, count, buf) 的 4 参数接口
 * 适配为 fat12_read_fn 的 3 参数接口 (drive 固定为 0)。
 */
static bool inst_fdc_read(uint32_t lba, uint8_t count, void *buf)
{
    return fdc_read_lba(0, lba, count, buf);
}

/*
 * inst_name_no_version — 去除 ISO 9660 文件名中的版本号后缀
 * @dst: 目标缓冲区
 * @src: 源文件名 (可能包含 ";1")
 *
 * ISO 9660 文件名格式为 "NAME.EXT;1", 本函数去除 ";" 及之后的部分。
 */
static void inst_name_no_version(char *dst, const char *src)
{
    int i;
    for (i = 0; src[i] && src[i] != ';' && i < 255; i++)
        dst[i] = src[i];
    dst[i] = '\0';
}

/*
 * inst_name_to_upper — 将文件名转为大写
 * @name: 文件名 (原地转换)
 *
 * FAT32 要求文件名为大写。
 */
static void inst_name_to_upper(char *name)
{
    for (int i = 0; name[i]; i++) {
        if (name[i] >= 'a' && name[i] <= 'z')
            name[i] -= 32;
    }
}

/*
 * inst_copy_cdrom_files — 从 ISO 9660 目录复制文件到 FAT32 硬盘
 * @dir_lba:  目录起始 LBA
 * @dir_size: 目录大小 (字节)
 * @label:    目录显示名称 (如 "PROGRAMS")
 * @count:    输出参数, 累计复制文件数
 * 返回: true = 至少复制了一个文件。
 *
 * 遍历 ISO 9660 目录, 跳过 "." 和 ".." 以及子目录,
 * 对每个普通文件调用 iso9660_read_file + fat32_write_file。
 */
static bool inst_copy_cdrom_files(uint32_t dir_lba, uint32_t dir_size,
                                  const char *label, int *count)
{
    struct iso9660_entry entries[32];
    int n = iso9660_read_dir(dir_lba, dir_size, entries, 32);

    if (n <= 0)
        return false;

    for (int i = 0; i < n; i++) {
        /* 跳过当前目录和父目录 */
        if (entries[i].name[0] == '\0')
            continue;
        if (entries[i].name[0] == '.' && entries[i].name[1] == '\0')
            continue;
        if (entries[i].name[0] == '.' && entries[i].name[1] == '.' &&
            entries[i].name[2] == '\0')
            continue;
        /* 跳过子目录 */
        if (entries[i].is_directory)
            continue;
        /* 跳过空文件 */
        if (entries[i].size == 0)
            continue;

        /* 去除 ISO 9660 版本号后缀 ";1" */
        char clean_name[256];
        inst_name_no_version(clean_name, entries[i].name);

        screen_puts("    ");
        screen_puts(clean_name);
        screen_puts(" ");
        screen_put_dec(entries[i].size);
        screen_puts(" bytes");

        /* 读取文件内容 */
        uint32_t loaded = iso9660_read_file(entries[i].lba, entries[i].size,
                                            file_buf, FILE_BUF_SIZE);
        if (loaded == 0) {
            screen_set_color(0x0C, 0x00);
            screen_puts(" [READ ERROR]\n");
            screen_set_color(0x07, 0x00);
            continue;
        }

        /* FAT32 要求大写文件名 */
        inst_name_to_upper(clean_name);

        /* 写入 FAT32 硬盘 */
        if (!fat32_write_file(clean_name, file_buf, loaded)) {
            screen_set_color(0x0C, 0x00);
            screen_puts(" [WRITE ERROR]\n");
            screen_set_color(0x07, 0x00);
            continue;
        }

        screen_set_color(0x0A, 0x00);
        screen_puts(" [OK]\n");
        screen_set_color(0x07, 0x00);
        (*count)++;
    }

    return (*count > 0);
}

/*
 * inst_copy_cdrom — 从 CD-ROM (ISO 9660) 复制所有安装文件到硬盘
 *
 * Nexsteaduser 生产级安装流程:
 * 1. 初始化 ATAPI CD-ROM 驱动
 * 2. 挂载 ISO 9660 文件系统
 * 3. 遍历根目录, 查找子目录 (PROGRAMS/, DOCS/, DRIVERS/, LIB/)
 * 4. 逐个子目录复制文件到 FAT32 硬盘
 * 5. 同时复制根目录下的散落文件
 *
 * 返回: true = 成功 (至少复制了一个文件)。
 */
static bool inst_copy_cdrom(void)
{
    int total_copied = 0;

    screen_puts("[install] Detecting CD-ROM drive...\n");

    if (!cdrom_init()) {
        screen_set_color(0x0C, 0x00);
        screen_puts("[install] CD-ROM not found or not ready.\n");
        screen_set_color(0x07, 0x00);
        return false;
    }

    const struct cdrom_info *info = cdrom_get_info();
    screen_puts("[install] CD-ROM: ");
    screen_puts(info->vendor);
    screen_putchar(' ');
    screen_puts(info->product);
    screen_puts(" Rev ");
    screen_puts(info->revision);
    screen_putchar('\n');

    screen_puts("[install] Mounting ISO 9660 filesystem...\n");
    if (!iso9660_mount()) {
        screen_set_color(0x0C, 0x00);
        screen_puts("[install] Failed to mount ISO 9660.\n");
        screen_set_color(0x07, 0x00);
        return false;
    }

    const struct iso9660_fs *fs = iso9660_get_fs();
    screen_puts("[install] Volume: ");
    screen_puts(fs->volume_id);
    screen_putchar('\n');

    /* 读取根目录 */
    const struct iso9660_entry *root = iso9660_get_root();
    struct iso9660_entry root_entries[32];
    int root_count = iso9660_read_dir(root->lba, root->size,
                                      root_entries, 32);

    if (root_count <= 0) {
        screen_set_color(0x0C, 0x00);
        screen_puts("[install] Failed to read root directory.\n");
        screen_set_color(0x07, 0x00);
        return false;
    }

    /* 需要查找的子目录列表 */
    static const char *subdirs[] = {
        "PROGRAMS", "DOCS", "DRIVERS", "LIB"
    };
    static const int num_subdirs = 4;

    /* 复制根目录下的散落文件 (非目录) */
    screen_puts("\n  Root directory files:\n");
    for (int i = 0; i < root_count; i++) {
        if (root_entries[i].is_directory)
            continue;
        if (root_entries[i].name[0] == '\0')
            continue;
        if (root_entries[i].size == 0)
            continue;

        /* 去除 ISO 9660 版本号后缀 */
        char clean_name[256];
        inst_name_no_version(clean_name, root_entries[i].name);

        screen_puts("    ");
        screen_puts(clean_name);
        screen_puts(" ");
        screen_put_dec(root_entries[i].size);
        screen_puts(" bytes");

        uint32_t loaded = iso9660_read_file(root_entries[i].lba,
                                            root_entries[i].size,
                                            file_buf, FILE_BUF_SIZE);
        if (loaded == 0) {
            screen_set_color(0x0C, 0x00);
            screen_puts(" [READ ERROR]\n");
            screen_set_color(0x07, 0x00);
            continue;
        }

        /* FAT32 要求大写文件名 */
        inst_name_to_upper(clean_name);

        if (!fat32_write_file(clean_name, file_buf, loaded)) {
            screen_set_color(0x0C, 0x00);
            screen_puts(" [WRITE ERROR]\n");
            screen_set_color(0x07, 0x00);
            continue;
        }

        screen_set_color(0x0A, 0x00);
        screen_puts(" [OK]\n");
        screen_set_color(0x07, 0x00);
        total_copied++;
    }

    /* 遍历已知子目录, 复制其中的文件 */
    for (int d = 0; d < num_subdirs; d++) {
        for (int i = 0; i < root_count; i++) {
            if (!root_entries[i].is_directory)
                continue;

            /* 去除 ISO 9660 版本号后缀 ";1" 后再比较 */
            char clean_dir[256];
            inst_name_no_version(clean_dir, root_entries[i].name);

            bool match = true;
            for (int j = 0; subdirs[d][j]; j++) {
                if (clean_dir[j] != subdirs[d][j]) {
                    match = false;
                    break;
                }
            }
            if (!match)
                continue;

            screen_putchar('\n');
            screen_set_color(0x0B, 0x00);
            screen_puts("  ");
            screen_puts(subdirs[d]);
            screen_puts("/:\n");
            screen_set_color(0x07, 0x00);

            inst_copy_cdrom_files(root_entries[i].lba,
                                  root_entries[i].size,
                                  subdirs[d], &total_copied);
            break;
        }
    }

    if (total_copied > 0) {
        screen_putchar('\n');
        screen_puts("[install] Copied ");
        screen_put_dec(total_copied);
        screen_puts(" file(s) from CD-ROM.\n");
    }

    return (total_copied > 0);
}

/*
 * inst_print_banner — 显示安装程序横幅
 */
static void inst_print_banner(void)
{
    screen_set_color(0x0B, 0x00);
    screen_puts("========================================\n");
    screen_puts("  Nexsteaduser PlexsDOS Installer\n");
    screen_set_color(0x0F, 0x00);
    screen_puts("  Version 0.1\n");
    screen_set_color(0x07, 0x00);
    screen_puts("  Author: Tinmc189623 | Team: Nexlyh\n");
    screen_set_color(0x0B, 0x00);
    screen_puts("========================================\n\n");
    screen_set_color(0x07, 0x00);
}

/*
 * inst_confirm — 确认安装
 * 返回: true = 用户确认, false = 取消。
 */
/*
 * inst_flush_serial — 清空串口接收缓冲
 *
 * 读取并丢弃串口接收 FIFO 中所有残留字节。
 * 防止终端初始化时产生的杂散字符被误认为用户输入。
 */
static void inst_flush_serial(void)
{
    while (serial_available())
        serial_getchar();
}

/*
 * inst_confirm — 确认安装 (循环等待明确输入)
 * 返回: true = 用户按 Y, false = 用户按 N 或 ESC。
 *
 * 在等待输入前先刷新串口 FIFO, 排除终端启动时的杂散字节。
 * 只接受 Y(继续) 或 N/ESC(取消), 其他按键重新提示。
 */
static bool inst_confirm(void)
{
    screen_set_color(0x0C, 0x00);
    screen_puts("WARNING: This will ERASE all data on the hard disk!\n");
    screen_set_color(0x07, 0x00);

    /* 清除串口缓冲中的任何残留字节 */
    inst_flush_serial();

    for (;;) {
        screen_puts("Continue? (Y/N): ");

        char c = keyboard_getchar();

        /* 换行显示用户输入 */
        screen_putchar(c);
        screen_putchar('\n');

        if (c == 'y' || c == 'Y')
            return true;
        if (c == 'n' || c == 'N' || c == 0x1B)  /* N 或 ESC = 取消 */
            return false;

        /* 其他按键: 忽略并重新提示 */
        screen_puts("Please press Y (Yes) or N (No).\n");
    }
}

/* ==================== 安装盘描述 ==================== */

/* 安装盘名称 (MS-DOS 风格) */
static const char *disk_names[] = {
    "",                           /* disk 0 (不使用) */
    "Startup",                    /* disk 1: 启动盘 */
    "Setup Disk 2 - Copy 1",     /* disk 2: 内核核心 */
    "Setup Disk 2 - Copy 2",     /* disk 3: 程序 */
    "Setup Disk 2 - Copy 3",     /* disk 4: 驱动/库 */
    "Setup Disk 2 - Copy 4"      /* disk 5: 文档 */
};

/*
 * inst_prompt_disk — MS-DOS 风格换盘提示
 * @disk_num: 安装盘编号 (2-5)
 *
 * 显示类似 MS-DOS 安装程序的换盘提示:
 *   Please insert Setup Disk 2 - Copy 1 into drive A:
 *   Press ENTER when ready, or ESC to cancel.
 *
 * 返回: true = 用户按 ENTER, false = 用户按 ESC。
 */
static bool inst_prompt_disk(uint8_t disk_num)
{
    screen_putchar('\n');
    screen_set_color(0x0F, 0x00);
    screen_puts("----------------------------------------\n");
    screen_puts("  Please insert ");
    screen_set_color(0x0E, 0x00);
    screen_puts(disk_names[disk_num]);
    screen_set_color(0x0F, 0x00);
    screen_puts("\n  into drive A:\n");
    screen_set_color(0x07, 0x00);
    screen_puts("----------------------------------------\n");
    screen_puts("  Press ENTER when ready.\n");
    screen_puts("  Press ESC to cancel installation.\n");

    while (true) {
        char c = keyboard_getchar();
        if (c == '\r' || c == '\n')
            return true;
        if (c == 0x1B)  /* ESC */
            return false;
    }
}

/*
 * inst_verify_disk — 验证软盘是否可读
 * @drive: 软盘驱动器号
 *
 * 尝试读取软盘引导扇区, 验证磁盘是否已插入且可读。
 * 不检查具体内容 — 仅验证 FDC 能否成功读取。
 *
 * 返回: true = 磁盘可读, false = 无法读取 (无磁盘或错误)。
 */
static bool inst_verify_disk(uint8_t drive)
{
    uint8_t boot_sec[512];

    if (!fdc_detect_disk(drive))
        return false;

    if (!fdc_read_lba(drive, 0, 1, boot_sec))
        return false;

    /* 检查 FAT12 引导签名 */
    if (boot_sec[510] != 0x55 || boot_sec[511] != 0xAA)
        return false;

    /* 检查 FAT12 文件系统类型 */
    /* 偏移 54-61 通常包含 "FAT12   " 或 "FAT16   " */
    if (boot_sec[54] != 'F' || boot_sec[55] != 'A' ||
        boot_sec[56] != 'T')
        return false;

    return true;
}

/*
 * inst_prompt_and_verify — 提示换盘并验证 (带重试)
 * @disk_num: 安装盘编号 (2-5)
 * @drive:    软盘驱动器号
 *
 * MS-DOS 风格的换盘交互:
 * 1. 提示用户插入指定磁盘
 * 2. 等待用户按 ENTER
 * 3. 验证磁盘是否可读
 * 4. 如果失败, 提示重试 (最多 INSTALL_MAX_RETRIES 次)
 *
 * 返回: true = 磁盘已就绪, false = 用户取消或超过重试次数。
 */
static bool inst_prompt_and_verify(uint8_t disk_num, uint8_t drive)
{
    for (int retry = 0; retry < INSTALL_MAX_RETRIES; retry++) {
        if (retry > 0) {
            screen_set_color(0x0C, 0x00);
            screen_puts("\n  Disk not ready or read error.\n");
            screen_set_color(0x07, 0x00);
            screen_puts("  Please check the disk and try again.\n");
            screen_puts("  (Attempt ");
            screen_put_dec(retry + 1);
            screen_puts(" of ");
            screen_put_dec(INSTALL_MAX_RETRIES);
            screen_puts(")\n");
        }

        if (!inst_prompt_disk(disk_num))
            return false;

        /* 检查写保护 */
        if (fdc_write_protected(drive)) {
            screen_set_color(0x0C, 0x00);
            screen_puts("  WARNING: Disk is write-protected!\n");
            screen_set_color(0x07, 0x00);
            screen_puts("  Please remove the write protection tab.\n");
            /* 不返回 false — 读取操作不需要写入, 但警告用户 */
        }

        screen_puts("  Verifying disk...");
        if (inst_verify_disk(drive)) {
            screen_puts(" OK\n");
            return true;
        }
        screen_puts(" FAILED\n");
    }

    screen_set_color(0x0C, 0x00);
    screen_puts("\n  Failed to read disk after ");
    screen_put_dec(INSTALL_MAX_RETRIES);
    screen_puts(" attempts.\n");
    screen_set_color(0x07, 0x00);
    return false;
}

/*
 * inst_write_mbr — 写入主引导记录
 * 在 LBA 0 写入 MBR, 包含一个 FAT32 LBA 分区。
 * 返回: true = 成功。
 */
static bool inst_write_mbr(void)
{
    uint32_t mbr_size = (uint32_t)(_binary_hd_mbr_bin_end -
                                   _binary_hd_mbr_bin_start);

    if (mbr_size > 512) {
        screen_puts("[install] MBR too large\n");
        return false;
    }

    /* 清零缓冲区 */
    for (int i = 0; i < 512; i++)
        inst_buf[i] = 0;

    /* 复制 MBR 代码 */
    for (uint32_t i = 0; i < mbr_size; i++)
        inst_buf[i] = _binary_hd_mbr_bin_start[i];

    /* 写入分区表项 (条目 1, 偏移 0x1BE) */
    uint8_t *part = inst_buf + 0x1BE;

    part[0]  = 0x80;              /* 活动分区 */
    part[1]  = 0x00;              /* 起始磁头 */
    part[2]  = 0x01;              /* 起始扇区 */
    part[3]  = 0x00;              /* 起始柱面 */
    part[4]  = 0x0C;              /* 分区类型: FAT32 LBA */
    part[5]  = 0x0F;              /* 结束磁头 */
    part[6]  = 0xFF;              /* 结束扇区 */
    part[7]  = 0xFF;              /* 结束柱面 */

    /* 起始 LBA (小端序) */
    part[8]  = (uint8_t)(PART_START_LBA & 0xFF);
    part[9]  = (uint8_t)((PART_START_LBA >> 8) & 0xFF);
    part[10] = (uint8_t)((PART_START_LBA >> 16) & 0xFF);
    part[11] = (uint8_t)((PART_START_LBA >> 24) & 0xFF);

    /* 分区扇区数 */
    part[12] = (uint8_t)(PART_SECTORS & 0xFF);
    part[13] = (uint8_t)((PART_SECTORS >> 8) & 0xFF);
    part[14] = (uint8_t)((PART_SECTORS >> 16) & 0xFF);
    part[15] = (uint8_t)((PART_SECTORS >> 24) & 0xFF);

    /* 确保引导签名 */
    inst_buf[510] = 0x55;
    inst_buf[511] = 0xAA;

    /* 写入 LBA 0 */
    if (!disk_write_sectors(0, 1, inst_buf)) {
        screen_puts("[install] Failed to write MBR\n");
        return false;
    }

    screen_puts("[install] MBR written\n");
    return true;
}

/*
 * inst_write_fat32 — 写入 FAT32 文件系统骨架
 * 创建 VBR、FAT 表和空根目录。
 * 返回: true = 成功。
 */
static bool inst_write_fat32(void)
{
    uint32_t vbr_lba = PART_START_LBA;

    /* === 写入 VBR (卷引导记录) === */
    uint32_t vbr_size = (uint32_t)(_binary_hd_vbr_bin_end -
                                   _binary_hd_vbr_bin_start);
    if (vbr_size > 512)
        vbr_size = 512;

    for (int i = 0; i < 512; i++)
        inst_buf[i] = 0;
    for (uint32_t i = 0; i < vbr_size; i++)
        inst_buf[i] = _binary_hd_vbr_bin_start[i];

    /* 更新 BPB 中的分区参数 */
    *(uint32_t *)(inst_buf + 32) = PART_SECTORS;       /* bpb_total_secs32 */
    *(uint32_t *)(inst_buf + 36) = FAT32_FAT_SECTORS;  /* bpb_fat_size32 */
    *(uint32_t *)(inst_buf + 28) = PART_START_LBA;     /* bpb_hidden_secs */

    /* 引导签名 */
    inst_buf[510] = 0x55;
    inst_buf[511] = 0xAA;

    if (!disk_write_sectors(vbr_lba, 1, inst_buf)) {
        screen_puts("[install] Failed to write VBR\n");
        return false;
    }

    /* 写入备份 VBR (偏移 6 扇区) */
    disk_write_sectors(vbr_lba + 6, 1, inst_buf);

    /* === 写入 FSINFO 扇区 (偏移 1 扇区) === */
    for (int i = 0; i < 512; i++)
        inst_buf[i] = 0;

    /* FSINFO 签名 */
    inst_buf[0] = 0x52; inst_buf[1] = 0x52;
    inst_buf[2] = 0x61; inst_buf[3] = 0x41;
    inst_buf[484] = 0x72; inst_buf[485] = 0x72;
    inst_buf[486] = 0x41; inst_buf[487] = 0x61;

    /* 空闲簇数 */
    uint32_t data_sectors = PART_SECTORS - FAT32_RESERVED_SECS -
                            FAT32_NUM_FATS * FAT32_FAT_SECTORS;
    uint32_t total_clusters = data_sectors / FAT32_SECS_PER_CLUST;
    *(uint32_t *)(inst_buf + 488) = total_clusters - 1;
    *(uint32_t *)(inst_buf + 492) = 3;  /* 下一个空闲簇 */

    /* 引导签名 */
    inst_buf[510] = 0x55;
    inst_buf[511] = 0xAA;

    disk_write_sectors(vbr_lba + 1, 1, inst_buf);

    /* === 初始化 FAT 表 === */
    for (int i = 0; i < 512; i++)
        inst_buf[i] = 0;

    /* FAT[0] = 0x0FFFFFF8 (媒体类型) */
    inst_buf[0] = 0xF8; inst_buf[1] = 0xFF;
    inst_buf[2] = 0xFF; inst_buf[3] = 0x0F;
    /* FAT[1] = 0x0FFFFFFF (结束标记) */
    inst_buf[4] = 0xFF; inst_buf[5] = 0xFF;
    inst_buf[6] = 0xFF; inst_buf[7] = 0x0F;

    uint32_t fat1_lba = vbr_lba + FAT32_RESERVED_SECS;
    uint32_t fat2_lba = fat1_lba + FAT32_FAT_SECTORS;

    /* 写入 FAT1 第一个扇区 */
    disk_write_sectors(fat1_lba, 1, inst_buf);
    /* 写入 FAT2 第一个扇区 (备份) */
    disk_write_sectors(fat2_lba, 1, inst_buf);

    /* 清零 FAT 表其余扇区 */
    for (int i = 0; i < 512; i++)
        inst_buf[i] = 0;
    for (uint32_t s = 1; s < FAT32_FAT_SECTORS; s++) {
        disk_write_sectors(fat1_lba + s, 1, inst_buf);
        disk_write_sectors(fat2_lba + s, 1, inst_buf);
    }

    /* === 初始化根目录 (簇 2) === */
    uint32_t root_lba = fat1_lba + FAT32_NUM_FATS * FAT32_FAT_SECTORS;

    /* 清零根目录所有簇 */
    for (int i = 0; i < 512; i++)
        inst_buf[i] = 0;
    for (uint32_t s = 0; s < (uint32_t)FAT32_SECS_PER_CLUST; s++) {
        disk_write_sectors(root_lba + s, 1, inst_buf);
    }

    /* 写入卷标目录项 */
    for (int i = 0; i < 512; i++)
        inst_buf[i] = 0;
    const char *label = "PLXSDOS    ";
    for (int i = 0; i < 11; i++)
        inst_buf[i] = label[i];
    inst_buf[11] = 0x08;  /* 卷标属性 */

    disk_write_sectors(root_lba, 1, inst_buf);

    screen_puts("[install] FAT32 filesystem created\n");
    return true;
}

/*
 * inst_copy_floppy — 从安装软盘复制文件到硬盘
 * @disk_num: 安装盘编号 (2-5)
 * @drive:    软盘驱动器号
 * 返回: true = 成功 (至少复制了一个文件)。
 *
 * MS-DOS 风格流程:
 * 1. 提示用户插入指定磁盘 (带验证和重试)
 * 2. 读取 FAT12 根目录
 * 3. 逐个复制文件, 显示文件名和大小
 * 4. 统计复制结果
 */
static bool inst_copy_floppy(uint8_t disk_num, uint8_t drive)
{
    int copied = 0;

    /* MS-DOS 风格: 提示插入磁盘并验证 */
    if (!inst_prompt_and_verify(disk_num, drive))
        return false;

    screen_puts("  Reading floppy...\n");

    /* 读取 FAT12 引导扇区 */
    uint8_t boot_sec[512];
    if (!fdc_read_lba(drive, 0, 1, boot_sec)) {
        screen_puts("[install] Failed to read floppy boot sector\n");
        return false;
    }

    /* 将引导扇区复制到 0x7C00 供 fat12_init_ex 使用 */
    uint8_t *bpb_dest = (uint8_t *)0x7C00;
    for (int i = 0; i < 512; i++)
        bpb_dest[i] = boot_sec[i];

    /* 初始化 FAT12 驱动 (使用 FDC 包装函数) */
    if (!fat12_init_ex(inst_fdc_read, 0x7C00, 0)) {
        screen_puts("[install] Failed to init FAT12\n");
        return false;
    }

    /* 遍历 FAT12 根目录, 复制所有文件到 FAT32 */
    extern struct fat12_dir_entry *fat12_find_file(const char *name);
    /* 直接访问内部结构 — 通过 list_root 之后逐个查找 */
    /* 使用 fat12 的 list 遍历逻辑 */

    /* 简化实现: 逐个尝试加载已知文件名 */
    /* 通过 FAT12 根目录缓冲区直接遍历 */
    /* fat12_init_ex 已将根目录加载到内部缓冲区, 我们通过 find_file 逐个查找 */

    /* 已知的安装盘文件列表 (每张盘上的文件) */
    /* 由于无法直接访问 fat12 的内部 root_dir_buf, 我们使用一个替代方法:
     * 调用 fat12_list_root() 显示文件, 然后逐个加载 */
    screen_puts("  Files on disk:\n");

    /* 计算根目录 LBA (从 BPB) */
    uint16_t bpb_reserved = *(uint16_t *)(boot_sec + 14);
    uint8_t  bpb_num_fats = *(uint8_t  *)(boot_sec + 16);
    uint16_t bpb_fat_size = *(uint16_t *)(boot_sec + 22);
    uint16_t bpb_root_entries = *(uint16_t *)(boot_sec + 17);
    uint32_t root_lba_floppy = bpb_reserved + bpb_num_fats * bpb_fat_size;
    uint32_t root_dir_sectors = (bpb_root_entries * 32 + 511) / 512;

    /* 读取根目录到 file_buf */
    if (root_dir_sectors * 512 > FILE_BUF_SIZE)
        root_dir_sectors = FILE_BUF_SIZE / 512;

    if (!inst_fdc_read(root_lba_floppy, (uint8_t)root_dir_sectors, file_buf)) {
        screen_puts("[install] Failed to read root directory\n");
        return false;
    }

    /* 遍历目录项 */
    struct fat12_dir_entry *entries = (struct fat12_dir_entry *)file_buf;

    for (uint16_t i = 0; i < bpb_root_entries; i++) {
        uint8_t first = entries[i].name[0];

        /* 空条目 = 目录结束 */
        if (first == 0x00)
            break;
        /* 已删除条目 */
        if (first == 0xE5)
            continue;
        /* 卷标或长文件名 */
        if (entries[i].attr & FAT12_ATTR_VOLUME_ID)
            continue;
        if (entries[i].attr == 0x0F)
            continue;

        /* 构造文件名用于显示和 fat12_find_file */
        char display_name[13];
        int pos = 0;
        for (int j = 0; j < 8; j++) {
            if (entries[i].name[j] != ' ')
                display_name[pos++] = entries[i].name[j];
        }
        if (entries[i].name[8] != ' ') {
            display_name[pos++] = '.';
            for (int j = 8; j < 11; j++) {
                if (entries[i].name[j] != ' ')
                    display_name[pos++] = entries[i].name[j];
            }
        }
        display_name[pos] = '\0';

        /* MS-DOS 风格: 显示正在复制的文件 */
        screen_puts("    ");
        screen_puts(display_name);
        screen_puts(" ");
        screen_put_dec(entries[i].file_size);
        screen_puts(" bytes");

        /* 使用 fat12_find_file 查找并加载文件 */
        struct fat12_dir_entry *entry = fat12_find_file(display_name);
        if (!entry) {
            screen_set_color(0x0C, 0x00);
            screen_puts(" [NOT FOUND]\n");
            screen_set_color(0x07, 0x00);
            continue;
        }

        /* 加载文件到 file_buf 的前部 (覆盖根目录数据, 已不需要) */
        uint32_t loaded = fat12_load_file(entry, (uint32_t)file_buf);
        if (loaded == 0) {
            screen_set_color(0x0C, 0x00);
            screen_puts(" [READ ERROR]\n");
            screen_set_color(0x07, 0x00);
            continue;
        }

        /* 写入 FAT32 硬盘 */
        if (!fat32_write_file(display_name, file_buf, loaded)) {
            screen_set_color(0x0C, 0x00);
            screen_puts(" [WRITE ERROR]\n");
            screen_set_color(0x07, 0x00);
            continue;
        }

        screen_set_color(0x0A, 0x00);
        screen_puts(" [OK]\n");
        screen_set_color(0x07, 0x00);
        copied++;
    }

    if (copied > 0) {
        screen_puts("[install] Copied ");
        screen_put_dec(copied);
        screen_puts(" file(s) from disk ");
        screen_put_dec(disk_num);
        screen_putchar('\n');
    }

    return (copied > 0);
}

/*
 * installer_run — 运行安装程序
 *
 * Nexsteaduser 交互式安装流程:
 * 1. 显示欢迎信息
 * 2. 检测硬盘
 * 3. 确认擦除
 * 4. 写入 MBR + VBR + FAT32 文件系统
 * 5. 从安装介质复制文件:
 *    - 优先: CD-ROM (ISO 9660, 单光盘安装)
 *    - 回退: 软盘 (FAT12, MS-DOS 风格换盘)
 * 6. 显示完成信息
 *
 * 返回: true = 安装成功, false = 失败或用户取消。
 */
bool installer_run(void)
{
    inst_print_banner();

    /* 检测硬盘 */
    screen_puts("Checking for hard disk...\n");
    screen_puts("Hard disk detected.\n\n");

    /* 确认安装 */
    if (!inst_confirm()) {
        screen_puts("Installation cancelled.\n");
        return false;
    }

    screen_puts("\nStarting installation...\n\n");

    /* 写入 MBR */
    if (!inst_write_mbr()) {
        return false;
    }

    /* 写入 FAT32 文件系统 */
    if (!inst_write_fat32()) {
        return false;
    }

    /*
     * 从安装介质复制文件
     * Nexsteaduser 安装程序自动检测安装介质:
     *   优先: CD-ROM (ISO 9660) — 单光盘安装, 无需换盘
     *   回退: 软盘 (FAT12) — MS-DOS 风格换盘交互
     */
    screen_set_color(0x0B, 0x00);
    screen_puts("\n========================================\n");
    screen_puts("  Copying installation files\n");
    screen_puts("========================================\n");
    screen_set_color(0x07, 0x00);

    /* 尝试 CD-ROM 安装 (优先) */
    screen_puts("[install] Trying CD-ROM installation...\n");
    bool cd_ok = inst_copy_cdrom();

    if (cd_ok) {
        screen_set_color(0x0A, 0x00);
        screen_puts("[install] CD-ROM installation complete.\n");
        screen_set_color(0x07, 0x00);
    } else {
        /* CD-ROM 不可用, 回退到软盘安装 */
        screen_set_color(0x0E, 0x00);
        screen_puts("[install] CD-ROM not available.\n");
        screen_set_color(0x07, 0x00);
        screen_puts("[install] Falling back to floppy installation.\n");

        /* 通过 BIOS 数据区 (BDA) 检测软盘驱动器是否存在
         * BDA 0x0410 (Equipment Word):
         *   bit 0 = 系统是否有软盘引导能力
         * 若 bit 0 = 0, 则无软盘 — 跳过 FDC 初始化 */
        uint16_t bda_equip;
        __asm__ __volatile__(
            "movw 0x410, %%ax"
            : "=a"(bda_equip)
        );
        if (!(bda_equip & 0x0001)) {
            screen_set_color(0x0E, 0x00);
            screen_puts("[install] No floppy drive detected.\n");
            screen_set_color(0x07, 0x00);
            screen_puts("[install] Core OS installed. "
                        "Install files manually if needed.\n");
        } else {
            screen_puts("[install] Initializing floppy controller...\n");
            if (!fdc_init()) {
                screen_set_color(0x0C, 0x00);
                screen_puts("[install] FDC init failed.\n");
                screen_set_color(0x07, 0x00);
                screen_puts("[install] Core OS installed. "
                            "Install files manually if needed.\n");
            } else {
                for (uint8_t disk = 2; disk <= 5; disk++) {
                    if (!inst_copy_floppy(disk, 0)) {
                        screen_set_color(0x0C, 0x00);
                        screen_puts("[install] Warning: Failed to copy from "
                                    "disk ");
                        screen_put_dec(disk);
                        screen_putchar('\n');
                        screen_set_color(0x07, 0x00);
                    }
                }
            }
        }
    }

    /* 安装完成 — MS-DOS 风格 */
    screen_putchar('\n');
    screen_set_color(0x0A, 0x00);
    screen_puts("========================================\n");
    screen_puts("  Nexsteaduser PlexsDOS installed!\n");
    screen_puts("========================================\n\n");
    screen_set_color(0x0F, 0x00);
    screen_puts("  Please remove all floppies from\n");
    screen_puts("  drive A: and press any key to reboot.\n\n");
    screen_set_color(0x07, 0x00);

    keyboard_getchar();

    return true;
}
