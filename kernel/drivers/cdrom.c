/*
 * Nexsteaduser — PlexsDOS
 * ATAPI CD-ROM 驱动 + ISO 9660 文件系统
 * 作者: Tinmc189623 | 团队: Nexlyh
 *
 * ATAPI PACKET 命令协议实现:
 * - 通过 ATA 主通道 (0x1F0-0x1F7) 发送 ATAPI 命令
 * - 支持 PIO 模式数据传输
 * - SCSI 命令集: INQUIRY, READ(10), READ CAPACITY, TEST UNIT READY
 *
 * ISO 9660 文件系统 (只读):
 * - 主卷描述符解析 (LBA 16)
 * - 目录遍历
 * - 文件读取
 *
 * 参考:
 * - ATA/ATAPI-6 规范 (T13 1410D)
 * - ECMA-119 / ISO 9660 标准
 * - Linux drivers/ide/cdrom.c
 */

#include <plexsdos/types.h>
#include <plexsdos/cdrom.h>
#include <plexsdos/disk.h>
#include <plexsdos/screen.h>
#include <plexsdos/interrupt.h>

/* ==================== 端口 I/O (复用 disk.h 中的定义) ==================== */

/* 从 I/O 端口读取一个字节 */
static inline uint8_t inb(uint16_t port)
{
    uint8_t val;
    __asm__ __volatile__("inb %1, %0" : "=a"(val) : "Nd"(port));
    return val;
}

/* 向 I/O 端口写入一个字节 */
static inline void outb(uint16_t port, uint8_t val)
{
    __asm__ __volatile__("outb %0, %1" : : "a"(val), "Nd"(port));
}

/* 从 I/O 端口读取一个字 (16-bit) */
static inline uint16_t inw(uint16_t port)
{
    uint16_t val;
    __asm__ __volatile__("inw %1, %0" : "=a"(val) : "Nd"(port));
    return val;
}

/* 向 I/O 端口写入一个字 */
static inline void outw(uint16_t port, uint16_t val)
{
    __asm__ __volatile__("outw %0, %1" : : "a"(val), "Nd"(port));
}

/* ==================== 延时 ==================== */

/*
 * cdrom_delay_400ns — ATAPI 规范要求的 400ns 延时
 * 通过读取备用状态寄存器 4 次实现 (每次约 100ns)。
 */
static void cdrom_delay_400ns(void)
{
    inb(ATA_ALT_STATUS);
    inb(ATA_ALT_STATUS);
    inb(ATA_ALT_STATUS);
    inb(ATA_ALT_STATUS);
}

/* ==================== 设备状态 ==================== */

/* CD-ROM 设备信息 */
static struct cdrom_info cdrom_dev;

/* ISO 9660 文件系统上下文 */
static struct iso9660_fs cdrom_fs;

/* ISO 9660 根目录条目 */
static struct iso9660_entry cdrom_root;

/* ATAPI PACKET 命令缓冲区 (12 字节) */
static uint8_t atapi_packet[12];

/* PIO 数据接收缓冲区 (一个 CD-ROM 扇区 = 2048 字节) */
static uint8_t cdrom_sector_buf[CDROM_SECTOR_SIZE];

/* ==================== ATAPI 等待函数 ==================== */

/*
 * cdrom_wait_ready — 等待 ATAPI 设备就绪
 * 返回: true = 设备就绪, false = 超时。
 */
static bool cdrom_wait_ready(void)
{
    int timeout = 10000;
    while (timeout-- > 0) {
        uint8_t status = inb(ATA_STATUS);
        if (!(status & ATA_STATUS_BSY)) {
            if (status & ATA_STATUS_RDY)
                return true;
            if (status & ATA_STATUS_ERR)
                return false;
        }
        cdrom_delay_400ns();
    }
    return false;
}

/*
 * cdrom_wait_drq — 等待数据请求 (DRQ)
 * 返回: true = DRQ 已设置, false = 超时或错误。
 */
static bool cdrom_wait_drq(void)
{
    int timeout = 10000;
    while (timeout-- > 0) {
        uint8_t status = inb(ATA_STATUS);
        if (status & ATA_STATUS_ERR)
            return false;
        if (status & ATA_STATUS_DF)
            return false;
        if (status & ATA_STATUS_DRQ)
            return true;
        cdrom_delay_400ns();
    }
    return false;
}

