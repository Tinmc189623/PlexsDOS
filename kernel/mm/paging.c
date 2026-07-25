/*
 * Nexsteaduser — PlexsDOS
 * paging.c — 分页管理实现
 * 作者: Tinmc189623 | 团队: Nexlyh
 *
 * 创建页目录和页表，身份映射前 1MB 内存。
 * 通过 U/S 位实现 Ring 0-2 (Supervisor) 和 Ring 3 (User) 的内存隔离。
 * 提供动态页映射/取消映射 API 和页帧分配器集成。
 * 启用 CR0.PG 后所有地址均为虚拟地址 (此处为恒等映射)。
 */

#include <plexsdos/paging.h>
#include <plexsdos/config.h>
#include <plexsdos/types.h>
#include <plexsdos/string.h>
#include <plexsdos/cpu.h>
#include <plexsdos/screen.h>
#include <plexsdos/serial.h>

/* 页目录: 1024 个条目, 每个 4 字节, 放在 0x100000 */
static uint32_t *page_directory = (uint32_t *)PAGE_DIR_ADDR;

/* 页表 0: 1024 个条目, 放在 0x101000 */
static uint32_t *page_table_0 = (uint32_t *)PAGE_TABLE_ADDR;

/* 页表区域: 0x102000 起, 最多 254 个页表 (覆盖 ~1GB 虚拟地址空间) */
#define PAGE_TABLE_BASE   0x102000
#define MAX_PAGE_TABLES   254
static uint32_t *extra_page_tables[MAX_PAGE_TABLES];

/*
 * paging_init — 初始化分页
 *
 * 页目录放在 0x100000，页表 0 放在 0x101000。
 * 恒等映射前 1MB (256 个 4KB 页)，通过 U/S 位控制:
 *   - 内核页 (0x30000-0x41FFF, 0x80000-0x91FFF): Supervisor
 *   - 用户页 (0x50000-0x81FFF): User
 *   - VGA 显存 (0xB8000): Supervisor
 *
 * 启用 CR0.PG (bit 31), 设置 CR3 = 页目录地址。
 */
void paging_init(void)
{
    uint32_t i;
    uint32_t flags;
    bool has_pse;

    /* 检测 PSE 支持 */
    has_pse = cpu_has_feature(CPU_FEATURE_PSE);

    /* 清零页目录 */
    fast_memset(page_directory, 0, 4096);

    /* 清零页表 0 */
    fast_memset(page_table_0, 0, 4096);

    /* 恒等映射前 4MB (1024 个 4KB 页, 需要 4KB 粒度以支持混合 U/S 标志) */
    for (i = 0; i < 1024; i++) {
        /* 默认: Supervisor, writable */
        flags = PAGE_PRESENT | PAGE_WRITABLE;

        /* 用户空间页: 0x50000 - 0x81FFF */
        if (i >= (USER_LOAD_ADDR >> PAGE_SHIFT) &&
            i <= (USER_STACK_TOP >> PAGE_SHIFT))
            flags |= PAGE_USER;

        /* VGA 显存: Supervisor */
        if (i == (VGA_TEXT_BUFFER >> PAGE_SHIFT))
            flags = PAGE_PRESENT | PAGE_WRITABLE;

        page_table_0[i] = (i * PAGE_SIZE) | flags;
    }

    /* 页目录条目 0 → 页表 0
     * 注意: PDE 必须设置 PAGE_USER 标志, 否则即使 PTE 设置了 USER 位,
     * Ring 3 访问前 4MB 中任何地址都会触发 #PF (页故障)。
     * 内核页面在 PTE 级别保持 Supervisor 标志, Ring 3 仍然无法访问。 */
    page_directory[0] = (uint32_t)page_table_0 | PAGE_PRESENT | PAGE_WRITABLE | PAGE_USER;

    if (has_pse) {
        /*
         * PSE 优化: 4-8MB 用单个 4MB 大页映射 (Supervisor, writable)
         * 消除 page_table_1 及其 1024 个 PTE 的设置开销
         */
        page_directory[1] = PAGE_4M_SIZE | PAGE_PRESENT | PAGE_WRITABLE | PDE_4MB;
        serial_puts("[paging] PSE enabled: 4-8MB identity-mapped with 4MB page\n");
    } else {
        /* 回退: 页表 1 在 0x102000, 映射 4-6MB 覆盖 BSS (0x200000 起) */
        uint32_t *page_table_1 = (uint32_t *)0x102000;
        fast_memset(page_table_1, 0, 4096);
        for (i = 1024; i < 1536; i++) {
            page_table_1[i - 1024] = (i * PAGE_SIZE) | PAGE_PRESENT | PAGE_WRITABLE;
        }
        page_directory[1] = (uint32_t)page_table_1 | PAGE_PRESENT | PAGE_WRITABLE;
    }

    /* 启用 CR4.PSE (如果支持) */
    if (has_pse) {
        __asm__ __volatile__(
            "mov %%cr4, %%eax\n\t"
            "or  %0, %%eax\n\t"
            "mov %%eax, %%cr4"
            : : "i"(CR4_PSE) : "eax", "memory"
        );
    }

    /* 设置 CR3 = 页目录物理地址 */
    __asm__ __volatile__("mov %0, %%cr3" : : "r"((uint32_t)page_directory) : "memory");

    /* 启用 CR0.PG (bit 31) */
    __asm__ __volatile__(
        "mov %%cr0, %%eax\n\t"
        "or  $0x80000000, %%eax\n\t"
        "mov %%eax, %%cr0"
        : : : "eax", "memory"
    );

    /* 初始化页帧分配器 (128MB / 4KB = 32768 页) */
    pfa_init(32768);
}

