/*
 * Nexsteaduser — PlexsDOS
 * 82077AA 软盘控制器 (FDC) 驱动
 * 作者: Tinmc189623 | 团队: Nexlyh
 *
 * 直接编程 82077AA FDC 硬件:
 * - 端口 I/O (0x3F0-0x3F7) 控制 FDC 寄存器
 * - 8237 DMA 控制器通道 2 进行数据传输
 * - IRQ 6 中断驱动的命令完成通知
 *
 * 改进特性:
 * - 马达状态机: 定时器自动关闭, 避免马达空转
 * - 软盘存在检测: READ ID 命令
 * - 写入支持: WRITE DATA 命令 + 写保护检测
 * - 多格式支持: 1.44MB / 1.2MB / 720KB
 * - 参考: Linux floppy.c, FreeBSD fdc.c, DOSBox FDC
 */

#include <plexsdos/types.h>
#include <plexsdos/fdc.h>
#include <plexsdos/interrupt.h>
#include <plexsdos/screen.h>
#include <plexsdos/config.h>

/* 外部汇编函数 — FDC 中断入口 */
extern void isr_fdc(void);

/* ==================== 端口 I/O 辅助函数 ==================== */

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

/* ==================== DMA 缓冲区 ==================== */

/*
 * DMA 传输缓冲区 — 必须满足 8237 DMA 控制器约束:
 * 1. 物理地址在前 16MB 内 (24-bit 地址总线)
 * 2. 不跨越 64KB 物理页边界
 * 3. 连续物理内存
 *
 * 使用 8KB 缓冲区 (16 个扇区), 远小于 64KB 限制。
 * 对齐到 4KB 边界确保不跨越 64KB 页。
 */
static uint8_t fdc_dma_buf[8192] __attribute__((aligned(4096)));

/* ==================== FDC 状态 ==================== */

/* IRQ 6 触发标志 — 由中断处理程序设置 */
static volatile bool fdc_irq_fired = false;

/* FDC 结果字节缓冲区 (最多 7 个字节) */
static uint8_t fdc_result[7];
static int     fdc_result_count = 0;

/* 当前选中的驱动器号 */
static uint8_t fdc_current_drive = 0xFF;

/* ==================== 马达状态机 ==================== */

/* 马达状态 */
static enum fdc_motor_state motor_state[2] = {MOTOR_OFF, MOTOR_OFF};

/* 马达关闭延迟计时器 (毫秒) */
static uint32_t motor_off_timer[2] = {0, 0};

/* ==================== 软盘几何参数 ==================== */

static struct fdc_geometry drive_geom[2];

/* ==================== 延时函数 ==================== */

/*
 * fdc_delay_ms — 简易毫秒级延时
 * @ms: 延时毫秒数
 *
 * 通过端口 I/O 操作产生约 1 微秒的延时,
 * 循环 ms * 1000 次近似毫秒延时。
 */
static void fdc_delay_ms(uint32_t ms)
{
    volatile uint32_t count = ms * 1000;
    while (count-- > 0) {
        inb(0x80);
    }
}

/* ==================== IRQ 6 处理 ==================== */

/*
 * fdc_interrupt_handler — FDC 中断处理程序 (IRQ 6, INT 0x26)
 *
 * 由汇编桩 _isr_fdc 调用。设置中断标志, 发送 EOI。
 */
void fdc_interrupt_handler(void)
{
    fdc_irq_fired = true;
    pic_eoi(FDC_IRQ);
}

/*
 * fdc_wait_irq — 等待 FDC 中断
 * @timeout_ms: 超时毫秒数
 * 返回: true = 收到中断, false = 超时。
 */
static bool fdc_wait_irq(uint32_t timeout_ms)
{
    uint32_t waited = 0;
    while (!fdc_irq_fired && waited < timeout_ms) {
        fdc_delay_ms(1);
        waited++;
    }
    if (fdc_irq_fired) {
        fdc_irq_fired = false;
        return true;
    }
    return false;
}