/*
 * cdrom_wait_irq — 等待 ATAPI 中断
 *
 * ATAPI 设备在完成命令后触发 IRQ。
 * 通过轮询备用状态寄存器的 BSY 位判断完成。
 */
static bool cdrom_wait_irq(uint32_t timeout_ms)
{
    /* 轮询方式等待 (无专用 ATAPI IRQ 处理) */
    while (timeout_ms-- > 0) {
        uint8_t status = inb(ATA_ALT_STATUS);
        if (!(status & ATA_STATUS_BSY)) {
            return true;
        }
        /* 约 1ms 延时 */
        volatile uint32_t count = 1000;
        while (count-- > 0) {
            inb(0x80);
        }
    }
    return false;
}

/* ==================== ATAPI 命令接口 ==================== */

/*
 * atapi_send_packet — 发送 ATAPI PACKET 命令
 * @packet:   12 字节 SCSI CDB
 * @byte_count: 期望传输的字节数
 * @dma:      是否使用 DMA 模式
 * 返回: true = 命令已发送, 设备进入数据阶段。
 */
static bool atapi_send_packet(const uint8_t *packet, uint16_t byte_count,
                              bool dma)
{
    /* 等待设备就绪 */
    if (!cdrom_wait_ready()) {
        return false;
    }

    /* 选择驱动器 (主盘) */
    outb(ATA_DRIVE_HEAD, 0xA0);  /* LBA 模式, 主盘 */
    cdrom_delay_400ns();

    /* 设置特征寄存器 (PIO 或 DMA) */
    outb(ATA_ERROR, dma ? ATAPI_FEATURE_DMA : ATAPI_FEATURE_PIO);

    /* 设置传输字节数 (LBA_MID/LBA_HI 用于 ATAPI 字节计数) */
    outb(ATA_LBA_MID, (uint8_t)(byte_count & 0xFF));
    outb(ATA_LBA_HI, (uint8_t)(byte_count >> 8));

    /* 发送 PACKET 命令 */
    outb(ATA_COMMAND, ATAPI_CMD_PACKET);
    cdrom_delay_400ns();

    /* 等待 DRQ — 设备请求 CDB */
    if (!cdrom_wait_drq()) {
        return false;
    }

    /* 发送 12 字节 CDB (以字为单位写入数据端口) */
    for (int i = 0; i < 6; i++) {
        uint16_t word = (uint16_t)packet[i * 2] |
                        ((uint16_t)packet[i * 2 + 1] << 8);
        outw(ATA_DATA, word);
    }

    return true;
}

/*
 * atapi_read_data — 从 ATAPI 设备读取数据 (PIO 模式)
 * @buf:       目标缓冲区
 * @byte_count: 期望读取的字节数
 * 返回: 实际读取的字节数。
 */
static uint32_t atapi_read_data(void *buf, uint32_t byte_count)
{
    uint8_t *dst = (uint8_t *)buf;
    uint32_t total_read = 0;

    while (total_read < byte_count) {
        /* 等待 DRQ */
        if (!cdrom_wait_drq()) {
            break;
        }

        /* 读取设备报告的传输大小 */
        uint16_t transfer_size = inb(ATA_LBA_MID) |
                                 ((uint16_t)inb(ATA_LBA_HI) << 8);

        if (transfer_size == 0)
            break;

        /* 确保不超过请求量 */
        if (total_read + transfer_size > byte_count)
            transfer_size = (uint16_t)(byte_count - total_read);

        /* 以字为单位从数据端口读取 */
        uint16_t words = (transfer_size + 1) / 2;
        uint16_t *dst16 = (uint16_t *)(dst + total_read);
        for (uint16_t i = 0; i < words; i++) {
            dst16[i] = inw(ATA_DATA);
        }

        total_read += transfer_size;
    }

    return total_read;
}

/* ==================== SCSI 命令封装 ==================== */

/*
 * scsi_test_unit_ready — 检查光盘是否就绪
 * 返回: true = 就绪, false = 未就绪。
 */
