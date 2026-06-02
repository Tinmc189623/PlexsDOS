/*
 * Nexsteaduser — PlexsDOS
 * debug.c — 交互式调试工具
 * 作者: Tinmc189623 | 团队: Nexlyh
 *
 * 类 DOS DEBUG.COM 的调试工具。
 * 子命令:
 *   D [addr] [len]  — 内存转储 (hex dump)
 *   E addr val...    — 修改内存字节
 *   R                — 显示寄存器状态
 *   P port           — 读取 I/O 端口
 *   O port val       — 写入 I/O 端口
 *   U [addr] [len]   — 反汇编 x86 指令
 *   H val1 val2      — 十六进制加减计算
 *   M addr len val   — 填充内存
 *   C addr1 addr2 n  — 比较内存块
 *   S addr len val   — 搜索内存
 *   Q                — 退出调试器
 */

#include <plexsdos/debug.h>
#include <plexsdos/types.h>
#include <plexsdos/screen.h>
#include <plexsdos/keyboard.h>
#include <plexsdos/hal.h>

/* 调试器输入缓冲区 */
#define DBG_CMD_MAX  80
static char dbg_cmd[DBG_CMD_MAX];

/* 默认地址指针 (D/E 命令连续使用时自动递增) */
static uint32_t dbg_ptr = 0;

/*
 * dbg_strlen — 计算字符串长度
 */
static int dbg_strlen(const char *s)
{
    int len = 0;
    while (*s++) len++;
    return len;
}

/*
 * dbg_strcmp — 字符串比较
 */
static int dbg_strcmp(const char *a, const char *b)
{
    while (*a && *b) {
        if (*a != *b) return (int)*a - (int)*b;
        a++; b++;
    }
    return (int)*a - (int)*b;
}

/*
 * dbg_skip_spaces — 跳过前导空白
 */
static const char *dbg_skip_spaces(const char *s)
{
    while (*s == ' ' || *s == '\t') s++;
    return s;
}

/*
 * dbg_hex_digit — 将十六进制字符转为数值
 * 返回: 0-15, -1 = 无效字符。
 */
static int dbg_hex_digit(char c)
{
    if (c >= '0' && c <= '9') return c - '0';
    if (c >= 'a' && c <= 'f') return c - 'a' + 10;
    if (c >= 'A' && c <= 'F') return c - 'A' + 10;
    return -1;
}

/*
 * dbg_parse_hex — 解析十六进制数值
 * @str: 输入字符串
 * @out: [输出] 解析结果
 * 返回: 消耗的字符数, 0 = 失败。
 */
static int dbg_parse_hex(const char *str, uint32_t *out)
{
    int n = 0;
    uint32_t val = 0;
    int d;

    while ((d = dbg_hex_digit(str[n])) >= 0) {
        val = (val << 4) | (uint32_t)d;
        n++;
    }

    if (n == 0)
        return 0;

    *out = val;
    return n;
}

/*
 * dbg_print_hex8 — 输出 8-bit 十六进制值 (不带前缀)
 */
static void dbg_print_hex8(uint8_t val)
{
    static const char hex[] = "0123456789ABCDEF";
    screen_putchar(hex[(val >> 4) & 0x0F]);
    screen_putchar(hex[val & 0x0F]);
}

/*
 * dbg_print_hex32 — 输出 32-bit 十六进制值 (带 0x 前缀)
 */
static void dbg_print_hex32(uint32_t val)
{
    screen_puts("0x");
    for (int i = 28; i >= 0; i -= 4)
        screen_putchar("0123456789ABCDEF"[(val >> i) & 0x0F]);
}

/*
 * dbg_print_dec — 输出十进制值
 */
static void dbg_print_dec(uint32_t val)
{
    if (val >= 10) dbg_print_dec(val / 10);
    screen_putchar('0' + (char)(val % 10));
}

/*
 * dbg_read_line — 读取一行输入
 */
static int dbg_read_line(char *buf, int max)
{
    int len = 0;
    char c;

    while (1) {
        c = keyboard_getchar();
        if (c == '\r' || c == '\n') {
            screen_putchar('\n');
            break;
        }
        if ((c == '\b' || c == 0x7F) && len > 0) {
            len--;
            screen_putchar('\b');
            screen_putchar(' ');
            screen_putchar('\b');
            continue;
        }
        if (c >= 32 && c < 127 && len < max - 1) {
            buf[len++] = c;
            screen_putchar(c);
        }
    }
    buf[len] = '\0';
    return len;
}

