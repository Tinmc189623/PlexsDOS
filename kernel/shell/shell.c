/*
 * Nexsteaduser — PlexsDOS
 * 命令行 Shell (32-bit 保护模式)
 * 作者: Tinmc189623 | 团队: Nexlyh
 *
 * 提供类 DOS 的命令行交互界面。
 * 支持 DOS 风格命令: DIR, TYPE, DEL, REN, COPY, CLS, VER, VOL, ECHO, HELP, RUN, EXIT, MKDIR, RMDIR, CD, DATE, TIME
 * 支持方向键 (VT100 转义序列) 编辑输入行。
 * 日期/时间通过 CMOS 端口 0x70/0x71 读取 MC146818 RTC。
 */

#include <plexsdos/types.h>
#include <plexsdos/config.h>
#include <plexsdos/screen.h>
#include <plexsdos/keyboard.h>
#include <plexsdos/shell.h>
#include <plexsdos/disk.h>
#include <plexsdos/fat32.h>
#include <plexsdos/loader.h>
#include <plexsdos/installer.h>
#include <plexsdos/cdrom.h>
#ifndef MINIMAL_KERNEL
#include <plexsdos/desktop.h>
#include <plexsdos/debug.h>
#include <plexsdos/editor.h>
#endif
#include <plexsdos/drive.h>

/* 显示管理器入口 (在 kernel/dm/plxdm_lightdm.c 中实现) */
extern int plxdm_lightdm_start(void);

/* 命令缓冲区 */
static char cmd_buf[SHELL_CMD_MAX_LEN];

/* 命令历史 (最近一条) */
static char last_cmd[SHELL_CMD_MAX_LEN];

/* 内部字符串函数 */

/*
 * shell_strcmp — 比较两个字符串
 * @s1: 字符串 1
 * @s2: 字符串 2
 * 返回: 0 = 相等, 负数 = s1 < s2, 正数 = s1 > s2。
 */
static int shell_strcmp(const char *s1, const char *s2)
{
    while (*s1 && *s2) {
        if (*s1 != *s2)
            return (int)*s1 - (int)*s2;
        s1++;
        s2++;
    }
    return (int)*s1 - (int)*s2;
}

/*
 * shell_strlen — 计算字符串长度
 * @s: 以 null 结尾的字符串
 * 返回: 字符数 (不含 null)。
 */
static int shell_strlen(const char *s)
{
    int len = 0;
    while (*s++)
        len++;
    return len;
}

/*
 * shell_strcpy — 复制字符串
 * @dst: 目标缓冲区
 * @src: 源字符串
 */
static void shell_strcpy(char *dst, const char *src)
{
    while ((*dst++ = *src++) != '\0')
        ;
}

/*
 * shell_skip_spaces — 跳过前导空白字符
 * @str: 输入字符串
 * 返回: 指向第一个非空白字符的指针。
 */
static const char *shell_skip_spaces(const char *str)
{
    while (*str == ' ' || *str == '\t')
        str++;
    return str;
}

/*
 * cmos_read — 从 CMOS/RTC 寄存器读取一个字节
 * @reg: 寄存器号 (0x00-0x7F)
 * 返回: 寄存器值。
 *
 * 通过端口 0x70 (地址) 和 0x71 (数据) 访问 MC146818 RTC。
 * 读取前清除 NMI 屏蔽位 (bit 7 = 0)。
 */
static uint8_t cmos_read(uint8_t reg)
{
    uint8_t addr = (uint8_t)(reg & 0x7F);
    uint8_t val;
    __asm__ __volatile__("outb %0, %1" : : "a"(addr), "Nd"((uint16_t)0x70));
    __asm__ __volatile__("inb %1, %0" : "=a"(val) : "Nd"((uint16_t)0x71));
    return val;
}

/*
 * bcd_to_bin — BCD 码转二进制
 * @val: BCD 值 (如 0x26 表示 26)
 * 返回: 二进制值 (26)。
 */
