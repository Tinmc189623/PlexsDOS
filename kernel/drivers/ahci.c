/*
 * Nexsteaduser — PlexsDOS
 * AHCI SATA 驱动 — 通过 PCI BAR5 (ABAR) 访问 Q35/QEMU AHCI 控制器
 * 作者: Tinmc189623 | 团队: Nexlyh
 *
 * 支持 48-bit LBA DMA Read/Write, 通过 HBA 内存映射寄存器控制。
 * 使用 page_map 将 ABAR 物理地址映射到内核虚拟地址空间。
 */

#include <plexsdos/types.h>
#include <plexsdos/ahci.h>
#include <plexsdos/pci.h>
#include <plexsdos/serial.h>
#include <plexsdos/paging.h>

/* ===== ABAR 映射地址 ===== */
#define AHCI_ABAR_VADDR     0xFFC00000  /* 映射 ABAR 到此虚拟地址 */
#define AHCI_ABAR_PAGES     4           /* 映射 16KB (AHCI 寄存器空间) */

/* ===== AHCI 全局寄存器偏移 (相对于 ABAR) ===== */
#define AHCI_CAP            0x00        /* 能力寄存器 */
#define AHCI_GHC            0x04        /* 全局主机控制 */
#define AHCI_IS             0x08        /* 中断状态 */
#define AHCI_PI             0x0C        /* 端口已实现 */
#define AHCI_VER            0x10        /* 版本 */
#define AHCI_CAP2           0x24        /* 能力寄存器 2 */

/* GHC 位 */
#define GHC_AE              (1 << 31)   /* AHCI 使能 */
#define GHC_IE              (1 << 1)    /* 中断使能 */
#define GHC_HR              (1 << 0)    /* HBA 复位 */

/* CAP 位 */
#define CAP_NP_SHIFT        0           /* Number of Ports (0-4) */
#define CAP_NP_MASK         0x1F
#define CAP_S64A            (1 << 31)   /* 64-bit addressing */

/* ===== 端口寄存器偏移 (port * 0x80 + 0x100) ===== */
#define PORT_PxCLB          0x00        /* 命令列表基址 (低 32) */
#define PORT_PxCLBU         0x04        /* 命令列表基址 (高 32) */
#define PORT_PxFB           0x08        /* FIS 基址 (低 32) */
#define PORT_PxFBU          0x0C        /* FIS 基址 (高 32) */
#define PORT_PxIS           0x10        /* 中断状态 */
#define PORT_PxIE           0x14        /* 中断使能 */
#define PORT_PxCMD          0x18        /* 命令与状态 */
#define PORT_PxTFD          0x1C        /* 任务文件数据 */
#define PORT_PxSIG          0x20        /* 签名 */
#define PORT_PxSSTS         0x24        /* SATA 状态 */
#define PORT_PxSCTL         0x28        /* SATA 控制 */
#define PORT_PxSERR         0x2C        /* SATA 错误 */
#define PORT_PxCI           0x38        /* 命令发布 */

/* PxCMD 位 */
#define PxCMD_ST            (1 << 0)    /* 启动端口 */
#define PxCMD_SUD           (1 << 1)    /* 旋转加速 */
#define PxCMD_POD           (1 << 2)    /* 加电 */
#define PxCMD_FRE           (1 << 4)    /* FIS 接收使能 */
#define PxCMD_CR            (1 << 15)   /* 命令正在运行 */
#define PxCMD_FR            (1 << 14)   /* FIS 正在接收 */
#define PxCMD_CCS_SHIFT     16          /* 当前命令槽 */
#define PxCMD_CCS_MASK      0x1F

/* PxSSTS 位 */
#define SSTS_DET_SHIFT      0
#define SSTS_DET_MASK       0x0F        /* 设备检测 */
#define   DET_NODEVICE      0x00        /* 无设备 */
#define   DET_PRESENT       0x03        /* 设备在线 */
#define   DET_OFFLINE       0x01        /* 设备离线 */