/* ===== 子命令实现 ===== */

/*
 * cmd_dump — D 命令: 内存转储 (hex dump)
 * @args: 参数字符串 "[addr] [len]"
 *
 * 以十六进制和 ASCII 显示内存内容。
 * 每行 16 字节, 格式: ADDR  HH HH HH ...  |ASCII...|
 */
static void cmd_dump(const char *args)
{
    uint32_t addr = dbg_ptr;
    uint32_t len = 128;  /* 默认 128 字节 (8 行) */
    int consumed;

    /* 解析地址 */
    args = dbg_skip_spaces(args);
    if (*args != '\0') {
        consumed = dbg_parse_hex(args, &addr);
        if (consumed > 0) {
            args += consumed;
            args = dbg_skip_spaces(args);
            /* 解析长度 */
            if (*args != '\0')
                dbg_parse_hex(args, &len);
        }
    }

    if (len == 0) len = 128;

    screen_set_color(0x0B, 0x00);

    for (uint32_t offset = 0; offset < len; offset += 16) {
        /* 地址 */
        dbg_print_hex32(addr + offset);
        screen_puts("  ");

        /* 十六进制部分 */
        for (uint32_t i = 0; i < 16; i++) {
            if (offset + i < len) {
                uint8_t val = *(uint8_t *)(addr + offset + i);
                dbg_print_hex8(val);
                screen_putchar(' ');
            } else {
                screen_puts("   ");
            }
            if (i == 7) screen_putchar(' ');
        }

        /* ASCII 部分 */
        screen_puts(" |");
        for (uint32_t i = 0; i < 16 && offset + i < len; i++) {
            char c = *(char *)(addr + offset + i);
            if (c >= 32 && c < 127)
                screen_putchar(c);
            else
                screen_putchar('.');
        }
        screen_puts("|\n");
    }

    screen_reset_color();
    dbg_ptr = addr + len;
}

/*
 * cmd_enter — E 命令: 修改内存字节
 * @args: 参数字符串 "addr val [val...]"
 */
static void cmd_enter(const char *args)
{
    uint32_t addr;
    uint32_t val;
    int consumed;

    args = dbg_skip_spaces(args);
    consumed = dbg_parse_hex(args, &addr);
    if (consumed == 0) {
        screen_puts("Usage: E addr val [val...]\n");
        return;
    }
    args += consumed;

    /* 写入字节 */
    int count = 0;
    while (1) {
        args = dbg_skip_spaces(args);
        if (*args == '\0') break;
        consumed = dbg_parse_hex(args, &val);
        if (consumed == 0) break;
        args += consumed;

        *(uint8_t *)(addr + (uint32_t)count) = (uint8_t)(val & 0xFF);
        count++;
    }

    if (count > 0) {
        screen_puts("Wrote ");
        dbg_print_dec((uint32_t)count);
        screen_puts(" byte(s) at ");
        dbg_print_hex32(addr);
        screen_putchar('\n');
        dbg_ptr = addr + (uint32_t)count;
    }
}

/*
 * cmd_registers — R 命令: 显示当前 CPU 寄存器状态
 *
 * 读取并显示 CR0-CR4、EFLAGS 和段寄存器。
 */
