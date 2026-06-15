/*
 * Nexsteaduser — PlexsDOS
 * pfa.c — 页帧分配器 (Page Frame Allocator)
 * 作者: Tinmc189623 | 团队: Nexlyh
 *
 * 使用位图管理物理页帧分配。
 * 每个位对应一个 4KB 物理页帧, 1 = 已用, 0 = 空闲。
 * 位图存储在内核数据区, 可管理最多 128MB 内存 (4KB 位图 = 32768 位 = 32768 页)。
 */

#include <plexsdos/paging.h>
#include <plexsdos/config.h>
#include <plexsdos/types.h>
#include <plexsdos/screen.h>
#include <plexsdos/string.h>

/* 位图大小: 4KB = 32768 位, 可管理 128MB 物理内存 */
#define PFA_BITMAP_SIZE   4096
#define PFA_MAX_PAGES     (PFA_BITMAP_SIZE * 8)

/* 位图存储区 (放在内核 BSS 段之后的预留区域) */
static uint8_t pfa_bitmap[PFA_BITMAP_SIZE];

/* 链接器符号: .bss 段起止地址 (由 linker.ld 定义, ABSOLUTE 修正)
 * 注意: GCC PE/COFF 自动为 C 符号添加前导 _,
 * 所以 C 声明 bss_start → 目标符号 _bss_start,
 * 匹配 linker.ld 中的 _bss_start / _bss_end。 */
extern char bss_start[];
extern char bss_end[];

/* 系统总页数和空闲页数 */
static uint32_t pfa_total_pages;
static uint32_t pfa_free_pages;

/*
 * pfa_set_bit — 设置位图中某位 (标记为已用)
 * @page: 页号
 */
static void pfa_set_bit(uint32_t page)
{
    pfa_bitmap[page >> 3] |= (uint8_t)(1 << (page & 7));
}

/*
 * pfa_clear_bit — 清除位图中某位 (标记为空闲)
 * @page: 页号
 */
static void pfa_clear_bit(uint32_t page)
{
    pfa_bitmap[page >> 3] &= (uint8_t)(~(1 << (page & 7)));
}

/*
 * pfa_test_bit — 测试位图中某位
 * @page: 页号
 * 返回: 1 = 已用, 0 = 空闲。
 */
static int pfa_test_bit(uint32_t page)
{
    return (pfa_bitmap[page >> 3] >> (page & 7)) & 1;
}

/*
 * pfa_init — 初始化页帧分配器
 * @total_pages: 系统总物理页数
 *
 * 清零位图, 然后标记已使用的物理页:
 *   - 页 0 (实模式 IVT + BDA, 0x0000-0x0FFF)
 *   - 页 7 (引导扇区, 0x7C00-0x7FFF)
 *   - 页 48-65 (内核代码+数据, 0x30000-0x41FFF)
 *   - 页 80-128 (用户程序区, 0x50000-0x80FFF)
 *   - 页 129 (用户栈, 0x81000)
 *   - 页 128-145 (内核栈区, 0x80000-0x91FFF)
 *   - 页 184 (VGA 显存, 0xB8000)
 *   - 页 256-257 (页目录 + 页表 0, 0x100000-0x101000)
 *   - 页 258 (页表 1, 0x102000)
 *   - BSS 段所占页 (由 _bss_start/_bss_end 动态计算, 0x200000 起)
 *   - 位图自身所在页
 */