/* PxSIG 签名 — 用于识别设备类型 */
#define SIG_SATA            0x00000101  /* SATA 驱动器 */
#define SIG_ATAPI           0xEB140101  /* SATAPI (光驱) */
#define SIG_PM              0x96690101  /* Port Multiplier */
#define SIG_SEMB            0xC33C0101  /* SEMB */

/* PxTFD 位 */
#define TFD_BSY             (1 << 7)    /* 忙 */
#define TFD_DRQ             (1 << 3)    /* 数据请求 */
#define TFD_ERR             (1 << 0)    /* 错误 */

/* ===== FIS (Frame Information Structure) 类型 ===== */
#define FIS_TYPE_H2D        0x27        /* Host to Device Register FIS */
#define FIS_TYPE_D2H        0x34        /* Device to Host Register FIS */
#define FIS_TYPE_PIO        0x5F        /* PIO Setup FIS */
#define FIS_TYPE_DMA_SETUP  0x41        /* DMA Setup FIS */

/* ===== 命令头结构 (32 字节) ===== */
struct ahci_cmd_header {
    uint16_t flags;         /* DW0 低 16 位: PMP, C, B, R, P, Write */
    uint16_t prdtl;         /* DW0 高 16 位: PRDT 条目数 */
    uint32_t prdbc;         /* DW1: 已传输字节数 (HBA 填充) */
    uint32_t ctba;          /* DW2: 命令表基址低 32 位 */
    uint32_t ctbau;         /* DW3: 命令表基址高 32 位 */
    uint32_t reserved[4];   /* DW4-DW7: 保留 */
} __attribute__((packed));

/* 命令头 DW0 标志 */
#define AHCI_CMD_PMP_SHIFT  0
#define AHCI_CMD_WRITE      (1 << 11)   /* 0=读(H2D), 1=写(D2H) */
#define AHCI_CMD_PREFETCH   (1 << 8)
#define AHCI_CMD_RESET      (1 << 7)
#define AHCI_CMD_BIST       (1 << 6)
#define AHCI_CMD_CLEAR_BUSY (1 << 5)
#define AHCI_CMD_ATAPI      (1 << 4)    /* ATAPI 命令 (需要 ATAPI 字节) */

/* ===== 命令表结构 ===== */
struct ahci_cmd_table {
    uint8_t  cfis[64];      /* 命令 FIS (64 字节) */
    uint8_t  acmd[16];      /* ATAPI 命令 (可选) */
    uint8_t  reserved[32];  /* 保留 */
    /* PRDT 条目紧随其后 (每个 16 字节) */
} __attribute__((packed));

/* ===== PRDT 条目 (16 字节) ===== */
struct ahci_prdt_entry {
    uint32_t dba;           /* 数据基址低 32 位 */
    uint32_t dbau;          /* 数据基址高 32 位 */
    uint32_t reserved;
    uint32_t dbc;           /* 字节数 (bit 23 = I, bit 31 = reserved) */
} __attribute__((packed));

#define PRDT_IOC            (1 << 23)   /* 传输完成时中断 */

/* Host to Device Register FIS (5 DW) */
struct fis_h2d {
    uint8_t  fis_type;      /* 0x27 */
    uint8_t  flags;         /* PMP+Update+Write */
    uint8_t  command;       /* ATA 命令 */
    uint8_t  features_low;

    uint8_t  lba0;          /* LBA 7:0 */
    uint8_t  lba1;          /* LBA 15:8 */
    uint8_t  lba2;          /* LBA 23:16 */
    uint8_t  device_head;   /* 设备/磁头 (LBA 模式 bit 6=1) */

    uint8_t  lba3;          /* LBA 31:24 */
    uint8_t  lba4;          /* LBA 39:32 */
    uint8_t  lba5;          /* LBA 47:40 */
    uint8_t  features_high;