/* ==================== FDC 命令接口 ==================== */

/*
 * fdc_read_msr — 读取 FDC 主状态寄存器
 * 返回: MSR 字节
 */
static uint8_t fdc_read_msr(void)
{
    return inb(FDC_MSR);
}

/*
 * fdc_write_byte — 向 FDC FIFO 写入一个字节
 * @val: 要写入的字节
 * 返回: true = 成功, false = 超时。
 */
static bool fdc_write_byte(uint8_t val)
{
    int timeout = 1000;
    while (timeout-- > 0) {
        uint8_t msr = fdc_read_msr();
        if ((msr & MSR_RQM) && !(msr & MSR_DIO)) {
            outb(FDC_FIFO, val);
            return true;
        }
    }
    return false;
}

/*
 * fdc_read_byte — 从 FDC FIFO 读取一个字节
 * @val: 输出字节
 * 返回: true = 成功, false = 超时。
 */
static bool fdc_read_byte(uint8_t *val)
{
    int timeout = 1000;
    while (timeout-- > 0) {
        uint8_t msr = fdc_read_msr();
        if ((msr & MSR_RQM) && (msr & MSR_DIO)) {
            *val = inb(FDC_FIFO);
            return true;
        }
    }
    return false;
}

/*
 * fdc_send_command — 向 FDC 发送命令
 * @cmd: 命令字节
 * 返回: true = 命令字节已接受。
 */
static bool fdc_send_command(uint8_t cmd)
{
    return fdc_write_byte(cmd);
}

/*
 * fdc_read_result — 读取 FDC 结果阶段的所有字节
 * 返回: 结果字节数。
 */
static int fdc_read_result(void)
{
    int count = 0;
    uint8_t byte;

    fdc_result_count = 0;
    while (count < 7) {
        uint8_t msr = fdc_read_msr();
        if (!(msr & MSR_RQM))
            break;
        if (!(msr & MSR_DIO))
            break;
        if (fdc_read_byte(&byte)) {
            fdc_result[count++] = byte;
        } else {
            break;
        }
    }
    fdc_result_count = count;
    return count;
}

/*
 * fdc_sense_interrupt — 发送 SENSE INTERRUPT STATUS 命令
 * @st0: 输出 ST0 寄存器
 * @cyl: 输出当前柱面号
 * 返回: true = 成功。
 */
static bool fdc_sense_interrupt(uint8_t *st0, uint8_t *cyl)
{
    if (!fdc_send_command(FDC_CMD_SENSE_INT))
        return false;

    uint8_t r1, r2;
    if (!fdc_read_byte(&r1))
        return false;
    if (!fdc_read_byte(&r2))
        return false;

    if (st0) *st0 = r1;
    if (cyl) *cyl = r2;
    return true;
}

/* ==================== DMA 编程 ==================== */

/*
 * fdc_dma_read — 编程 DMA 通道 2 进行软盘读取
 * @addr:  物理地址
 * @count: 传输字节数 - 1
 */
static void fdc_dma_read(uint32_t addr, uint32_t count)
{
    uint16_t dma_count = (uint16_t)(count - 1);

    outb(DMA_MASK, 0x06);
    outb(DMA_CLEAR_FF, 0x00);
    outb(DMA_MODE, DMA_MODE_READ | DMA_MODE_SINGLE | 0x02);
    outb(DMA_ADDR_CH2, (uint8_t)(addr & 0xFF));
    outb(DMA_ADDR_CH2, (uint8_t)((addr >> 8) & 0xFF));
    outb(DMA_PAGE_CH2, (uint8_t)((addr >> 16) & 0xFF));
    outb(DMA_COUNT_CH2, (uint8_t)(dma_count & 0xFF));
    outb(DMA_COUNT_CH2, (uint8_t)((dma_count >> 8) & 0xFF));
    outb(DMA_MASK, 0x02);
}

