/*
 * Nexsteaduser — PlexsDOS
 * 驱动器抽象层接口
 * 作者: Tinmc189623 | 团队: Nexlyh
 *
 * 管理多驱动器 (软盘 A:/B:, 硬盘 C:, CD-ROM D:)。
 * 提供驱动器注册、切换和查询功能。
 */

#ifndef _PLXSDOS_DRIVE_H
#define _PLXSDOS_DRIVE_H

#include <plexsdos/types.h>

#ifdef __cplusplus
extern "C" {
#endif

/* 最大驱动器数量 */
#define DRIVE_MAX        4

/* 驱动器字母索引 */
#define DRIVE_LETTER_A   0
#define DRIVE_LETTER_B   1
#define DRIVE_LETTER_C   2
#define DRIVE_LETTER_D   3

/* 驱动器类型 */
#define DRIVE_TYPE_NONE    0
#define DRIVE_TYPE_FLOPPY  1
#define DRIVE_TYPE_HDD     2
#define DRIVE_TYPE_CDROM   3

/* 驱动器信息结构 */
struct drive_info {
    uint8_t  type;           /* DRIVE_TYPE_* */
    uint8_t  device_id;      /* 设备索引 (ATA master=0, slave=1, FDC=0/1) */
    uint32_t partition_lba;  /* 分区起始 LBA (硬盘) */
    bool     mounted;        /* 是否已挂载文件系统 */
    char     label[12];      /* 卷标 */
};

/*
 * drive_init — 初始化驱动器子系统
 *
 * 注册检测到的设备:
 *   A: = 软盘 (如果 FDC 检测到)
 *   C: = 硬盘 FAT32 (如果 ATA 检测到)
 *   D: = CD-ROM (如果 ATAPI 检测到)
 *
 * 默认当前驱动器 = C:
 */
void drive_init(void);

/*
 * drive_get_current — 获取当前驱动器字母索引
 * 返回: DRIVE_LETTER_A ~ DRIVE_LETTER_D
 */
int drive_get_current(void);

/*
 * drive_set_current — 切换当前驱动器
 * @letter: DRIVE_LETTER_A ~ DRIVE_LETTER_D
 * 返回: true = 切换成功, false = 驱动器不存在
 *
 * 如果目标驱动器是 HDD 类型, 会重新初始化 FAT32。
 */
bool drive_set_current(int letter);

/*
 * drive_get_info — 获取驱动器信息
 * @letter: DRIVE_LETTER_A ~ DRIVE_LETTER_D
 * 返回: 驱动器信息指针, 无效返回 NULL
 */
const struct drive_info *drive_get_info(int letter);

/*
 * drive_register — 注册驱动器
 * @letter:        驱动器字母索引
 * @type:          驱动器类型
 * @device_id:     设备索引
 * @partition_lba: 分区起始 LBA
 */
void drive_register(int letter, uint8_t type, uint8_t device_id,
                    uint32_t partition_lba);

/*
 * drive_letter_to_char — 将驱动器索引转为字母字符
 * @letter: DRIVE_LETTER_A ~ DRIVE_LETTER_D
 * 返回: 'A' ~ 'D', 无效返回 '?'
 */
char drive_letter_to_char(int letter);

/*
 * drive_get_type_name — 获取驱动器类型名称
 * @type: DRIVE_TYPE_*
 * 返回: 类型名称字符串
 */
const char *drive_get_type_name(uint8_t type);

#ifdef __cplusplus
}
#endif

#endif /* _PLXSDOS_DRIVE_H */