    uint16_t sector_count;  /* 扇区数 (低字节在前) */
    uint8_t  reserved;
    uint8_t  control;

    uint32_t reserved2[4];
} __attribute__((packed));

/* ===== AHCI 端口状态 ===== */
#define AHCI_CMD_SLOTS      8           /* 命令槽数 */
#define AHCI_MAX_PORTS      32
#define AHCI_MAX_DEVICES    8

/* 每个端口的 DMA 内存 (静态分配, 物理连续) */
struct ahci_port_mem {
    struct ahci_cmd_header  cmd_list[AHCI_CMD_SLOTS] __attribute__((aligned(1024)));
    /* 命令表: 每个槽 256 字节 (64 CFIS + 192 PRDT) */
    uint8_t                 cmd_table[AHCI_CMD_SLOTS][256] __attribute__((aligned(256)));
    /* 接收 FIS: 256 字节, 256 对齐 */
    uint8_t                 rx_fis[256] __attribute__((aligned(256)));
};

static struct ahci_port_mem port_mem[AHCI_MAX_PORTS];

/* 端口设备信息 */
static struct {
    int          port;       /* AHCI 端口号 */
    int          present;    /* 是否在线 */
    int          dev_type;   /* AHCI_DEV_* */
    uint64_t     total_sectors; /* IDENTIFY 数据中获取 */
} ahci_devices[AHCI_MAX_DEVICES];

static int ahci_device_count_val = 0;

/* ABAR 虚拟地址 (由 ahci_init 通过 page_map 建立映射) */
static volatile uint32_t *ahci_abar = NULL;

/* ===== MMIO 读写辅助 (32-bit) ===== */
static inline uint32_t ahci_read(uint32_t reg)
{
    return ahci_abar[reg / 4];
}

static inline void ahci_write(uint32_t reg, uint32_t val)
{
    ahci_abar[reg / 4] = val;
}

static inline uint32_t port_read(int port, uint32_t reg)
{
    uint32_t off = 0x100 + port * 0x80 + reg;
    return ahci_abar[off / 4];
}

static inline void port_write(int port, uint32_t reg, uint32_t val)
{
    uint32_t off = 0x100 + port * 0x80 + reg;
    ahci_abar[off / 4] = val;
}

/* ===== 端口复位与初始化 ===== */

/*
 * ahci_port_stop — 停止端口
 * @port: 端口号
 *
 * 清除 PxCMD.ST 和 PxCMD.FRE, 等待 CR 和 FR 清零。
 */
static void ahci_port_stop(int port)
{
    uint32_t cmd = port_read(port, PORT_PxCMD);

    /* 清除 ST 和 FRE */
    cmd &= ~PxCMD_ST;
    cmd &= ~PxCMD_FRE;
    port_write(port, PORT_PxCMD, cmd);

    /* 等待命令和 FIS 接收停止 */
    int timeout = 100000;
    while (timeout--) {
        cmd = port_read(port, PORT_PxCMD);
        if (!(cmd & (PxCMD_CR | PxCMD_FR)))
            return;
    }
    serial_puts("[ahci] port stop timeout\n");
}

/*
 * ahci_port_start — 启动端口
 * @port: 端口号
 *
 * 设置 PxCMD.ST 和 PxCMD.FUD/POD/SUD。
 */
static void ahci_port_start(int port)
{
    uint32_t cmd = port_read(port, PORT_PxCMD);

    /* 设置标准标志 */
    cmd |= PxCMD_ST | PxCMD_SUD | PxCMD_POD | PxCMD_FRE;
    port_write(port, PORT_PxCMD, cmd);
}

/*
 * ahci_port_init — 初始化单个 AHCI 端口
 * @port:  端口号
 * @pi:    端口已实现位图 (用于调试)
 * 返回: true = 端口就绪且连接设备。
 */
