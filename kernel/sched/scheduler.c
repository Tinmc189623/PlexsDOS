/*
 * Nexsteaduser — PlexsDOS
 * scheduler.c — 进程调度器实现
 * 作者: Tinmc189623 | 团队: Nexlyh
 *
 * 32-bit 保护模式多进程轮转调度器。
 * 管理 Ring 0/3 进程的创建、调度、销毁。
 */

#include <plexsdos/types.h>
#include <plexsdos/config.h>
#include <plexsdos/scheduler.h>
#include <plexsdos/screen.h>
#include <plexsdos/serial.h>
#include <plexsdos/hal.h>

/* ===== PCB 池 ===== */
static struct pcb pcb_pool[PROC_MAX];

/* ===== 就绪队列 ===== */
static struct ready_queue ready;
static struct pcb *current = NULL;
static int next_pid = 1;

/* 空闲进程 PCB */
static struct pcb idle_pcb;

/* ===== 内部辅助 ===== */

static struct pcb *alloc_pcb(void)
{
    for (int i = 0; i < PROC_MAX; i++) {
        if (pcb_pool[i].state == PROC_FREE)
            return &pcb_pool[i];
    }
    return NULL;
}

static void enqueue_ready(struct pcb *proc)
{
    proc->state = PROC_READY;
    proc->next = NULL;

    if (!ready.head) {
        ready.head = proc;
        ready.tail = proc;
    } else {
        ready.tail->next = proc;
        ready.tail = proc;
    }
    ready.count++;
}

static struct pcb *dequeue_ready(void)
{
    if (!ready.head)
        return NULL;

    struct pcb *proc = ready.head;
    ready.head = proc->next;
    if (!ready.head)
        ready.tail = NULL;
    proc->next = NULL;
    ready.count--;
    return proc;
}

static void remove_from_ready(struct pcb *proc)
{
    struct pcb *prev = NULL;
    struct pcb *curr = ready.head;

    while (curr) {
        if (curr == proc) {
            if (prev)
                prev->next = curr->next;
            else
                ready.head = curr->next;

            if (!curr->next)
                ready.tail = prev;

            curr->next = NULL;
            ready.count--;
            return;
        }
        prev = curr;
        curr = curr->next;
    }
}

/* ===== 上下文切换 (汇编) ===== */

/*
 * sched_switch_context — 上下文切换
 * @old_esp: 当前进程 ESP 保存位置
 * @new_esp: 新进程的 ESP
 *
 * 保存: pusha + EFLAGS
 * 恢复: popa + iret
 *
 * 注意: 这是一个简化实现, 保存通用寄存器和标志。
 * 完整实现需要保存 FPU/SSE/段寄存器等。
 */
__attribute__((naked)) void sched_switch_context(uint32_t *old_esp, uint32_t new_esp)
{
    __asm__ __volatile__(
        "pushfl\n\t"
        "pusha\n\t"

        "movl  40(%%esp), %%eax\n\t"
        "movl  %%esp, (%%eax)\n\t"

        "movl  44(%%esp), %%eax\n\t"
        "movl  %%eax, %%esp\n\t"

        "popa\n\t"
        "popfl\n\t"

        "ret\n\t"
        : : : "memory"
    );
}

/* ===== 空闲进程 ===== */

void sched_idle(void)
{
    serial_puts("[sched] idle started.\n");
    while (1) {
        hal_hlt();  /* 节省电力, 等待中断 */
    }
}

/* ===== 调度核心 ===== */

static void sched_switch_to(struct pcb *next_proc)
{
    struct pcb *prev = current;
    current = next_proc;
    current->state = PROC_RUNNING;

    /* 重置时间片 */
    current->ticks_remaining = current->time_slice;

    /* 执行上下文切换 */
    sched_switch_context(&prev->esp, next_proc->esp);
    /* 切换到新进程后不返回此处 */
}

/*
 * sched_yield — 主动让出 CPU
 */