void pfa_init(uint32_t total_pages)
{
    uint32_t i;
    uint32_t bitmap_page_start;
    uint32_t bitmap_page_end;

    /* 清零位图 */
    fast_memset(pfa_bitmap, 0, PFA_BITMAP_SIZE);

    /* 显式初始化空闲页计数 (BSS 可能因 PE/COFF 对齐未被正确清零) */
    pfa_free_pages = 0;

    /* 限制最大页数 */
    if (total_pages > PFA_MAX_PAGES)
        total_pages = PFA_MAX_PAGES;

    pfa_total_pages = total_pages;

    /* 标记所有页为已用 (保守策略), 然后释放可用区域 */
    fast_memset(pfa_bitmap, 0xFF, PFA_BITMAP_SIZE);

    /* 释放空闲页: 从 0x98000 到 0xFFFFF (排除已知使用区域) */
    /* 页 0x51-0x7F: 内核 BSS 扩展区 — 已用 (保持标记) */

    /* 页 0x98-0xB7: 保护间隙 — 空闲 */
    for (i = 0x98; i <= 0xB7; i++) {
        if (i < total_pages) {
            pfa_clear_bit(i);
            pfa_free_pages++;
        }
    }

    /* 页 0xB9-0xFF: VGA 之后到 1MB 边界 — 空闲 */
    for (i = 0xB9; i <= 0xFF; i++) {
        if (i < total_pages) {
            pfa_clear_bit(i);
            pfa_free_pages++;
        }
    }

    /* 1MB 以上 (页 256+) 除页表区域外全部空闲 */
    for (i = 259; i < total_pages; i++) {
        pfa_clear_bit(i);
        pfa_free_pages++;
    }

    /* === 重新标记内核 BSS 页为已用 (paging identity-map 4MB 包含 BSS) ===
     * 上述代码将 1MB 以上的页全部释放为空闲, 但内核 BSS 可延伸至 ~4MB。
     * 必须重新保留 BSS 范围 [bss_pg_start, bss_pg_end] 的页帧,
     * 否则 pfa_alloc_frame 可能分发 BSS 覆盖的物理页, 破坏内核数据。 */
    {
        uint32_t bss_addr_start = (uint32_t)(void *)bss_start;
        uint32_t bss_addr_end   = (uint32_t)(void *)bss_end;
        uint32_t bss_pg_start   = bss_addr_start >> PAGE_SHIFT;
        uint32_t bss_pg_end     = (bss_addr_end + PAGE_SIZE - 1) >> PAGE_SHIFT;

        for (i = bss_pg_start; i < bss_pg_end && i < total_pages; i++) {
            if (!pfa_test_bit(i)) {
                pfa_set_bit(i);
                pfa_free_pages--;
            }
        }
    }

    /* 标记位图自身所在页为已用 */
    bitmap_page_start = (uint32_t)pfa_bitmap >> PAGE_SHIFT;
    bitmap_page_end = ((uint32_t)pfa_bitmap + PFA_BITMAP_SIZE - 1) >> PAGE_SHIFT;
    for (i = bitmap_page_start; i <= bitmap_page_end; i++) {
        if (i < total_pages && !pfa_test_bit(i)) {
            pfa_set_bit(i);
            pfa_free_pages--;
        }
    }

    screen_puts("[pfa] bitmap: ");
    screen_put_dec(PFA_BITMAP_SIZE);
    screen_puts(" bytes, ");
    screen_put_dec(pfa_free_pages);
    screen_puts(" free pages (");
    screen_put_dec(pfa_free_pages * 4);
    screen_puts(" KB)\n");
}

/*
 * pfa_alloc_frame — 分配一个物理页帧
 *
 * 扫描位图找到第一个空闲位, 标记为已用, 返回物理地址。
 * 返回: 物理页帧地址 (4KB 对齐), 0 = 无空闲帧。
 */
uint32_t pfa_alloc_frame(void)
{
    uint32_t byte_idx, bit_idx;

    /* 逐字节扫描, 跳过满字节 (0xFF) */
    uint32_t max_bytes = (pfa_total_pages + 7) >> 3;
    for (byte_idx = 0; byte_idx < max_bytes; byte_idx++) {
        uint8_t byte = pfa_bitmap[byte_idx];
        if (byte != 0xFF) {
            /* 找到第一个空闲位 */
            for (bit_idx = 0; bit_idx < 8; bit_idx++) {
                if (!(byte & (1 << bit_idx))) {
                    uint32_t page = (byte_idx << 3) | bit_idx;
                    if (page >= pfa_total_pages)
                        return 0;
                    pfa_bitmap[byte_idx] |= (uint8_t)(1 << bit_idx);
                    pfa_free_pages--;
                    return page << PAGE_SHIFT;
                }
            }
        }
    }

    return 0;  /* 无空闲帧 */
}

/*
 * pfa_free_frame — 释放一个物理页帧
 * @frame_addr: 物理页帧地址 (4KB 对齐)
 *
 * 将对应位图位清零。
 */
void pfa_free_frame(uint32_t frame_addr)
{
    uint32_t page = frame_addr >> PAGE_SHIFT;

    if (page >= pfa_total_pages)
        return;

    if (pfa_test_bit(page)) {
        pfa_clear_bit(page);
        pfa_free_pages++;
    }
}

/*
 * pfa_get_free_count — 获取空闲页帧数
 * 返回: 当前空闲页帧数量。
 */
uint32_t pfa_get_free_count(void)
{
    return pfa_free_pages;
}