static uint8_t bcd_to_bin(uint8_t val)
{
    return (val >> 4) * 10 + (val & 0x0F);
}

/* ===== DOS 命令实现 ===== */

/*
 * cmd_help — 显示帮助信息
 *
 * 列出所有可用的 DOS 命令及简要说明。
 */
static void cmd_help(void)
{
    screen_set_color(0x0B, 0x00);
    screen_puts("Nexsteaduser PlexsDOS v0.1 - Command Reference\n\n");
    screen_reset_color();

    screen_set_color(0x0F, 0x00);
    screen_puts("  DIR          List directory contents\n");
    screen_puts("  TYPE <file>  Display file contents\n");
    screen_puts("  CLS          Clear screen\n");
    screen_puts("  VER          Show version info\n");
    screen_puts("  VOL          Show volume label\n");
    screen_puts("  ECHO <text>  Display text\n");
    screen_puts("  DATE         Show current date\n");
    screen_puts("  TIME         Show current time\n");
    screen_puts("  RUN <file>   Execute a .COMX program\n");
    screen_puts("  DRIVE        List available drives\n");
    screen_puts("  X:           Switch to drive (e.g. A:, C:)\n");
    screen_puts("  INSTALL      Install PlexsDOS to hard disk\n");
    screen_puts("  CDIR         List CD-ROM directory\n");
    screen_puts("  CCAT <file>  Display CD-ROM file contents\n");
    screen_puts("  CDMOUNT      Mount CD-ROM filesystem\n");
    screen_puts("  CDEJECT      Eject CD-ROM tray\n");
    screen_puts("  CDINFO       Show CD-ROM device info\n");
    screen_puts("  REBOOT       Reboot system\n");
    screen_puts("  HELP         Show this help\n");
    screen_puts("  EXIT         Exit shell (reboot)\n");
    screen_putchar('\n');
    screen_set_color(0x08, 0x00);
    screen_puts("  Also: LS, CAT, CLEAR (Unix aliases)\n");
    screen_reset_color();
    screen_putchar('\n');
}

/*
 * cmd_dir — 列出当前目录内容 (DOS DIR 命令)
 *
 * 调用 FAT32 驱动列出根目录内容。
 */
static void cmd_dir(void)
{
    fat32_list_root();
}

/*
 * cmd_type — 显示文件内容 (DOS TYPE 命令)
 * @filename: 文件名参数
 *
 * 从 FAT32 根目录查找文件, 加载到临时缓冲区并打印。
 */
static void cmd_type(const char *filename)
{
    struct fat32_dir_entry *entry;
    uint8_t buf[512];
    uint32_t size;
    uint32_t i;

    if (*filename == '\0') {
        screen_puts("Required parameter missing\n");
        return;
    }

    entry = fat32_find_file(filename);
    if (!entry) {
        screen_puts("File not found - ");
        screen_puts(filename);
        screen_putchar('\n');
        return;
    }

    size = fat32_load_file(entry, (uint32_t)buf);
    if (size == 0) {
        screen_puts("Empty file or read error.\n");
        return;
    }

    for (i = 0; i < size && i < sizeof(buf); i++) {
        char c = (char)buf[i];
        if (c >= 32 && c <= 126) {
            screen_putchar(c);
        } else if (c == '\n') {
            screen_putchar('\n');
        } else if (c == '\r') {
            /* skip */
        } else if (c == '\t') {
            screen_putchar('\t');
        } else {
            screen_putchar('.');
        }
    }
    screen_putchar('\n');
}

/*
 * cmd_cls — 清屏 (DOS CLS 命令)
 */
static void cmd_cls(void)
{
    screen_clear();
}

/*
 * cmd_ver — 显示版本信息 (DOS VER 命令)
 */