void sched_yield(void)
{
    uint32_t flags = hal_irq_save();

    if (!current || ready.count == 0) {
        hal_irq_restore(flags);
        return;
    }

    /* 当前进程放回就绪队列 */
    if (current != &idle_pcb && current->state == PROC_RUNNING) {
        enqueue_ready(current);
    }

    /* 取出下一个就绪进程 */
    struct pcb *next = dequeue_ready();
    if (!next) {
        next = &idle_pcb;
    }

    hal_irq_restore(flags);

    if (next != current)
        sched_switch_to(next);
}

/*
 * sched_tick — 时钟 tick (由定时器中断周期性调用)
 */
void sched_tick(void)
{
    if (!current || current == &idle_pcb)
        return;

    if (current->ticks_remaining > 0) {
        current->ticks_remaining--;
        current->total_ticks++;
    }

    if (current->ticks_remaining == 0) {
        /* 时间片用完, 请求调度
         * 注意: 在中断上下文中, 需要设置标志让中断返回时切换 */
        sched_yield();
    }
}

/*
 * sched_block — 阻塞当前进程
 */
void sched_block(void)
{
    uint32_t flags = hal_irq_save();

    if (current && current->state == PROC_RUNNING) {
        current->state = PROC_BLOCKED;
    }

    struct pcb *next = dequeue_ready();
    if (!next)
        next = &idle_pcb;

    hal_irq_restore(flags);
    sched_switch_to(next);
}

/*
 * sched_unblock — 解除进程阻塞
 */
bool sched_unblock(int pid)
{
    struct pcb *proc = sched_get_pcb(pid);
    if (!proc || proc->state != PROC_BLOCKED)
        return false;

    enqueue_ready(proc);
    return true;
}

/* ===== 进程管理 ===== */

/* 内核线程包装: 调用 entry 函数, 返回后自动退出 */
static void sched_thread_wrapper(uint32_t entry)
{
    void (*func)(void) = (void (*)(void))entry;
    func();
    sched_exit(0);
}

/*
 * sched_create_process — 创建新内核线程
 * @name:    线程名
 * @entry:   入口函数地址
 * @flags:   PROC_FLAG_KERNEL 或 PROC_FLAG_USER
 * 返回: PID, 失败返回 -1。
 *
 * 内核线程: 在 Ring 0 运行。
 * 栈帧布局 (从高到低):
 *   [返回地址 → entry]    — sched_thread_wrapper 的返回地址
 *   [pusha × 8]           — 通用寄存器 (初始为 0)
 *   [EFLAGS]              — 标志寄存器 (IF=1)
 *   ↑ ESP 指向这里
 */
int sched_create_process(const char *name, uint32_t entry,
                         uint32_t user_esp, uint32_t flags)
{
    (void)user_esp;  /* 用户栈仅用于 PROC_FLAG_USER (暂不支持) */
    struct pcb *pcb = alloc_pcb();
    if (!pcb) {
        screen_puts("[sched] no free PCB\n");
        return -1;
    }

    uint32_t pid = next_pid++;
    if (pid == 0)   /* PID 回绕保护 */
        pid = next_pid++;
    pcb->pid = pid;
    pcb->state = PROC_READY;
    pcb->flags = PROC_FLAG_KERNEL;  /* 当前仅支持内核线程 */
    pcb->time_slice = PROC_TIME_SLICE_DEFAULT;
    pcb->ticks_remaining = pcb->time_slice;
    pcb->total_ticks = 0;
    pcb->next = NULL;

    /* 复制进程名 */
    int i;
    for (i = 0; name[i] && i < 31; i++)
        pcb->name[i] = name[i];
    pcb->name[i] = '\0';

    /* 分配内核栈 (2KB/进程) */
    static uint32_t kernel_stack_pool[PROC_MAX][512];
    int pool_idx = (int)(pcb - pcb_pool);  /* PCB 在池中的索引 */
    uint32_t *stack = &kernel_stack_pool[pool_idx][512];

    /* 构造上下文切换帧:
     * 当 sched_switch_context 切换到本进程时:
     *   popfl → 恢复 EFLAGS
     *   popa  → 恢复通用寄存器
     *   ret   → 跳转到 sched_thread_wrapper
     * sched_thread_wrapper 调用 entry, 返回后调用 sched_exit */
    *--stack = entry;                     /* ret 地址 → sched_thread_wrapper 调用 entry */
    *--stack = 0; /* edi */              /* pusha 恢复顺序: edi, esi, ebp, esp, ebx, edx, ecx, eax */
    *--stack = 0; /* esi */
    *--stack = 0; /* ebp */
    *--stack = 0; /* esp (popa 丢弃此值) */
    *--stack = 0; /* ebx */
    *--stack = 0; /* edx */
    *--stack = 0; /* ecx */
    *--stack = 0; /* eax */
    *--stack = 0x0200;                    /* EFLAGS: IF=1, IOPL=0 */

    pcb->kernel_esp = (uint32_t)stack;
    pcb->esp = (uint32_t)stack;
    pcb->ss  = 0x10;  /* 内核数据段 */

    /* 加入就绪队列 */
    enqueue_ready(pcb);

    serial_puts("[sched] created process #");
    serial_put_hex(pid);
    serial_puts(": ");
    serial_puts(name);
    serial_putchar('\n');

    return pid;
}

