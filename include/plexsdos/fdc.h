/*
 * Nexsteaduser — PlexsDOS
 * 82077AA 软盘控制器 (FDC) 驱动接口
 * 作者: Tinmc189623 | 团队: Nexlyh
 *
 * 直接编程 82077AA FDC 硬件:
 * - 端口 I/O (0x3F0-0x3F7) 控制 FDC 寄存器
 * - 8237 DMA 控制器通道 2 进行数据传输
 * - IRQ 6 中断驱动的命令完成通知
 * - 马达状态机自动管理
 * - 多格式支持 (1.44MB/1.2MB/720KB)
 */

#ifndef _PLXSDOS_FDC_H
#define _PLXSDOS_FDC_H

#include <plexsdos/types.h>

#ifdef __cplusplus
extern "C" {
#endif

/* FDC I/O 端口 */
#define FDC_DOR   0x3F2   /* Digital Output Register */
#define FDC_MSR   0x3F4   /* Main Status Register */
#define FDC_FIFO  0x3F5   /* Data Register (FIFO) */
#define FDC_DIR   0x3F7   /* Digital Input Register (read) */
#define FDC_DCR   0x3F7   /* Digital Control Register (write) */

/* DOR 位定义 */
#define DOR_DRIVE0      0x00   /* 选择驱动器 0 */
#define DOR_DRIVE1      0x01   /* 选择驱动器 1 */
#define DOR_RESET       0x04   /* FDC 复位 (0=复位, 1=正常) */
#define DOR_DMA_GATE    0x08   /* DMA 请求允许 */
#define DOR_MOTOR0      0x10   /* 驱动器 0 马达 */
#define DOR_MOTOR1      0x20   /* 驱动器 1 马达 */

/* MSR 位定义 */
#define MSR_DRV0_BUSY   0x01   /* 驱动器 0 忙 */
#define MSR_DRV1_BUSY   0x02   /* 驱动器 1 忙 */
#define MSR_BUSY        0x10   /* FDC 命令忙 */
#define MSR_NON_DMA     0x20   /* 非 DMA 模式 */
#define MSR_DIO         0x40   /* 数据方向: 1=从 FDC 读, 0=向 FDC 写 */
#define MSR_RQM         0x80   /* 请求主机: FIFO 就绪 */

/* FDC 命令 */
#define FDC_CMD_READ_TRACK      0x02
#define FDC_CMD_SPECIFY         0x03
#define FDC_CMD_SENSE_DRIVE     0x04
#define FDC_CMD_WRITE_DATA      0x05
#define FDC_CMD_READ_DATA       0x06
#define FDC_CMD_RECALIBRATE     0x07
#define FDC_CMD_SENSE_INT       0x08
#define FDC_CMD_WRITE_DEL_DATA  0x09
#define FDC_CMD_READ_ID         0x0A
#define FDC_CMD_READ_DEL_DATA   0x0C
#define FDC_CMD_FORMAT_TRACK    0x0D
#define FDC_CMD_SEEK            0x0F

/* DMA 端口 (8237 控制器) */
#define DMA_MASK        0x0A   /* 通道掩码寄存器 */
#define DMA_MODE        0x0B   /* 模式寄存器 */
#define DMA_CLEAR_FF    0x0C   /* 清除触发器 */
#define DMA_PAGE_CH2    0x81   /* 通道 2 页寄存器 */
#define DMA_ADDR_CH2    0x04   /* 通道 2 地址寄存器 */
#define DMA_COUNT_CH2   0x05   /* 通道 2 计数寄存器 */

/* DMA 模式字节 */
#define DMA_MODE_SINGLE  0x40  /* 单次传输模式 */
#define DMA_MODE_READ    0x44  /* 读 (设备→内存) */
#define DMA_MODE_WRITE   0x48  /* 写 (内存→设备) */
#define DMA_MODE_AUTO    0x10  /* 自动初始化 */

/* DIR 寄存器位 (Digital Input Register, 读 0x3F7) */
#define DIR_DISK_CHANGED  0x80  /* 磁盘已更换 (Change Line) */
#define DIR_WRITE_PROTECT 0x40  /* 写保护 */

/* 马达状态 */
enum fdc_motor_state {
    MOTOR_OFF,       /* 马达关闭 */
    MOTOR_STARTING,  /* 马达启动中 (等待旋转稳定) */
    MOTOR_ON,        /* 马达运行中 */
    MOTOR_STOPPING   /* 马达关闭中 (延迟关闭) */
};

/* 马达时间参数 */
#define MOTOR_SPINUP_MS     300   /* 旋转稳定时间 (ms) */
#define MOTOR_OFF_DELAY_MS  2000  /* 无操作后自动关闭 (ms) */

/* 软盘介质类型 */
enum fdc_media_type {
    FDC_MEDIA_1440K,  /* 80 cyl, 2 heads, 18 SPT — 1.44MB 3.5" */
    FDC_MEDIA_1200K,  /* 80 cyl, 2 heads, 15 SPT — 1.2MB 5.25" */
    FDC_MEDIA_720K    /* 80 cyl, 2 heads,  9 SPT — 720KB 3.5" */
};

/* 软盘几何参数结构 */
struct fdc_geometry {
    uint8_t cylinders;
    uint8_t heads;
    uint8_t spt;           /* Sectors Per Track */
    uint16_t sector_size;
    uint32_t total_sectors;
    enum fdc_media_type media;
};

/* 1.44MB 软盘几何参数 (默认) */
#define FDC_CYLINDERS   80
#define FDC_HEADS       2
#define FDC_SPT         18     /* Sectors Per Track */
#define FDC_SECTOR_SIZE 512

/* IRQ 号和中断向量 */
#define FDC_IRQ         6
#define FDC_INT_VECTOR  0x26   /* IRQ6 重映射后的中断向量 */

/*
 * fdc_init — 初始化软盘控制器
 * 复位 FDC, 检测驱动器, 注册 IRQ 6, 设置 DMA。
 * 返回: true = FDC 就绪, false = 初始化失败。
 */
bool fdc_init(void);

/*
 * fdc_read_sectors — 从软盘读取扇区
 * @drive:    驱动器号 (0 或 1)
 * @head:     磁头号 (0 或 1)
 * @cylinder: 柱面号 (0-79)
 * @sector:   扇区号 (1-18, 注意从 1 开始)
 * @count:    扇区数
 * @buf:      目标缓冲区 (至少 count * 512 字节)
 * 返回: true = 成功, false = 失败。
 */
bool fdc_read_sectors(uint8_t drive, uint8_t head, uint8_t cylinder,
                      uint8_t sector, uint8_t count, void *buf);

/*
 * fdc_write_sectors — 向软盘写入扇区
 * @drive:    驱动器号 (0 或 1)
 * @head:     磁头号 (0 或 1)
 * @cylinder: 柱面号 (0-79)
 * @sector:   扇区号 (1-18)
 * @count:    扇区数
 * @buf:      源数据缓冲区
 * 返回: true = 成功, false = 失败。
 */
bool fdc_write_sectors(uint8_t drive, uint8_t head, uint8_t cylinder,
                       uint8_t sector, uint8_t count, const void *buf);

/*
 * fdc_get_geometry — 获取软盘几何参数
 * @drive: 驱动器号
 * @heads: 输出磁头数
 * @spt:   输出每磁道扇区数
 * 返回: true = 成功。
 */
bool fdc_get_geometry(uint8_t drive, uint8_t *heads, uint8_t *spt);

/*
 * fdc_read_lba — 以 LBA 方式从软盘读取扇区
 * @drive: 驱动器号
 * @lba:   逻辑块地址
 * @count: 扇区数
 * @buf:   目标缓冲区
 * 返回: true = 成功。
 *
 * 将 LBA 转换为 CHS 并调用 fdc_read_sectors()。
 */
bool fdc_read_lba(uint8_t drive, uint32_t lba, uint8_t count, void *buf);

/*
 * fdc_detect_disk — 检测驱动器中是否有磁盘
 * @drive: 驱动器号
 * 返回: true = 有磁盘, false = 无磁盘。
 *
 * 使用 READ ID 命令检测。
 */
bool fdc_detect_disk(uint8_t drive);

/*
 * fdc_disk_changed — 检测磁盘是否已更换
 * @drive: 驱动器号
 * 返回: true = 磁盘已更换, false = 未更换。
 *
 * 读取 DIR 寄存器的 Change Line 位 (bit 7)。
 */
bool fdc_disk_changed(uint8_t drive);

/*
 * fdc_write_protected — 检测磁盘是否写保护
 * @drive: 驱动器号
 * 返回: true = 写保护, false = 可写。
 *
 * 读取 DIR 寄存器的 Write Protect 位 (bit 6)。
 */
bool fdc_write_protected(uint8_t drive);

/*
 * fdc_motor_tick — 马达定时器递减
 *
 * 由定时器中断或主循环定期调用。
 * 当计时器到期时自动关闭马达。
 */
void fdc_motor_tick(void);

#ifdef __cplusplus
}
#endif

#endif /* _PLXSDOS_FDC_H */
