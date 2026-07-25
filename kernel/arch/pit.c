/*
 * Nexsteaduser — PlexsDOS
 * pit.c — 8253/8254 可编程间隔定时器 (PIT) 驱动
 * 作者: Tinmc189623 | 团队: Nexlyh
 *
 * 初始化 PIT 通道 0 产生周期性时钟中断 (IRQ0 = INT 0x20),
 * 作为进程调度器的时间基准 (~100Hz, 10ms/tick)。
 */

#include <plexsdos/types.h>
#include <plexsdos/config.h>
#include <plexsdos/hal.h>
#include <plexsdos/scheduler.h>
#include <plexsdos/serial.h>

/* PIT 硬件端口 */
#define PIT_CHANNEL0_DATA  0x40
#define PIT_COMMAND_PORT   0x43

/* PIC 端口 */
#define PIC_MASTER_CMD     0x20
#define PIC_MASTER_DATA    0x21
#define PIC_EOI            0x20

/*
 * inb — 从 I/O 端口读取一个字节
 * @port: 端口号 (0-65535)
 */
static inline uint8_t inb(uint16_t port)
{
    uint8_t val;
    __asm__ __volatile__("inb %1, %0" : "=a"(val) : "Nd"(port));
    return val;
}

/*
 * outb — 向 I/O 端口写入一个字节
 * @port: 端口号
 * @val:  要写入的值
 */
static inline void outb(uint16_t port, uint8_t val)
{
    __asm__ __volatile__("outb %0, %1" : : "a"(val), "Nd"(port));
}

/*
 * pit_set_frequency — 设置 PIT 通道 0 的中断频率
 * @hz: 期望的频率 (Hz), 推荐 100Hz
 *
 * PIT 输入时钟为 1193182 Hz。
 * 分频值 = 1193182 / hz, 写入通道 0 (方式 3: 方波发生器)。
 */
static void pit_set_frequency(uint32_t hz)
{
    uint32_t divisor = 1193182 / hz;

    /* 命令字: 通道0, 先低后高字节, 方式3(方波), 16位二进制 */
    outb(PIT_COMMAND_PORT, 0x36);

    /* 写入低字节, 再写高字节 */
    outb(PIT_CHANNEL0_DATA, (uint8_t)(divisor & 0xFF));
    outb(PIT_CHANNEL0_DATA, (uint8_t)((divisor >> 8) & 0xFF));
}

/*
 * pit_interrupt_handler — IRQ0 定时器中断处理程序 (C 部分)
 *
 * 由汇编 isr_timer 桩调用。
 * 更新调度器 tick 计数, 发送 EOI。
 * 是否切换进程由汇编入口在返回前检查 sched_need_resched() 决定。
 */
void pit_interrupt_handler(void)
{
    sched_tick();

    /* 发送 EOI 到主 PIC */
    outb(PIC_MASTER_CMD, PIC_EOI);
}

/*
 * pit_init — 初始化 PIT 定时器
 *
 * 设置频率为 100Hz, 并通过 idt_set_gate 注册 IRQ0 处理程序。
 * 注意: isr_timer 汇编桩必须已在 interrupt.asm 中定义并声明为 extern。
 */
extern void isr_timer(void);
void idt_set_gate(uint8_t vector, void (*handler)(void));

void pit_init(void)
{
    pit_set_frequency(100);  /* 100 Hz = 10ms per tick */
    idt_set_gate(0x20, isr_timer);

    /* 确保 IRQ0 (PIT) 在 PIC 中未被屏蔽 */
    uint8_t mask = inb(PIC_MASTER_DATA);
    mask &= ~0x01;  /* 清除 bit 0, 启用 IRQ0 */
    outb(PIC_MASTER_DATA, mask);

    serial_puts("[pit] PIT initialized at 100 Hz.\n");
}