static void cmd_registers(void)
{
    uint32_t cr0, cr2, cr3, cr4;
    uint32_t eflags;
    uint16_t cs, ds, es, ss;

    cr0 = hal_read_cr0();
    cr2 = hal_read_cr2();
    cr3 = hal_read_cr3();
    cr4 = hal_read_cr4();

    __asm__ __volatile__("pushfl; popl %0" : "=r"(eflags));
    __asm__ __volatile__("mov %%cs, %0" : "=r"(cs));
    __asm__ __volatile__("mov %%ds, %0" : "=r"(ds));
    __asm__ __volatile__("mov %%es, %0" : "=r"(es));
    __asm__ __volatile__("mov %%ss, %0" : "=r"(ss));

    screen_set_color(0x0B, 0x00);
    screen_puts("CR0="); dbg_print_hex32(cr0);
    screen_puts(" CR2="); dbg_print_hex32(cr2);
    screen_puts(" CR3="); dbg_print_hex32(cr3);
    screen_puts(" CR4="); dbg_print_hex32(cr4);
    screen_putchar('\n');

    screen_puts("EFLAGS="); dbg_print_hex32(eflags);
    screen_puts(" [");
    if (eflags & 0x001) screen_putchar('C');
    if (eflags & 0x004) screen_putchar('P');
    if (eflags & 0x040) screen_putchar('Z');
    if (eflags & 0x080) screen_putchar('S');
    if (eflags & 0x800) screen_putchar('O');
    if (eflags & 0x200) screen_putchar('I');
    screen_puts("]\n");

    screen_puts("CS="); dbg_print_hex8((uint8_t)(cs >> 8)); dbg_print_hex8((uint8_t)cs);
    screen_puts(" DS="); dbg_print_hex8((uint8_t)(ds >> 8)); dbg_print_hex8((uint8_t)ds);
    screen_puts(" ES="); dbg_print_hex8((uint8_t)(es >> 8)); dbg_print_hex8((uint8_t)es);
    screen_puts(" SS="); dbg_print_hex8((uint8_t)(ss >> 8)); dbg_print_hex8((uint8_t)ss);
    screen_putchar('\n');

    /* TSC */
    uint64_t tsc = hal_rdtsc();
    screen_puts("TSC="); dbg_print_hex32((uint32_t)(tsc >> 32));
    dbg_print_hex32((uint32_t)tsc);
    screen_putchar('\n');

    screen_reset_color();
}

/*
 * cmd_port_in — P 命令: 读取 I/O 端口
 * @args: 参数字符串 "port"
 */
static void cmd_port_in(const char *args)
{
    uint32_t port;
    args = dbg_skip_spaces(args);
    if (dbg_parse_hex(args, &port) == 0) {
        screen_puts("Usage: P port\n");
        return;
    }

    uint8_t val = hal_inb((uint16_t)port);
    screen_puts("Port ");
    dbg_print_hex32(port);
    screen_puts(" = ");
    dbg_print_hex8(val);
    screen_putchar('\n');
}

/*
 * cmd_port_out — O 命令: 写入 I/O 端口
 * @args: 参数字符串 "port val"
 */
static void cmd_port_out(const char *args)
{
    uint32_t port, val;
    int consumed;

    args = dbg_skip_spaces(args);
    consumed = dbg_parse_hex(args, &port);
    if (consumed == 0) {
        screen_puts("Usage: O port val\n");
        return;
    }
    args = dbg_skip_spaces(args + consumed);
    if (dbg_parse_hex(args, &val) == 0) {
        screen_puts("Usage: O port val\n");
        return;
    }

    hal_outb((uint16_t)port, (uint8_t)(val & 0xFF));
    screen_puts("Wrote ");
    dbg_print_hex8((uint8_t)(val & 0xFF));
    screen_puts(" to port ");
    dbg_print_hex32(port);
    screen_putchar('\n');
}

/*
 * cmd_unassemble — U 命令: 简易 x86 反汇编
 * @args: 参数字符串 "[addr] [count]"
 *
 * 反汇编指定地址的 x86 指令。支持常见指令的助记符显示。
 * 注意: 这是一个简化实现, 仅覆盖常用指令。
 */