static void cmd_ver(void)
{
    screen_puts("\nNexsteaduser PlexsDOS v0.1\n");
    screen_puts("Author: Tinmc189623 | Team: Nexlyh\n");
    screen_puts("Kernel: 32-bit protected mode (self-written)\n\n");
}

/*
 * cmd_vol — 显示卷标 (DOS VOL 命令)
 *
 * 从当前驱动器的 VBR 读取卷标和卷序列号。
 * VBR 中: 卷标在偏移 0x47 (11 字节), 序列号在偏移 0x43 (4 字节)。
 */
static void cmd_vol(void)
{
    uint8_t sec[512];
    int cur = drive_get_current();
    const struct drive_info *info = drive_get_info(cur);
    char letter = drive_letter_to_char(cur);

    if (!info || info->type != DRIVE_TYPE_HDD) {
        screen_puts("Current drive has no volume info\n");
        return;
    }

    /* 读取当前驱动器的 VBR */
    if (!disk_read_sectors(info->partition_lba, 1, sec)) {
        screen_puts("Error reading volume info\n");
        return;
    }

    /* 提取卷标 (11 字节, 偏移 0x47) */
    screen_puts(" Volume in drive ");
    screen_putchar(letter);
    screen_puts(" is ");
    for (int i = 0; i < 11; i++) {
        char c = (char)sec[0x47 + i];
        if (c >= 32 && c < 127)
            screen_putchar(c);
    }
    screen_putchar('\n');

    /* 提取卷序列号 (4 字节, 偏移 0x43, 格式 XXXX-XXXX) */
    uint32_t serial = *(uint32_t *)(sec + 0x43);
    screen_puts(" Volume Serial Number is ");
    screen_put_hex((serial >> 16) & 0xFFFF);
    screen_putchar('-');
    screen_put_hex(serial & 0xFFFF);
    screen_putchar('\n');
}

/*
 * cmd_echo — 显示文本 (DOS ECHO 命令)
 * @args: 要显示的文本
 */
static void cmd_echo(const char *args)
{
    screen_puts(args);
    screen_putchar('\n');
}

/*
 * cmd_date — 显示日期 (DOS DATE 命令)
 *
 * 通过 CMOS 端口 0x70/0x71 读取 MC146818 RTC 日期。
 * RTC 寄存器: 0x07=日, 0x08=月, 0x09=年 (BCD 格式)。
 */
static void cmd_date(void)
{
    uint8_t day  = bcd_to_bin(cmos_read(0x07));
    uint8_t month = bcd_to_bin(cmos_read(0x08));
    uint8_t year  = bcd_to_bin(cmos_read(0x09));

    screen_puts("Current date is: 20");
    screen_put_dec(year);
    screen_putchar('-');
    screen_put_dec(month);
    screen_putchar('-');
    screen_put_dec(day);
    screen_putchar('\n');
}

/*
 * cmd_time — 显示时间 (DOS TIME 命令)
 *
 * 通过 CMOS 端口 0x70/0x71 读取 MC146818 RTC 时间。
 * RTC 寄存器: 0x00=秒, 0x02=分, 0x04=时 (BCD 格式)。
 */
static void cmd_time(void)
{
    uint8_t sec  = bcd_to_bin(cmos_read(0x00));
    uint8_t min  = bcd_to_bin(cmos_read(0x02));
    uint8_t hour = bcd_to_bin(cmos_read(0x04));

    screen_puts("Current time is: ");
    screen_put_dec(hour);
    screen_putchar(':');
    screen_put_dec(min);
    screen_putchar(':');
    screen_put_dec(sec);
    screen_putchar('\n');
}

/*
 * cmd_run — 加载并执行 .COMX 程序
 * @filename: 文件名 (如 "HELLO.COMX")
 */
static void cmd_run(const char *filename)
{
    if (*filename == '\0') {
        screen_puts("Required parameter missing\n");
        return;
    }

    loader_run(filename);
}