/*
 * user_ptr_valid — 验证用户空间指针
 * @addr: 用户指针地址
 * @size: 访问大小 (字节)
 *
 * 检查地址范围 [addr, addr+size) 是否全部位于用户可访问页面。
 * 用户页面范围: 0x50000 - 0x81FFF (由 paging_init 设置 U/S=1)。
 *
 * 返回: true = 指针有效。
 */
bool user_ptr_valid(uint32_t addr, uint32_t size)
{
    uint32_t end;

    /* 防止溢出 */
    if (addr + size < addr)
        return false;

    end = addr + size - 1;

    /* 用户空间范围: 0x50000 - 0x81FFF */
    if (addr < USER_LOAD_ADDR)
        return false;
    if (end > (USER_STACK_TOP + PAGE_SIZE - 1))
        return false;

    return true;
}

/* ===== 动态页映射 ===== */

/*
 * tlb_flush_page — 刷新单个页的 TLB 条目
 * @vaddr: 虚拟地址
 *
 * 执行 invlpg 指令使 TLB 中该页的条目失效。
 */
void tlb_flush_page(uint32_t vaddr)
{
    __asm__ __volatile__("invlpg (%0)" : : "r"(vaddr) : "memory");
}

/*
 * page_table_is_empty — 检查页表是否所有条目均为非 Present
 * @pd_idx: 页目录索引 (0 = 静态页表, 永不释放)
 * 返回: true = 页表空, 可释放。
 */
static bool page_table_is_empty(uint32_t pd_idx)
{
    /* 页表 0 恒等映射内核/用户区, 永不释放 */
    if (pd_idx == 0)
        return false;

    uint32_t *pt = (uint32_t *)(page_directory[pd_idx] & 0xFFFFF000);

    /* 页目录条目不存在则视为空 */
    if (!(page_directory[pd_idx] & PAGE_PRESENT))
        return true;

    for (int i = 0; i < 1024; i++) {
        if (pt[i] & PAGE_PRESENT)
            return false;
    }
    return true;
}

/*
 * page_table_free — 释放页表帧并清除页目录条目
 * @pd_idx: 页目录索引 (跳过 pd_idx=0)
 *
 * 将页表物理帧归还给页帧分配器, 清除 PDE 和 extra_page_tables 引用。
 */
static void page_table_free(uint32_t pd_idx)
{
    if (pd_idx == 0)
        return;

    uint32_t pt_phys = page_directory[pd_idx] & 0xFFFFF000;
    if (pt_phys == 0)
        return;

    page_directory[pd_idx] = 0;
    pfa_free_frame(pt_phys);

    if (pd_idx > 0 && pd_idx - 1 < MAX_PAGE_TABLES)
        extra_page_tables[pd_idx - 1] = NULL;
}

/*
 * page_map — 将虚拟地址映射到物理地址
 * @vaddr: 虚拟地址 (4KB 对齐)
 * @paddr: 物理地址 (4KB 对齐)
 * @flags: 页表项标志位
 * 返回: 0 = 成功, -1 = 失败。
 *
 * 计算页目录索引和页表索引。
 * 如果页目录条目不存在, 分配新页表并初始化。
 * 设置页表条目并刷新 TLB。
 */