static void cmd_unassemble(const char *args)
{
    uint32_t addr = dbg_ptr;
    uint32_t count = 16;  /* 默认反汇编 16 条指令 */
    int consumed;

    args = dbg_skip_spaces(args);
    if (*args != '\0') {
        consumed = dbg_parse_hex(args, &addr);
        if (consumed > 0) {
            args = dbg_skip_spaces(args + consumed);
            if (*args != '\0')
                dbg_parse_hex(args, &count);
        }
    }
    if (count == 0) count = 16;

    screen_set_color(0x0B, 0x00);

    uint32_t ip = addr;
    for (uint32_t i = 0; i < count; i++) {
        /* 地址 */
        dbg_print_hex32(ip);
        screen_puts("  ");

        /* 读取操作码 */
        uint8_t op = *(uint8_t *)ip;

        /* 简易指令解码 */
        switch (op) {
        case 0x00: case 0x01: case 0x02: case 0x03:
            screen_puts("ADD (ModRM)"); ip++; break;
        case 0x06: screen_puts("PUSH ES"); ip++; break;
        case 0x07: screen_puts("POP ES"); ip++; break;
        case 0x0E: screen_puts("PUSH CS"); ip++; break;
        case 0x16: screen_puts("PUSH SS"); ip++; break;
        case 0x17: screen_puts("POP SS"); ip++; break;
        case 0x1E: screen_puts("PUSH DS"); ip++; break;
        case 0x1F: screen_puts("POP DS"); ip++; break;
        case 0x50: case 0x51: case 0x52: case 0x53:
        case 0x54: case 0x55: case 0x56: case 0x57:
            screen_puts("PUSH ");
            { static const char *rn[] = {"EAX","ECX","EDX","EBX","ESP","EBP","ESI","EDI"};
              screen_puts(rn[op - 0x50]); }
            ip++; break;
        case 0x58: case 0x59: case 0x5A: case 0x5B:
        case 0x5C: case 0x5D: case 0x5E: case 0x5F:
            screen_puts("POP ");
            { static const char *rn[] = {"EAX","ECX","EDX","EBX","ESP","EBP","ESI","EDI"};
              screen_puts(rn[op - 0x58]); }
            ip++; break;
        case 0x70: case 0x71: case 0x72: case 0x73:
        case 0x74: case 0x75: case 0x76: case 0x77:
        case 0x78: case 0x79: case 0x7A: case 0x7B:
        case 0x7C: case 0x7D: case 0x7E: case 0x7F:
            { static const char *cc[] = {"JO","JNO","JB","JNB","JZ","JNZ","JBE","JA",
                                         "JS","JNS","JP","JNP","JL","JNL","JLE","JG"};
              screen_puts(cc[op - 0x70]); screen_putchar(' ');
              int8_t off = (int8_t)*(uint8_t *)(ip + 1);
              dbg_print_hex32(ip + 2 + (uint32_t)off); }
            ip += 2; break;
        case 0x89: screen_puts("MOV (ModRM,r)"); ip++; break;
        case 0x8B: screen_puts("MOV (r,ModRM)"); ip++; break;
        case 0x90: screen_puts("NOP"); ip++; break;
        case 0x91: case 0x92: case 0x93:
        case 0x94: case 0x95: case 0x96: case 0x97:
            screen_puts("XCHG EAX,");
            { static const char *rn[] = {"ECX","EDX","EBX","ESP","EBP","ESI","EDI"};
              screen_puts(rn[op - 0x91]); }
            ip++; break;
        case 0xB8: case 0xB9: case 0xBA: case 0xBB:
        case 0xBC: case 0xBD: case 0xBE: case 0xBF:
            screen_puts("MOV ");
            { static const char *rn[] = {"EAX","ECX","EDX","EBX","ESP","EBP","ESI","EDI"};
              screen_puts(rn[op - 0xB8]); screen_putchar(',');
              uint32_t imm = *(uint32_t *)(ip + 1);
              dbg_print_hex32(imm); }
            ip += 5; break;
        case 0xC3: screen_puts("RET"); ip++; break;
        case 0xC9: screen_puts("LEAVE"); ip++; break;
        case 0xCC: screen_puts("INT3"); ip++; break;
        case 0xCD:
            screen_puts("INT ");
            dbg_print_hex8(*(uint8_t *)(ip + 1));
            ip += 2; break;
        case 0xE8:
            screen_puts("CALL ");
            { int32_t off = *(int32_t *)(ip + 1);
              dbg_print_hex32(ip + 5 + (uint32_t)off); }
            ip += 5; break;
        case 0xE9:
            screen_puts("JMP ");
            { int32_t off = *(int32_t *)(ip + 1);
              dbg_print_hex32(ip + 5 + (uint32_t)off); }
            ip += 5; break;
        case 0xEB:
            screen_puts("JMP SHORT ");
            { int8_t off = (int8_t)*(uint8_t *)(ip + 1);
              dbg_print_hex32(ip + 2 + (uint32_t)off); }
            ip += 2; break;
        case 0xF4: screen_puts("HLT"); ip++; break;
        case 0xFA: screen_puts("CLI"); ip++; break;
        case 0xFB: screen_puts("STI"); ip++; break;
        case 0xFC: screen_puts("CLD"); ip++; break;
        case 0xFD: screen_puts("STD"); ip++; break;
        case 0xFF:
            if ((*(uint8_t *)(ip + 1) & 0x38) == 0x10) { screen_puts("CALL [ModRM]"); ip += 2; }
            else if ((*(uint8_t *)(ip + 1) & 0x38) == 0x20) { screen_puts("JMP [ModRM]"); ip += 2; }
            else if ((*(uint8_t *)(ip + 1) & 0x38) == 0x00) { screen_puts("INC [ModRM]"); ip += 2; }
            else if ((*(uint8_t *)(ip + 1) & 0x38) == 0x08) { screen_puts("DEC [ModRM]"); ip += 2; }
            else { screen_puts("DB "); dbg_print_hex8(op); ip++; }
            break;
        default:
            screen_puts("DB ");
            dbg_print_hex8(op);
            ip++;
            break;
        }
        screen_putchar('\n');
    }

    screen_reset_color();
    dbg_ptr = ip;
}

