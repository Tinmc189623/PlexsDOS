/*
 * Nexsteaduser — PlexsDOS
 * 内核主函数
 * 作者: Tinmc189623 | 团队: Nexlyh
 *
 * 内核 C 语言入口点。由 kernel_entry.S 的 _start 调用。
 * 负责初始化各子系统并启动 Shell。
 */

#include <plexsdos/types.h>
#include <plexsdos/config.h>
#include <plexsdos/screen.h>
#include <plexsdos/interrupt.h>
#include <plexsdos/keyboard.h>
#include <plexsdos/shell.h>
#include <plexsdos/disk.h>
#include <plexsdos/fat32.h>
#include <plexsdos/interrupt.hpp>
#include <plexsdos/serial.h>
#include <plexsdos/cpu.h>
#include <plexsdos/pci.h>
#include <plexsdos/fdc.h>
#include <plexsdos/cdrom.h>
#include <plexsdos/panic.h>
#include <plexsdos/desktop.h>
#include <plexsdos/gdt.h>
#include <plexsdos/paging.h>
#include <plexsdos/drive.h>
#include <plexsdos/installer.h>
#include <plexsdos/hal.h>
#include <plexsdos/scheduler.h>
#include <plexsdos/users.h>
#include <plexsdos/isa.h>
#include <plexsdos/mouse.h>
#include <plexsdos/ahci.h>
#include <plexsdos/config_sys.h>
#include <plexsdos/string.h>

/*
 * print_banner — 打印带框标题横幅
 * @title: 主标题 (居中)
 * @subtitle: 副标题 (居中, 可 NULL)
 *
 * 青色文字 + 分隔线, 之后恢复默认颜色。
 */
static void print_banner(const char *title, const char *subtitle)
{
    screen_set_color(0x03, 0x00);  /* 青色 */
    screen_puts("\n");
    screen_puts("  ============================================\n");
    screen_puts(title);
    if (subtitle)
        screen_puts(subtitle);
    screen_puts("  ============================================\n");
    screen_set_color(0x07, 0x00);
}

/*
 * boot_ok / boot_skip / boot_fail / boot_info — 启动状态输出辅助函数
 *
 * 消除初始化序列中反复出现的 "设置颜色→打印标记→恢复颜色→打印消息" 四行模式。
 * 颜色方案:
 *   [OK]  绿色 (0x0A) — 子系统初始化成功
 *   [--]  黄色 (0x0E) — 设备不存在/跳过 (非致命)
 *   [!!]  红色 (0x0C) — 致命错误
 *   [II]  黄色 (0x0E) — 信息提示
 */
static void boot_ok(const char *msg)
{
    screen_set_color(0x0A, 0x00);
    screen_puts("  [OK]");
    screen_set_color(0x07, 0x00);
    screen_puts(" ");
    screen_puts(msg);
    screen_puts("\n");
}

static void boot_skip(const char *msg)
{
    screen_set_color(0x0E, 0x00);
    screen_puts("  [--]");
    screen_set_color(0x07, 0x00);
    screen_puts(" ");
    screen_puts(msg);
    screen_puts("\n");
}

static void boot_fail(const char *msg)
{
    screen_set_color(0x0C, 0x00);
    screen_puts("  [!!]");
    screen_set_color(0x07, 0x00);
    screen_puts(" ");
    screen_puts(msg);
    screen_puts("\n");
}

static void boot_info(const char *msg)
{
    screen_set_color(0x0E, 0x00);
    screen_puts("  [II]");
    screen_set_color(0x07, 0x00);
    screen_puts(" ");
    screen_puts(msg);
    screen_puts("\n");
}

/* 启动驱动器号 (由 kernel_entry.S 从 BIOS DL 寄存器保存) */
extern uint8_t boot_drive;

/* 内存操作分派初始化 (lib/fast_mem.c) */
extern void fast_mem_init(void);
extern void *fast_memcpy(void *dst, const void *src, unsigned n);

/* FDC 块设备操作包装 (去除 drive 参数) */
static bool fdc0_read(uint32_t lba, uint8_t count, void *buf)
{
    return fdc_read_lba(0, lba, count, buf);
}
static bool fdc1_read(uint32_t lba, uint8_t count, void *buf)
{
    return fdc_read_lba(1, lba, count, buf);
}

/* AHCI 磁盘操作包装 */
static bool ahci_read_wrap(uint32_t lba, uint8_t count, void *buf)
{
    return ahci_read_sectors(0, (uint64_t)lba, count, buf);
}
static bool ahci_write_wrap(uint32_t lba, uint8_t count, const void *buf)
{
    return ahci_write_sectors(0, (uint64_t)lba, count, buf);
}