static bool scsi_test_unit_ready(void)
{
    for (int i = 0; i < 20; i++) {
        /* 构造 TEST UNIT READY CDB */
        for (int j = 0; j < 12; j++)
            atapi_packet[j] = 0;
        atapi_packet[0] = SCSI_CMD_TEST_UNIT_READY;

        if (!atapi_send_packet(atapi_packet, 0, false))
            return false;

        /* 检查状态 */
        cdrom_delay_400ns();
        uint8_t status = inb(ATA_STATUS);
        if (!(status & ATA_STATUS_ERR)) {
            return true;
        }

        /* 设备可能正在初始化, 等待后重试 */
        volatile uint32_t delay = 50000;
        while (delay-- > 0) {
            inb(0x80);
        }
    }

    return false;
}

/*
 * scsi_inquiry — 获取设备信息
 * 返回: true = 成功。
 */
static bool scsi_inquiry(void)
{
    uint8_t inquiry_data[36];

    /* 构造 INQUIRY CDB */
    for (int j = 0; j < 12; j++)
        atapi_packet[j] = 0;
    atapi_packet[0] = SCSI_CMD_INQUIRY;
    atapi_packet[3] = 0x00;  /* EVPD = 0 */
    atapi_packet[4] = 36;    /* 分配长度 */

    if (!atapi_send_packet(atapi_packet, 36, false))
        return false;

    uint32_t read = atapi_read_data(inquiry_data, 36);
    if (read < 36)
        return false;

    /* 解析设备类型 */
    cdrom_dev.type = (enum cdrom_device_type)(inquiry_data[0] & 0x1F);

    /* 解析厂商信息 */
    for (int i = 0; i < 8; i++)
        cdrom_dev.vendor[i] = inquiry_data[8 + i];
    cdrom_dev.vendor[8] = '\0';

    for (int i = 0; i < 16; i++)
        cdrom_dev.product[i] = inquiry_data[16 + i];
    cdrom_dev.product[16] = '\0';

    for (int i = 0; i < 4; i++)
        cdrom_dev.revision[i] = inquiry_data[32 + i];
    cdrom_dev.revision[4] = '\0';

    return true;
}

/*
 * scsi_read_capacity — 读取光盘容量
 * 返回: true = 成功。
 */
static bool scsi_read_capacity(void)
{
    uint8_t cap_data[8];

    /* 构造 READ CAPACITY CDB */
    for (int j = 0; j < 12; j++)
        atapi_packet[j] = 0;
    atapi_packet[0] = SCSI_CMD_READ_CAPACITY;

    if (!atapi_send_packet(atapi_packet, 8, false))
        return false;

    uint32_t read = atapi_read_data(cap_data, 8);
    if (read < 8)
        return false;

    /* 最后一个 LBA (大端序) */
    cdrom_dev.total_sectors = ((uint32_t)cap_data[0] << 24) |
                             ((uint32_t)cap_data[1] << 16) |
                             ((uint32_t)cap_data[2] << 8) |
                             (uint32_t)cap_data[3];

    /* 扇区大小 (大端序, 通常 2048) */
    cdrom_dev.sector_size = ((uint16_t)cap_data[6] << 8) |
                            (uint16_t)cap_data[7];

    return true;
}

/*
 * scsi_read_10 — 读取 CD-ROM 扇区 (SCSI READ(10) 命令)
 * @lba:   起始 LBA
 * @count: 扇区数
 * @buf:   目标缓冲区
 * 返回: true = 成功。
 */
static bool scsi_read_10(uint32_t lba, uint16_t count, void *buf)
{
    uint8_t *dst = (uint8_t *)buf;
    uint32_t total_bytes = (uint32_t)count * CDROM_SECTOR_SIZE;

    /* 构造 READ(10) CDB */
    for (int j = 0; j < 12; j++)
        atapi_packet[j] = 0;
    atapi_packet[0] = SCSI_CMD_READ_10;
    atapi_packet[2] = (uint8_t)(lba >> 24);  /* LBA 大端序 */
    atapi_packet[3] = (uint8_t)(lba >> 16);
    atapi_packet[4] = (uint8_t)(lba >> 8);
    atapi_packet[5] = (uint8_t)(lba);
    atapi_packet[7] = (uint8_t)(count >> 8);  /* 传输长度大端序 */
    atapi_packet[8] = (uint8_t)(count);

    if (!atapi_send_packet(atapi_packet, (uint16_t)total_bytes, false))
        return false;

    /* 读取数据 */
    uint32_t read = atapi_read_data(dst, total_bytes);
    if (read < total_bytes) {
        return false;
    }

    return true;
}