/*
 * fdc_dma_write — 编程 DMA 通道 2 进行软盘写入
 * @addr:  物理地址
 * @count: 传输字节数 - 1
 */
static void fdc_dma_write(uint32_t addr, uint32_t count)
{
    uint16_t dma_count = (uint16_t)(count - 1);

    outb(DMA_MASK, 0x06);
    outb(DMA_CLEAR_FF, 0x00);
    outb(DMA_MODE, DMA_MODE_WRITE | DMA_MODE_SINGLE | 0x02);
    outb(DMA_ADDR_CH2, (uint8_t)(addr & 0xFF));
    outb(DMA_ADDR_CH2, (uint8_t)((addr >> 8) & 0xFF));
    outb(DMA_PAGE_CH2, (uint8_t)((addr >> 16) & 0xFF));
    outb(DMA_COUNT_CH2, (uint8_t)(dma_count & 0xFF));
    outb(DMA_COUNT_CH2, (uint8_t)((dma_count >> 8) & 0xFF));
    outb(DMA_MASK, 0x02);
}

/* ==================== 马达状态机 ==================== */

/*
 * fdc_motor_on — 开启指定驱动器的马达
 * @drive: 驱动器号 (0 或 1)
 *
 * 如果马达已关闭, 开启并等待旋转稳定。
 * 如果马达正在运行, 重置关闭计时器。
 */
static void fdc_motor_on(uint8_t drive)
{
    uint8_t dor;

    if (drive > 1)
        return;

    if (motor_state[drive] == MOTOR_ON) {
        /* 马达已在运行, 重置计时器 */
        motor_off_timer[drive] = MOTOR_OFF_DELAY_MS;
        return;
    }

    /* 开启马达 */
    dor = DOR_RESET | DOR_DMA_GATE;
    dor |= (drive == 0) ? DOR_MOTOR0 | DOR_DRIVE0 : DOR_MOTOR1 | DOR_DRIVE1;

    outb(FDC_DOR, dor);
    fdc_current_drive = drive;
    motor_state[drive] = MOTOR_STARTING;

    /* 等待磁盘旋转稳定 */
    fdc_delay_ms(MOTOR_SPINUP_MS);

    motor_state[drive] = MOTOR_ON;
    motor_off_timer[drive] = MOTOR_OFF_DELAY_MS;
}

/*
 * fdc_motor_off — 关闭指定驱动器的马达
 * @drive: 驱动器号
 */
static void fdc_motor_off(uint8_t drive)
{
    uint8_t dor;

    if (drive > 1)
        return;

    dor = DOR_RESET | DOR_DMA_GATE;
    if (drive == 0)
        dor |= DOR_DRIVE0;
    else
        dor |= DOR_DRIVE1;

    outb(FDC_DOR, dor);
    motor_state[drive] = MOTOR_OFF;
    motor_off_timer[drive] = 0;
}

/*
 * fdc_motor_tick — 马达定时器递减
 *
 * 由主循环定期调用 (约每 1ms 一次)。
 * 当计时器到期时自动关闭马达。
 */
void fdc_motor_tick(void)
{
    for (uint8_t drive = 0; drive < 2; drive++) {
        if (motor_state[drive] == MOTOR_ON) {
            if (motor_off_timer[drive] > 0) {
                motor_off_timer[drive]--;
                if (motor_off_timer[drive] == 0) {
                    fdc_motor_off(drive);
                }
            }
        }
    }
}

/* ==================== FDC 操作 ==================== */

/*
 * fdc_reset — 复位 FDC 控制器
 * 返回: true = 复位成功。
 */