/*
 * sched_exit — 终止当前进程
 *
 * 保存退出进程 PCB 指针, 标记为 FREE 以回收 PCB 和内核栈。
 * 当前无 waitpid 机制, Zombie 状态无意义且会泄漏 PCB 池。
 */
void sched_exit(int status)
{
    struct pcb *zombie = current;

    if (!zombie || zombie == &idle_pcb)
        return;

    serial_puts("[sched] process #");
    serial_put_hex(zombie->pid);
    serial_puts(" exited with status ");
    serial_put_hex((uint32_t)status);
    serial_putchar('\n');

    /* 立即回收 PCB (无 waitpid, Zombie 无用) */
    zombie->state = PROC_FREE;

    /* 切换到下一个进程 */
    struct pcb *next = dequeue_ready();
    if (!next)
        next = &idle_pcb;

    current = next;
    current->state = PROC_RUNNING;
    current->ticks_remaining = current->time_slice;

    /* 恢复新进程上下文 */
    __asm__ __volatile__(
        "movl  %0, %%esp\n\t"
        "popa\n\t"
        "popfl\n\t"
        "ret\n\t"
        : : "r"(next->esp) : "memory"
    );
    /* 不返回 */
    while (1) hal_hlt();
}

/* ===== 查询函数 ===== */

struct pcb *sched_get_pcb(int pid)
{
    for (int i = 0; i < PROC_MAX; i++) {
        if (pcb_pool[i].pid == pid && pcb_pool[i].state != PROC_FREE)
            return &pcb_pool[i];
    }
    return NULL;
}

struct pcb *sched_get_current(void)
{
    return current;
}

/* ===== 初始化 ===== */

/*
 * sched_init — 初始化进程调度器
 */
void sched_init(void)
{
    /* 初始化 PCB 池 */
    for (int i = 0; i < PROC_MAX; i++)
        pcb_pool[i].state = PROC_FREE;

    /* 初始化就绪队列 */
    ready.head = NULL;
    ready.tail = NULL;
    ready.count = 0;

    /* 初始化空闲进程 */
    idle_pcb.pid = 0;
    idle_pcb.state = PROC_READY;
    idle_pcb.flags = PROC_FLAG_SYSTEM;
    idle_pcb.name[0] = 'i'; idle_pcb.name[1] = 'd';
    idle_pcb.name[2] = 'l'; idle_pcb.name[3] = 'e';
    idle_pcb.name[4] = '\0';
    idle_pcb.esp = 0;
    idle_pcb.ss = 0x10;
    idle_pcb.time_slice = 0;  /* 空闲进程无时间片限制 */

    current = &idle_pcb;

    serial_puts("[sched] initialized.\n");
}