/*
 * vga_dbg — 直接写 VGA 文本缓冲区用于调试
 * @pos: 屏幕列号 (0-79)
 * @c: 要显示的字符
 *
 * 写入第一行指定位置, 白色前景, 黑色背景。
 * 用于在串口不可用时追踪内核初始化进度。
 * MINIMAL_KERNEL 构建中编译为 no-op。
 */
#ifdef MINIMAL_KERNEL
#define vga_dbg(pos, c) ((void)0)
#else
static void vga_dbg(int pos, char c)
{
    volatile char *vga = (volatile char *)0xB8000;
    vga[pos * 2] = c;
    vga[pos * 2 + 1] = 0x0F;
}
#endif

/*
 * kernel_main — 内核 C 语言主函数
 *
 * 初始化顺序:
 * 1.  屏幕驱动 (VGA 文本模式)
 * 2.  CPU 特性检测 (SSE/MMX)
 * 3.  GDT (Ring 0-3 段描述符 + TSS)
 * 4.  分页 (页目录/页表, 身份映射, 启用 CR0.PG)
 * 5.  IDT + PIC 初始化 (中断描述符表 + 8259A 重映射)
 * 6.  CPU 异常处理 (红屏 RSOD)
 * 7.  键盘驱动 (INT 0x21, IRQ1)
 * 8.  sti — 启用中断
 * 9.  PCI / 磁盘 / FAT32 / CD-ROM
 * 10. C++ 中断管理器 (INT 0x22 系统调用, DPL=3)
 * 11. 启动 Shell
 *
 * 每个初始化步骤均输出带颜色标记的启动信息,
 * 为启动过程营造清晰的仪式感。
 */