static bool fdc_reset(void)
{
    uint8_t st0, cyl;

    outb(FDC_DOR, 0x00);
    fdc_delay_ms(20);

    outb(FDC_DOR, DOR_RESET | DOR_DMA_GATE);
    fdc_delay_ms(20);

    if (!fdc_wait_irq(500)) {
        screen_puts("[fdc] reset timeout\n");
        return false;
    }

    fdc_sense_interrupt(&st0, &cyl);
    fdc_sense_interrupt(&st0, &cyl);
    fdc_sense_interrupt(&st0, &cyl);
    fdc_sense_interrupt(&st0, &cyl);

    /* 设置传输速率: 500 Kbps (1.44MB 软盘) */
    outb(FDC_DCR, 0x00);

    return true;
}

/*
 * fdc_specify — 发送 SPECIFY 命令设置 FDC 参数
 * @step_rate: 步进速率 (0-15)
 * @load_time: 磁头加载时间 (0-15, 单位 4ms)
 * @dma_mode:  true = DMA 模式
 */
static void fdc_specify(uint8_t step_rate, uint8_t load_time, bool dma_mode)
{
    uint8_t spec1 = (step_rate << 4) | (load_time & 0x0F);
    uint8_t spec2 = dma_mode ? 0x00 : 0x01;

    fdc_send_command(FDC_CMD_SPECIFY);
    fdc_send_command(spec1);
    fdc_send_command(spec2);
}

/*
 * fdc_recalibrate — 重新校准磁头到柱面 0
 * @drive: 驱动器号
 * 返回: true = 成功到达柱面 0。
 */
static bool fdc_recalibrate(uint8_t drive)
{
    uint8_t st0, cyl;
    int retries = 3;

    while (retries-- > 0) {
        fdc_send_command(FDC_CMD_RECALIBRATE);
        fdc_send_command(drive);

        if (!fdc_wait_irq(3000))
            continue;

        fdc_sense_interrupt(&st0, &cyl);

        if ((st0 & 0x60) == 0x00 && cyl == 0)
            return true;
    }

    return false;
}

/*
 * fdc_seek — 寻道到指定柱面
 * @drive:    驱动器号
 * @cylinder: 目标柱面号 (0-79)
 * 返回: true = 磁头已到达目标柱面。
 */
static bool fdc_seek(uint8_t drive, uint8_t cylinder)
{
    uint8_t st0, cyl;
    int retries = 3;

    while (retries-- > 0) {
        fdc_send_command(FDC_CMD_SEEK);
        fdc_send_command(drive);
        fdc_send_command(cylinder);

        if (!fdc_wait_irq(3000))
            continue;

        fdc_sense_interrupt(&st0, &cyl);

        if ((st0 & 0x60) == 0x00 && cyl == cylinder) {
            fdc_delay_ms(20);
            return true;
        }
    }

    return false;
}

/* ==================== 磁盘检测 ==================== */

/*
 * fdc_detect_disk — 检测驱动器中是否有磁盘
 * @drive: 驱动器号
 * 返回: true = 有磁盘, false = 无磁盘。
 *
 * 使用 READ ID 命令检测: 如果磁盘存在, FDC 会返回
 * 当前磁头位置的 C/H/R/N 信息; 如果无磁盘, 会超时。
 */
bool fdc_detect_disk(uint8_t drive)
{
    bool found;

    if (drive > 1)
        return false;

    fdc_motor_on(drive);

    /* 发送 READ ID 命令 (使用磁头 0) */
    fdc_send_command(FDC_CMD_READ_ID);
    fdc_send_command(drive);

    found = fdc_wait_irq(1000);

    if (found) {
        fdc_read_result();
        /* 检查 ST0 状态 */
        found = ((fdc_result[0] & 0x60) == 0x00);
    }

    fdc_motor_off(drive);
    return found;
}

/*
 * fdc_disk_changed — 检测磁盘是否已更换
 * @drive: 驱动器号
 * 返回: true = 磁盘已更换, false = 未更换。
 *
 * 读取 DIR 寄存器 (0x3F7) 的 Change Line 位 (bit 7)。
 * 注意: DIR 寄存器与 FDC_DCR 共用端口, 读取时为 DIR, 写入时为 DCR。
 */
