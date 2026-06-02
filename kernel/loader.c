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
 * loader_run — 加载并执行 .comx 程序
 * @filename: FAT32 文件名
 *
 * 从 FAT32 硬盘查找文件, 验证 .comx 头部, 加载代码到内存,
 * 清零 BSS 段, 跳转到入口点执行。
 *
 * 返回: true 表示程序已执行并返回, false 表示加载失败。
 */
bool loader_run(const char *filename)
{
    struct fat32_dir_entry *entry;
    struct comx_header *hdr;
    uint32_t file_size;
    uint32_t code_start;
    uint8_t *bss_start;
    uint32_t i;
    /* loader_enter_ring3 声明在 loader.h 中 */

    serial_puts("[loader] searching: ");
    serial_puts(filename);
    serial_puts("\n");

    /* 在 FAT32 根目录查找文件 */
    entry = fat32_find_file(filename);
    if (!entry) {
        screen_puts("File not found: ");
        screen_puts(filename);
        screen_putchar('\n');
        serial_puts("[loader] file not found\n");
        return false;
    }
    serial_puts("[loader] found, loading...\n");

    /* 加载整个文件到静态缓冲区 */
    file_size = fat32_load_file(entry, (uint32_t)file_buf);
    serial_puts("[loader] loaded ");
    serial_put_hex(file_size);
    serial_puts(" bytes\n");
    if (file_size < COMX_HEADER_SIZE) {
        screen_puts("File too small.\n");
        serial_puts("[loader] too small\n");
        return false;
    }

    /* 解析头部 */
    hdr = (struct comx_header *)file_buf;

    /* 验证魔数 */
    if (!comx_check_magic(hdr)) {
        screen_puts("Not a .comx file.\n");
        return false;
    }

    /* 验证版本 */
    if (!comx_check_version(hdr)) {
        screen_puts("Unsupported .comx version.\n");
        return false;
    }

    /* 验证代码大小 */
    if (hdr->code_size == 0 || hdr->code_size > COMX_MAX_SIZE) {
        screen_puts("Invalid code size.\n");
        return false;
    }

    if (COMX_HEADER_SIZE + hdr->code_size > file_size) {
        screen_puts("Truncated .comx file.\n");
        return false;
    }

    /* 检查 CPU 特性需求 */
    if ((hdr->flags & COMX_FLAG_SSE) && !cpu_has_feature(CPU_FEATURE_SSE)) {
        screen_puts("Program requires SSE. Not available.\n");
        return false;
    }
    if ((hdr->flags & COMX_FLAG_SSE2) && !cpu_has_feature(CPU_FEATURE_SSE2)) {
        screen_puts("Program requires SSE2. Not available.\n");
        return false;
    }
    if ((hdr->flags & COMX_FLAG_MMX) && !cpu_has_feature(CPU_FEATURE_MMX)) {
        screen_puts("Program requires MMX. Not available.\n");
        return false;
    }

    /* 验证校验和 */
    code_start = COMX_HEADER_SIZE;
    if (loader_checksum(file_buf + code_start, hdr->code_size) != hdr->checksum) {
        screen_puts("Checksum mismatch.\n");
        return false;
    }

    /* 验证加载地址在用户空间范围内 (Ring 0/3 分离) */
    serial_puts("[loader] validating load_addr=0x");
    serial_put_hex(hdr->load_addr);
    serial_puts(" size=0x");
    serial_put_hex(hdr->code_size + hdr->bss_size);
    serial_puts("\n");
    if (!user_ptr_valid(hdr->load_addr, hdr->code_size + hdr->bss_size)) {
        screen_puts("Error: load address outside user space.\n");
        serial_puts("[loader] FAIL: address outside user space\n");
        return false;
    }
    serial_puts("[loader] address OK\n");

    /* 将代码复制到加载地址 */
    uint8_t *load_ptr = (uint8_t *)hdr->load_addr;
    for (i = 0; i < hdr->code_size; i++) {
        load_ptr[i] = file_buf[code_start + i];
    }

    /* 清零 BSS 段 */
    bss_start = load_ptr + hdr->code_size;
    for (i = 0; i < hdr->bss_size; i++) {
        bss_start[i] = 0;
    }

    /* 显示加载信息 */
    screen_puts("Loaded .comx: ");
    screen_put_dec(hdr->code_size);
    screen_puts(" bytes code, ");
    screen_put_dec(hdr->bss_size);
    screen_puts(" bytes bss.\n");

    /* 切换到 Ring 3 执行用户程序 */
    serial_puts("[loader] entering Ring 3: EIP=0x");
    serial_put_hex(hdr->load_addr + hdr->entry_offset);
    serial_puts(" ESP=0x");
    serial_put_hex(USER_STACK_TOP);
    serial_puts(" load_addr=0x");
    serial_put_hex(hdr->load_addr);
    serial_puts("\n");

    /* 验证页表: 读取 0x50000 处的页表项 */
    serial_puts("[loader] PT[80]=0x");
    {
        uint32_t *pt = (uint32_t *)0x101000;
        serial_put_hex(pt[80]);
        serial_puts(" PT[129]=0x");
        serial_put_hex(pt[129]);
        serial_puts("\n");
    }

    /* VGA 调试标记: 屏幕左上角显示 'R3' */
    {
        volatile uint16_t *vga = (volatile uint16_t *)0xB8000;
        vga[0] = 0x4F52;  /* 'R' 红底白字 */
        vga[1] = 0x4F33;  /* '3' 红底白字 */
    }
    loader_enter_ring3(hdr->load_addr + hdr->entry_offset, USER_STACK_TOP);

    screen_puts("Program returned.\n");
    return true;
}