static bool ahci_port_init(int port)
{
    /* 检查设备是否存在 (SATA 状态) */
    uint32_t ssts = port_read(port, PORT_PxSSTS);
    if ((ssts & 0x0F) != DET_PRESENT)
        return false;

    /* 停止端口 */
    ahci_port_stop(port);

    /* 清除错误状态 */
    port_write(port, PORT_PxSERR, 0xFFFFFFFF);

    /* 设置命令列表物理地址 */
    uint32_t clb_phys = (uint32_t)&port_mem[port].cmd_list[0];
    port_write(port, PORT_PxCLB, clb_phys);
    port_write(port, PORT_PxCLBU, 0);

    /* 设置 FIS 接收缓冲区物理地址 */
    uint32_t fb_phys = (uint32_t)&port_mem[port].rx_fis[0];
    port_write(port, PORT_PxFB, fb_phys);
    port_write(port, PORT_PxFBU, 0);

    /* 启动端口 */
    ahci_port_start(port);

    /* 读取签名识别设备类型 */
    uint32_t sig = port_read(port, PORT_PxSIG);
    int dev_type = AHCI_DEV_NONE;
    switch (sig) {
    case SIG_SATA:  dev_type = AHCI_DEV_SATA;  break;
    case SIG_ATAPI: dev_type = AHCI_DEV_SATAPI; break;
    case SIG_PM:    dev_type = AHCI_DEV_PM;    break;
    case SIG_SEMB:  dev_type = AHCI_DEV_SEMB;  break;
    default:
        if ((sig & 0xFFFF) == 0x0000)
            dev_type = AHCI_DEV_SATA;  /* 部分模拟器无严格签名 */
        else
            return false;
    }

    /* 记录设备 */
    if (ahci_device_count_val < AHCI_MAX_DEVICES) {
        ahci_devices[ahci_device_count_val].port = port;
        ahci_devices[ahci_device_count_val].present = 1;
        ahci_devices[ahci_device_count_val].dev_type = dev_type;
        ahci_devices[ahci_device_count_val].total_sectors = 0;
        ahci_device_count_val++;
    }

    serial_puts("[ahci] port ");
    serial_put_hex((uint32_t)port);
    serial_puts(": device type ");
    serial_put_hex((uint32_t)dev_type);
    serial_putchar('\n');

    return (dev_type == AHCI_DEV_SATA);
}

/* ===== 命令执行 ===== */

/*
 * ahci_cmd_send — 向指定端口发送 ATA 命令
 * @dev:      设备索引
 * @cmd:      ATA 命令码
 * @lba:      48-bit LBA 地址
 * @count:    扇区数 (1-255)
 * @buf:      数据缓冲区 (NULL 表示无数据传输)
 * @is_write: true = 写入, false = 读取
 * 返回: 0 = 成功, -1 = 失败。
 *
 * 使用端口第一个未使用的命令槽。
 * 构造 H2D Register FIS → 写入 CFIS → 设置 PRDT → 触发 PxCI。
 */
