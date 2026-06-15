/*
 * Nexsteaduser — PlexsDOS
 * .comx 程序加载器
 * 作者: Tinmc189623 | 团队: Nexlyh
 *
 * 解析 .comx 文件头部, 校验格式, 加载代码到内存并跳转执行。
 * BSS 段在加载后自动清零。
 *
 * 注意: file_buf 使用静态缓冲区而非栈上分配, 避免 32KB 栈溢出
 * (内核栈仅 4KB: 0x8F000-0x90000)。
 */

#include <plexsdos/types.h>
#include <plexsdos/comx.h>
#include <plexsdos/com.h>
#include <plexsdos/screen.h>
#include <plexsdos/fat32.h>
#include <plexsdos/cpu.h>
#include <plexsdos/config.h>
#include <plexsdos/loader.h>
#include <plexsdos/paging.h>
#include <plexsdos/serial.h>

/* 静态文件缓冲区 (避免栈溢出: 内核栈仅 4KB) */
static uint8_t file_buf[COMX_MAX_SIZE + COMX_HEADER_SIZE];

/*
 * loader_checksum — 计算代码段校验和
 * @data: 代码数据指针
 * @size: 代码大小
 *
 * 返回: 32 位累加校验和
 */
static uint32_t loader_checksum(const uint8_t *data, uint32_t size)
{
    uint32_t sum = 0;
    uint32_t i;
    for (i = 0; i < size; i++) {
        sum = (sum << 3) ^ (sum >> 29) ^ (uint32_t)data[i];
    }
    return sum;
}

/*
 * try_load — 尝试以指定扩展名查找并加载文件
 * @name: 文件名 (无扩展名或带扩展名)
 * @ext:  要尝试的扩展名 (如 "COM", "EXE", "COMX")
 * @entry: 输出找到的 FAT32 目录项
 * 返回: 加载的文件大小, 0 = 未找到
 */
static uint32_t try_load(const char *name, const char *ext,
                         struct fat32_dir_entry **entry)
{
    char fname[48];
    int i = 0, j = 0;

    /* 复制文件名 */
    while (name[i] && i < 40) { fname[i] = name[i]; i++; }

    /* 检查是否已有扩展名 */
    bool has_ext = false;
    for (int k = i - 1; k >= 0; k--) {
        if (fname[k] == '.') { has_ext = true; break; }
    }

    if (has_ext) {
        fname[i] = '\0';
        *entry = fat32_find_file(fname);
        if (*entry) return fat32_load_file(*entry, (uint32_t)file_buf);
        return 0;
    }

    /* 尝试添加扩展名 */
    fname[i++] = '.';
    while (ext[j] && i < 44) fname[i++] = ext[j++];
    fname[i] = '\0';

    *entry = fat32_find_file(fname);
    if (*entry) return fat32_load_file(*entry, (uint32_t)file_buf);
    return 0;
}

/*
 * loader_run_com — 加载并执行 .COM 程序
 * @data: 文件数据
 * @size: 文件大小
 * 返回: true = 成功
 */
static bool loader_run_com(const uint8_t *data, uint32_t size)
{
    uint32_t i;

    if (size == 0 || size > COM_MAX_SIZE) {
        screen_puts("Invalid .COM size.\n");
        return false;
    }

    /* 验证加载地址在用户空间 */
    if (!user_ptr_valid(COM_LOAD_ADDR, size)) {
        screen_puts("COM load address outside user space.\n");
        return false;
    }

    /* 复制代码到 COM 加载地址 */
    uint8_t *dst = (uint8_t *)COM_LOAD_ADDR;
    for (i = 0; i < size; i++) dst[i] = data[i];

    screen_puts("Loaded .COM: ");
    screen_put_dec((int)size);
    screen_puts(" bytes at 0x");
    screen_put_hex(COM_LOAD_ADDR);
    screen_putchar('\n');

    /* 切换到 Ring 3 — COM 入口 = 加载地址 */
    loader_enter_ring3(COM_LOAD_ADDR, USER_STACK_TOP);
    screen_puts("Program returned.\n");
    return true;
}

/*
 * loader_run_exe — 加载并执行 .EXE (MZ) 程序
 * @data: 文件数据
 * @size: 文件大小
 * 返回: true = 成功
 */
