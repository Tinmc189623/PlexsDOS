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

/* 启动驱动器号 (由 kernel_entry.S 从 BIOS DL 寄存器保存) */
extern uint8_t boot_drive;

/*
 * vga_dbg — 直接写 VGA 文本缓冲区用于调试
 * @pos: 屏幕列号 (0-79)
 * @c: 要显示的字符
 *
 * 写入第一行指定位置, 白色前景, 黑色背景。
 * 用于在串口不可用时追踪内核初始化进度。
 */
static void vga_dbg(int pos, char c)
{
    volatile char *vga = (volatile char *)0xB8000;
    vga[pos * 2] = c;
    vga[pos * 2 + 1] = 0x0F;
}

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
 */
void kernel_main(void)
{
    /* 早期 VGA 调试: 在 screen_init 清屏前写入, 确认内核入口到达 */
    vga_dbg(0, 'E');
    vga_dbg(1, 'N');
    vga_dbg(2, 'T');
    vga_dbg(3, 'R');
    vga_dbg(4, 'Y');

    serial_puts("[PlexsDOS] kernel_main entered.\n");

    /* 初始化屏幕驱动 (VGA 文本模式) */
    screen_init();
    vga_dbg(0, 'S');
    serial_puts("[PlexsDOS] screen OK.\n");

    /* 初始化 CPU 特性 (SSE/MMX) */
    cpu_init();
    vga_dbg(1, 'C');
    serial_puts("[PlexsDOS] CPU OK.\n");

    /* 初始化 GDT (Ring 0-3 段描述符 + TSS) */
    gdt_init();
    vga_dbg(2, 'G');
    serial_puts("[PlexsDOS] GDT OK.\n");

    /* 初始化分页 (页目录/页表, 身份映射, 启用 CR0.PG) */
    paging_init();
    vga_dbg(3, 'P');
    serial_puts("[PlexsDOS] paging OK.\n");

    /* 初始化中断描述符表 (IDT) + 重映射 PIC */
    idt_init();
    vga_dbg(4, 'I');
    serial_puts("[PlexsDOS] IDT OK.\n");

    /* 注册 CPU 异常处理程序 (红屏 RSOD) */
    panic_init();
    vga_dbg(5, 'X');
    serial_puts("[PlexsDOS] panic OK.\n");

    /* 初始化键盘驱动 (注册 INT 0x21 处理程序) */
    keyboard_init();
    vga_dbg(6, 'K');
    serial_puts("[PlexsDOS] keyboard OK.\n");

    /* 重新启用中断 (引导扇区在进入保护模式前执行了 cli) */
    __asm__ __volatile__("sti");
    vga_dbg(7, 'T');
    serial_puts("[PlexsDOS] sti OK.\n");

    /* 初始化 PCI 总线 (查找 IDE 控制器, 启用 DMA) */
    pci_init();
    vga_dbg(8, 'B');
    serial_puts("[PlexsDOS] PCI OK.\n");

    /* 初始化驱动器子系统 */
    drive_init();
    vga_dbg(9, 'D');

    /* 初始化磁盘驱动 (ATA PIO + DMA) */
    if (disk_init()) {
        vga_dbg(10, 'd');
        serial_puts("[PlexsDOS] disk OK.\n");
        if (fat32_init()) {
            serial_puts("[PlexsDOS] FAT32 OK.\n");
            drive_register(DRIVE_LETTER_C, DRIVE_TYPE_HDD, 0, 2048);
        } else {
            serial_puts("[PlexsDOS] FAT32 FAIL.\n");
        }
    } else {
        serial_puts("[PlexsDOS] no disk.\n");
    }

    /* 初始化 CD-ROM (ATAPI) */
    if (cdrom_init()) {
        vga_dbg(11, 'R');
        serial_puts("[PlexsDOS] CD-ROM OK.\n");
        drive_register(DRIVE_LETTER_D, DRIVE_TYPE_CDROM, 0, 0);
        if (iso9660_mount()) {
            serial_puts("[PlexsDOS] ISO9660 OK.\n");
        }
    }

    /* 初始化 C++ 中断管理器 (INT 21h 系统调用) */
    interrupt_manager_init();
    vga_dbg(12, 'M');
    serial_puts("[PlexsDOS] INT OK.\n");

    vga_dbg(13, '!');
    serial_puts("[PlexsDOS] ready.\n");

    /*
     * 安装介质检测: 根据启动驱动器号决定是否进入安装模式
     *   DL < 0x80: 从软盘/El Torito 启动 → 运行安装程序
     *   DL >= 0x80: 从硬盘启动 → 正常启动 Shell
     *
     * boot_drive 由 kernel_entry.S 在 _start 入口处从 BIOS DL 寄存器保存,
     * 该值由 BIOS 设置, 经 MBR → VBR 传递至此。
     */
    if (boot_drive < 0x80) {
        serial_puts("[PlexsDOS] boot from floppy/CD, starting installer...\n");
        installer_run();
        /* 安装程序返回后停机 — 用户已被告知移除介质并重启 */
        serial_puts("[PlexsDOS] installer finished, halting.\n");
        for (;;)
            __asm__ __volatile__("cli; hlt");
    }

    /* 正常启动 Shell */
    shell_main();
}
