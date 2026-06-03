/*
 * Nexsteaduser — PlexsDOS
 * paging.h — 分页管理接口
 * 作者: Tinmc189623 | 团队: Nexlyh
 *
 * 提供页目录/页表初始化、地址映射和页帧分配。
 * 实现 Ring 0-2 (Supervisor) 和 Ring 3 (User) 的内存隔离。
 */

#ifndef _PLXSDOS_PAGING_H
#define _PLXSDOS_PAGING_H

#include <plexsdos/types.h>

#ifdef __cplusplus
extern "C" {
#endif

/* 页表项标志位 */
#define PAGE_PRESENT       0x01   /* 页存在 */
#define PAGE_WRITABLE      0x02   /* 可写 */
#define PAGE_USER          0x04   /* Ring 3 可访问 (U/S=1) */
#define PAGE_SUPERVISOR    0x00   /* 仅 Ring 0-2 (U/S=0) */

/* 页大小 */
#define PAGE_SIZE          4096
#define PAGE_SHIFT         12

/* ===== 分页核心 ===== */

/*
 * paging_init — 初始化分页
 *
 * 在物理地址 0x100000 创建页目录，0x101000 创建页表 0。
 * 身份映射前 1MB 内存 (物理 = 虚拟)。
 * 设置 U/S 位区分内核页和用户页。
 * 启用 CR0.PG。
 * 初始化页帧分配器 (PFA)。
 */
void paging_init(void);

/*
 * user_ptr_valid — 验证用户空间指针
 * @addr: 用户指针地址
 * @size: 访问大小 (字节)
 *
 * 检查 [addr, addr+size) 是否全部位于用户可访问页面。
 * 返回: true = 有效。
 */
bool user_ptr_valid(uint32_t addr, uint32_t size);

/* ===== 页帧分配器 (PFA) ===== */

/*
 * pfa_init — 初始化页帧分配器
 * @total_pages: 系统总物理页数
 *
 * 使用位图管理物理页帧。标记已使用的页 (内核代码/数据/页表区域)。
 * 位图存储在内核数据区，可管理最多 128MB 内存 (4KB 位图)。
 */
void pfa_init(uint32_t total_pages);

/*
 * pfa_alloc_frame — 分配一个物理页帧
 * 返回: 物理页帧地址 (4KB 对齐), 0 = 无空闲帧。
 */
uint32_t pfa_alloc_frame(void);

/*
 * pfa_free_frame — 释放一个物理页帧
 * @frame_addr: 物理页帧地址 (4KB 对齐)
 */
void pfa_free_frame(uint32_t frame_addr);

/*
 * pfa_get_free_count — 获取空闲页帧数
 * 返回: 当前空闲页帧数量。
 */
uint32_t pfa_get_free_count(void);

/* ===== 动态页映射 ===== */

/*
 * page_map — 将虚拟地址映射到物理地址
 * @vaddr: 虚拟地址 (4KB 对齐)
 * @paddr: 物理地址 (4KB 对齐)
 * @flags: 页表项标志 (PAGE_PRESENT | PAGE_WRITABLE | PAGE_USER 等)
 * 返回: 0 = 成功, -1 = 失败。
 *
 * 自动查找或创建页表条目。
 */
int page_map(uint32_t vaddr, uint32_t paddr, uint32_t flags);

/*
 * page_unmap — 取消虚拟地址的映射
 * @vaddr: 虚拟地址 (4KB 对齐)
 *
 * 清除页表条目并刷新 TLB。
 * 不释放物理页帧 (调用者需自行管理)。
 */
void page_unmap(uint32_t vaddr);

/*
 * page_unmap_and_free — 取消虚拟地址的映射并释放物理页帧
 * @vaddr: 虚拟地址 (4KB 对齐)
 *
 * 清除页表条目, 刷新 TLB, 并将对应的物理页帧归还给页帧分配器。
 * 仅用于动态映射的页面, 不可用于 identity-map 的内核页面。
 */
void page_unmap_and_free(uint32_t vaddr);

/*
 * page_get_mapping — 获取虚拟地址对应的物理地址
 * @vaddr: 虚拟地址
 * 返回: 物理地址, 0 = 未映射。
 */
uint32_t page_get_mapping(uint32_t vaddr);

/*
 * tlb_flush_page — 刷新单个页的 TLB 条目
 * @vaddr: 虚拟地址
 *
 * 执行 invlpg 指令。
 */
void tlb_flush_page(uint32_t vaddr);

#ifdef __cplusplus
}
#endif

#endif /* _PLXSDOS_PAGING_H */
