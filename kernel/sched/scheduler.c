/*
 * Nexsteaduser — PlexsDOS
 * scheduler.c — 进程调度器实现
 * 作者: Tinmc189623 | 团队: Nexlyh
 *
 * 32-bit 保护模式多进程轮转调度器。
 * 管理 Ring 0/3 进程的创建、调度、销毁。
 * 所有进程统一使用与中断入口一致的栈帧进行上下文切换。
 *
 * 栈帧布局 (从低地址到高地址, 即栈顶到栈底):
 *   [EDI, ESI, EBP, ESP, EBX, EDX, ECX, EAX]  ← pushad (32 字节)
 *   [GS, FS, ES, DS]                           ← 段寄存器 (16 字节)
 *   [EIP, CS, EFLAGS]                          ← iret 帧 (Ring 0, 12 字节)
 *   [EIP, CS, EFLAGS, ESP, SS]                 ← iret 帧 (Ring 3, 20 字节)
 *
 * 恢复顺序: pop gs → pop fs → pop es → pop ds → popad → iret
 * 段寄存器由内核在切换时设置, Ring 3 程序入口可自行设置用户段。
 */

#include <plexsdos/types.h>
#include <plexsdos/config.h>
#include <plexsdos/scheduler.h>
#include <plexsdos/screen.h>
#include <plexsdos/serial.h>
#include <plexsdos/hal.h>
#include <plexsdos/gdt.h>

/* ===== PCB 池 ===== */
static struct pcb pcb_pool[PROC_MAX];

/* ===== 就绪队列 ===== */
static struct ready_queue ready;
struct pcb *current = NULL;
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

/* ===== 上下文切换核心 (汇编) ===== */

/*
 * sched_restore_context — 恢复进程上下文并通过 iret 跳转
 * @new_esp: 新进程的内核栈 (指向 pushad 结构的 EDI 位置, 即栈顶)
 *
 * 从新内核栈恢复: pop gs/fs/es/ds → popad → iret
 * 进入 Ring 3 或 Ring 0 取决于 iret 帧中的 CS RPL。
 */
__attribute__((naked)) void sched_restore_context(uint32_t new_esp)
{
    __asm__ __volatile__(
        "movl 4(%%esp), %%eax\n\t"    /* eax = new_esp 参数 */
        "movl %%eax, %%esp\n\t"      /* 切换到新进程的内核栈 */
        "pop %%gs\n\t"
        "pop %%fs\n\t"
        "pop %%es\n\t"
        "pop %%ds\n\t"
        "popal\n\t"                   /* 恢复通用寄存器 (AT&T: popal = Intel: popad) */
        "iret\n\t"                   /* 中断返回, 跳转 */
        : : : "memory"
    );
}

/* ===== 空闲进程 ===== */

void sched_idle(void)
{
    serial_puts("[sched] idle started.\n");
    while (1) {
        __asm__ __volatile__("sti; hlt");
    }
}

/* ===== 调度核心 ===== */

static void sched_switch_to(struct pcb *next_proc)
{
    current = next_proc;
    current->state = PROC_RUNNING;
    current->ticks_remaining = current->time_slice;

    if (current->flags & PROC_FLAG_USER) {
        tss_set_esp0(current->kernel_esp_top);
    }

    sched_restore_context(current->kernel_esp);
}

/*
 * sched_do_switch_asm — 从汇编中断入口调用的切换函数
 * @context_esp: 当前被中断进程的内核栈指针 (指向 pushad 的 EDI)
 *
 * 由定时器中断或 SYS_EXIT 调用。
 * 保存当前 ESP 到 current->kernel_esp, 然后切换到下一个进程。
 * 如果没有其他进程, 则返回 (恢复中断的进程继续执行)。
 */
void sched_do_switch_asm(uint32_t context_esp);
void sched_do_switch_asm(uint32_t context_esp)
{
    if (!current)
        return;

    /* 保存当前进程的上下文栈指针 */
    current->kernel_esp = context_esp;

    if (current != &idle_pcb && current->state == PROC_RUNNING) {
        enqueue_ready(current);
    }

    struct pcb *next = dequeue_ready();
    if (!next)
        next = &idle_pcb;

    if (next == current)
        return;

    sched_switch_to(next);
}

/*
 * sched_yield — 主动让出 CPU
 */