bool fdc_disk_changed(uint8_t drive)
{
    (void)drive;
    uint8_t dir = inb(FDC_DIR);
    return (dir & DIR_DISK_CHANGED) != 0;
}

/*
 * fdc_write_protected — 检测磁盘是否写保护
 * @drive: 驱动器号
 * 返回: true = 写保护, false = 可写。
 *
 * 读取 DIR 寄存器的 Write Protect 位 (bit 6)。
 */
bool fdc_write_protected(uint8_t drive)
{
    (void)drive;
    uint8_t dir = inb(FDC_DIR);
    return (dir & DIR_WRITE_PROTECT) != 0;
}

/* ==================== 多格式支持 ==================== */

/*
 * fdc_detect_media — 检测软盘介质类型
 * @drive: 驱动器号
 *
 * 通过读取 FAT12 BPB 的介质描述符字节检测格式。
 * 介质描述符在引导扇区偏移 21 (0x15)。
 */
static void fdc_detect_media(uint8_t drive)
{
    uint8_t boot_sec[512];

    /* 默认 1.44MB */
    drive_geom[drive].cylinders = 80;
    drive_geom[drive].heads = 2;
    drive_geom[drive].spt = 18;
    drive_geom[drive].sector_size = 512;
    drive_geom[drive].total_sectors = 2880;
    drive_geom[drive].media = FDC_MEDIA_1440K;

    /* 尝试读取引导扇区获取 BPB 信息 */
    if (!fdc_read_sectors(drive, 0, 0, 1, 1, boot_sec))
        return;

    /* 检查 BPB 签名 */
    if (boot_sec[510] != 0x55 || boot_sec[511] != 0xAA)
        return;

    /* 读取介质描述符 (偏移 21) */
    uint8_t media_desc = boot_sec[21];

    /* 读取 BPB 参数 */
    uint16_t bpb_spt = *(uint16_t *)(boot_sec + 24);
    uint16_t bpb_heads = *(uint16_t *)(boot_sec + 26);

    if (bpb_spt > 0 && bpb_spt <= 36 && bpb_heads > 0 && bpb_heads <= 2) {
        drive_geom[drive].spt = (uint8_t)bpb_spt;
        drive_geom[drive].heads = (uint8_t)bpb_heads;
        drive_geom[drive].total_sectors =
            80 * bpb_heads * bpb_spt;

        if (bpb_spt == 15) {
            drive_geom[drive].media = FDC_MEDIA_1200K;
        } else if (bpb_spt == 9) {
            drive_geom[drive].media = FDC_MEDIA_720K;
        } else {
            drive_geom[drive].media = FDC_MEDIA_1440K;
        }
    }

    (void)media_desc;
}

/* ==================== 公共 API ==================== */

/*
 * fdc_init — 初始化软盘控制器
 *
 * 1. 注册 IRQ 6 中断处理程序
 * 2. 复位 FDC
 * 3. 设置 SPECIFY 参数
 * 4. 重新校准驱动器 0
 * 5. 检测驱动器是否存在
 * 6. 检测介质类型
 *
 * 返回: true = FDC 就绪, false = 初始化失败。
 */