static int ahci_cmd_send(int dev, uint8_t cmd, uint64_t lba,
                          uint16_t count, void *buf, int is_write)
{
    if (dev < 0 || dev >= ahci_device_count_val)
        return -1;

    int port = ahci_devices[dev].port;
    int slot = 0;  /* 使用固定槽 0 */

    /* 等待命令槽可用 */
    uint32_t ci = port_read(port, PORT_PxCI);
    if (ci & (1 << slot)) {
        /* 槽正忙 — 等待 */
        int timeout = 1000000;
        while (timeout--) {
            ci = port_read(port, PORT_PxCI);
            if (!(ci & (1 << slot)))
                goto slot_free;
        }
        serial_puts("[ahci] slot busy timeout\n");
        return -1;
    }
slot_free:

    /* 获取命令头和命令表 */
    struct ahci_cmd_header *hdr = &port_mem[port].cmd_list[slot];
    struct ahci_cmd_table *tbl = (struct ahci_cmd_table *)&port_mem[port].cmd_table[slot];

    /* 清零命令头和命令表 */
    hdr->flags = 0;
    hdr->prdtl = 0;
    hdr->prdbc = 0;
    hdr->ctba = (uint32_t)tbl;
    hdr->ctbau = 0;

    for (int i = 0; i < 64; i++)
        tbl->cfis[i] = 0;
    for (int i = 0; i < 16; i++)
        tbl->acmd[i] = 0;

    /* 构造 H2D Register FIS */
    struct fis_h2d *fis = (struct fis_h2d *)tbl->cfis;
    fis->fis_type    = FIS_TYPE_H2D;
    fis->flags       = 0x80;           /* bit 7 = 1 (command), bit 0-3 = PMP */
    fis->command     = cmd;
    fis->features_low = 0;

    /* 48-bit LBA — 分 6 个字节填入 */
    fis->lba0        = (uint8_t)(lba & 0xFF);
    fis->lba1        = (uint8_t)((lba >> 8) & 0xFF);
    fis->lba2        = (uint8_t)((lba >> 16) & 0xFF);
    fis->device_head = 0x40 | (uint8_t)((lba >> 24) & 0x0F); /* LBA 模式 */

    fis->lba3        = (uint8_t)((lba >> 32) & 0xFF);
    fis->lba4        = (uint8_t)((lba >> 40) & 0xFF);
    fis->lba5        = 0;
    fis->features_high = 0;

    fis->sector_count = count;
    fis->reserved    = 0;
    fis->control     = 0;

    /* PRDT — 如果 buf 非空且有传输 */
    if (buf != NULL && count > 0) {
        uint32_t bytes = (uint32_t)count * 512;
        uint32_t phys_addr = (uint32_t)buf;

        struct ahci_prdt_entry *prdt = (struct ahci_prdt_entry *)tbl->reserved;
        prdt->dba = phys_addr;
        prdt->dbau = 0;
        prdt->reserved = 0;
        prdt->dbc = (bytes - 1) | PRDT_IOC;  /* 字节数 - 1 */

        hdr->prdtl = 1;
    }

    /* 设置方向标志 */
    if (is_write)
        hdr->flags |= AHCI_CMD_WRITE;

    __asm__ __volatile__("" : : : "memory");  /* 内存屏障 */

    /* 发布命令 */
    port_write(port, PORT_PxCI, (1 << slot));

    /* 等待命令完成 */
    int timeout = 2000000;
    while (timeout--) {
        ci = port_read(port, PORT_PxCI);
        if (!(ci & (1 << slot)))
            break;
    }
    if (timeout <= 0) {
        serial_puts("[ahci] cmd timeout\n");
        return -1;
    }

    /* 检查错误 */
    uint32_t tfd = port_read(port, PORT_PxTFD);
    if (tfd & TFD_ERR) {
        uint32_t serr = port_read(port, PORT_PxSERR);
        serial_puts("[ahci] cmd error: tfd=");
        serial_put_hex(tfd);
        serial_puts(" serr=");
        serial_put_hex(serr);
        serial_putchar('\n');
        port_write(port, PORT_PxSERR, serr);
        return -1;
    }

    return 0;
}

/* ===== ATA 命令 ===== */

#define ATA_CMD_IDENTIFY        0xEC
#define ATA_CMD_READ_DMA_EXT    0x25
#define ATA_CMD_WRITE_DMA_EXT   0x35

/* IDENTIFY DEVICE 数据中总扇区数的偏移 */
#define IDENTIFY_TOTAL_SECTORS  100     /* WORD 100-103: 48-bit LBA 总扇区数 */

/*
 * ahci_identify — 发送 IDENTIFY DEVICE 命令
 * @dev: 设备索引
 * @buf: 512 字节缓冲区
 * 返回: true = 成功。
 */