void sched_yield(void)
{
    uint32_t flags = hal_irq_save();

    if (!current) {
        hal_irq_restore(flags);
        return;
    }

    if (ready.count == 0) {
        hal_irq_restore(flags);
        return;
    }

    if (current != &idle_pcb && current->state == PROC_RUNNING) {
        enqueue_ready(current);
    }

    struct pcb *next = dequeue_ready();
    if (!next)
        next = &idle_pcb;

    if (next == current) {
        hal_irq_restore(flags);
        return;
    }

    hal_irq_restore(flags);
    sched_switch_to(next);
}

/*
 * sched_tick — 时钟 tick (由定时器中断周期性调用)
 * 由 IRQ0 中断处理程序在保存完整上下文后调用。
 */
void sched_tick(void)
{
    if (!current || current == &idle_pcb)
        return;

    if (current->ticks_remaining > 0) {
        current->ticks_remaining--;
        current->total_ticks++;
    }
}

/*
 * sched_need_resched — 检查是否需要调度
 * 返回: 1 = 需要切换进程, 0 = 继续当前进程
 */
int sched_need_resched(void)
{
    if (!current || current == &idle_pcb)
        return ready.count > 0;

    return current->ticks_remaining == 0;
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

/*
 * sched_create_process — 创建新进程
 * @name:    进程名
 * @entry:   入口函数地址
 * @user_esp: 用户栈顶 (Ring 3 进程使用)
 * @flags:   PROC_FLAG_KERNEL 或 PROC_FLAG_USER
 * 返回: PID, 失败返回 -1。
 */
int sched_create_process(const char *name, uint32_t entry,
                         uint32_t user_esp, uint32_t flags)
{
    struct pcb *pcb = alloc_pcb();
    if (!pcb) {
        screen_puts("[sched] no free PCB\n");
        return -1;
    }

    uint32_t pid = next_pid++;
    if (pid == 0)
        pid = next_pid++;
    pcb->pid = pid;
    pcb->state = PROC_READY;
    pcb->flags = flags;
    pcb->time_slice = PROC_TIME_SLICE_DEFAULT;
    pcb->ticks_remaining = pcb->time_slice;
    pcb->total_ticks = 0;
    pcb->next = NULL;
    pcb->page_dir = 0;

    int i;
    for (i = 0; name[i] && i < 31; i++)
        pcb->name[i] = name[i];
    pcb->name[i] = '\0';

    /* 分配内核栈 (2KB/进程) */
    static uint32_t kernel_stack_pool[PROC_MAX][512];
    int pool_idx = (int)(pcb - pcb_pool);
    uint32_t *kstack = &kernel_stack_pool[pool_idx][512];
    uint32_t *kstack_top = kstack;

    if (flags & PROC_FLAG_USER) {
        pcb->ss = GDT_SEL_USER_DATA;
        pcb->esp = user_esp;

        /* Ring 3 iret 帧 (从高到低压栈): SS, ESP, EFLAGS, CS, EIP */
        *--kstack = GDT_SEL_USER_DATA;     /* SS */
        *--kstack = user_esp;              /* ESP */
        *--kstack = 0x202;                 /* EFLAGS (IF=1) */
        *--kstack = GDT_SEL_USER_CODE;     /* CS */
        *--kstack = entry;                 /* EIP */
    } else {
        pcb->ss = GDT_SEL_KERNEL_DATA;
        pcb->esp = 0;

        /* Ring 0 iret 帧: EFLAGS, CS, EIP */
        *--kstack = 0x202;                 /* EFLAGS (IF=1) */
        *--kstack = GDT_SEL_KERNEL_CODE;   /* CS */
        *--kstack = entry;                 /* EIP */
    }

    /*
     * pushad 结构 (从高到低压栈): EAX, ECX, EDX, EBX, ESP, EBP, ESI, EDI
     * popad 从栈顶弹出顺序: EDI, ESI, EBP, ESP, EBX, EDX, ECX, EAX
     * 所以 EDI 最后压入 (在栈顶方向), EAX 最先压入 (靠近 iret 帧)
     */
    *--kstack = 0; /* eax */
    *--kstack = 0; /* ecx */
    *--kstack = 0; /* edx */
    *--kstack = 0; /* ebx */
    *--kstack = 0; /* esp (ignored by popad) */
    *--kstack = 0; /* ebp */
    *--kstack = 0; /* esi */
    *--kstack = 0; /* edi */

    /*
     * 段寄存器 (DS/ES/FS/GS):
     *   - Ring 0 进程: 使用内核数据段选择子 (GDT_SEL_KERNEL_DATA)
     *   - Ring 3 进程: 使用用户数据段选择子 (GDT_SEL_USER_DATA)
     *     注意: iret 不会自动恢复 DS/ES/FS/GS, 必须由内核在上下文切换时设置正确,
     *     否则 Ring 3 代码执行第一条访存指令就会触发 #GP 异常。
     * 压栈顺序对应 ISR_SAVE_REGS: push ds → push es → push fs → push gs
     * 所以 DS 先压 (靠近 pushad), GS 最后压 (栈顶)
     * pop 顺序: pop gs → pop fs → pop es → pop ds
     */
    {
        uint32_t data_sel = (flags & PROC_FLAG_USER)
                            ? GDT_SEL_USER_DATA
                            : GDT_SEL_KERNEL_DATA;
        *--kstack = data_sel; /* DS */
        *--kstack = data_sel; /* ES */
        *--kstack = data_sel; /* FS */
        *--kstack = data_sel; /* GS */  /* 栈顶 */
    }

    pcb->kernel_esp = (uint32_t)kstack;
    pcb->kernel_esp_top = (uint32_t)kstack_top;

    enqueue_ready(pcb);

    serial_puts("[sched] created ");
    serial_puts(flags & PROC_FLAG_USER ? "user" : "kernel");
    serial_puts(" process #");
    serial_put_hex(pid);
    serial_puts(": ");
    serial_puts(name);
    serial_putchar('\n');

    return pid;
}

/*
 * sched_exit — 终止当前进程
 * 仅供内核线程直接调用 (非中断上下文)。
 */
void sched_exit(int status)
{
    struct pcb *zombie = current;
    uint32_t flags = hal_irq_save();

    if (!zombie || zombie == &idle_pcb) {
        hal_irq_restore(flags);
        return;
    }

    serial_puts("[sched] process #");
    serial_put_hex(zombie->pid);
    serial_puts(" exited with status ");
    serial_put_hex((uint32_t)status);
    serial_putchar('\n');

    zombie->state = PROC_FREE;

    struct pcb *next = dequeue_ready();
    if (!next)
        next = &idle_pcb;

    hal_irq_restore(flags);
    sched_switch_to(next);

    while (1) hal_hlt();
}

/*
 * sched_exit_asm — 从汇编中断入口终止当前进程
 * @context_esp: 当前进程的内核栈指针 (指向 pushad 的 EDI)
 * @status: 退出状态码
 *
 * 由 SYS_EXIT 系统调用的 C 分发函数调用。
 * 与 sched_do_switch_asm 类似, 但不将当前进程重新入队 (直接释放)。
 */
void sched_exit_asm(uint32_t context_esp, int status)
{
    struct pcb *zombie = current;

    (void)context_esp;

    if (!zombie || zombie == &idle_pcb)
        return;

    serial_puts("[sched] process #");
    serial_put_hex(zombie->pid);
    serial_puts(" exited with status ");
    serial_put_hex((uint32_t)status);
    serial_putchar('\n');

    zombie->state = PROC_FREE;

    struct pcb *next = dequeue_ready();
    if (!next)
        next = &idle_pcb;

    sched_switch_to(next);

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

void sched_init(void)
{
    for (int i = 0; i < PROC_MAX; i++)
        pcb_pool[i].state = PROC_FREE;

    ready.head = NULL;
    ready.tail = NULL;
    ready.count = 0;

    idle_pcb.pid = 0;
    idle_pcb.state = PROC_READY;
    idle_pcb.flags = PROC_FLAG_SYSTEM | PROC_FLAG_KERNEL;
    idle_pcb.name[0] = 'i'; idle_pcb.name[1] = 'd';
    idle_pcb.name[2] = 'l'; idle_pcb.name[3] = 'e';
    idle_pcb.name[4] = '\0';
    idle_pcb.esp = 0;
    idle_pcb.ss = GDT_SEL_KERNEL_DATA;
    idle_pcb.kernel_esp = 0;
    idle_pcb.kernel_esp_top = INTERRUPT_STACK_TOP;
    idle_pcb.time_slice = 0;
    idle_pcb.ticks_remaining = 0;
    idle_pcb.total_ticks = 0;

    current = &idle_pcb;

    serial_puts("[sched] initialized.\n");
}

/*
 * sched_start — 启动调度器, 切换到第一个就绪进程
 */
void sched_start(void)
{
    uint32_t flags = hal_irq_save();

    struct pcb *first = dequeue_ready();
    if (!first) {
        hal_irq_restore(flags);
        serial_puts("[sched] no processes to run, returning.\n");
        return;
    }

    /* 为 idle 进程构造栈 */
    {
        static uint8_t idle_stack[2048];
        uint32_t *isp = (uint32_t *)(idle_stack + sizeof(idle_stack));

        idle_pcb.kernel_esp_top = (uint32_t)isp;

        /* Ring 0 iret 帧 (从高到低: EFLAGS, CS, EIP) */
        *--isp = 0x202;                    /* EFLAGS */
        *--isp = GDT_SEL_KERNEL_CODE;      /* CS */
        *--isp = (uint32_t)sched_idle;     /* EIP */

        /* pushad 结构 (从高到低: eax, ecx, edx, ebx, esp, ebp, esi, edi) */
        *--isp = 0; /* eax */
        *--isp = 0; /* ecx */
        *--isp = 0; /* edx */
        *--isp = 0; /* ebx */
        *--isp = 0; /* esp */
        *--isp = 0; /* ebp */
        *--isp = 0; /* esi */
        *--isp = 0; /* edi */

        /* 段寄存器 (从高到低: DS, ES, FS, GS) — GS 在栈顶 */
        *--isp = GDT_SEL_KERNEL_DATA; /* DS */
        *--isp = GDT_SEL_KERNEL_DATA; /* ES */
        *--isp = GDT_SEL_KERNEL_DATA; /* FS */
        *--isp = GDT_SEL_KERNEL_DATA; /* GS */

        idle_pcb.kernel_esp = (uint32_t)isp;
    }

    current = first;
    current->state = PROC_RUNNING;
    current->ticks_remaining = current->time_slice;

    if (current->flags & PROC_FLAG_USER) {
        tss_set_esp0(current->kernel_esp_top);
    }

    hal_irq_restore(flags);

    serial_puts("[sched] starting first process: ");
    serial_puts(first->name);
    serial_putchar('\n');

    sched_restore_context(first->kernel_esp);

    for (;;)
        __asm__ __volatile__("hlt");
}

/*
 * loader_enter_ring3 — 从 Ring 0 切换到 Ring 3 执行用户程序
 * @user_eip: 用户程序入口地址 (Ring 3 EIP)
 * @user_esp: 用户栈顶地址 (Ring 3 ESP)
 *
 * 在当前内核栈上构造 Ring 3 iret 帧, 设置段寄存器为 Ring 3 选择子,
 * 然后通过 iret 指令跳转到 Ring 3 用户代码。
 * 当用户程序调用 SYS_EXIT 时, 调度器会终止当前进程并切换到其他进程。
 * 注意: 此函数不返回 (iret 后进入 Ring 3)。
 */
void loader_enter_ring3(uint32_t user_eip, uint32_t user_esp)
{
    if (current && current->kernel_esp_top)
        tss_set_esp0(current->kernel_esp_top);

    __asm__ __volatile__(
        "cli\n\t"
        "movl %[user_data], %%eax\n\t"
        "movw %%ax, %%ds\n\t"
        "movw %%ax, %%es\n\t"
        "movw %%ax, %%fs\n\t"
        "movw %%ax, %%gs\n\t"
        "pushl %[user_data]\n\t"      /* SS = Ring 3 data segment */
        "pushl %[user_esp]\n\t"       /* ESP = user stack */
        "pushfl\n\t"                   /* EFLAGS */
        "popl %%eax\n\t"
        "orl $0x200, %%eax\n\t"       /* Set IF (enable interrupts in Ring 3) */
        "pushl %%eax\n\t"
        "pushl %[user_code]\n\t"      /* CS = Ring 3 code segment */
        "pushl %[user_eip]\n\t"       /* EIP = user entry point */
        "iret\n\t"
        :
        : [user_eip] "g"(user_eip),
          [user_esp] "g"(user_esp),
          [user_code] "i"(GDT_SEL_USER_CODE),
          [user_data] "i"(GDT_SEL_USER_DATA)
        : "eax", "memory"
    );

    __builtin_unreachable();
}