bool fdc_init(void)
{
    uint8_t st0, cyl;

    /* 初始化马达状态 */
    motor_state[0] = MOTOR_OFF;
    motor_state[1] = MOTOR_OFF;
    motor_off_timer[0] = 0;
    motor_off_timer[1] = 0;

    /* 注册 IRQ 6 中断处理程序 (INT 0x26) */
    interrupt_register(FDC_INT_VECTOR, (interrupt_handler_t)isr_fdc);

    /* 取消屏蔽 IRQ 6 */
    {
        uint8_t mask = inb(0x21);
        mask &= ~(1 << 6);
        outb(0x21, mask);
    }

    if (!fdc_reset())
        return false;

    /* SRT=0 (最快步进), HLT=5 (20ms), DMA 模式 */
    fdc_specify(0, 5, true);

    /* 开启马达并重新校准驱动器 0 */
    fdc_motor_on(0);
    fdc_delay_ms(200);

    if (!fdc_recalibrate(0)) {
        screen_puts("[fdc] recalibrate failed\n");
        fdc_motor_off(0);
        return false;
    }

    /* 检测驱动器 0 */
    fdc_seek(0, 1);
    fdc_recalibrate(0);
    fdc_sense_interrupt(&st0, &cyl);

    /* 检测介质类型 */
    fdc_detect_media(0);

    fdc_motor_off(0);

    screen_puts("[fdc] floppy drive 0 ready\n");
    return true;
}

/*
 * fdc_read_sectors — 从软盘读取扇区
 * @drive:    驱动器号 (0 或 1)
 * @head:     磁头号 (0 或 1)
 * @cylinder: 柱面号 (0-79)
 * @sector:   扇区号 (1-18, 注意从 1 开始)
 * @count:    扇区数 (最多 16, 受 DMA 缓冲区限制)
 * @buf:      目标缓冲区
 * 返回: true = 成功, false = 失败。
 */
bool fdc_read_sectors(uint8_t drive, uint8_t head, uint8_t cylinder,
                      uint8_t sector, uint8_t count, void *buf)
{
    uint8_t st0;
    uint32_t bytes;
    uint8_t spt = drive_geom[drive].spt;

    if (drive > 1 || head > 1 || cylinder >= FDC_CYLINDERS ||
        sector < 1 || sector > spt || count == 0)
        return false;

    if (count > 16)
        count = 16;

    bytes = (uint32_t)count * FDC_SECTOR_SIZE;

    fdc_motor_on(drive);

    if (!fdc_seek(drive, cylinder)) {
        screen_puts("[fdc] seek failed\n");
        fdc_motor_off(drive);
        return false;
    }

    fdc_dma_read((uint32_t)fdc_dma_buf, bytes);

    fdc_send_command(FDC_CMD_READ_DATA | 0xC0);  /* MT=1, MFM=1 */
    fdc_send_command(head << 2 | drive);
    fdc_send_command(cylinder);
    fdc_send_command(head);
    fdc_send_command(sector);
    fdc_send_command(2);                           /* N=512 */
    fdc_send_command(spt);                         /* EOT */
    fdc_send_command(0x1B);                        /* GPL */
    fdc_send_command(0xFF);                        /* DTL */

    if (!fdc_wait_irq(5000)) {
        screen_puts("[fdc] read timeout\n");
        fdc_motor_off(drive);
        return false;
    }

    fdc_read_result();

    st0 = fdc_result[0];
    if ((st0 & 0x60) != 0x00) {
        screen_puts("[fdc] read error ST0=");
        screen_put_hex(st0);
        screen_putchar('\n');
        fdc_motor_off(drive);
        return false;
    }

    /* 从 DMA 缓冲区复制到目标缓冲区 */
    uint8_t *src = fdc_dma_buf;
    uint8_t *dst = (uint8_t *)buf;
    for (uint32_t i = 0; i < bytes; i++)
        dst[i] = src[i];

    /* 重置马达计时器 (不立即关闭) */
    motor_off_timer[drive] = MOTOR_OFF_DELAY_MS;

    return true;
}

/*
 * fdc_write_sectors — 向软盘写入扇区
 * @drive:    驱动器号 (0 或 1)
 * @head:     磁头号 (0 或 1)
 * @cylinder: 柱面号 (0-79)
 * @sector:   扇区号 (1-18)
 * @count:    扇区数
 * @buf:      源数据缓冲区
 * 返回: true = 成功, false = 失败。
 *
 * 使用 WRITE DATA 命令通过 DMA 写入软盘数据。
 */