/*
 * cmd_install — 运行安装程序
 *
 * 将 PlexsDOS 安装到硬盘。
 */
static void cmd_install(void)
{
    installer_run();
}

/*
 * cmd_cdir — 列出 CD-ROM 目录内容
 *
 * 挂载 ISO 9660 文件系统 (如果尚未挂载),
 * 然后读取并显示根目录或指定目录的内容。
 */
static void cmd_cdir(void)
{
    const struct iso9660_fs *fs = iso9660_get_fs();
    if (!fs || !fs->mounted) {
        screen_puts("No CD-ROM filesystem mounted.\n");
        screen_puts("Use CDMOUNT to mount a disc first.\n");
        return;
    }

    const struct iso9660_entry *root = iso9660_get_root();
    if (!root) {
        screen_puts("Error reading root directory.\n");
        return;
    }

    struct iso9660_entry entries[64];
    int count = iso9660_read_dir(root->lba, root->size, entries, 64);

    screen_set_color(0x0B, 0x00);
    screen_puts(" Volume label: ");
    screen_puts(fs->volume_id);
    screen_putchar('\n');
    screen_puts(" Directory of CD-ROM\n\n");
    screen_set_color(0x0F, 0x00);

    for (int i = 0; i < count; i++) {
        if (entries[i].is_directory) {
            screen_set_color(0x0E, 0x00);
            screen_puts("[DIR]  ");
            screen_puts(entries[i].name);
        } else {
            screen_set_color(0x07, 0x00);
            screen_puts("       ");
            screen_puts(entries[i].name);
            screen_puts("  ");
            screen_put_dec(entries[i].size);
            screen_puts(" bytes");
        }
        screen_putchar('\n');
    }

    screen_set_color(0x07, 0x00);
    screen_putchar('\n');
    screen_put_dec(count);
    screen_puts(" file(s)\n");
}

/*
 * cmd_ccat — 显示 CD-ROM 文件内容
 * @filename: 文件名
 *
 * 在 CD-ROM 根目录中查找文件并显示其内容。
 */
static void cmd_ccat(const char *filename)
{
    if (*filename == '\0') {
        screen_puts("Required parameter missing\n");
        return;
    }

    const struct iso9660_fs *fs = iso9660_get_fs();
    if (!fs || !fs->mounted) {
        screen_puts("No CD-ROM filesystem mounted.\n");
        return;
    }

    const struct iso9660_entry *root = iso9660_get_root();
    struct iso9660_entry entries[64];
    int count = iso9660_read_dir(root->lba, root->size, entries, 64);

    /* 在根目录中查找文件 */
    struct iso9660_entry *found = NULL;
    for (int i = 0; i < count; i++) {
        /* 简单字符串比较 */
        const char *a = entries[i].name;
        const char *b = filename;
        bool match = true;
        while (*a && *b) {
            char ca = *a;
            char cb = *b;
            if (ca >= 'A' && ca <= 'Z') ca += 32;
            if (cb >= 'A' && cb <= 'Z') cb += 32;
            if (ca != cb) {
                match = false;
                break;
            }
            a++;
            b++;
        }
        if (match && *a == *b && !entries[i].is_directory) {
            found = &entries[i];
            break;
        }
    }

    if (!found) {
        screen_puts("File not found - ");
        screen_puts(filename);
        screen_putchar('\n');
        return;
    }

    /* 读取文件内容 (使用静态缓冲区避免栈溢出) */
    static uint8_t cdrom_file_buf[4096];
    uint32_t size = iso9660_read_file(found->lba, found->size,
                                       cdrom_file_buf, sizeof(cdrom_file_buf));
    if (size == 0) {
        screen_puts("Empty file or read error.\n");
        return;
    }

    /* 显示内容 */
    for (uint32_t i = 0; i < size; i++) {
        char c = (char)cdrom_file_buf[i];
        if (c >= 32 && c <= 126) {
            screen_putchar(c);
        } else if (c == '\n') {
            screen_putchar('\n');
        } else if (c == '\r') {
            /* skip */
        } else if (c == '\t') {
            screen_putchar('\t');
        } else {
            screen_putchar('.');
        }
    }
    screen_putchar('\n');
}

