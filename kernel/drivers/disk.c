/*
 * Nexsteaduser — PlexsDOS
 * ATA 磁盘驱动 (PIO + DMA/UDMA)
 * 作者: Tinmc189623 | 团队: Nexlyh
 *
 * 支持两种传输模式:
 * 1. PIO 模式: CPU 参与每次数据传输 (兼容性好)
 * 2. DMA 模式: 使用 PCI Bus Master IDE 控制器 (奔腾3 UDMA, 高性能)
 *
 * DMA 传输流程:
 * 1. 设置 PRDT (Physical Region Descriptor Table)
 * 2. 设置 ATA 命令寄存器 (LBA、扇区数)
 * 3. 发送 ATA READ DMA 命令
 * 4. 启动 Bus Master DMA 传输
 * 5. 等待传输完成 (IRQ 或轮询)
 */

#include <plexsdos/types.h>
#include <plexsdos/disk.h>
#include <plexsdos/pci.h>
#include <plexsdos/screen.h>

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

/* 从 I/O 端口读取一个 16-bit 字 */
static inline uint16_t inw(uint16_t port)
{
    uint16_t val;
    __asm__ __volatile__("inw %1, %0" : "=a"(val) : "Nd"(port));
    return val;
}

/* 向 I/O 端口写入一个 16-bit 字 */
static inline void outw(uint16_t port, uint16_t val)
{
    __asm__ __volatile__("outw %0, %1" : : "a"(val), "Nd"(port));
}

/* 向 I/O 端口写入一个 32-bit 双字 */
static inline void outl(uint16_t port, uint32_t val)
{
    __asm__ __volatile__("outl %0, %1" : : "a"(val), "Nd"(port));
}

/* 当前传输模式 */
static uint8_t current_mode = DMA_MODE_PIO;

/* Bus Master 基址 (从 PCI BAR4 获取) */
static uint16_t bm_base = 0;

/* PRDT (Physical Region Descriptor Table) — 必须 4KB 对齐 */
static struct prd_entry prdt[2] __attribute__((aligned(4096)));

/* DMA 传输缓冲区 — 必须物理连续且 64KB 对齐 */
static uint8_t dma_buf[65536] __attribute__((aligned(4096)));

/* DMA 就绪标志 */
static bool dma_ready = false;

/*
 * ata_wait_ready — 等待 BSY 标志清除
 * 返回: true = 就绪, false = 超时。
 */
static bool ata_wait_ready(void)
{
    int timeout = 100000;
    while (timeout-- > 0) {
        uint8_t status = inb(ATA_STATUS);
        if (!(status & ATA_STATUS_BSY))
            return true;
    }
    return false;
}

/*
 * ata_wait_drq — 等待 DRQ (数据就绪) 标志置位
 * 返回: true = 数据就绪, false = 超时或错误。
 */
static bool ata_wait_drq(void)
{
    int timeout = 100000;
    while (timeout-- > 0) {
        uint8_t status = inb(ATA_STATUS);
        if (status & ATA_STATUS_ERR)
            return false;
        if (status & ATA_STATUS_DF)
            return false;
        if (status & ATA_STATUS_DRQ)
            return true;
    }
    return false;
}

/*
 * disk_init — 初始化 ATA 磁盘
 *
 * 检测磁盘状态, 查找 PCI IDE 控制器, 设置 DMA 模式。
 * 返回: true = 磁盘就绪, false = 磁盘不可用。
 */
bool disk_init(void)
{
    struct pci_device *ide;

    /* 选择主盘 */
    outb(ATA_DRIVE_HEAD, 0xE0);

    /* 等待就绪 */
    if (!ata_wait_ready()) {
        screen_puts("[disk] ATA not ready\n");
        return false;
    }

    /* 检测驱动器是否存在 */
    uint8_t status = inb(ATA_STATUS);
    if (status == 0x00 || status == 0xFF) {
        screen_puts("[disk] No ATA drive detected\n");
        return false;
    }

    screen_puts("[disk] ATA drive ready\n");

    /* 查找 PCI IDE 控制器 */
    ide = pci_get_ide_controller();
    if (ide != NULL) {
        /* 获取 Bus Master 基址 (BAR4) */
        bm_base = (uint16_t)(ide->bar4 & 0xFFFC);

        if (bm_base != 0) {
            /* 设置 PRDT */
            prdt[0].phys_addr = (uint32_t)dma_buf;
            prdt[0].byte_count = 0;  /* 0 = 65536 字节 */
            prdt[0].flags = PRD_EOT;

            /* 设置 PRDT 地址到 Bus Master 寄存器 */
            outl(bm_base + BM_PRDT_ADDR, (uint32_t)prdt);

            dma_ready = true;
            current_mode = DMA_MODE_DMA;

            screen_puts("[disk] DMA mode enabled\n");
        }
    }

    return true;
}

