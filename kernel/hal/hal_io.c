/*
 * Nexsteaduser — PlexsDOS
 * hal.c — 硬件抽象层实现
 * 作者: Tinmc189623 | 团队: Nexlyh
 *
 * 集中实现所有底层硬件访问函数。
 * 驱动程序通过 HAL API 访问硬件, 无需直接使用内联汇编。
 */

#include <plexsdos/hal.h>
#include <plexsdos/types.h>

/* ===== 端口 I/O ===== */

/*
 * hal_inb — 从 I/O 端口读取一个字节
 * @port: I/O 端口地址
 * 返回: 读取的字节值。
 */
uint8_t hal_inb(uint16_t port)
{
    uint8_t val;
    __asm__ __volatile__("inb %1, %0" : "=a"(val) : "Nd"(port));
    return val;
}

/*
 * hal_outb — 向 I/O 端口写入一个字节
 * @port: I/O 端口地址
 * @val:  要写入的字节值
 */
void hal_outb(uint16_t port, uint8_t val)
{
    __asm__ __volatile__("outb %0, %1" : : "a"(val), "Nd"(port));
}

/*
 * hal_inw — 从 I/O 端口读取一个 16-bit 字
 * @port: I/O 端口地址
 * 返回: 读取的字值。
 */
uint16_t hal_inw(uint16_t port)
{
    uint16_t val;
    __asm__ __volatile__("inw %1, %0" : "=a"(val) : "Nd"(port));
    return val;
}

/*
 * hal_outw — 向 I/O 端口写入一个 16-bit 字
 * @port: I/O 端口地址
 * @val:  要写入的字值
 */
void hal_outw(uint16_t port, uint16_t val)
{
    __asm__ __volatile__("outw %0, %1" : : "a"(val), "Nd"(port));
}

/*
 * hal_inl — 从 I/O 端口读取一个 32-bit 双字
 * @port: I/O 端口地址
 * 返回: 读取的双字值。
 */
uint32_t hal_inl(uint16_t port)
{
    uint32_t val;
    __asm__ __volatile__("inl %1, %0" : "=a"(val) : "Nd"(port));
    return val;
}

/*
 * hal_outl — 向 I/O 端口写入一个 32-bit 双字
 * @port: I/O 端口地址
 * @val:  要写入的双字值
 */
void hal_outl(uint16_t port, uint32_t val)
{
    __asm__ __volatile__("outl %0, %1" : : "a"(val), "Nd"(port));
}

/*
 * hal_insw — 从 I/O 端口读取多个 16-bit 字
 * @port:  I/O 端口地址
 * @buf:   目标缓冲区
 * @count: 字数
 */
void hal_insw(uint16_t port, void *buf, uint32_t count)
{
    __asm__ __volatile__(
        "cld\n\t"
        "rep insw"
        : : "d"(port), "D"(buf), "c"(count) : "memory"
    );
}

/*
 * hal_outsw — 向 I/O 端口写入多个 16-bit 字
 * @port:  I/O 端口地址
 * @buf:   源缓冲区
 * @count: 字数
 */
void hal_outsw(uint16_t port, const void *buf, uint32_t count)
{
    __asm__ __volatile__(
        "cld\n\t"
        "rep outsw"
        : : "d"(port), "S"(buf), "c"(count) : "memory"
    );
}

/* ===== 中断控制 ===== */

/*
 * hal_cli — 清除中断标志 (禁用中断)
 */
void hal_cli(void)
{
    __asm__ __volatile__("cli");
}

/*
 * hal_sti — 设置中断标志 (启用中断)
 */
void hal_sti(void)
{
    __asm__ __volatile__("sti");
}

/*
 * hal_irq_save — 保存中断状态并禁用中断
 * 返回: 保存的 EFLAGS 值。
 */
uint32_t hal_irq_save(void)
{
    uint32_t flags;
    __asm__ __volatile__(
        "pushfl\n\t"
        "popl %0\n\t"
        "cli"
        : "=r"(flags)
    );
    return flags;
}

/*
 * hal_irq_restore — 恢复中断状态
 * @flags: hal_irq_save() 返回的 EFLAGS 值
 */
void hal_irq_restore(uint32_t flags)
{
    __asm__ __volatile__(
        "pushl %0\n\t"
        "popfl"
        : : "r"(flags) : "memory"
    );
}