static bool ahci_identify(int dev, void *buf)
{
    if (ahci_cmd_send(dev, ATA_CMD_IDENTIFY, 0, 0, buf, 0) != 0)
        return false;

    /* 解析总扇区数 (48-bit LBA, WORD 100-103) */
    uint16_t *ident = (uint16_t *)buf;
    uint32_t sects_lo = ((uint32_t)ident[100]) | ((uint32_t)ident[101] << 16);
    uint32_t sects_hi = ((uint32_t)ident[102]) | ((uint32_t)ident[103] << 16);

    ahci_devices[dev].total_sectors = ((uint64_t)sects_hi << 32) | sects_lo;

    serial_puts("[ahci] dev ");
    serial_put_hex((uint32_t)dev);
    serial_puts(": sectors ");
    serial_put_hex((uint32_t)(ahci_devices[dev].total_sectors & 0xFFFFFFFF));
    serial_putchar('\n');

    return true;
}

/* ===== 公共 API ===== */

/*
 * ahci_read_sectors — 从 AHCI 磁盘读取扇区
 * @dev:   设备索引
 * @lba:   起始 48-bit LBA
 * @count: 扇区数
 * @buf:   目标缓冲区
 * 返回: true = 成功。
 */
bool ahci_read_sectors(int dev, uint64_t lba, uint8_t count, void *buf)
{
    return (ahci_cmd_send(dev, ATA_CMD_READ_DMA_EXT, lba, count, buf, 0) == 0);
}

/*
 * ahci_write_sectors — 向 AHCI 磁盘写入扇区
 * @dev:   设备索引
 * @lba:   起始 48-bit LBA
 * @count: 扇区数
 * @buf:   源数据缓冲区
 * 返回: true = 成功。
 */
bool ahci_write_sectors(int dev, uint64_t lba, uint8_t count, const void *buf)
{
    return (ahci_cmd_send(dev, ATA_CMD_WRITE_DMA_EXT, lba, count, (void *)buf, 1) == 0);
}

/*
 * ahci_device_count — 获取 AHCI 检测到的磁盘数
 */
int ahci_device_count(void)
{
    return ahci_device_count_val;
}

/* ===== 初始化 ===== */

/*
 * ahci_map_abar — 将 ABAR 物理地址映射到内核虚拟地址
 * @phys: ABAR 物理地址
 * 返回: true = 映射成功。
 */
static bool ahci_map_abar(uint32_t phys)
{
    uint32_t base = phys & 0xFFFFF000;    /* 页对齐基址 */
    uint32_t offset = phys & 0xFFF;       /* 页内偏移 */
    int i;

    /* 逐页映射 */
    for (i = 0; i < AHCI_ABAR_PAGES; i++) {
        uint32_t vaddr = AHCI_ABAR_VADDR + i * 0x1000;
        uint32_t paddr = base + i * 0x1000;
        if (page_map(vaddr, paddr, PAGE_PRESENT | PAGE_WRITABLE) != 0) {
            serial_puts("[ahci] page_map failed at page ");
            serial_put_hex((uint32_t)i);
            serial_putchar('\n');
            /* 回滚已映射的页, 防止泄漏 */
            for (int j = 0; j < i; j++)
                page_unmap_and_free(AHCI_ABAR_VADDR + j * 0x1000);
            return false;
        }
    }

    ahci_abar = (volatile uint32_t *)(AHCI_ABAR_VADDR + offset);
    return true;
}

/*
 * ahci_init — 初始化 AHCI 控制器
 *
 * 1. 通过 PCI 查找 AHCI 控制器 (class=0x01, subclass=0x06)
 * 2. 读取 BAR5 获取 ABAR 物理地址
 * 3. 映射 ABAR 到虚拟地址空间
 * 4. 启用 AHCI 模式 (GHC.AE=1)
 * 5. 枚举所有已实现端口, 初始化在线 SATA 设备
 * 6. 对每个 SATA 设备发送 IDENTIFY 获取容量
 */
