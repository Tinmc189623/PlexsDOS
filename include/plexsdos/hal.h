/*
 * Nexsteaduser — PlexsDOS
 * hal.h — 硬件抽象层接口 (Hardware Abstraction Layer)
 * 作者: Tinmc189623 | 团队: Nexlyh
 *
 * 集中管理所有底层硬件访问:
 *   - 端口 I/O (inb/outb/inw/outw/inl/outl)
 *   - 中断控制 (cli/sti/save/restore, PIC 操作)
 *   - CPU 控制寄存器 (CR0/CR2/CR3/CR4)
 *   - TSC 时间戳计数器
 *   - 内存屏障
 *   - I/O 延迟
 *
 * 所有驱动程序应通过 HAL 访问硬件, 而非直接使用内联汇编。
 */

#ifndef _PLXSDOS_HAL_H
#define _PLXSDOS_HAL_H

#include <plexsdos/types.h>

#ifdef __cplusplus
extern "C" {
#endif

/* ===== 常量 ===== */

/* 块设备类型 */
#define HAL_BLKDEV_NONE     0
#define HAL_BLKDEV_FLOPPY   1
#define HAL_BLKDEV_ATA      2
#define HAL_BLKDEV_ATAPI    3

#define HAL_MAX_BLKDEV      8       /* 最大注册块设备数 */
#define HAL_MAX_PARTITIONS  16      /* 最大分区数 */

/* 分区类型 (MBR) */
#define PART_TYPE_EMPTY     0x00
#define PART_TYPE_FAT12     0x01
#define PART_TYPE_FAT16     0x06
#define PART_TYPE_FAT32     0x0B
#define PART_TYPE_FAT32_LBA 0x0C
#define PART_TYPE_EXTENDED  0x05    /* CHS 扩展分区 */
#define PART_TYPE_EXTENDED_LBA 0x0F /* LBA 扩展分区 */

/* ===== 初始化 ===== */

/*
 * hal_init — 初始化硬件抽象层
 *
 * 初始化 HAL 子系统、检测 CPU 特性、PIC 状态。
 * 由 kernel_main() 在各子系统初始化前调用。
 */
void hal_init(void);

/* ===== 块设备抽象层 ===== */

/* 块设备操作函数表 */
struct hal_blkdev_ops {
    /*
     * read — 从块设备读取扇区
     * @lba:   起始 LBA
     * @count: 扇区数
     * @buf:   目标缓冲区
     * 返回: true = 成功。
     */
    bool (*read)(uint32_t lba, uint8_t count, void *buf);

    /*
     * write — 向块设备写入扇区
     * @lba:   起始 LBA
     * @count: 扇区数
     * @buf:   源数据缓冲区
     * 返回: true = 成功。
     */
    bool (*write)(uint32_t lba, uint8_t count, const void *buf);
};

/* 块设备实例 */
struct hal_blkdev {
    uint8_t  type;                /* HAL_BLKDEV_* */
    uint8_t  driver_id;           /* 驱动实例号 */
    struct hal_blkdev_ops *ops;   /* I/O 操作 */
};

/*
 * hal_blkdev_register — 注册块设备
 * @type:      HAL_BLKDEV_FLOPPY / ATA / ATAPI
 * @driver_id: 驱动实例号 (FDC drive 0/1, ATA master=0)
 * @ops:       操作函数表
 * 返回: 设备 ID (>=0), 失败返回 -1。
 */
int hal_blkdev_register(uint8_t type, uint8_t driver_id,
                        struct hal_blkdev_ops *ops);

/*
 * hal_blkdev_count — 获取注册的块设备数量
 * 返回: 设备数量。
 */
int hal_blkdev_count(void);

/*
 * hal_blkdev_get — 获取块设备信息
 * @dev_id: 设备 ID
 * 返回: 设备指针, 无效返回 NULL。
 */
struct hal_blkdev *hal_blkdev_get(int dev_id);

/*
 * hal_blk_read — 从块设备读取扇区
 * @dev_id: 设备 ID
 * @lba:    起始 LBA
 * @count:  扇区数
 * @buf:    目标缓冲区
 * 返回: true = 成功。
 */
bool hal_blk_read(int dev_id, uint32_t lba, uint8_t count, void *buf);

/*
 * hal_blk_write — 向块设备写入扇区
 * @dev_id: 设备 ID
 * @lba:    起始 LBA
 * @count:  扇区数
 * @buf:    源数据缓冲区
 * 返回: true = 成功。
 */
bool hal_blk_write(int dev_id, uint32_t lba, uint8_t count, const void *buf);

/* ===== 分区扫描 ===== */

/* MBR 分区条目 */
struct hal_part_entry {
    uint8_t  status;         /* 0x80 = 活动 (可引导) */
    uint8_t  type;           /* 分区类型 */
    uint32_t lba_start;      /* 起始 LBA (绝对) */
    uint32_t sector_count;   /* 扇区数 */
};

/*
 * hal_mbr_scan — 扫描 MBR 主分区
 * @dev_id:   块设备 ID
 * @parts:    [输出] 分区数组
 * @max_parts: 数组大小
 * 返回: 找到的主分区数。
 *
 * 读取设备的 LBA 0 (MBR), 解析 4 个主分区条目。
 * 扩展分区本身不计入, 但其内的逻辑分区通过 hal_extended_scan 获取。
 */
int hal_mbr_scan(int dev_id, struct hal_part_entry *parts, int max_parts);

/*
 * hal_extended_scan — 递归扫描扩展分区 (EBR 链)
 * @dev_id:   块设备 ID
 * @ebr_lba:  EBR 起始 LBA (来自主扩展分区的 lba_start)
 * @parts:    [输出] 逻辑分区数组
 * @max_parts: 数组大小
 * @offset:   写入 parts 的起始索引
 * 返回: 找到的逻辑分区数。
 *
 * 遍历 EBR 链:
 *   每个 EBR 中:
 *     条目 1 → 逻辑分区数据区
 *     条目 2 → 下一个 EBR 的 LBA (链指针, 0=结束)
 */
int hal_extended_scan(int dev_id, uint32_t ebr_lba,
                      struct hal_part_entry *parts,
                      int max_parts, int offset);

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
                           int max_parts);