/*
 * disk_set_dma_mode — 设置 DMA 传输模式
 * @mode: DMA_MODE_PIO 或 DMA_MODE_DMA
 */
void disk_set_dma_mode(uint8_t mode)
{
    if (mode == DMA_MODE_DMA && !dma_ready) {
        screen_puts("[disk] DMA not available, using PIO\n");
        return;
    }
    current_mode = mode;
}

/*
 * disk_get_mode — 获取当前传输模式
 * 返回: DMA_MODE_PIO 或 DMA_MODE_DMA
 */
uint8_t disk_get_mode(void)
{
    return current_mode;
}

/*
 * disk_read_pio — PIO 模式读取扇区
 * @lba: 起始逻辑块地址
 * @count: 扇区数
 * @buf: 目标缓冲区
 * 返回: true = 成功, false = 失败。
 */
static bool disk_read_pio(uint32_t lba, uint8_t count, void *buf)
{
    uint16_t *ptr = (uint16_t *)buf;

    if (!ata_wait_ready())
        return false;

    /* 设置 LBA 地址和扇区数 */
    outb(ATA_SECTOR_COUNT, count);
    outb(ATA_LBA_LO,  (uint8_t)(lba & 0xFF));
    outb(ATA_LBA_MID, (uint8_t)((lba >> 8) & 0xFF));
    outb(ATA_LBA_HI,  (uint8_t)((lba >> 16) & 0xFF));
    outb(ATA_DRIVE_HEAD, (uint8_t)(0xE0 | ((lba >> 24) & 0x0F)));

    /* 发送 PIO 读取命令 */
    outb(ATA_COMMAND, ATA_CMD_READ_PIO);

    /* 逐扇区读取数据 */
    for (uint8_t s = 0; s < count; s++) {
        if (!ata_wait_drq())
            return false;

        for (int i = 0; i < 256; i++) {
            ptr[s * 256 + i] = inw(ATA_DATA);
        }
    }

    return true;
}

/*
 * disk_read_dma — DMA 模式读取扇区
 * @lba: 起始逻辑块地址
 * @count: 扇区数 (最多 128 扇区 = 64KB)
 * @buf: 目标缓冲区
 * 返回: true = 成功, false = 失败。
 *
 * DMA 传输流程:
 * 1. 清除 Bus Master 状态
 * 2. 设置 ATA 命令寄存器 (LBA、扇区数)
 * 3. 发送 ATA READ DMA 命令
 * 4. 启动 Bus Master (设置 Start/Read 位)
 * 5. 等待 IRQ 或轮询状态
 * 6. 停止 Bus Master
 * 7. 复制数据到目标缓冲区
 */