/*
 * cmd_cdmount — 挂载 CD-ROM ISO 9660 文件系统
 *
 * 初始化 CD-ROM 驱动 (如果未初始化) 并挂载 ISO 9660。
 */
static void cmd_cdmount(void)
{
    const struct cdrom_info *info = cdrom_get_info();
    if (!info->present) {
        screen_puts("No CD-ROM device detected.\n");
        return;
    }

    if (iso9660_mount()) {
        screen_puts("CD-ROM mounted successfully.\n");
    } else {
        screen_puts("Failed to mount CD-ROM.\n");
    }
}

/*
 * cmd_cdeject — 弹出 CD-ROM 托盘
 */
static void cmd_cdeject(void)
{
    cdrom_eject();
}

/*
 * cmd_cdinfo — 显示 CD-ROM 设备信息
 *
 * 显示 ATAPI 设备检测结果、厂商信息、容量等。
 */
static void cmd_cdinfo(void)
{
    const struct cdrom_info *info = cdrom_get_info();

    if (!info->present) {
        screen_puts("No CD-ROM device detected.\n");
        return;
    }

    screen_set_color(0x0B, 0x00);
    screen_puts("CD-ROM Device Info\n\n");
    screen_set_color(0x0F, 0x00);

    screen_puts("  Vendor:    ");
    screen_puts(info->vendor);
    screen_putchar('\n');

    screen_puts("  Product:   ");
    screen_puts(info->product);
    screen_putchar('\n');

    screen_puts("  Version:   ");
    screen_puts(info->revision);
    screen_putchar('\n');

    screen_puts("  Sector:    ");
    screen_put_dec(info->sector_size);
    screen_puts(" bytes\n");

    screen_puts("  Capacity:  ");
    screen_put_dec(info->total_sectors);
    screen_puts(" sectors (");
    screen_put_dec(info->total_sectors / 512);
    screen_puts(" MB)\n");

    const struct iso9660_fs *fs = iso9660_get_fs();
    if (fs && fs->mounted) {
        screen_set_color(0x0A, 0x00);
        screen_puts("\n  ISO 9660:  mounted\n");
        screen_set_color(0x0F, 0x00);
        screen_puts("  Volume:    ");
        screen_puts(fs->volume_id);
        screen_putchar('\n');
        screen_puts("  Root LBA:  ");
        screen_put_dec(fs->root_lba);
        screen_putchar('\n');
    }

    screen_set_color(0x07, 0x00);
    screen_putchar('\n');
}

/*
 * cmd_reboot — 重启系统
 */
static void cmd_reboot(void)
{
    screen_puts("Rebooting...\n");

    __asm__ __volatile__(
        "mov $0xFE, %%al\n\t"
        "out %%al, $0x64\n\t"
        :
        :
        : "ax", "memory"
    );

    __asm__ __volatile__(
        "cli\n\t"
        "lidt (%%eax)\n\t"
        "int $0x00\n\t"
        :
        : "a"(0)
        : "memory"
    );

    while (1)
        __asm__ __volatile__("hlt");
}

/*
 * cmd_dm — 启动显示管理器 (LightDM)
 */
static void cmd_dm(void)
{
    screen_puts("Starting Display Manager...\n");
    plxdm_lightdm_start();
    screen_init();
    screen_puts("Display Manager exited.\n");
}

#ifndef MINIMAL_KERNEL
/*
 * cmd_gui — 启动图形界面
 *
 * 初始化桌面环境并进入 GUI 主循环。
 * 按 ESC 退出 GUI 返回 Shell。
 */