/*
 * cmd_hex — H 命令: 十六进制加减计算
 * @args: 参数字符串 "val1 val2"
 */
static void cmd_hex(const char *args)
{
    uint32_t a, b;
    int consumed;

    args = dbg_skip_spaces(args);
    consumed = dbg_parse_hex(args, &a);
    if (consumed == 0) {
        screen_puts("Usage: H val1 val2\n");
        return;
    }
    args = dbg_skip_spaces(args + consumed);
    if (dbg_parse_hex(args, &b) == 0) {
        screen_puts("Usage: H val1 val2\n");
        return;
    }

    screen_puts("Sum:  "); dbg_print_hex32(a + b); screen_putchar('\n');
    screen_puts("Diff: "); dbg_print_hex32(a - b); screen_putchar('\n');
    screen_puts("And:  "); dbg_print_hex32(a & b); screen_putchar('\n');
    screen_puts("Or:   "); dbg_print_hex32(a | b); screen_putchar('\n');
    screen_puts("Xor:  "); dbg_print_hex32(a ^ b); screen_putchar('\n');
}

/*
 * cmd_fill — M 命令: 填充内存
 * @args: 参数字符串 "addr len val"
 */
static void cmd_fill(const char *args)
{
    uint32_t addr, len, val;
    int consumed;

    args = dbg_skip_spaces(args);
    consumed = dbg_parse_hex(args, &addr);
    if (consumed == 0) goto usage;
    args = dbg_skip_spaces(args + consumed);
    consumed = dbg_parse_hex(args, &len);
    if (consumed == 0) goto usage;
    args = dbg_skip_spaces(args + consumed);
    if (dbg_parse_hex(args, &val) == 0) goto usage;

    for (uint32_t i = 0; i < len; i++)
        *(uint8_t *)(addr + i) = (uint8_t)(val & 0xFF);

    screen_puts("Filled ");
    dbg_print_dec(len);
    screen_puts(" bytes at ");
    dbg_print_hex32(addr);
    screen_putchar('\n');
    return;

usage:
    screen_puts("Usage: M addr len val\n");
}

/*
 * cmd_compare — C 命令: 比较内存块
 * @args: 参数字符串 "addr1 addr2 len"
 */
static void cmd_compare(const char *args)
{
    uint32_t a1, a2, len;
    int consumed;

    args = dbg_skip_spaces(args);
    consumed = dbg_parse_hex(args, &a1);
    if (consumed == 0) goto usage;
    args = dbg_skip_spaces(args + consumed);
    consumed = dbg_parse_hex(args, &a2);
    if (consumed == 0) goto usage;
    args = dbg_skip_spaces(args + consumed);
    if (dbg_parse_hex(args, &len) == 0) goto usage;

    {
        int diffs = 0;
        for (uint32_t i = 0; i < len; i++) {
            uint8_t v1 = *(uint8_t *)(a1 + i);
            uint8_t v2 = *(uint8_t *)(a2 + i);
            if (v1 != v2) {
                dbg_print_hex32(a1 + i);
                screen_puts(": "); dbg_print_hex8(v1);
                screen_puts(" != "); dbg_print_hex8(v2);
                screen_putchar('\n');
                diffs++;
            }
        }
        if (diffs == 0)
            screen_puts("Blocks are identical.\n");
        else {
            dbg_print_dec((uint32_t)diffs);
            screen_puts(" difference(s) found.\n");
        }
    }
    return;

usage:
    screen_puts("Usage: C addr1 addr2 len\n");
}