void kernel_main(void)
{
    /* ---- 早期 VGA 调试标记 (screen_init 前可见) ---- */
    vga_dbg(0, 'E');
    vga_dbg(1, 'N');
    vga_dbg(2, 'T');
    vga_dbg(3, 'R');
    vga_dbg(4, 'Y');

    serial_puts("[PlexsDOS] kernel_main entered.\n");

    /* ---- 启动横幅 ---- */
    screen_init();
    vga_dbg(0, 'S');
    serial_puts("[PlexsDOS] screen OK.\n");

    print_banner("        Nexsteaduser PlexsDOS  v0.2\n",
                 "        x86 32-bit  Monolithic Kernel OS\n");
    serial_puts("[PlexsDOS] system started.\n");

    /*
     * ---- CPU 初始化 (提前到 CONFIG.SYS 解析之前) ----
     * perf: fast_mem_init() 必须在使用 fast_memcpy/fast_memset 前调用。
     * 原来 cpu_init/fast_mem_init 排在 CONFIG.SYS 之后, 导致
     * CONFIG.SYS 必须用字节循环复制 (lib/string.c 的 baseline memcpy
     * 也是字节循环, 编译器 -O2 也无法完全向量化)。
     * 提前到此处后, CONFIG.SYS 可直接走 fast_memcpy
     * (AVX 256-bit 或 SSE2 128-bit 块拷贝)。
     */
    cpu_init();
    vga_dbg(1, 'C');
    serial_puts("[PlexsDOS] CPU OK.\n");
    fast_mem_init();  /* 缓存最优内存操作路径 */
    screen_set_color(0x0A, 0x00);
    screen_puts("  [OK]");
    screen_set_color(0x07, 0x00);
    screen_puts(" CPU initialized (");
    {
        char vendor[13];
        uint32_t *vp = (uint32_t *)vendor;
        __asm__ __volatile__("xor %%eax, %%eax; cpuid"
            : "=b"(vp[0]), "=c"(vp[2]), "=d"(vp[1]) : "a"(0));
        vendor[12] = '\0';
        screen_puts(vendor);
    }
    screen_puts(")\n");

    /* ---- 解析 CONFIG.SYS (使用 fast_memcpy) ---- */
    config_sys_init();
    {
        /*
         * CONFIG.SYS 内嵌在引导扇区之后的内核数据区。
         * 实际部署中由引导加载器从磁盘加载, 此处在编译时嵌入。
         */
        extern const char binary_programs_CONFIG_SYS_start[];
        extern const char binary_programs_CONFIG_SYS_end[];
        int cfg_len = (int)(binary_programs_CONFIG_SYS_end -
                            binary_programs_CONFIG_SYS_start);
        if (cfg_len > 0 && cfg_len < 4096) {
            /* 复制到栈缓冲区 (避免修改原始数据) */
            char cfg_buf[4096];
            fast_memcpy(cfg_buf, binary_programs_CONFIG_SYS_start, cfg_len);
            cfg_buf[cfg_len] = '\0';
            config_sys_parse(cfg_buf);
            boot_ok("CONFIG.SYS loaded");
        }
    }

    /* ---- GDT ---- */
    gdt_init();
    vga_dbg(2, 'G');
    serial_puts("[PlexsDOS] GDT OK.\n");
    boot_ok("GDT loaded (Ring 0-3 segments)");

    /* ---- 分页 ---- */
    paging_init();
    vga_dbg(3, 'P');
    serial_puts("[PlexsDOS] paging OK.\n");
    boot_ok("Paging enabled (4KB pages)");

    /* ---- IDT + PIC ---- */
    idt_init();
    vga_dbg(4, 'I');
    serial_puts("[PlexsDOS] IDT OK.\n");
    boot_ok("IDT initialized (256 vectors)");

    /* ---- 异常处理 ---- */
    panic_init();
    vga_dbg(5, 'X');
    serial_puts("[PlexsDOS] panic OK.\n");
    boot_ok("Exception handlers installed");

    /* ---- 键盘 ---- */
    keyboard_init();
    vga_dbg(6, 'K');
    serial_puts("[PlexsDOS] keyboard OK.\n");
    boot_ok("PS/2 keyboard ready");

    /* ---- 鼠标 ---- */
    mouse_init();
    vga_dbg(7, 'M');
    serial_puts("[PlexsDOS] mouse OK.\n");
    boot_ok("PS/2 mouse ready");

    /* ---- 启用中断 ---- */
    __asm__ __volatile__("sti");
    vga_dbg(8, 'T');
    serial_puts("[PlexsDOS] sti OK.\n");
    boot_ok("Interrupts enabled");

    /* ---- PCI ---- */
    pci_init();
    vga_dbg(9, 'B');
    serial_puts("[PlexsDOS] PCI OK.\n");
    boot_ok("PCI bus enumerated");

    /* ---- AHCI SATA ---- */
    {
        int ahci_ok = ahci_init();
        if (ahci_ok) {
            disk_set_override(ahci_read_wrap, ahci_write_wrap);
            serial_puts("[PlexsDOS] AHCI SATA OK.\n");
            boot_ok("AHCI SATA controller ready");
        } else {
            serial_puts("[PlexsDOS] no AHCI.\n");
            boot_skip("No AHCI controller (fallback to ATA)");
        }
    }

    /* ---- 驱动器子系统 ---- */
    drive_init();
    vga_dbg(10, 'D');

    /* ---- FDC (软盘控制器) ---- */
    if (fdc_init()) {
        serial_puts("[PlexsDOS] FDC OK.\n");
        drive_register(DRIVE_LETTER_A, DRIVE_TYPE_FLOPPY, 0, 0);
        drive_register(DRIVE_LETTER_B, DRIVE_TYPE_FLOPPY, 1, 0);
        boot_ok("Floppy controller ready (A: B:)");
    } else {
        serial_puts("[PlexsDOS] no FDC.\n");
        boot_skip("No floppy controller");
    }

    /* ---- ATA/AHCI 磁盘 + FAT32 ---- */
    {
        int disk_ok = 0;

        if (disk_read_override_active()) {
            disk_ok = 1;
            serial_puts("[PlexsDOS] disk I/O via AHCI.\n");
        } else if (disk_init()) {
            disk_ok = 1;
        }

        if (disk_ok) {
            vga_dbg(11, 'd');
            boot_ok("ATA/ATAPI disk detected");

            if (fat32_init()) {
                serial_puts("[PlexsDOS] FAT32 OK.\n");
                drive_register(DRIVE_LETTER_C, DRIVE_TYPE_HDD, 0, 2048);
                boot_ok("FAT32 filesystem mounted (C:)");
            } else {
                serial_puts("[PlexsDOS] FAT32 FAIL.\n");
                boot_fail("FAT32 mount failed");
            }

            /* MBR 分区扫描 */
            {
                uint8_t mbr[512];
                if (disk_read_sectors(0, 1, mbr) && mbr[510] == 0x55 && mbr[511] == 0xAA) {
                    int next_drive = DRIVE_LETTER_E;
                    int extra_count = 0;
                    for (int part_idx = 0; part_idx < 4 && next_drive < DRIVE_MAX; part_idx++) {
                        uint8_t *entry = mbr + 0x1BE + part_idx * 16;
                        uint8_t part_type = entry[4];
                        uint32_t part_lba = *(uint32_t *)(entry + 8);

                        if (part_type != 0x00 && part_lba != 0) {
                            if (part_type == 0x0B || part_type == 0x0C ||
                                part_type == 0x01 || part_type == 0x04 ||
                                part_type == 0x06 || part_type == 0x0E) {
                                if (part_lba != 2048 && next_drive < DRIVE_MAX) {
                                    drive_register(next_drive++, DRIVE_TYPE_HDD, 0, part_lba);
                                    extra_count++;
                                }
                            }
                        }
                    }
                    if (extra_count > 0) {
                        serial_puts("[PlexsDOS] extra partitions registered.\n");
                        boot_ok("Extra partitions scanned");
                    }
                }
            }
        } else {
            serial_puts("[PlexsDOS] no disk.\n");
            boot_skip("No disk drive found");
        }
    }

    /* ---- CD-ROM ---- */
    if (cdrom_init()) {
        vga_dbg(12, 'R');
        serial_puts("[PlexsDOS] CD-ROM OK.\n");
        drive_register(DRIVE_LETTER_D, DRIVE_TYPE_CDROM, 0, 0);
        boot_ok("CD-ROM drive ready (D:)");
        if (iso9660_mount()) {
            serial_puts("[PlexsDOS] ISO9660 OK.\n");
            boot_ok("ISO 9660 filesystem mounted");
        }
    } else {
        boot_skip("No CD-ROM drive");
    }

    /* ---- C++ 中断管理器 + 系统调用 ---- */
    interrupt_manager_init();
    vga_dbg(13, 'M');
    serial_puts("[PlexsDOS] INT OK.\n");
    boot_ok("System call handler ready (INT 0x22)");

    /* ===== HAL 块设备注册 ===== */
    {
        static struct hal_blkdev_ops fdc0_ops = { .read = fdc0_read, .write = NULL };
        static struct hal_blkdev_ops fdc1_ops = { .read = fdc1_read, .write = NULL };
        static struct hal_blkdev_ops ata_ops  = { .read = disk_read_sectors, .write = disk_write_sectors };
        hal_blkdev_register(HAL_BLKDEV_FLOPPY, 0, &fdc0_ops);
        hal_blkdev_register(HAL_BLKDEV_FLOPPY, 1, &fdc1_ops);
        hal_blkdev_register(HAL_BLKDEV_ATA, 0, &ata_ops);
    }
    serial_puts("[PlexsDOS] HAL block devices registered.\n");
    boot_ok("HAL block device layer ready");

    /* ===== ISA 传统设备枚举 =====
     * perf: 软盘/El Torito 启动 (DL < 0x80) 时系统不带 ISA 设备,
     * 跳过枚举可省 ~1-2ms。
     */
    if (boot_drive >= 0x80) {
        isa_init();
        serial_puts("[PlexsDOS] ISA devices enumerated.\n");
        boot_ok("ISA legacy devices enumerated");
    } else {
        serial_puts("[PlexsDOS] skip ISA enumeration (removable boot).\n");
        boot_info("ISA enumeration skipped (removable boot)");
    }

    /* ===== 进程调度器初始化 ===== */
    sched_init();
    vga_dbg(14, 'S');
    serial_puts("[PlexsDOS] scheduler OK.\n");
    boot_ok("Process scheduler initialized");

    /* ===== 用户系统初始化 ===== */
    users_init();
    serial_puts("[PlexsDOS] users OK.\n");
    boot_ok("User accounts subsystem ready");

    /* ---- 启动完成横幅 ---- */
    vga_dbg(15, '!');
    serial_puts("[PlexsDOS] system ready.\n");
    print_banner("       Nexsteaduser PlexsDOS  System Ready\n", NULL);

    /*
     * 安装介质检测: 根据启动驱动器号决定是否进入安装模式
     *   DL < 0x80: 从软盘/El Torito 启动 → 运行安装程序
     *   DL >= 0x80: 从硬盘启动 → 正常启动 Shell
     *
     * boot_drive 由 kernel_entry.S 在 _start 入口处从 BIOS DL 寄存器保存,
     * 该值由 BIOS 设置, 经 MBR → VBR 传递至此。
     */
    if (boot_drive < 0x80) {
        serial_puts("[PlexsDOS] boot source: floppy/CD (DL < 0x80).\n");
        boot_info("Boot source: removable media");
#if defined(MINIMAL_KERNEL) || defined(DEBUG_SKIP_INSTALLER)
        serial_puts("[PlexsDOS] boot from floppy/CD, entering shell (debug mode)...\n");
        boot_info("Installer bypassed (debug), launching shell...");
        shell_main();
#else
        serial_puts("[PlexsDOS] boot from floppy/CD, starting installer...\n");
        boot_info("Installation mode activated");
        installer_run();
        /* 安装程序返回后停机 — 用户已被告知移除介质并重启 */
        serial_puts("[PlexsDOS] installer finished, halting.\n");
        boot_fail("Installation complete — remove media and reboot");
        for (;;)
            __asm__ __volatile__("cli; hlt");
#endif
    } else {
        serial_puts("[PlexsDOS] boot source: hard disk (DL >= 0x80).\n");
        boot_ok("Boot source: hard disk");
    }

    /* 正常启动 Shell */
    print_banner("       Welcome to Nexsteaduser PlexsDOS\n", NULL);
    screen_putchar('\n');
    shell_main();
}
