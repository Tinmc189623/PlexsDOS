/*
 * Nexsteaduser — PlexsDOS
 * com.h — .COM / .EXE (MZ) 可执行文件格式定义
 * 作者: Tinmc189623 | 团队: Nexlyh
 */

#ifndef _PLXSDOS_COM_H
#define _PLXSDOS_COM_H

#include <plexsdos/types.h>

#ifdef __cplusplus
extern "C" {
#endif

/* ===== .COM 格式 ===== */
/* .COM: 无头部, 原始 x86 二进制, 最大 65280 字节 (64K - PSP) */
#define COM_LOAD_ADDR      0x50100  /* 0x50000 + PSP (256 bytes) */
#define COM_MAX_SIZE       65280

/* ===== .EXE (MZ) 格式 ===== */
/* MZ 头部 (28 字节固定部分) */
struct mz_header {
    uint16_t signature;     /* 00: "MZ" (0x5A4D) */
    uint16_t last_page;     /* 02: 最后一页的字节数 */
    uint16_t pages;         /* 04: 文件总页数 (512字节/页) */
    uint16_t relocs;        /* 06: 重定位条目数 */
    uint16_t header_para;   /* 08: 头部段落数 (16字节/段) */
    uint16_t min_alloc;     /* 0A: 最小额外内存 (段落) */
    uint16_t max_alloc;     /* 0C: 最大额外内存 (段落) */
    uint16_t init_ss;       /* 0E: 初始 SS */
    uint16_t init_sp;       /* 10: 初始 SP */
    uint16_t checksum;      /* 12: 校验和 */
    uint16_t init_ip;       /* 14: 初始 IP */
    uint16_t init_cs;       /* 16: 初始 CS */
    uint16_t reloc_offset;  /* 18: 重定位表偏移 */
    uint16_t overlay;       /* 1A: 覆盖号 */
};

/* 重定位条目 (4 字节) */
struct mz_reloc {
    uint16_t offset;  /* 段内偏移 */
    uint16_t segment; /* 段 */
};

#define MZ_SIGNATURE      0x5A4D  /* "MZ" */
#define MZ_PAGE_SIZE      512
#define MZ_PARA_SIZE      16
#define MZ_HEADER_MIN     28

/* EXE 加载基址 (用户空间, Ring 3) */
#define EXE_LOAD_SEG      0x5000   /* 段落地址 0x50000 */

/*
 * com_check — 检查是否为 .COM 文件 (无魔数, 检查文件扩展名或大小)
 * @data: 文件数据
 * @size: 文件大小
 * 返回: true = 可能是 .COM
 */
static inline bool com_check(const uint8_t *data __attribute__((unused)),
                             uint32_t size)
{
    /* .COM 无魔数, 仅检查大小合理 (≤ 64K-256) */
    return size > 0 && size <= COM_MAX_SIZE;
}

/*
 * mz_check — 检查 MZ 签名
 * @data: 文件开头的至少 2 字节
 * 返回: true = 是 MZ 文件
 */
static inline bool mz_check(const uint8_t *data)
{
    return data[0] == 'M' && data[1] == 'Z';
}

/*
 * mz_get_size — 计算 MZ 文件的代码大小
 * @hdr: MZ 头部
 * 返回: 代码字节数
 */
static inline uint32_t mz_get_size(const struct mz_header *hdr)
{
    uint32_t size = (uint32_t)(hdr->pages - 1) * MZ_PAGE_SIZE + hdr->last_page;
    uint32_t header_bytes = (uint32_t)hdr->header_para * MZ_PARA_SIZE;
    if (size < header_bytes) return 0;
    return size - header_bytes;
}

#ifdef __cplusplus
}
#endif

#endif /* _PLXSDOS_COM_H */