/*
 * scsi_request_sense — 获取请求检测信息 (错误报告)
 * @sense_key:  输出检测键
 * @asc:        附加检测码
 * @ascq:       附加检测码限定符
 * 返回: true = 成功。
 */
static bool scsi_request_sense(uint8_t *sense_key, uint8_t *asc,
                               uint8_t *ascq)
{
    uint8_t sense_data[18];

    /* 构造 REQUEST SENSE CDB */
    for (int j = 0; j < 12; j++)
        atapi_packet[j] = 0;
    atapi_packet[0] = SCSI_CMD_REQUEST_SENSE;
    atapi_packet[4] = 18;  /* 分配长度 */

    if (!atapi_send_packet(atapi_packet, 18, false))
        return false;

    uint32_t read = atapi_read_data(sense_data, 18);
    if (read < 18)
        return false;

    if (sense_key) *sense_key = sense_data[2] & 0x0F;
    if (asc)       *asc = sense_data[12];
    if (ascq)      *ascq = sense_data[13];

    return true;
}

/* ==================== ATAPI 设备检测 ==================== */

/*
 * cdrom_detect — 检测 ATAPI CD-ROM 设备
 *
 * 在 ATA 主通道上尝试 IDENTIFY PACKET DEVICE 命令。
 * 先尝试主盘, 再尝试从盘。
 * 返回: true = 检测到 ATAPI 设备。
 */
static bool cdrom_detect(void)
{
    uint16_t identify_data[256];

    /* 尝试主盘和从盘 */
    for (int slave = 0; slave <= 1; slave++) {
        uint8_t drive_head = slave ? 0xB0 : 0xA0;

        /* 选择驱动器 */
        outb(ATA_DRIVE_HEAD, drive_head);
        cdrom_delay_400ns();

        /* 清除寄存器 */
        outb(ATA_SECTOR_COUNT, 0);
        outb(ATA_LBA_LO, 0);
        outb(ATA_LBA_MID, 0);
        outb(ATA_LBA_HI, 0);

        /* 发送 IDENTIFY PACKET DEVICE 命令 */
        outb(ATA_COMMAND, ATAPI_CMD_IDENTIFY);
        cdrom_delay_400ns();

        /* 读取状态 */
        uint8_t status = inb(ATA_STATUS);
        if (status == 0x00) {
            /* 无设备 */
            continue;
        }

        /* 等待 BSY 清除 */
        int timeout = 1000;
        while (timeout-- > 0) {
            status = inb(ATA_STATUS);
            if (!(status & ATA_STATUS_BSY))
                break;
        }

        /* 检查 LBA_MID/LBA_HI 确认是 ATAPI 设备 */
        /* ATAPI 设备返回 LBA_MID=0x14, LBA_HI=0xEB */
        uint8_t mid = inb(ATA_LBA_MID);
        uint8_t hi = inb(ATA_LBA_HI);
        if (mid != 0x14 || hi != 0xEB) {
            continue;
        }

        /* 等待 DRQ */
        if (!cdrom_wait_drq()) {
            continue;
        }

        /* 读取 IDENTIFY 数据 */
        for (int i = 0; i < 256; i++) {
            identify_data[i] = inw(ATA_DATA);
        }

        cdrom_dev.present = true;
        return true;
    }

    return false;
}

/* ==================== 公共 API ==================== */

/*
 * cdrom_init — 初始化 ATAPI CD-ROM 驱动
 *
 * 1. 检测 ATAPI 设备 (IDENTIFY PACKET DEVICE)
 * 2. 发送 ATAPI RESET
 * 3. 获取设备信息 (INQUIRY)
 * 4. 检查光盘就绪 (TEST UNIT READY)
 * 5. 读取容量 (READ CAPACITY)
 *
 * 返回: true = CD-ROM 就绪, false = 未检测到。
 */