/*
 * cmd_search — S 命令: 搜索内存
 * @args: 参数字符串 "addr len val"
 */
static void cmd_search(const char *args)
{
    uint32_t addr, len, val;
    int consumed;

    args = dbg_skip_spaces(args);
    consumed = dbg_parse_hex(args, &addr);
    if (consumed == 0) goto usage;
    args = dbg_skip_spaces(args + consumed);
    consumed = dbg_parse_hex(args, &len);
    if (consumed == 0) goto usage;
    args = dbg_skip_spaces(args + consumed);
    if (dbg_parse_hex(args, &val) == 0) goto usage;

    {
        int found = 0;
        for (uint32_t i = 0; i < len; i++) {
            if (*(uint8_t *)(addr + i) == (uint8_t)(val & 0xFF)) {
                dbg_print_hex32(addr + i);
                screen_putchar('\n');
                found++;
            }
        }
        if (found == 0)
            screen_puts("Not found.\n");
        else {
            dbg_print_dec((uint32_t)found);
            screen_puts(" match(es).\n");
        }
    }
    return;

usage:
    screen_puts("Usage: S addr len val\n");
}

/*
 * dbg_show_help — 显示调试器帮助
 */
static void dbg_show_help(void)
{
    screen_set_color(0x0B, 0x00);
    screen_puts("Nexsteaduser PlexsDOS Debug Tool\n\n");
    screen_set_color(0x0F, 0x00);
    screen_puts("  D [addr] [len]   Hex dump memory\n");
    screen_puts("  E addr val...    Edit memory bytes\n");
    screen_puts("  R                Show CPU registers\n");
    screen_puts("  P port           Read I/O port byte\n");
    screen_puts("  O port val       Write I/O port byte\n");
    screen_puts("  U [addr] [count] Unassemble (disasm)\n");
    screen_puts("  H val1 val2      Hex add/sub/and/or/xor\n");
    screen_puts("  M addr len val   Fill memory\n");
    screen_puts("  C a1 a2 len      Compare memory blocks\n");
    screen_puts("  S addr len val   Search memory for byte\n");
    screen_puts("  ?                Show this help\n");
    screen_puts("  Q                Quit debugger\n");
    screen_reset_color();
    screen_putchar('\n');
}

/*
 * debug_main — 调试工具主循环
 *
 * 显示提示符 '-', 读取命令, 解析并执行子命令。
 */
void debug_main(void)
{
    screen_puts("Nexsteaduser Debug Tool. Type ? for help.\n\n");

    dbg_ptr = 0;

    while (1) {
        screen_set_color(0x0E, 0x00);
        screen_putchar('-');
        screen_reset_color();
        screen_putchar(' ');

        dbg_read_line(dbg_cmd, DBG_CMD_MAX);

        const char *cmd = dbg_skip_spaces(dbg_cmd);
        if (*cmd == '\0')
            continue;

        char sub = *cmd;
        if (sub >= 'a' && sub <= 'z')
            sub -= 32;

        const char *args = dbg_skip_spaces(cmd + 1);

        switch (sub) {
        case 'D': cmd_dump(args); break;
        case 'E': cmd_enter(args); break;
        case 'R': cmd_registers(); break;
        case 'P': cmd_port_in(args); break;
        case 'O': cmd_port_out(args); break;
        case 'U': cmd_unassemble(args); break;
        case 'H': cmd_hex(args); break;
        case 'M': cmd_fill(args); break;
        case 'C': cmd_compare(args); break;
        case 'S': cmd_search(args); break;
        case '?': dbg_show_help(); break;
        case 'Q': return;
        default:
            screen_puts("Unknown command. Type ? for help.\n");
            break;
        }
    }
}
