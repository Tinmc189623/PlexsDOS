/*
 * Nexsteaduser — PlexsDOS
 * .comx 可执行文件格式定义
 * 作者: Tinmc189623 | 团队: Nexlyh
 *
 * .comx 是 PlexsDOS 自研的 32-bit 保护模式可执行文件格式。
 * 文件以 32 字节头部开始，后跟代码和只读数据。
 * BSS 段由加载器在加载后清零。
 */

#ifndef _PLXSDOS_COMX_H
#define _PLXSDOS_COMX_H

#include <plexsdos/types.h>

#ifdef __cplusplus
extern "C" {
#endif

/* 魔数: CPX\x00 */
#define COMX_MAGIC        0x43505800
#define COMX_MAGIC_STR    "CPX"

/* 当前版本 */
#define COMX_VERSION      0x01

/* 标志位 */
#define COMX_FLAG_SSE     (1 << 0)    /* 程序需要 SSE */
#define COMX_FLAG_SSE2    (1 << 1)    /* 程序需要 SSE2 */
#define COMX_FLAG_MMX     (1 << 2)    /* 程序需要 MMX */

/* 默认加载地址 (用户空间, Ring 3 可访问) */
#define COMX_LOAD_ADDR    0x50000

/* 最大程序大小 (32KB - 头部) */
#define COMX_MAX_SIZE     (32 * 1024)

/*
 * comx_header — .comx 文件头部 (32 字节)
 *
 * 所有多字节字段为小端序。
 */
struct comx_header {
    uint32_t magic;          /* 0x00: 魔数 COMX_MAGIC */
    uint8_t  version;        /* 0x04: 格式版本 */
    uint8_t  flags;          /* 0x05: 标志位 (COMX_FLAG_*) */
    uint16_t reserved0;      /* 0x06: 保留 */
    uint32_t entry_offset;   /* 0x08: 入口点偏移 (相对于代码起始) */
    uint32_t code_size;      /* 0x0C: 代码 + 只读数据大小 */
    uint32_t bss_size;       /* 0x10: BSS 段大小 (加载后清零) */
    uint32_t load_addr;      /* 0x14: 建议加载地址 */
    uint32_t checksum;       /* 0x18: 代码部分校验和 */
    uint32_t reserved1;      /* 0x1C: 保留 */
} __attribute__((packed));

#define COMX_HEADER_SIZE  32

/* 验证头部魔数 */
static inline bool comx_check_magic(const struct comx_header *hdr)
{
    return (hdr->magic == COMX_MAGIC) ? true : false;
}

/* 验证版本号 */
static inline bool comx_check_version(const struct comx_header *hdr)
{
    return (hdr->version == COMX_VERSION) ? true : false;
}

#ifdef __cplusplus
}
#endif

#endif /* _PLXSDOS_COMX_H */