bool cdrom_init(void)
{
    /* 初始化状态 */
    cdrom_dev.present = false;
    cdrom_dev.type = CDROM_TYPE_NONE;
    cdrom_dev.sector_size = CDROM_SECTOR_SIZE;
    cdrom_dev.total_sectors = 0;
    cdrom_fs.mounted = false;

    screen_puts("[cdrom] detecting ATAPI device...\n");

    /* 检测 ATAPI 设备 */
    if (!cdrom_detect()) {
        screen_puts("[cdrom] no ATAPI device found\n");
        return false;
    }

    screen_puts("[cdrom] ATAPI device detected\n");

    /* 发送 ATAPI RESET */
    outb(ATA_DRIVE_HEAD, 0xA0);
    cdrom_delay_400ns();
    outb(ATA_COMMAND, ATAPI_CMD_RESET);
    cdrom_delay_400ns();

    /* 等待复位完成 */
    volatile uint32_t delay = 100000;
    while (delay-- > 0) {
        inb(0x80);
    }

    /* 获取设备信息 (INQUIRY) */
    if (!scsi_inquiry()) {
        screen_puts("[cdrom] INQUIRY failed\n");
        return false;
    }

    screen_puts("[cdrom] vendor:  ");
    screen_puts(cdrom_dev.vendor);
    screen_putchar('\n');
    screen_puts("[cdrom] product: ");
    screen_puts(cdrom_dev.product);
    screen_putchar('\n');
    screen_puts("[cdrom] version: ");
    screen_puts(cdrom_dev.revision);
    screen_putchar('\n');

    /* 检查光盘就绪 */
    screen_puts("[cdrom] checking media...\n");
    if (!scsi_test_unit_ready()) {
        screen_puts("[cdrom] no disc in drive\n");
        /* 设备存在但无光盘 — 返回 false 但保留设备信息 */
        return false;
    }

    /* 读取容量 */
    if (!scsi_read_capacity()) {
        screen_puts("[cdrom] READ CAPACITY failed\n");
        return false;
    }

    screen_puts("[cdrom] capacity: ");
    screen_put_dec(cdrom_dev.total_sectors);
    screen_puts(" sectors (");
    /* 计算 MB: total_sectors * 2048 / 1048576 */
    uint32_t size_mb = cdrom_dev.total_sectors / 512;
    screen_put_dec(size_mb);
    screen_puts(" MB)\n");

    screen_puts("[cdrom] CD-ROM ready\n");
    return true;
}

/*
 * cdrom_read_sector — 从 CD-ROM 读取一个扇区 (2048 字节)
 * @lba: 扇区 LBA 地址
 * @buf: 目标缓冲区 (至少 2048 字节)
 * 返回: true = 成功。
 */
bool cdrom_read_sector(uint32_t lba, void *buf)
{
    if (!cdrom_dev.present)
        return false;

    return scsi_read_10(lba, 1, buf);
}

/*
 * cdrom_read_sectors — 从 CD-ROM 读取多个扇区
 * @lba:   起始 LBA
 * @count: 扇区数
 * @buf:   目标缓冲区
 * 返回: true = 成功。
 *
 * ATAPI 单次传输有字节数限制 (通常 64KB),
 * 超过时分多次传输。
 */
bool cdrom_read_sectors(uint32_t lba, uint16_t count, void *buf)
{
    if (!cdrom_dev.present)
        return false;

    uint8_t *dst = (uint8_t *)buf;

    /* 分批读取 (每次最多 32 个扇区 = 64KB) */
    while (count > 0) {
        uint16_t batch = count > 32 ? 32 : count;

        if (!scsi_read_10(lba, batch, dst))
            return false;

        lba += batch;
        dst += (uint32_t)batch * CDROM_SECTOR_SIZE;
        count -= batch;
    }

    return true;
}

/*
 * cdrom_get_info — 获取 CD-ROM 设备信息
 * 返回: 指向 cdrom_info 结构的指针。
 */
const struct cdrom_info *cdrom_get_info(void)
{
    return &cdrom_dev;
}