static bool loader_run_exe(const uint8_t *data, uint32_t size)
{
    struct mz_header *hdr;
    uint32_t code_size, header_bytes;
    uint32_t load_addr, entry_addr;
    uint8_t *dst;
    uint32_t i;

    if (size < MZ_HEADER_MIN || !mz_check(data)) {
        screen_puts("Invalid MZ header.\n");
        return false;
    }

    hdr = (struct mz_header *)data;
    code_size = mz_get_size(hdr);
    header_bytes = (uint32_t)hdr->header_para * MZ_PARA_SIZE;

    if (code_size == 0 || header_bytes + code_size > size) {
        screen_puts("Corrupt .EXE file.\n");
        return false;
    }

    load_addr = EXE_LOAD_SEG * MZ_PARA_SIZE;

    if (!user_ptr_valid(load_addr, code_size + 4096)) {
        screen_puts("EXE load address outside user space.\n");
        return false;
    }

    /* 复制代码段 */
    dst = (uint8_t *)load_addr;
    for (i = 0; i < code_size; i++)
        dst[i] = data[header_bytes + i];

    /* 处理重定位 */
    if (hdr->relocs > 0 && hdr->reloc_offset + hdr->relocs * 4 <= size) {
        struct mz_reloc *reloc = (struct mz_reloc *)(data + hdr->reloc_offset);
        for (i = 0; i < hdr->relocs; i++) {
            uint32_t fixup_addr = load_addr +
                (uint32_t)reloc[i].segment * MZ_PARA_SIZE +
                reloc[i].offset;
            if (fixup_addr < load_addr + code_size - 1) {
                uint16_t *fixup = (uint16_t *)fixup_addr;
                *fixup += EXE_LOAD_SEG;
            }
        }
    }

    /* 计算入口地址 */
    entry_addr = load_addr +
        (uint32_t)hdr->init_cs * MZ_PARA_SIZE +
        hdr->init_ip;

    screen_puts("Loaded .EXE: ");
    screen_put_dec((int)code_size);
    screen_puts(" bytes, ");
    screen_put_dec((int)hdr->relocs);
    screen_puts(" relocs, entry=0x");
    screen_put_hex(entry_addr);
    screen_putchar('\n');

    /* 设置 SS:SP (Ring 3 用用户栈) */
    loader_enter_ring3(entry_addr, USER_STACK_TOP);
    screen_puts("Program returned.\n");
    return true;
}

/*
 * loader_run — 自动检测格式并执行程序
 * @filename: 文件名 (可带或不带扩展名)
 *
 * 按以下顺序尝试: 1) 原文件名, 2) +.COMX, 3) +.EXE, 4) +.COM
 * 检测到格式后调用对应的加载器。
 * 返回: true = 程序已执行并返回, false = 加载失败。
 */
bool loader_run(const char *filename)
{
    struct fat32_dir_entry *entry = NULL;
    uint32_t file_size;
    struct comx_header *hdr;

    serial_puts("[loader] searching: ");
    serial_puts(filename);
    serial_puts("\n");

    /* 尝试加载 .COMX */
    file_size = try_load(filename, "COMX", &entry);
    if (file_size >= COMX_HEADER_SIZE) {
        uint8_t *data = file_buf;
        hdr = (struct comx_header *)data;
        if (comx_check_magic(hdr)) {
            serial_puts("[loader] detected .COMX\n");
            /* 以下为原有 COMX 加载逻辑 */
            if (!comx_check_version(hdr)) {
                screen_puts("Unsupported .comx version.\n");
                return false;
            }
            if (hdr->code_size == 0 || hdr->code_size > COMX_MAX_SIZE) {
                screen_puts("Invalid code size.\n");
                return false;
            }
            if (COMX_HEADER_SIZE + hdr->code_size > file_size) {
                screen_puts("Truncated .comx file.\n");
                return false;
            }
            if ((hdr->flags & COMX_FLAG_SSE) && !cpu_has_feature(CPU_FEATURE_SSE))
                { screen_puts("Program requires SSE.\n"); return false; }
            if ((hdr->flags & COMX_FLAG_SSE2) && !cpu_has_feature(CPU_FEATURE_SSE2))
                { screen_puts("Program requires SSE2.\n"); return false; }
            if ((hdr->flags & COMX_FLAG_MMX) && !cpu_has_feature(CPU_FEATURE_MMX))
                { screen_puts("Program requires MMX.\n"); return false; }

            uint32_t cs = COMX_HEADER_SIZE;
            if (loader_checksum(data + cs, hdr->code_size) != hdr->checksum) {
                screen_puts("Checksum mismatch.\n"); return false;
            }
            if (!user_ptr_valid(hdr->load_addr, hdr->code_size + hdr->bss_size)) {
                screen_puts("Load address outside user space.\n"); return false;
            }
            uint8_t *lp = (uint8_t *)hdr->load_addr;
            for (uint32_t i = 0; i < hdr->code_size; i++) lp[i] = data[cs + i];
            for (uint32_t i = 0; i < hdr->bss_size; i++) lp[hdr->code_size + i] = 0;

            screen_puts("Loaded .comx: "); screen_put_dec(hdr->code_size);
            screen_puts(" bytes code, "); screen_put_dec(hdr->bss_size);
            screen_puts(" bytes bss.\n");

            loader_enter_ring3(hdr->load_addr + hdr->entry_offset, USER_STACK_TOP);
            screen_puts("Program returned.\n");
            return true;
        }
    }

    /* 尝试加载 .EXE */
    file_size = try_load(filename, "EXE", &entry);
    if (file_size >= MZ_HEADER_MIN && mz_check(file_buf)) {
        serial_puts("[loader] detected .EXE\n");
        return loader_run_exe(file_buf, file_size);
    }

    /* 尝试加载 .COM */
    file_size = try_load(filename, "COM", &entry);
    if (file_size > 0 && file_size <= COM_MAX_SIZE) {
        serial_puts("[loader] detected .COM\n");
        return loader_run_com(file_buf, file_size);
    }

    screen_puts("File not found: ");
    screen_puts(filename);
    screen_putchar('\n');
    serial_puts("[loader] not found\n");
    return false;
}
