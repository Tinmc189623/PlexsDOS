/*
 * Nexsteaduser — PlexsDOS
 * 驱动器抽象层实现
 * 作者: Tinmc189623 | 团队: Nexlyh
 *
 * 管理多驱动器: A:-Z:, 支持软盘、硬盘、CD-ROM。
 * 提供驱动器注册、切换和查询功能。
 */

#include <plexsdos/drive.h>
#include <plexsdos/screen.h>
#include <plexsdos/serial.h>

/* 驱动器表 */
static struct drive_info drive_table[DRIVE_MAX];

/* 当前驱动器 (默认 C:) */
static int g_current_drive = DRIVE_LETTER_C;

/*
 * drive_init — 初始化驱动器子系统
 *
 * 清零驱动器表, 默认当前驱动器为 C:。
 * 实际设备注册由 kernel_main 在检测到设备后调用 drive_register()。
 */
void drive_init(void)
{
    for (int i = 0; i < DRIVE_MAX; i++) {
        drive_table[i].type = DRIVE_TYPE_NONE;
        drive_table[i].device_id = 0;
        drive_table[i].partition_lba = 0;
        drive_table[i].mounted = false;
        drive_table[i].label[0] = '\0';
    }

    g_current_drive = DRIVE_LETTER_C;

    serial_puts("[drive] initialized (A:-Z: available).\n");
}

/*
 * drive_get_current — 获取当前驱动器字母索引
 */
int drive_get_current(void)
{
    return g_current_drive;
}

/*
 * drive_set_current — 切换当前驱动器
 * @letter: DRIVE_LETTER_A ~ DRIVE_LETTER_Z
 *
 * 检查驱动器是否已注册, 切换当前索引。
 * 文件系统初始化由 fs 层负责。
 */
bool drive_set_current(int letter)
{
    if (letter < 0 || letter >= DRIVE_MAX)
        return false;

    if (drive_table[letter].type == DRIVE_TYPE_NONE)
        return false;

    g_current_drive = letter;

    serial_puts("[drive] switched to ");
    serial_putchar(drive_letter_to_char(letter));
    serial_putchar('\n');

    return true;
}

/*
 * drive_get_info — 获取驱动器信息
 * @letter: DRIVE_LETTER_A ~ DRIVE_LETTER_Z
 */
const struct drive_info *drive_get_info(int letter)
{
    if (letter < 0 || letter >= DRIVE_MAX)
        return NULL;

    if (drive_table[letter].type == DRIVE_TYPE_NONE)
        return NULL;

    return &drive_table[letter];
}

/*
 * drive_register — 注册驱动器到驱动器表
 * @letter:        驱动器字母索引
 * @type:          驱动器类型
 * @device_id:     设备索引
 * @partition_lba: 分区起始 LBA
 */
void drive_register(int letter, uint8_t type, uint8_t device_id,
                    uint32_t partition_lba)
{
    if (letter < 0 || letter >= DRIVE_MAX)
        return;

    drive_table[letter].type = type;
    drive_table[letter].device_id = device_id;
    drive_table[letter].partition_lba = partition_lba;
    drive_table[letter].mounted = (type != DRIVE_TYPE_NONE);

    serial_puts("[drive] registered ");
    serial_putchar(drive_letter_to_char(letter));
    serial_puts(": as ");
    serial_puts(drive_get_type_name(type));
    serial_puts(", LBA ");
    serial_put_hex(partition_lba);
    serial_putchar('\n');
}

/*
 * drive_letter_to_char — 将驱动器索引转为字母字符
 */
char drive_letter_to_char(int letter)
{
    if (letter >= 0 && letter < DRIVE_MAX)
        return (char)('A' + letter);
    return '?';
}

/*
 * drive_get_type_name — 获取驱动器类型名称
 */
const char *drive_get_type_name(uint8_t type)
{
    switch (type) {
    case DRIVE_TYPE_FLOPPY: return "Floppy";
    case DRIVE_TYPE_HDD:    return "Hard Disk";
    case DRIVE_TYPE_CDROM:  return "CD-ROM";
    default:                return "Unknown";
    }
}