bool fdc_write_sectors(uint8_t drive, uint8_t head, uint8_t cylinder,
                       uint8_t sector, uint8_t count, const void *buf)
{
    uint8_t st0;
    uint32_t bytes;
    uint8_t spt = drive_geom[drive].spt;

    if (drive > 1 || head > 1 || cylinder >= FDC_CYLINDERS ||
        sector < 1 || sector > spt || count == 0)
        return false;

    if (count > 16)
        count = 16;

    bytes = (uint32_t)count * FDC_SECTOR_SIZE;

    /* 检查写保护 */
    if (fdc_write_protected(drive)) {
        screen_puts("[fdc] disk write-protected\n");
        return false;
    }

    fdc_motor_on(drive);

    if (!fdc_seek(drive, cylinder)) {
        screen_puts("[fdc] seek failed\n");
        fdc_motor_off(drive);
        return false;
    }

    /* 从源缓冲区复制到 DMA 缓冲区 */
    const uint8_t *src = (const uint8_t *)buf;
    uint8_t *dst = fdc_dma_buf;
    for (uint32_t i = 0; i < bytes; i++)
        dst[i] = src[i];

    /* 编程 DMA 为写入方向 */
    fdc_dma_write((uint32_t)fdc_dma_buf, bytes);

    fdc_send_command(FDC_CMD_WRITE_DATA | 0xC0);  /* MT=1, MFM=1 */
    fdc_send_command(head << 2 | drive);
    fdc_send_command(cylinder);
    fdc_send_command(head);
    fdc_send_command(sector);
    fdc_send_command(2);                           /* N=512 */
    fdc_send_command(spt);                         /* EOT */
    fdc_send_command(0x1B);                        /* GPL */
    fdc_send_command(0xFF);                        /* DTL */

    if (!fdc_wait_irq(5000)) {
        screen_puts("[fdc] write timeout\n");
        fdc_motor_off(drive);
        return false;
    }

    fdc_read_result();

    st0 = fdc_result[0];
    if ((st0 & 0x60) != 0x00) {
        screen_puts("[fdc] write error ST0=");
        screen_put_hex(st0);
        screen_putchar('\n');
        fdc_motor_off(drive);
        return false;
    }

    /* 重置马达计时器 */
    motor_off_timer[drive] = MOTOR_OFF_DELAY_MS;

    return true;
}

/*
 * fdc_get_geometry — 获取软盘几何参数
 * @drive: 驱动器号
 * @heads: 输出磁头数
 * @spt:   输出每磁道扇区数
 * 返回: true = 成功。
 */
bool fdc_get_geometry(uint8_t drive, uint8_t *heads, uint8_t *spt)
{
    if (drive > 1)
        return false;
    if (heads) *heads = drive_geom[drive].heads;
    if (spt)   *spt   = drive_geom[drive].spt;
    return true;
}

/*
 * fdc_read_lba — 以 LBA 方式从软盘读取扇区
 * @drive: 驱动器号 (0 或 1)
 * @lba:   逻辑块地址 (0-2879)
 * @count: 扇区数
 * @buf:   目标缓冲区
 * 返回: true = 成功。
 */
bool fdc_read_lba(uint8_t drive, uint32_t lba, uint8_t count, void *buf)
{
    uint8_t *dst = (uint8_t *)buf;
    uint8_t heads = drive_geom[drive].heads;
    uint8_t spt = drive_geom[drive].spt;

    for (uint8_t i = 0; i < count; i++) {
        uint32_t cur_lba = lba + i;
        uint8_t cylinder = (uint8_t)(cur_lba / (heads * spt));
        uint8_t head     = (uint8_t)((cur_lba / spt) % heads);
        uint8_t sector   = (uint8_t)((cur_lba % spt) + 1);

        if (!fdc_read_sectors(drive, head, cylinder, sector, 1,
                              dst + (uint32_t)i * FDC_SECTOR_SIZE))
            return false;
    }

    return true;
}
