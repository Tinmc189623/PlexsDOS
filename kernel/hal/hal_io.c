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