/*
 * cdrom_eject — 弹出光盘托盘
 *
 * 发送 START/STOP UNIT 命令 (SCSI 0x1B):
 * START=0, LOEJ=1 → 弹出
 */
void cdrom_eject(void)
{
    if (!cdrom_dev.present)
        return;

    for (int j = 0; j < 12; j++)
        atapi_packet[j] = 0;
    atapi_packet[0] = 0x1B;  /* START/STOP UNIT */
    atapi_packet[4] = 0x02;  /* START=0, LOEJ=1 */

    atapi_send_packet(atapi_packet, 0, false);

    screen_puts("[cdrom] disc ejected\n");
}

/* ==================== ISO 9660 文件系统 ==================== */

/*
 * iso9660_mount — 挂载 ISO 9660 文件系统
 *
 * 读取主卷描述符 (LBA 16), 解析根目录记录。
 * ISO 9660 标准规定主卷描述符在 LBA 16 处。
 *
 * 返回: true = 挂载成功。
 */
bool iso9660_mount(void)
{
    if (!cdrom_dev.present) {
        screen_puts("[iso9660] no CD-ROM device\n");
        return false;
    }

    /* 读取 LBA 16 (主卷描述符) */
    if (!cdrom_read_sector(16, cdrom_sector_buf)) {
        screen_puts("[iso9660] failed to read PVD\n");
        return false;
    }

    struct iso9660_pvd *pvd = (struct iso9660_pvd *)cdrom_sector_buf;

    /* 验证卷描述符类型 */
    if (pvd->type != ISO9660_VD_PRIMARY) {
        screen_puts("[iso9660] invalid PVD type\n");
        return false;
    }

    /* 验证 "CD001" 标识 */
    if (pvd->id[0] != 'C' || pvd->id[1] != 'D' ||
        pvd->id[2] != '0' || pvd->id[3] != '0' ||
        pvd->id[4] != '1') {
        screen_puts("[iso9660] invalid PVD signature\n");
        return false;
    }

    /* 解析逻辑块大小 (小端序) */
    cdrom_fs.block_size = pvd->logical_block_size_le;
    if (cdrom_fs.block_size == 0)
        cdrom_fs.block_size = CDROM_SECTOR_SIZE;

    /* 解析卷空间大小 */
    cdrom_fs.total_sectors = pvd->volume_space_le;

    /* 解析卷标识 (去除尾部空格) */
    for (int i = 0; i < 32; i++)
        cdrom_fs.volume_id[i] = pvd->volume_id[i];
    cdrom_fs.volume_id[32] = '\0';

    /* 去除尾部空格 */
    for (int i = 31; i >= 0; i--) {
        if (cdrom_fs.volume_id[i] == ' ')
            cdrom_fs.volume_id[i] = '\0';
        else
            break;
    }

    /* 解析根目录记录 */
    /* 根目录记录在 PVD 偏移 0x9C 处, 长度 34 字节 */
    struct iso9660_dir_record *root_rec =
        (struct iso9660_dir_record *)pvd->root_dir_record;

    cdrom_fs.root_lba = root_rec->extent_le;
    cdrom_fs.root_size = root_rec->extent_size_le;

    /* 设置根目录条目 */
    cdrom_root.lba = cdrom_fs.root_lba;
    cdrom_root.size = cdrom_fs.root_size;
    cdrom_root.is_directory = true;
    cdrom_root.name[0] = '/';
    cdrom_root.name[1] = '\0';

    cdrom_fs.mounted = true;

    screen_puts("[iso9660] volume: ");
    screen_puts(cdrom_fs.volume_id);
    screen_putchar('\n');
    screen_puts("[iso9660] root LBA: ");
    screen_put_dec(cdrom_fs.root_lba);
    screen_puts(", size: ");
    screen_put_dec(cdrom_fs.root_size);
    screen_puts(" bytes\n");
    screen_puts("[iso9660] mounted\n");

    return true;
}

/*
 * iso9660_read_dir — 读取目录内容
 * @lba:       目录起始 LBA
 * @size:      目录大小 (字节)
 * @entries:   输出条目数组
 * @max_count: 数组最大容量
 * 返回: 实际条目数。
 */