int page_map(uint32_t vaddr, uint32_t paddr, uint32_t flags)
{
    uint32_t pd_idx = vaddr >> 22;          /* 页目录索引 (高 10 位) */
    uint32_t pt_idx = (vaddr >> 12) & 0x3FF; /* 页表索引 (中 10 位) */
    uint32_t *pt;

    /* 对齐检查 */
    if ((vaddr & 0xFFF) || (paddr & 0xFFF))
        return -1;

    /* 页目录索引范围检查 */
    if (pd_idx >= 1024)
        return -1;

    /* 检查页目录条目 */
    if (!(page_directory[pd_idx] & PAGE_PRESENT)) {
        /* 需要创建新页表 */
        uint32_t new_pt_phys = pfa_alloc_frame();
        if (new_pt_phys == 0)
            return -1;

        /* 清零新页表 */
        pt = (uint32_t *)new_pt_phys;
        fast_memset(pt, 0, 4096);

        /* 设置页目录条目 (supervisor, writable, present) */
        page_directory[pd_idx] = new_pt_phys | PAGE_PRESENT | PAGE_WRITABLE;

        /* 记录页表指针 */
        if (pd_idx > 0 && pd_idx - 1 < MAX_PAGE_TABLES)
            extra_page_tables[pd_idx - 1] = pt;
    }

    /* 获取页表地址 */
    if (pd_idx == 0) {
        pt = page_table_0;
    } else {
        pt = (uint32_t *)(page_directory[pd_idx] & 0xFFFFF000);
    }

    /* 检查是否存在旧映射 (潜在泄漏) */
    if (pt[pt_idx] & PAGE_PRESENT) {
        uint32_t old_paddr = pt[pt_idx] & 0xFFFFF000;
        if (old_paddr != (paddr & 0xFFFFF000))
            serial_puts("[pfa] warning: page_map overwrote mapping\n");
    }

    /* 设置页表条目 */
    pt[pt_idx] = (paddr & 0xFFFFF000) | (flags & 0xFFF) | PAGE_PRESENT;

    /* 刷新 TLB */
    tlb_flush_page(vaddr);

    return 0;
}

/*
 * page_unmap — 取消虚拟地址的映射
 * @vaddr: 虚拟地址 (4KB 对齐)
 *
 * 清除页表条目并刷新 TLB。
 * 如果页表变空则释放页表帧。
 * 不释放被映射的物理页帧 (调用者需自行管理)。
 */
void page_unmap(uint32_t vaddr)
{
    uint32_t pd_idx = vaddr >> 22;
    uint32_t pt_idx = (vaddr >> 12) & 0x3FF;
    uint32_t *pt;

    if (vaddr & 0xFFF)
        return;
    if (pd_idx >= 1024)
        return;
    if (!(page_directory[pd_idx] & PAGE_PRESENT))
        return;

    if (pd_idx == 0)
        pt = page_table_0;
    else
        pt = (uint32_t *)(page_directory[pd_idx] & 0xFFFFF000);

    if (pt[pt_idx] & PAGE_PRESENT) {
        pt[pt_idx] = 0;
        tlb_flush_page(vaddr);
    }

    /* 页表全空时释放帧 */
    if (page_table_is_empty(pd_idx))
        page_table_free(pd_idx);
}

/*
 * page_unmap_and_free — 取消虚拟地址的映射并释放物理页帧
 * @vaddr: 虚拟地址 (4KB 对齐)
 *
 * 清除页表条目, 刷新 TLB, 并将对应的物理页帧归还给页帧分配器。
 * 如果页表变空则释放页表帧。
 * 注意: 仅用于动态映射的页面, 不可用于 identity-map 的内核区。
 */
void page_unmap_and_free(uint32_t vaddr)
{
    uint32_t pd_idx = vaddr >> 22;
    uint32_t pt_idx = (vaddr >> 12) & 0x3FF;
    uint32_t *pt;
    uint32_t paddr;

    if (vaddr & 0xFFF)
        return;
    if (pd_idx >= 1024)
        return;
    if (!(page_directory[pd_idx] & PAGE_PRESENT))
        return;

    if (pd_idx == 0)
        pt = page_table_0;
    else
        pt = (uint32_t *)(page_directory[pd_idx] & 0xFFFFF000);

    if (!(pt[pt_idx] & PAGE_PRESENT))
        return;

    paddr = pt[pt_idx] & 0xFFFFF000;
    pt[pt_idx] = 0;
    tlb_flush_page(vaddr);

    pfa_free_frame(paddr);

    /* 页表全空时释放帧 */
    if (page_table_is_empty(pd_idx))
        page_table_free(pd_idx);
}

/*
 * page_get_mapping — 获取虚拟地址对应的物理地址
 * @vaddr: 虚拟地址
 * 返回: 物理地址, 0 = 未映射。
 */
uint32_t page_get_mapping(uint32_t vaddr)
{
    uint32_t pd_idx = vaddr >> 22;
    uint32_t pt_idx = (vaddr >> 12) & 0x3FF;
    uint32_t *pt;

    if (pd_idx >= 1024)
        return 0;

    if (!(page_directory[pd_idx] & PAGE_PRESENT))
        return 0;

    if (pd_idx == 0) {
        pt = page_table_0;
    } else {
        pt = (uint32_t *)(page_directory[pd_idx] & 0xFFFFF000);
    }

    if (!(pt[pt_idx] & PAGE_PRESENT))
        return 0;

    return (pt[pt_idx] & 0xFFFFF000) + (vaddr & 0xFFF);
}