static bool disk_read_dma(uint32_t lba, uint8_t count, void *buf)
{
    uint8_t bm_status;
    int timeout;

    /* 限制传输大小 (DMA 缓冲区 64KB) */
    if (count > 128)
        count = 128;

    /* 设置 PRDT — 单次传输整个请求 */
    uint32_t total_bytes = (uint32_t)count * 512;
    prdt[0].phys_addr = (uint32_t)dma_buf;
    prdt[0].byte_count = (uint16_t)(total_bytes & 0xFFFF);
    prdt[0].flags = PRD_EOT;

    /* 清除 Bus Master 状态 */
    bm_status = inb(bm_base + BM_STATUS);
    outb(bm_base + BM_STATUS, bm_status | 0x06);  /* 清除 IRQ 和错误 */

    /* 清除 DMA 缓冲区 */
    for (uint32_t i = 0; i < total_bytes; i++)
        dma_buf[i] = 0;

    if (!ata_wait_ready())
        return false;

    /* 设置 LBA 地址和扇区数 */
    outb(ATA_SECTOR_COUNT, count);
    outb(ATA_LBA_LO,  (uint8_t)(lba & 0xFF));
    outb(ATA_LBA_MID, (uint8_t)((lba >> 8) & 0xFF));
    outb(ATA_LBA_HI,  (uint8_t)((lba >> 16) & 0xFF));
    outb(ATA_DRIVE_HEAD, (uint8_t)(0xE0 | ((lba >> 24) & 0x0F)));

    /* 发送 READ DMA 命令 */
    outb(ATA_COMMAND, ATA_CMD_READ_DMA);

    /* 启动 Bus Master DMA (读取方向 + 启动) */
    outb(bm_base + BM_COMMAND, BM_CMD_READ | BM_CMD_START);

    /* 等待传输完成 (轮询 Bus Master 状态) */
    timeout = 1000000;
    while (timeout-- > 0) {
        bm_status = inb(bm_base + BM_STATUS);

        /* 检查 IRQ 标志 (传输完成) */
        if (bm_status & BM_STATUS_IRQ)
            break;

        /* 检查错误 */
        if (bm_status & BM_STATUS_ERROR) {
            screen_puts("[disk] DMA transfer error\n");
            /* 停止 Bus Master */
            outb(bm_base + BM_COMMAND, 0);
            return false;
        }
    }

    /* 停止 Bus Master */
    outb(bm_base + BM_COMMAND, 0);

    /* 检查超时 */
    if (timeout <= 0) {
        screen_puts("[disk] DMA timeout\n");
        return false;
    }

    /* 清除 IRQ 和 Active 状态 */
    outb(bm_base + BM_STATUS, bm_status | 0x04);

    /* 从 DMA 缓冲区复制到目标缓冲区 */
    uint8_t *src = dma_buf;
    uint8_t *dst = (uint8_t *)buf;
    for (uint32_t i = 0; i < total_bytes; i++)
        dst[i] = src[i];

    return true;
}

/*
 * disk_read_sectors — 从 LBA 扇区号读取 count 个扇区到 buf
 * 优先使用 DMA 模式, 回退到 PIO 模式。
 * 返回: true = 成功, false = 失败。
 */
bool disk_read_sectors(uint32_t lba, uint8_t count, void *buf)
{
    if (current_mode == DMA_MODE_DMA && dma_ready) {
        return disk_read_dma(lba, count, buf);
    }
    return disk_read_pio(lba, count, buf);
}

/*
 * disk_write_pio — PIO 模式写入扇区
 * @lba: 起始逻辑块地址
 * @count: 扇区数
 * @buf: 源数据缓冲区
 * 返回: true = 成功, false = 失败。
 *
 * 向 ATA 磁盘逐扇区写入数据。
 * 每个扇区 512 字节 (256 个 16-bit 字)。
 */
static bool disk_write_pio(uint32_t lba, uint8_t count, const void *buf)
{
    const uint16_t *ptr = (const uint16_t *)buf;

    if (!ata_wait_ready())
        return false;

    /* 设置 LBA 地址和扇区数 */
    outb(ATA_SECTOR_COUNT, count);
    outb(ATA_LBA_LO,  (uint8_t)(lba & 0xFF));
    outb(ATA_LBA_MID, (uint8_t)((lba >> 8) & 0xFF));
    outb(ATA_LBA_HI,  (uint8_t)((lba >> 16) & 0xFF));
    outb(ATA_DRIVE_HEAD, (uint8_t)(0xE0 | ((lba >> 24) & 0x0F)));

    /* 发送 PIO 写入命令 */
    outb(ATA_COMMAND, ATA_CMD_WRITE_PIO);

    /* 逐扇区写入数据 */
    for (uint8_t s = 0; s < count; s++) {
        if (!ata_wait_drq())
            return false;

        for (int i = 0; i < 256; i++) {
            outw(ATA_DATA, ptr[s * 256 + i]);
        }
    }

    /* 刷新磁盘缓存 */
    outb(ATA_COMMAND, ATA_CMD_CACHE_FLUSH);
    if (!ata_wait_ready())
        return false;

    return true;
}

/*
 * disk_write_sectors — 将 buf 中的数据写入 LBA 扇区号起始的 count 个扇区
 * 使用 PIO 模式写入。
 * 返回: true = 成功, false = 失败。
 */
bool disk_write_sectors(uint32_t lba, uint8_t count, const void *buf)
{
    return disk_write_pio(lba, count, buf);
}