int iso9660_read_dir(uint32_t lba, uint32_t size,
                     struct iso9660_entry *entries, int max_count)
{
    if (!cdrom_fs.mounted)
        return 0;

    /* 计算需要读取的扇区数 */
    uint32_t sectors = (size + CDROM_SECTOR_SIZE - 1) / CDROM_SECTOR_SIZE;
    if (sectors == 0)
        sectors = 1;

    /* 最多读取 4 个扇区 (8KB) */
    if (sectors > 4)
        sectors = 4;

    /* 使用静态缓冲区避免栈溢出 */
    static uint8_t dir_buf[4 * CDROM_SECTOR_SIZE];
    if (!cdrom_read_sectors(lba, (uint16_t)sectors, dir_buf))
        return 0;

    int count = 0;
    uint32_t offset = 0;

    while (offset < size && count < max_count) {
        struct iso9660_dir_record *rec =
            (struct iso9660_dir_record *)(dir_buf + offset);

        /* 目录结束 (长度为 0) */
        if (rec->length == 0) {
            /* 跳到下一个扇区边界 */
            uint32_t next_sector = ((offset / CDROM_SECTOR_SIZE) + 1) *
                                   CDROM_SECTOR_SIZE;
            if (next_sector >= size)
                break;
            offset = next_sector;
            continue;
        }

        /* 跳过 "." 和 ".." 条目 */
        if (rec->name_len == 1 && rec->name[0] == '\0') {
            /* "." — 当前目录 */
            offset += rec->length;
            continue;
        }
        if (rec->name_len == 1 && rec->name[0] == '\1') {
            /* ".." — 父目录 */
            offset += rec->length;
            continue;
        }

        /* 构造文件名 (去除版本号 ";1") */
        struct iso9660_entry *entry = &entries[count];
        int name_len = rec->name_len;
        if (name_len > 255)
            name_len = 255;

        for (int i = 0; i < name_len; i++) {
            entry->name[i] = rec->name[i];
            /* 去除版本号分隔符及之后的内容 */
            if (rec->name[i] == ';') {
                name_len = i;
                break;
            }
        }
        entry->name[name_len] = '\0';

        /* 去除文件名中的点 (如果扩展名为空) */
        if (name_len > 0 && entry->name[name_len - 1] == '.') {
            entry->name[name_len - 1] = '\0';
        }

        /* 转换为小写 (ISO 9660 默认大写) */
        for (int i = 0; i < name_len; i++) {
            if (entry->name[i] >= 'A' && entry->name[i] <= 'Z') {
                entry->name[i] = entry->name[i] - 'A' + 'a';
            }
        }

        entry->lba = rec->extent_le;
        entry->size = rec->extent_size_le;
        entry->is_directory = (rec->file_flags & ISO9660_FLAG_DIRECTORY) != 0;

        count++;
        offset += rec->length;
    }

    return count;
}

/*
 * iso9660_read_file — 读取文件内容
 * @lba:      文件起始 LBA
 * @size:     文件大小 (字节)
 * @buf:      目标缓冲区
 * @buf_size: 缓冲区大小
 * 返回: 实际读取的字节数。
 */
uint32_t iso9660_read_file(uint32_t lba, uint32_t size,
                           void *buf, uint32_t buf_size)
{
    if (!cdrom_fs.mounted)
        return 0;

    if (size > buf_size)
        size = buf_size;

    uint32_t sectors = (size + CDROM_SECTOR_SIZE - 1) / CDROM_SECTOR_SIZE;

    if (!cdrom_read_sectors(lba, (uint16_t)sectors, buf))
        return 0;

    return size;
}

/*
 * iso9660_get_root — 获取根目录条目
 * 返回: 指向根目录 iso9660_entry 的指针。
 */
const struct iso9660_entry *iso9660_get_root(void)
{
    if (!cdrom_fs.mounted)
        return NULL;
    return &cdrom_root;
}

/*
 * iso9660_get_fs — 获取文件系统上下文
 * 返回: 指向 iso9660_fs 结构的指针。
 */
const struct iso9660_fs *iso9660_get_fs(void)
{
    return &cdrom_fs;
}