/*
 * hal_pic_eoi — 向 PIC 发送中断结束信号 (EOI)
 * @irq: IRQ 号 (0-15)
 */
void hal_pic_eoi(uint8_t irq)
{
    if (irq >= 8)
        hal_outb(0xA0, 0x20);  /* 从 PIC EOI */
    hal_outb(0x20, 0x20);      /* 主 PIC EOI */
}

/*
 * hal_pic_mask — 设置 PIC 中断屏蔽字
 * @master: 主 PIC 屏蔽字
 * @slave:  从 PIC 屏蔽字
 */
void hal_pic_mask(uint8_t master, uint8_t slave)
{
    hal_outb(0x21, master);
    hal_outb(0xA1, slave);
}

/*
 * hal_pic_unmask — 取消屏蔽指定 IRQ
 * @irq: IRQ 号 (0-15)
 */
void hal_pic_unmask(uint8_t irq)
{
    uint16_t port;
    uint8_t mask;

    if (irq < 8) {
        port = 0x21;
    } else {
        port = 0xA1;
        irq -= 8;
    }

    mask = hal_inb(port);
    mask &= ~(1 << irq);
    hal_outb(port, mask);
}

/*
 * hal_pic_mask_irq — 屏蔽指定 IRQ
 * @irq: IRQ 号 (0-15)
 */
void hal_pic_mask_irq(uint8_t irq)
{
    uint16_t port;
    uint8_t mask;

    if (irq < 8) {
        port = 0x21;
    } else {
        port = 0xA1;
        irq -= 8;
    }

    mask = hal_inb(port);
    mask |= (1 << irq);
    hal_outb(port, mask);
}

/* ===== CPU 控制 ===== */

/*
 * hal_hlt — 停止 CPU 直到下一个中断
 */
void hal_hlt(void)
{
    __asm__ __volatile__("hlt");
}

/*
 * hal_nop — 空操作
 */
void hal_nop(void)
{
    __asm__ __volatile__("nop");
}

/*
 * hal_io_delay — I/O 端口延迟 (~1μs)
 */
void hal_io_delay(void)
{
    hal_outb(0x80, 0);
}

/*
 * hal_rdtsc — 读取时间戳计数器 (TSC)
 * 返回: 64-bit TSC 值。
 */
uint64_t hal_rdtsc(void)
{
    uint32_t lo, hi;
    __asm__ __volatile__("rdtsc" : "=a"(lo), "=d"(hi));
    return ((uint64_t)hi << 32) | lo;
}

/*
 * hal_read_cr0 — 读取 CR0 控制寄存器
 * 返回: CR0 值。
 */
uint32_t hal_read_cr0(void)
{
    uint32_t val;
    __asm__ __volatile__("movl %%cr0, %0" : "=r"(val));
    return val;
}

/*
 * hal_write_cr0 — 写入 CR0 控制寄存器
 * @val: CR0 新值
 */
void hal_write_cr0(uint32_t val)
{
    __asm__ __volatile__("movl %0, %%cr0" : : "r"(val) : "memory");
}

/*
 * hal_read_cr2 — 读取 CR2 (页错误地址)
 * 返回: CR2 值。
 */
uint32_t hal_read_cr2(void)
{
    uint32_t val;
    __asm__ __volatile__("movl %%cr2, %0" : "=r"(val));
    return val;
}

/*
 * hal_read_cr3 — 读取 CR3 (页目录基址)
 * 返回: CR3 值。
 */
uint32_t hal_read_cr3(void)
{
    uint32_t val;
    __asm__ __volatile__("movl %%cr3, %0" : "=r"(val));
    return val;
}

/*
 * hal_write_cr3 — 写入 CR3 (页目录基址)
 * @val: CR3 新值
 */
void hal_write_cr3(uint32_t val)
{
    __asm__ __volatile__("movl %0, %%cr3" : : "r"(val) : "memory");
}

/*
 * hal_read_cr4 — 读取 CR4 控制寄存器
 * 返回: CR4 值。
 */
uint32_t hal_read_cr4(void)
{
    uint32_t val;
    __asm__ __volatile__("movl %%cr4, %0" : "=r"(val));
    return val;
}

/*
 * hal_write_cr4 — 写入 CR4 控制寄存器
 * @val: CR4 新值
 */