/* ===== 端口 I/O ===== */

/*
 * hal_inb — 从 I/O 端口读取一个字节
 * @port: I/O 端口地址 (0x0000 - 0xFFFF)
 * 返回: 读取的字节值。
 */
uint8_t hal_inb(uint16_t port);

/*
 * hal_outb — 向 I/O 端口写入一个字节
 * @port: I/O 端口地址
 * @val:  要写入的字节值
 */
void hal_outb(uint16_t port, uint8_t val);

/*
 * hal_inw — 从 I/O 端口读取一个 16-bit 字
 * @port: I/O 端口地址
 * 返回: 读取的字值。
 */
uint16_t hal_inw(uint16_t port);

/*
 * hal_outw — 向 I/O 端口写入一个 16-bit 字
 * @port: I/O 端口地址
 * @val:  要写入的字值
 */
void hal_outw(uint16_t port, uint16_t val);

/*
 * hal_inl — 从 I/O 端口读取一个 32-bit 双字
 * @port: I/O 端口地址
 * 返回: 读取的双字值。
 */
uint32_t hal_inl(uint16_t port);

/*
 * hal_outl — 向 I/O 端口写入一个 32-bit 双字
 * @port: I/O 端口地址
 * @val:  要写入的双字值
 */
void hal_outl(uint16_t port, uint32_t val);

/*
 * hal_insw — 从 I/O 端口读取多个 16-bit 字 (串行输入)
 * @port: I/O 端口地址
 * @buf:  目标缓冲区
 * @count: 字数
 */
void hal_insw(uint16_t port, void *buf, uint32_t count);

/*
 * hal_outsw — 向 I/O 端口写入多个 16-bit 字 (串行输出)
 * @port: I/O 端口地址
 * @buf:  源缓冲区
 * @count: 字数
 */
void hal_outsw(uint16_t port, const void *buf, uint32_t count);

/* ===== 中断控制 ===== */

/*
 * hal_cli — 清除中断标志 (禁用中断)
 *
 * 执行 cli 指令。返回前一个中断状态 (用于嵌套禁用)。
 */
void hal_cli(void);

/*
 * hal_sti — 设置中断标志 (启用中断)
 *
 * 执行 sti 指令。
 */
void hal_sti(void);

/*
 * hal_irq_save — 保存中断状态并禁用中断
 * 返回: 保存的 EFLAGS 值 (包含先前的 IF 状态)。
 *
 * 用于嵌套中断禁用: flags = hal_irq_save(); ... hal_irq_restore(flags);
 */
uint32_t hal_irq_save(void);

/*
 * hal_irq_restore — 恢复中断状态
 * @flags: hal_irq_save() 返回的 EFLAGS 值
 */
void hal_irq_restore(uint32_t flags);

/*
 * hal_pic_eoi — 向 PIC 发送中断结束信号 (EOI)
 * @irq: IRQ 号 (0-15)
 *
 * 如果 IRQ >= 8, 同时向从 PIC 发送 EOI。
 */
void hal_pic_eoi(uint8_t irq);

/*
 * hal_pic_mask — 设置 PIC 中断屏蔽字
 * @master: 主 PIC 屏蔽字 (IRQ 0-7, 1=屏蔽)
 * @slave:  从 PIC 屏蔽字 (IRQ 8-15, 1=屏蔽)
 */
void hal_pic_mask(uint8_t master, uint8_t slave);

/*
 * hal_pic_unmask — 取消屏蔽指定 IRQ
 * @irq: IRQ 号 (0-15)
 */
