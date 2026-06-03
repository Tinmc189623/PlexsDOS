/*
 * Nexsteaduser — PlexsDOS
 * scheduler.h — 进程调度器接口
 * 作者: Tinmc189623 | 团队: Nexlyh
 *
 * 32-bit 保护模式多进程调度器。
 * 支持 Ring 0 (内核) 和 Ring 3 (用户) 进程管理,
 * 时间片轮转 (Round-Robin) 调度算法。
 */

#ifndef _PLXSDOS_SCHEDULER_H
#define _PLXSDOS_SCHEDULER_H

#include <plexsdos/types.h>

#ifdef __cplusplus
extern "C" {
#endif

/* 进程状态 */
#define PROC_FREE      0   /* PCB 空闲 */
#define PROC_READY     1   /* 就绪 */
#define PROC_RUNNING   2   /* 运行中 */
#define PROC_BLOCKED   3   /* 阻塞 (等待资源) */
#define PROC_ZOMBIE    4   /* 已终止, 等待父进程回收 */

/* 进程标志 */
#define PROC_FLAG_KERNEL  0x01  /* 内核进程 (Ring 0) */
#define PROC_FLAG_USER    0x02  /* 用户进程 (Ring 3) */
#define PROC_FLAG_SYSTEM  0x04  /* 系统进程 (不被调度器杀死) */

/* 默认时间片 (毫秒, 由定时器中断换算为 tick 数) */
#define PROC_TIME_SLICE_DEFAULT  30

/* 进程最大数 */
#define PROC_MAX  32

/* 进程控制块 */
struct pcb {
    uint32_t pid;              /* 进程 ID */
    uint32_t state;            /* 进程状态 */
    uint32_t flags;            /* 进程标志 */
    uint32_t esp;              /* 保存的内核栈指针 */
    uint32_t ss;               /* 保存的栈段 */
    uint32_t kernel_esp;       /* 进程的内核栈 (用于 Ring 3 进程) */
    uint32_t page_dir;         /* 页目录基址 (future: 进程独立地址空间) */
    uint32_t time_slice;       /* 时间片 (tick 数) */
    uint32_t ticks_remaining;  /* 剩余 tick 数 */
    uint32_t total_ticks;      /* 总运行 tick 数 */
    char     name[32];         /* 进程名 */
    struct pcb *next;          /* 就绪队列链表 */
};

/* 就绪队列 */
struct ready_queue {
    struct pcb *head;          /* 队首 */
    struct pcb *tail;          /* 队尾 */
    uint32_t count;            /* 进程数 */
};

/* ===== 调度器初始化 ===== */

/*
 * sched_init — 初始化进程调度器
 *
 * 初始化 PCB 池、就绪队列, 创建空闲进程 (idle)。
 */
void sched_init(void);

/* ===== 进程管理 ===== */

/*
 * sched_create_process — 创建新进程
 * @name:    进程名
 * @entry:   入口地址 (EIP)
 * @user_esp: 用户栈顶 (Ring 3, 内核线程传 0)
 * @flags:   PROC_FLAG_KERNEL 或 PROC_FLAG_USER
 * 返回: PID, 失败返回 -1。
 *
 * 内核进程: 在 Ring 0 执行, 共享内核页表。
 * 用户进程: 在 Ring 3 执行, 构造 iret 帧, 通过 iret 进入用户态。
 */
int sched_create_process(const char *name, uint32_t entry,
                         uint32_t user_esp, uint32_t flags);

/*
 * sched_exit — 终止当前进程
 * @status: 退出状态码
 */
void sched_exit(int status);

/*
 * sched_get_pcb — 通过 PID 获取进程控制块
 * @pid: 进程 ID
 * 返回: PCB 指针, 未找到返回 NULL。
 */
struct pcb *sched_get_pcb(int pid);

/*
 * sched_get_current — 获取当前运行的进程控制块
 * 返回: 当前进程 PCB 指针。
 */
struct pcb *sched_get_current(void);

/* ===== 调度控制 ===== */

/*
 * sched_yield — 主动让出 CPU
 *
 * 保存当前进程上下文, 切换到就绪队列的下一个进程。
 */
void sched_yield(void);

/*
 * sched_tick — 时钟 tick (由定时器中断周期性调用)
 *
 * 递减当前进程的时间片剩余计数。
 * 如果时间片用完, 触发进程切换。
 */
void sched_tick(void);

/*
 * sched_block — 阻塞当前进程
 *
 * 将当前进程移出就绪队列, 切换到下一个就绪进程。
 */
void sched_block(void);

/*
 * sched_unblock — 解除进程阻塞
 * @pid: 进程 ID
 * 返回: true = 成功, false = 进程不存在。
 *
 * 将指定进程重新加入就绪队列。
 */
bool sched_unblock(int pid);

/* ===== 进程间切换 (汇编实现) ===== */

/*
 * sched_switch_context — 上下文切换 (汇编实现)
 * @old_esp: [输出] 保存的当前进程 ESP
 * @new_esp: 新进程的内核栈 ESP
 *
 * 保存当前寄存器状态 (pusha), 切换到新栈, 恢复新进程寄存器 (popa)。
 * 在 sched_yield 内部调用。
 */
void sched_switch_context(uint32_t *old_esp, uint32_t new_esp);

/*
 * sched_idle — 空闲进程
 *
 * 当就绪队列为空时运行。执行 hlt 指令等待中断。
 * 永远不会返回。
 */
void sched_idle(void) __attribute__((noreturn));

#ifdef __cplusplus
}
#endif

#endif /* _PLXSDOS_SCHEDULER_H */