void hal_write_cr4(uint32_t val)
{
    __asm__ __volatile__("movl %0, %%cr4" : : "r"(val) : "memory");
}

/*
 * hal_invlpg — 刷新单个页的 TLB 条目
 * @vaddr: 虚拟地址
 */
void hal_invlpg(uint32_t vaddr)
{
    __asm__ __volatile__("invlpg (%0)" : : "r"(vaddr) : "memory");
}

/*
 * hal_cpuid — 执行 CPUID 指令
 * @leaf: CPUID 叶子号
 * @sub:  子叶子号
 * @eax:  [输出] EAX
 * @ebx:  [输出] EBX
 * @ecx:  [输出] ECX
 * @edx:  [输出] EDX
 */
void hal_cpuid(uint32_t leaf, uint32_t sub,
               uint32_t *eax, uint32_t *ebx,
               uint32_t *ecx, uint32_t *edx)
{
    __asm__ __volatile__(
        "cpuid"
        : "=a"(*eax), "=b"(*ebx), "=c"(*ecx), "=d"(*edx)
        : "a"(leaf), "c"(sub)
    );
}

/* ===== 内存屏障 ===== */

/*
 * hal_memory_barrier — 完整内存屏障
 */
void hal_memory_barrier(void)
{
    __asm__ __volatile__("mfence" : : : "memory");
}

/* ===== 原子操作 ===== */

/*
 * hal_atomic_inc — 原子递增
 * @ptr: 指向 32-bit 变量的指针
 * 返回: 递增后的值。
 */
uint32_t hal_atomic_inc(volatile uint32_t *ptr)
{
    uint32_t result;
    __asm__ __volatile__(
        "lock incl %0"
        : "=m"(*ptr), "=a"(result)
        : "m"(*ptr)
        : "memory"
    );
    (void)result;
    return *ptr;
}

/*
 * hal_atomic_dec — 原子递减
 * @ptr: 指向 32-bit 变量的指针
 * 返回: 递减后的值。
 */
uint32_t hal_atomic_dec(volatile uint32_t *ptr)
{
    uint32_t result;
    __asm__ __volatile__(
        "lock decl %0"
        : "=m"(*ptr), "=a"(result)
        : "m"(*ptr)
        : "memory"
    );
    (void)result;
    return *ptr;
}

/*
 * hal_atomic_test_and_set — 原子测试并设置位
 * @ptr: 指向 32-bit 变量的指针
 * @bit: 位号 (0-31)
 * 返回: 设置前该位的值。
 */
uint32_t hal_atomic_test_and_set(volatile uint32_t *ptr, uint32_t bit)
{
    uint32_t old;
    __asm__ __volatile__(
        "lock btsl %2, %1\n\t"
        "sbbl %0, %0"
        : "=r"(old), "=m"(*ptr)
        : "r"(bit), "m"(*ptr)
        : "memory"
    );
    return old & 1;
}

/*
 * hal_spin_lock — 自旋锁获取
 * @lock: 指向锁变量的指针 (0=空闲, 1=锁定)
 *
 * 使用 xchg 原子指令实现忙等待锁。
 */
void hal_spin_lock(volatile uint32_t *lock)
{
    uint32_t tmp = 1;
    while (1) {
        __asm__ __volatile__(
            "xchgl %0, %1"
            : "=r"(tmp), "=m"(*lock)
            : "0"(tmp)
            : "memory"
        );
        if (tmp == 0)
            break;
        /* 短暂退让 */
        __asm__ __volatile__("pause");
    }
}

/*
 * hal_spin_unlock — 自旋锁释放
 * @lock: 指向锁变量的指针
 */
void hal_spin_unlock(volatile uint32_t *lock)
{
    uint32_t tmp = 0;
    __asm__ __volatile__(
        "xchgl %0, %1"
        : "=r"(tmp), "=m"(*lock)
        : "0"(tmp)
        : "memory"
    );
}

/* ===== 块设备抽象层 ===== */

/* 块设备注册表 */
static struct hal_blkdev hal_blkdev_table[HAL_MAX_BLKDEV];
static int hal_blkdev_count_val = 0;

/*
 * hal_blkdev_register — 注册块设备
 * @type:      HAL_BLKDEV_FLOPPY / ATA / ATAPI
 * @driver_id: 驱动实例号
 * @ops:       操作函数表
 * 返回: 设备 ID (>=0), 失败返回 -1。
 */