static void cmd_gui(void)
{
    screen_puts("Starting GUI...\n");

    if (desktop_init()) {
        desktop_run();
        desktop_shutdown();
        screen_init();  /* 恢复文本模式屏幕 */
        screen_puts("GUI exited.\n");
    } else {
        screen_puts("GUI init failed.\n");
    }
}

/*
 * cmd_debug — 启动调试工具 (DOS DEBUG 命令)
 *
 * 进入交互式调试器, 提供内存转储、端口 I/O、反汇编等功能。
 */
static void cmd_debug(void)
{
    debug_main();
    screen_init();
    screen_puts("Debug exited.\n");
}

/*
 * cmd_edit — 启动文本编辑器 (DOS EDIT 命令)
 * @filename: 要编辑的文件名 (可选)
 *
 * 打开全屏文本编辑器。支持加载/保存 FAT32 文件。
 */
static void cmd_edit(const char *filename)
{
    editor_main(filename);
}
#endif

/*
 * cmd_drive — 显示驱动器列表 (DOS DRIVE 命令)
 *
 * 遍历驱动器表, 显示所有已注册的驱动器及当前驱动器。
 */
static void cmd_drive(void)
{
    int cur = drive_get_current();

    screen_set_color(0x0B, 0x00);
    screen_puts("Drive  Type        Status\n");
    screen_puts("-----  ----------  ------\n");
    screen_reset_color();

    for (int i = 0; i < DRIVE_MAX; i++) {
        const struct drive_info *info = drive_get_info(i);
        if (!info || info->type == DRIVE_TYPE_NONE)
            continue;

        screen_putchar(' ');
        screen_putchar(drive_letter_to_char(i));
        screen_puts(":    ");

        /* 类型名称 */
        const char *type_name = drive_get_type_name(info->type);
        screen_puts(type_name);
        /* 对齐 */
        int len = 0;
        while (type_name[len]) len++;
        for (int j = len; j < 10; j++)
            screen_putchar(' ');

        /* 状态 */
        if (i == cur) {
            screen_set_color(0x0A, 0x00);
            screen_puts("(current)");
            screen_reset_color();
        }
        screen_putchar('\n');
    }
    screen_putchar('\n');
}

/*
 * cmd_switch_drive — 切换当前驱动器 (DOS X: 命令)
 * @letter: 驱动器字母 ('A' ~ 'D')
 *
 * 切换当前驱动器。如果驱动器不存在, 显示错误信息。
 */
static void cmd_switch_drive(char letter)
{
    int idx;

    if (letter >= 'a' && letter <= 'z')
        letter -= 32;

    idx = letter - 'A';

    if (drive_set_current(idx)) {
        screen_putchar(letter);
        screen_puts(":\\> Drive switched.\n");
    } else {
        screen_putchar(letter);
        screen_puts(": Drive not available.\n");
    }
}

/*
 * shell_exec — 解析并执行命令
 * @cmd: 完整命令行字符串
 *
 * 将命令名转大写后匹配内置命令, 调用对应处理函数。
 */