void hal_pic_unmask(uint8_t irq);

/*
 * hal_pic_mask_irq — 屏蔽指定 IRQ
 * @irq: IRQ 号 (0-15)
 */
void hal_pic_mask_irq(uint8_t irq);

/* ===== CPU 控制 ===== */

/*
 * hal_hlt — 停止 CPU 直到下一个中断
 *
 * 执行 hlt 指令。用于空闲循环。
 */
void hal_hlt(void);

/*
 * hal_nop — 空操作
 *
 * 执行 nop 指令。用于短暂延迟或编译器屏障。
 */
void hal_nop(void);

/*
 * hal_io_delay — I/O 端口延迟
 *
 * 通过端口 0x80 执行一次 I/O 操作, 产生约 1μs 延迟。
 * 用于需要短暂等待的硬件时序。
 */
void hal_io_delay(void);

/*
 * hal_rdtsc — 读取时间戳计数器 (TSC)
 * 返回: 64-bit TSC 值。
 *
 * 需要 CPU 支持 TSC 特性。
 */
uint64_t hal_rdtsc(void);

/*
 * hal_read_cr0 — 读取 CR0 控制寄存器
 * 返回: CR0 值。
 */
uint32_t hal_read_cr0(void);

/*
 * hal_write_cr0 — 写入 CR0 控制寄存器
 * @val: CR0 新值
 */
void hal_write_cr0(uint32_t val);

/*
 * hal_read_cr2 — 读取 CR2 (页错误地址)
 * 返回: CR2 值。
 */
uint32_t hal_read_cr2(void);

/*
 * hal_read_cr3 — 读取 CR3 (页目录基址)
 * 返回: CR3 值。
 */
uint32_t hal_read_cr3(void);

/*
 * hal_write_cr3 — 写入 CR3 (页目录基址)
 * @val: CR3 新值
 */
void hal_write_cr3(uint32_t val);

/*
 * hal_read_cr4 — 读取 CR4 控制寄存器
 * 返回: CR4 值。
 */
uint32_t hal_read_cr4(void);

/*
 * hal_write_cr4 — 写入 CR4 控制寄存器
 * @val: CR4 新值
 */
void hal_write_cr4(uint32_t val);

/*
 * hal_invlpg — 刷新单个页的 TLB 条目
 * @vaddr: 虚拟地址
 *
 * 执行 invlpg 指令。
 */
void hal_invlpg(uint32_t vaddr);

/*
 * hal_cpuid — 执行 CPUID 指令
 * @leaf:   CPUID 叶子号 (EAX 输入)
 * @sub:    子叶子号 (ECX 输入)
 * @eax:    [输出] EAX 寄存器值
 * @ebx:    [输出] EBX 寄存器值
 * @ecx:    [输出] ECX 寄存器值
 * @edx:    [输出] EDX 寄存器值
 */
void hal_cpuid(uint32_t leaf, uint32_t sub,
               uint32_t *eax, uint32_t *ebx,
               uint32_t *ecx, uint32_t *edx);

/* ===== 内存屏障 ===== */

/*
 * hal_memory_barrier — 完整内存屏障
 *
 * 确保所有之前的内存操作在后续操作之前完成。
 */
void hal_memory_barrier(void);

/*
 * hal_compiler_barrier — 编译器屏障
 *
 * 防止编译器跨此点重排内存访问。
 */
#define hal_compiler_barrier() __asm__ __volatile__("" : : : "memory")

/* ===== 原子操作 ===== */

/*
 * hal_atomic_inc — 原子递增
 * @ptr: 指向 32-bit 变量的指针
 * 返回: 递增后的值。
 */
uint32_t hal_atomic_inc(volatile uint32_t *ptr);

/*
 * hal_atomic_dec — 原子递减
 * @ptr: 指向 32-bit 变量的指针
 * 返回: 递减后的值。
 */
uint32_t hal_atomic_dec(volatile uint32_t *ptr);

/*
 * hal_atomic_test_and_set — 原子测试并设置
 * @ptr: 指向 32-bit 变量的指针
 * @bit: 要设置的位号 (0-31)
 * 返回: 设置前该位的值 (0 或 1)。
 */
uint32_t hal_atomic_test_and_set(volatile uint32_t *ptr, uint32_t bit);

/*
 * hal_spin_lock — 自旋锁获取
 * @lock: 指向锁变量的指针 (0=空闲, 1=锁定)
 *
 * 忙等待直到获取锁。使用原子 xchg 指令。
 */
void hal_spin_lock(volatile uint32_t *lock);

/*
 * hal_spin_unlock — 自旋锁释放
 * @lock: 指向锁变量的指针
 */
void hal_spin_unlock(volatile uint32_t *lock);

#ifdef __cplusplus
}
#endif

#endif /* _PLXSDOS_HAL_H */