int hal_blkdev_register(uint8_t type, uint8_t driver_id,
                        struct hal_blkdev_ops *ops)
{
    if (hal_blkdev_count_val >= HAL_MAX_BLKDEV)
        return -1;

    int id = hal_blkdev_count_val++;
    hal_blkdev_table[id].type = type;
    hal_blkdev_table[id].driver_id = driver_id;
    hal_blkdev_table[id].ops = ops;
    return id;
}

/*
 * hal_blkdev_count — 获取注册的块设备数量
 * 返回: 设备数量。
 */
int hal_blkdev_count(void)
{
    return hal_blkdev_count_val;
}

/*
 * hal_blkdev_get — 获取块设备信息
 * @dev_id: 设备 ID
 * 返回: 设备指针, 无效返回 NULL。
 */
struct hal_blkdev *hal_blkdev_get(int dev_id)
{
    if (dev_id < 0 || dev_id >= hal_blkdev_count_val)
        return NULL;
    return &hal_blkdev_table[dev_id];
}

/*
 * hal_blk_read — 从块设备读取扇区
 * @dev_id: 设备 ID
 * @lba:    起始 LBA
 * @count:  扇区数
 * @buf:    目标缓冲区
 * 返回: true = 成功。
 */
bool hal_blk_read(int dev_id, uint32_t lba, uint8_t count, void *buf)
{
    struct hal_blkdev *dev = hal_blkdev_get(dev_id);
    if (!dev || !dev->ops || !dev->ops->read)
        return false;
    return dev->ops->read(lba, count, buf);
}

/*
 * hal_blk_write — 向块设备写入扇区
 * @dev_id: 设备 ID
 * @lba:    起始 LBA
 * @count:  扇区数
 * @buf:    源数据缓冲区
 * 返回: true = 成功。
 */
bool hal_blk_write(int dev_id, uint32_t lba, uint8_t count, const void *buf)
{
    struct hal_blkdev *dev = hal_blkdev_get(dev_id);
    if (!dev || !dev->ops || !dev->ops->write)
        return false;
    return dev->ops->write(lba, count, buf);
}

/* ===== 分区扫描 ===== */

/* 内部: 读取 MBR/EBR 扇区 */
static bool hal_read_sector(int dev_id, uint32_t lba, uint8_t *buf)
{
    return hal_blk_read(dev_id, lba, 1, buf);
}

/* MBR 分区表偏移 */
#define MBR_PART_TABLE_OFFSET  0x1BE
#define MBR_SIGNATURE_OFFSET   0x1FE
#define MBR_SIGNATURE          0xAA55
#define PARTITION_ENTRY_SIZE   16

/*
 * hal_mbr_scan — 扫描 MBR 主分区
 * @dev_id:   块设备 ID
 * @parts:    [输出] 分区数组
 * @max_parts: 数组大小
 * 返回: 找到的主分区数。
 */
int hal_mbr_scan(int dev_id, struct hal_part_entry *parts, int max_parts)
{
    uint8_t mbr[512];
    int count = 0;

    if (!hal_read_sector(dev_id, 0, mbr))
        return 0;

    /* 验证引导签名 */
    uint16_t sig = *(uint16_t *)(mbr + MBR_SIGNATURE_OFFSET);
    if (sig != MBR_SIGNATURE)
        return 0;

    /* 解析 4 个主分区条目 */
    for (int i = 0; i < 4 && count < max_parts; i++) {
        uint8_t *entry = mbr + MBR_PART_TABLE_OFFSET + i * PARTITION_ENTRY_SIZE;
        uint8_t type = entry[4];

        if (type == PART_TYPE_EMPTY)
            continue;

        parts[count].status      = entry[0];
        parts[count].type        = type;
        parts[count].lba_start   = *(uint32_t *)(entry + 8);
        parts[count].sector_count = *(uint32_t *)(entry + 12);

        /* 跳过扩展分区本身, 但记住其 LBA */
        if (type == PART_TYPE_EXTENDED || type == PART_TYPE_EXTENDED_LBA)
            continue;

        count++;
    }

    return count;
}