static void shell_exec(const char *cmd)
{
    char cmd_name[16];
    const char *args;
    int i;

    cmd = shell_skip_spaces(cmd);
    if (*cmd == '\0')
        return;

    /* 保存最后一条命令 */
    shell_strcpy(last_cmd, cmd);

    /* 检测驱动器切换命令 (X:) */
    if (cmd[0] != '\0' && cmd[1] == ':') {
        char c = cmd[0];
        if ((c >= 'A' && c <= 'Z') || (c >= 'a' && c <= 'z')) {
            const char *rest = shell_skip_spaces(cmd + 2);
            if (*rest == '\0') {
                cmd_switch_drive(c);
                return;
            }
        }
    }

    /* 提取命令名 (转大写) */
    i = 0;
    while (*cmd && *cmd != ' ' && *cmd != '\t' && i < 15) {
        char c = *cmd;
        if (c >= 'a' && c <= 'z')
            c -= 32;
        cmd_name[i++] = c;
        cmd++;
    }
    cmd_name[i] = '\0';

    args = shell_skip_spaces(cmd);

    if (shell_strcmp(cmd_name, "HELP") == 0 || shell_strcmp(cmd_name, "?") == 0) {
        cmd_help();
    } else if (shell_strcmp(cmd_name, "DRIVE") == 0 || shell_strcmp(cmd_name, "DRV") == 0) {
        cmd_drive();
    } else if (shell_strcmp(cmd_name, "DIR") == 0 || shell_strcmp(cmd_name, "LS") == 0) {
        cmd_dir();
    } else if (shell_strcmp(cmd_name, "TYPE") == 0 || shell_strcmp(cmd_name, "CAT") == 0) {
        cmd_type(args);
    } else if (shell_strcmp(cmd_name, "CLS") == 0 || shell_strcmp(cmd_name, "CLEAR") == 0) {
        cmd_cls();
    } else if (shell_strcmp(cmd_name, "VER") == 0) {
        cmd_ver();
    } else if (shell_strcmp(cmd_name, "VOL") == 0) {
        cmd_vol();
    } else if (shell_strcmp(cmd_name, "ECHO") == 0) {
        cmd_echo(args);
    } else if (shell_strcmp(cmd_name, "DATE") == 0) {
        cmd_date();
    } else if (shell_strcmp(cmd_name, "TIME") == 0) {
        cmd_time();
    } else if (shell_strcmp(cmd_name, "RUN") == 0) {
        cmd_run(args);
    } else if (shell_strcmp(cmd_name, "INSTALL") == 0) {
        cmd_install();
    } else if (shell_strcmp(cmd_name, "CDIR") == 0) {
        cmd_cdir();
    } else if (shell_strcmp(cmd_name, "CCAT") == 0) {
        cmd_ccat(args);
    } else if (shell_strcmp(cmd_name, "CDMOUNT") == 0) {
        cmd_cdmount();
    } else if (shell_strcmp(cmd_name, "CDEJECT") == 0) {
        cmd_cdeject();
    } else if (shell_strcmp(cmd_name, "CDINFO") == 0) {
        cmd_cdinfo();
#ifndef MINIMAL_KERNEL
    } else if (shell_strcmp(cmd_name, "GUI") == 0) {
        cmd_gui();
    } else if (shell_strcmp(cmd_name, "DEBUG") == 0 || shell_strcmp(cmd_name, "DBG") == 0) {
        cmd_debug();
    } else if (shell_strcmp(cmd_name, "EDIT") == 0 || shell_strcmp(cmd_name, "ED") == 0) {
        cmd_edit(args);
#endif
    } else if (shell_strcmp(cmd_name, "DM") == 0) {
        cmd_dm();
    } else if (shell_strcmp(cmd_name, "REBOOT") == 0 || shell_strcmp(cmd_name, "RESET") == 0) {
        cmd_reboot();
    } else if (shell_strcmp(cmd_name, "EXIT") == 0 || shell_strcmp(cmd_name, "QUIT") == 0) {
        cmd_reboot();
    } else {
        screen_puts("Bad command or file name\n");
    }
}

/*
 * shell_read_line — 带行编辑的命令行读取
 * @buf:     目标缓冲区
 * @max_len: 缓冲区最大长度 (含 null 终止符)
 *
 * 支持: 字符输入、退格删除、Enter 确认、方向键 (VT100 转义序列)
 * 左/右方向键移动光标, 上方向键召回历史命令。
 */