bool ahci_init(void)
{
    struct pci_device *ahci_dev = NULL;

    /* 查找 AHCI 控制器 */
    int count = pci_device_count();
    for (int i = 0; i < count; i++) {
        struct pci_device *d = pci_get_device(i);
        if (d && d->class_code == PCI_CLASS_MASS_STORAGE &&
            d->subclass == PCI_SUBCLASS_SATA) {
            ahci_dev = d;
            break;
        }
    }

    if (!ahci_dev) {
        serial_puts("[ahci] no controller found\n");
        return false;
    }

    serial_puts("[ahci] controller at ");
    serial_put_hex((uint32_t)ahci_dev->bus);
    serial_puts(":");
    serial_put_hex((uint32_t)ahci_dev->slot);
    serial_puts(".");
    serial_put_hex((uint32_t)ahci_dev->func);
    serial_putchar('\n');

    /* 启用总线主控 */
    pci_enable_bus_master(ahci_dev);

    /* 读取 BAR5 (ABAR 物理地址) — 偏移 0x24 */
    uint32_t bar5 = pci_read_config_dword(ahci_dev->bus, ahci_dev->slot,
                                           ahci_dev->func, 0x24);
    uint32_t abar_phys = bar5 & 0xFFFFFFF0;  /* 32-bit memory BAR 掩码 */
    if (abar_phys == 0) {
        serial_puts("[ahci] ABAR is zero\n");
        return false;
    }

    serial_puts("[ahci] ABAR phys: ");
    serial_put_hex(abar_phys);
    serial_putchar('\n');

    /* 映射 ABAR 到虚拟地址 */
    if (!ahci_map_abar(abar_phys)) {
        serial_puts("[ahci] ABAR mapping failed\n");
        return false;
    }

    /* 启用 AHCI 模式 */
    uint32_t ghc = ahci_read(AHCI_GHC);
    ghc |= GHC_AE;
    ahci_write(AHCI_GHC, ghc);

    /* 读取版本 */
    uint32_t ver = ahci_read(AHCI_VER);
    serial_puts("[ahci] version ");
    serial_put_hex(ver);
    serial_putchar('\n');

    /* 枚举已实现端口 */
    uint32_t pi = ahci_read(AHCI_PI);
    ahci_device_count_val = 0;

    serial_puts("[ahci] ports implemented: ");
    serial_put_hex(pi);
    serial_putchar('\n');

    /* 为所有端口初始化 DMA 结构 (清零) */
    for (int p = 0; p < AHCI_MAX_PORTS; p++) {
        if (!(pi & (1 << p)))
            continue;

        /* 清零端口内存 */
        for (int i = 0; i < AHCI_CMD_SLOTS; i++) {
            int off = 0;
            for (off = 0; off < (int)sizeof(struct ahci_cmd_header); off++)
                ((uint8_t *)&port_mem[p].cmd_list[i])[off] = 0;
            for (off = 0; off < 256; off++)
                port_mem[p].cmd_table[i][off] = 0;
        }
        for (int i = 0; i < 256; i++)
            port_mem[p].rx_fis[i] = 0;

        /* 初始化端口 */
        if (!ahci_port_init(p))
            continue;

        /* 如果是 SATA 设备, 发送 IDENTIFY */
        int dev_idx = ahci_device_count_val - 1;
        if (dev_idx >= 0 && ahci_devices[dev_idx].dev_type == AHCI_DEV_SATA) {
            uint8_t ident_buf[512] __attribute__((aligned(4)));
            if (ahci_identify(dev_idx, ident_buf)) {
                serial_puts("[ahci] SATA device ready on port ");
                serial_put_hex((uint32_t)p);
                serial_putchar('\n');
            }
        }
    }

    if (ahci_device_count_val == 0) {
        serial_puts("[ahci] no SATA devices found\n");
        return false;
    }

    serial_puts("[ahci] initialization complete\n");
    return true;
}