/*
 * hal_extended_scan — 递归扫描扩展分区 (EBR 链)
 * @dev_id:   块设备 ID
 * @ebr_lba:  EBR 起始 LBA
 * @parts:    [输出] 逻辑分区数组
 * @max_parts: 数组大小
 * @offset:   写入 parts 的起始索引
 * 返回: 找到的逻辑分区数。
 *
 * 扩展分区由链式 EBR 组成。每个 EBR 布局与 MBR 相同(引导扇区格式),
 * 但只有条目 1 和 2 有意义:
 *   条目 1 → 逻辑分区 (type!=0)
 *   条目 2 → 下一个 EBR (链指针, type=0 或为扩展分区类型)
 */
int hal_extended_scan(int dev_id, uint32_t ebr_lba,
                      struct hal_part_entry *parts,
                      int max_parts, int offset)
{
    uint8_t ebr[512];
    int count = 0;
    uint32_t current_ebr = ebr_lba;
    uint32_t base_lba = ebr_lba; /* 第一个 EBR 的 LBA = 扩展分区的基址 */

    while (count + offset < max_parts) {
        if (!hal_read_sector(dev_id, current_ebr, ebr))
            break;

        uint16_t sig = *(uint16_t *)(ebr + MBR_SIGNATURE_OFFSET);
        if (sig != MBR_SIGNATURE)
            break;

        /* 条目 1: 逻辑分区 */
        {
            uint8_t *entry = ebr + MBR_PART_TABLE_OFFSET;
            uint8_t type = entry[4];
            uint32_t lba = *(uint32_t *)(entry + 8);
            uint32_t size = *(uint32_t *)(entry + 12);

            if (type != PART_TYPE_EMPTY && lba != 0) {
                int idx = offset + count;
                if (idx < max_parts) {
                    parts[idx].status      = entry[0];
                    parts[idx].type        = type;
                    /* 逻辑分区的 LBA 是相对于扩展分区基址的 */
                    parts[idx].lba_start   = base_lba + lba;
                    parts[idx].sector_count = size;
                    count++;
                }
            }
        }

        /* 条目 2: 下一个 EBR */
        {
            uint8_t *entry = ebr + MBR_PART_TABLE_OFFSET + PARTITION_ENTRY_SIZE;
            uint8_t type = entry[4];
            uint32_t next_lba = *(uint32_t *)(entry + 8);

            if (type == PART_TYPE_EXTENDED || type == PART_TYPE_EXTENDED_LBA) {
                /* 下一个 EBR 的 LBA 是相对于扩展分区基址的 */
                current_ebr = ebr_lba + next_lba;
            } else {
                break; /* 链结束 */
            }
        }
    }

    return count;
}

/*
 * hal_partition_scan_all — 扫描设备上的全部分区 (主+扩展)
 * @dev_id:   块设备 ID
 * @parts:    [输出] 分区数组
 * @max_parts: 数组大小
 * 返回: 找到的全部分区数。
 *
 * 先扫描 MBR 主分区, 如果找到扩展分区则递归扫描其 EBR 链。
 */
int hal_partition_scan_all(int dev_id, struct hal_part_entry *parts,
                           int max_parts)
{
    uint8_t mbr[512];
    int count = 0;
    uint32_t ebr_lba = 0;

    if (!hal_read_sector(dev_id, 0, mbr))
        return 0;

    uint16_t sig = *(uint16_t *)(mbr + MBR_SIGNATURE_OFFSET);
    if (sig != MBR_SIGNATURE)
        return 0;

    /* 第一遍: 解析主分区, 记住扩展分区 LBA */
    for (int i = 0; i < 4 && count < max_parts; i++) {
        uint8_t *entry = mbr + MBR_PART_TABLE_OFFSET + i * PARTITION_ENTRY_SIZE;
        uint8_t type = entry[4];

        if (type == PART_TYPE_EMPTY)
            continue;

        if (type == PART_TYPE_EXTENDED || type == PART_TYPE_EXTENDED_LBA) {
            ebr_lba = *(uint32_t *)(entry + 8);
            continue;
        }

        parts[count].status      = entry[0];
        parts[count].type        = type;
        parts[count].lba_start   = *(uint32_t *)(entry + 8);
        parts[count].sector_count = *(uint32_t *)(entry + 12);
        count++;
    }

    /* 第二遍: 如果存在扩展分区, 扫描 EBR 链 */
    if (ebr_lba != 0 && count < max_parts) {
        int extra = hal_extended_scan(dev_id, ebr_lba, parts, max_parts, count);
        count += extra;
    }

    return count;
}