static int shell_read_line(char *buf, int max_len)
{
    int len = 0;
    int pos = 0;  /* 光标在行内的位置 */
    char c;
    int esc_state = 0;  /* 转义序列状态: 0=正常, 1=收到ESC, 2=收到[ */

    while (1) {
        c = keyboard_getchar();

        /* 处理 VT100 转义序列 (方向键) */
        if (esc_state == 2) {
            esc_state = 0;
            switch (c) {
            case 'A': /* 上: 召回历史命令 */
                if (last_cmd[0] != '\0') {
                    int tmp;
                    /* 清除当前行显示 */
                    while (pos > 0) {
                        screen_putchar('\b');
                        pos--;
                    }
                    tmp = len;
                    while (tmp > 0) {
                        screen_putchar(' ');
                        tmp--;
                    }
                    tmp = len;
                    while (tmp > 0) {
                        screen_putchar('\b');
                        tmp--;
                    }
                    /* 复制历史命令 */
                    shell_strcpy(buf, last_cmd);
                    len = shell_strlen(buf);
                    pos = len;
                    screen_puts(buf);
                }
                continue;
            case 'B': /* 下: 暂不处理 */
                continue;
            case 'C': /* 右: 光标右移 */
                if (pos < len) {
                    screen_putchar(buf[pos]);
                    pos++;
                }
                continue;
            case 'D': /* 左: 光标左移 */
                if (pos > 0) {
                    screen_putchar('\b');
                    pos--;
                }
                continue;
            case 'H': /* Home */
                while (pos > 0) {
                    screen_putchar('\b');
                    pos--;
                }
                continue;
            case 'F': /* End */
                while (pos < len) {
                    screen_putchar(buf[pos]);
                    pos++;
                }
                continue;
            default:
                continue;
            }
        }

        if (esc_state == 1) {
            esc_state = 0;
            if (c == '[') {
                esc_state = 2;
                continue;
            }
            continue;
        }

        /* ESC */
        if (c == 0x1B) {
            esc_state = 1;
            continue;
        }

        /* Enter */
        if (c == '\r' || c == '\n') {
            while (pos < len) {
                screen_putchar(buf[pos]);
                pos++;
            }
            screen_putchar('\n');
            break;
        }

        /* Backspace / DEL */
        if (c == '\b' || c == 0x7F) {
            if (pos > 0) {
                int j;
                pos--;
                len--;
                for (j = pos; j < len; j++)
                    buf[j] = buf[j + 1];
                buf[len] = '\0';
                screen_putchar('\b');
                for (j = pos; j < len; j++)
                    screen_putchar(buf[j]);
                screen_putchar(' ');
                for (j = len; j >= pos; j--)
                    screen_putchar('\b');
            }
            continue;
        }

        /* 普通可打印字符 */
        if (c >= 32 && c < 127 && len < max_len - 1) {
            int j;
            if (pos < len) {
                for (j = len; j > pos; j--)
                    buf[j] = buf[j - 1];
            }
            buf[pos] = c;
            len++;
            pos++;
            buf[len] = '\0';
            for (j = pos - 1; j < len; j++)
                screen_putchar(buf[j]);
            for (j = len; j > pos; j--)
                screen_putchar('\b');
        }
    }

    buf[len] = '\0';
    return len;
}

/*
 * shell_main — Shell 主循环
 *
 * 初始化并进入命令行循环: 显示提示符, 读取命令, 执行命令。
 */
void shell_main(void)
{
    screen_puts("Nexsteaduser PlexsDOS Shell.\n");
    screen_puts("Type HELP for available commands.\n\n");

    last_cmd[0] = '\0';

    while (1) {
        /* 动态提示符: "C:\> " 根据当前驱动器变化 */
        int cur = drive_get_current();
        char letter = drive_letter_to_char(cur);
        char prompt[12];
        prompt[0] = letter;
        prompt[1] = ':';
        prompt[2] = '\\';
        prompt[3] = '>';
        prompt[4] = ' ';
        prompt[5] = '\0';

        screen_set_color(0x0A, 0x00);
        screen_puts(prompt);
        screen_reset_color();

        shell_read_line(cmd_buf, SHELL_CMD_MAX_LEN);
        shell_exec(cmd_buf);
    }
}
