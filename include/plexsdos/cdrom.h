/*
 * Nexsteaduser — PlexsDOS
 * ATAPI CD-ROM 驱动接口 + ISO 9660 文件系统
 * 作者: Tinmc189623 | 团队: Nexlyh
 *
 * 硬件层: ATAPI PACKET 命令协议 (通过 ATA 主通道)
 * 文件系统层: ISO 9660 (只读, 支持 Rock Ridge 扩展)
 *
 * 参考:
 * - ATA/ATAPI-6 规范 (T13 1410D)
 * - ECMA-119 / ISO 9660 标准
 * - Linux cdrom.c / isofs/inode.c
 */

#ifndef _PLXSDOS_CDROM_H
#define _PLXSDOS_CDROM_H

#include <plexsdos/types.h>

#ifdef __cplusplus
extern "C" {
#endif

/* ==================== ATAPI 命令 ==================== */

/* ATAPI PACKET 命令码 */
#define ATAPI_CMD_PACKET        0xA0
#define ATAPI_CMD_IDENTIFY      0xA1
#define ATAPI_CMD_RESET         0x08

/* SCSI 命令 (通过 ATAPI PACKET 发送) */
#define SCSI_CMD_TEST_UNIT_READY  0x00
#define SCSI_CMD_INQUIRY          0x12
#define SCSI_CMD_READ_CAPACITY    0x25
#define SCSI_CMD_READ_10          0x28
#define SCSI_CMD_REQUEST_SENSE    0x03

/* ATAPI 特征寄存器值 */
#define ATAPI_FEATURE_DMA     0x01  /* 使用 DMA 传输 */
#define ATAPI_FEATURE_PIO     0x00  /* 使用 PIO 传输 */

/* ATAPI 扇区大小 (CD-ROM 标准) */
#define CDROM_SECTOR_SIZE     2048

/* CD-ROM 最大速度检测重试次数 */
#define CDROM_MAX_RETRIES     3

/* ==================== ATAPI 设备信息 ==================== */

/* ATAPI 设备类型 (来自 INQUIRY 命令) */
enum cdrom_device_type {
    CDROM_TYPE_NONE = 0,       /* 未检测到 */
    CDROM_TYPE_CDROM = 5,      /* CD-ROM 设备 */
    CDROM_TYPE_DVD = 7         /* DVD 设备 */
};

/* ATAPI 设备信息结构 */
struct cdrom_info {
    bool present;               /* 设备是否存在 */
    enum cdrom_device_type type; /* 设备类型 */
    uint16_t sector_size;       /* 扇区大小 (通常 2048) */
    uint32_t total_sectors;     /* 总扇区数 */
    char vendor[9];             /* 厂商 (8 字符) */
    char product[17];           /* 产品名 (16 字符) */
    char revision[5];           /* 固件版本 (4 字符) */
};

/* ==================== ISO 9660 文件系统 ==================== */

/* ISO 9660 卷描述符类型 */
#define ISO9660_VD_BOOT         0x00  /* 引导记录 */
#define ISO9660_VD_PRIMARY      0x01  /* 主卷描述符 */
#define ISO9660_VD_SUPPLEMENTARY 0x02 /* 补充卷描述符 (Joliet) */
#define ISO9660_VD_TERMINATOR   0xFF  /* 卷描述符终止符 */

/* ISO 9660 目录记录中的文件标志 */
#define ISO9660_FLAG_HIDDEN     0x01  /* 隐藏文件 */
#define ISO9660_FLAG_DIRECTORY  0x02  /* 目录 */
#define ISO9660_FLAG_ASSOCIATED 0x04  /* 关联文件 */

/* ISO 9660 主卷描述符 (偏移和大小基于 ECMA-119) */
struct iso9660_pvd {
    uint8_t  type;              /* 0x00: 卷描述符类型 (必须为 1) */
    char     id[5];             /* 0x01: "CD001" */
    uint8_t  version;           /* 0x06: 版本 (必须为 1) */
    uint8_t  unused1;           /* 0x07 */
    char     system_id[32];     /* 0x08: 系统标识 */
    char     volume_id[32];     /* 0x28: 卷标识 */
    uint8_t  unused2[8];        /* 0x48 */
    uint32_t volume_space_le;   /* 0x50: 卷空间大小 (小端序) */
    uint32_t volume_space_be;   /* 0x54: 卷空间大小 (大端序) */
    uint8_t  unused3[32];       /* 0x58 */
    uint16_t volume_set_size_le; /* 0x78: 卷集大小 */
    uint16_t volume_seq_num_le;  /* 0x7C: 卷序号 */
    uint16_t logical_block_size_le; /* 0x80: 逻辑块大小 */
    uint16_t logical_block_size_be; /* 0x82 */
    uint32_t path_table_size_le;    /* 0x84: 路径表大小 */
    uint8_t  unused4[4];        /* 0x88 */
    uint32_t l_path_table;      /* 0x8C: L 路径表 LBA */
    uint32_t opt_l_path_table;  /* 0x90: 可选 L 路径表 */
    uint32_t m_path_table;      /* 0x94: M 路径表 */
    uint32_t opt_m_path_table;  /* 0x98: 可选 M 路径表 */
    /* 0x9C: 根目录记录 (34 字节) */
    uint8_t  root_dir_record[34];
    /* 0xBE: 卷集标识 */
    char     volume_set_id[128];
    char     publisher_id[128];
    char     preparer_id[128];
    char     application_id[128];
    char     copyright_file_id[37];
    char     abstract_file_id[37];
    char     bibliographic_file_id[37];
    /* 0x333: 创建/修改日期 */
    char     creation_date[17];
    char     modification_date[17];
    char     expiration_date[17];
    char     effective_date[17];
    uint8_t  file_structure_version;
} __attribute__((packed));

/* ISO 9660 目录记录 (通用格式) */
struct iso9660_dir_record {
    uint8_t  length;            /* 记录长度 */
    uint8_t  ext_attr_length;   /* 扩展属性长度 */
    uint32_t extent_le;         /* 文件起始 LBA (小端序) */
    uint32_t extent_be;         /* 文件起始 LBA (大端序) */
    uint32_t extent_size_le;    /* 文件大小 (小端序) */
    uint32_t extent_size_be;    /* 文件大小 (大端序) */
    uint8_t  date[7];           /* 日期 (BCD 格式) */
    uint8_t  file_flags;        /* 文件标志 */
    uint8_t  file_unit_size;    /* 文件单元大小 */
    uint8_t  interleave_gap;    /* 交错间隔 */
    uint16_t vol_seq_num_le;    /* 卷序号 (小端序) */
    uint16_t vol_seq_num_be;    /* 卷序号 (大端序) */
    uint8_t  name_len;          /* 文件名长度 */
    char     name[];            /* 文件名 (不含分号后的版本号) */
} __attribute__((packed));

/* ISO 9660 目录条目 (用于公共 API) */
struct iso9660_entry {
    char     name[256];         /* 文件名 (已处理) */
    uint32_t lba;               /* 起始 LBA */
    uint32_t size;              /* 文件大小 (字节) */
    bool     is_directory;      /* 是否为目录 */
};

/* ISO 9660 文件系统上下文 */
struct iso9660_fs {
    bool mounted;               /* 是否已挂载 */
    uint32_t root_lba;          /* 根目录 LBA */
    uint32_t root_size;         /* 根目录大小 (字节) */
    uint16_t block_size;        /* 逻辑块大小 (通常 2048) */
    uint32_t total_sectors;     /* 总扇区数 */
    char volume_id[33];         /* 卷标识 */
};

/* ==================== 公共 API ==================== */

/*
 * cdrom_init — 初始化 ATAPI CD-ROM 驱动
 *
 * 1. 发送 ATAPI RESET
 * 2. 发送 IDENTIFY PACKET DEVICE 获取设备信息
 * 3. 发送 TEST UNIT READY 检查光盘是否就绪
 * 4. 发送 READ CAPACITY 获取容量
 *
 * 返回: true = CD-ROM 就绪, false = 未检测到或初始化失败。
 */
bool cdrom_init(void);

/*
 * cdrom_read_sector — 从 CD-ROM 读取一个扇区
 * @lba:  扇区 LBA 地址
 * @buf:  目标缓冲区 (至少 2048 字节)
 * 返回: true = 成功。
 */
bool cdrom_read_sector(uint32_t lba, void *buf);

/*
 * cdrom_read_sectors — 从 CD-ROM 读取多个扇区
 * @lba:   起始 LBA
 * @count: 扇区数
 * @buf:   目标缓冲区
 * 返回: true = 成功。
 */
bool cdrom_read_sectors(uint32_t lba, uint16_t count, void *buf);

/*
 * cdrom_get_info — 获取 CD-ROM 设备信息
 * 返回: 指向 cdrom_info 结构的指针。
 */
const struct cdrom_info *cdrom_get_info(void);

/*
 * cdrom_eject — 弹出光盘托盘
 */
void cdrom_eject(void);

/* ==================== ISO 9660 文件系统 API ==================== */

/*
 * iso9660_mount — 挂载 ISO 9660 文件系统
 *
 * 读取主卷描述符 (LBA 16), 解析根目录信息。
 * 返回: true = 挂载成功。
 */
bool iso9660_mount(void);

/*
 * iso9660_open_dir — 打开目录 (读取目录内容)
 * @lba:       目录起始 LBA
 * @size:      目录大小 (字节)
 * @entries:   输出条目数组
 * @max_count: 数组最大容量
 * 返回: 实际条目数。
 */
int iso9660_read_dir(uint32_t lba, uint32_t size,
                     struct iso9660_entry *entries, int max_count);

/*
 * iso9660_read_file — 读取文件内容
 * @lba:    文件起始 LBA
 * @size:   文件大小 (字节)
 * @buf:    目标缓冲区
 * @buf_size: 缓冲区大小
 * 返回: 实际读取的字节数。
 */
uint32_t iso9660_read_file(uint32_t lba, uint32_t size,
                           void *buf, uint32_t buf_size);

/*
 * iso9660_get_root — 获取根目录条目
 * 返回: 指向根目录 iso9660_entry 的指针。
 */
const struct iso9660_entry *iso9660_get_root(void);

/*
 * iso9660_get_fs — 获取文件系统上下文
 * 返回: 指向 iso9660_fs 结构的指针。
 */
const struct iso9660_fs *iso9660_get_fs(void);

#ifdef __cplusplus
}
#endif

#endif /* _PLXSDOS_CDROM_H */
