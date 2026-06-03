/*
 * Nexsteaduser — PlexsDOS
 * PS/2 键盘驱动
 * 作者: Tinmc189623 | 团队: Nexlyh
 *
 * 通过 IRQ1 (INT 0x21) 中断处理键盘输入。
 * 初始化 8042 PS/2 控制器, 注册中断处理程序,
 * 维护环形缓冲区存储扫描码转换后的 ASCII 字符。
 */

#include <plexsdos/types.h>
#include <plexsdos/config.h>
#include <plexsdos/keyboard.h>
#include <plexsdos/screen.h>
#include <plexsdos/interrupt.h>
#include <plexsdos/serial.h>

/* 键盘缓冲区 (环形队列) */
static char kbd_buffer[KBD_BUFFER_SIZE];
static int  kbd_head = 0;   /* 读指针 */
static int  kbd_tail = 0;   /* 写指针 */
static int  kbd_count = 0;  /* 缓冲区中的字符数 */

/* 修饰键状态 (volatile: 在中断处理器中跨调用修改) */
static volatile int shift_pressed = 0;
static volatile int caps_lock = 0;
static volatile int ctrl_pressed = 0;
static volatile int alt_pressed = 0;

/* E0 扩展扫描码前缀标志 */
static volatile int e0_prefix = 0;

/*
 * 方向键转义序列 (VT100 兼容)
 * 先存入 ESC, 再存入 '[', 最后存入方向字符
 */
#define KBD_ESC_UP      'A'    /* 上 */
#define KBD_ESC_DOWN    'B'    /* 下 */
#define KBD_ESC_RIGHT   'C'    /* 右 */
#define KBD_ESC_LEFT    'D'    /* 左 */

/* 8042 PS/2 控制器端口 */
#define KBD_DATA_PORT   0x60    /* 数据端口 (读/写) */
#define KBD_CMD_PORT    0x64    /* 命令端口 (写) / 状态端口 (读) */

/* 8042 控制器命令 */
#define KBD_CMD_DISABLE_KBD     0xAD    /* 禁用键盘 */
#define KBD_CMD_ENABLE_KBD      0xAE    /* 启用键盘 */
#define KBD_CMD_READ_CONFIG     0x20    /* 读取控制器配置字节 */
#define KBD_CMD_WRITE_CONFIG    0x60    /* 写入控制器配置字节 */
#define KBD_CMD_SELF_TEST       0xAA    /* 控制器自检 */
#define KBD_CMD_IF_TEST         0xAB    /* 接口测试 */

/* 键盘命令 (发送到数据端口 0x60) */
#define KBD_RESET               0xFF    /* 复位键盘 */
#define KBD_ENABLE_SCAN         0xF4    /* 启用扫描 */
#define KBD_SET_DEFAULTS        0xF6    /* 恢复默认参数 */
#define KBD_ACK                 0xFA    /* 键盘应答 (ACK) */
#define KBD_SELF_TEST_OK        0xAA    /* 键盘自检通过 */

/* 控制器配置字节位 */
#define KBD_CONFIG_IRQ1_EN      0x01    /* bit 0: 启用键盘中断 (IRQ1) */
#define KBD_CONFIG_IRQ12_EN     0x02    /* bit 1: 启用鼠标中断 (IRQ12) */
#define KBD_CONFIG_SYS_FLAG     0x04    /* bit 2: 系统标志 (POST 完成) */
#define KBD_CONFIG_DISABLE_KBD  0x10    /* bit 4: 禁用时钟 (键盘) */
#define KBD_CONFIG_DISABLE_AUX  0x20    /* bit 5: 禁用时钟 (鼠标) */
#define KBD_CONFIG_TRANSLATE    0x40    /* bit 6: 扫描码翻译 */

/*
 * kbd_wait_input — 等待 8042 控制器输入缓冲区空
 *
 * 控制器在可以接受新命令前需要轮询状态寄存器。
 * 超时约 1ms (约 1000 次循环)。
 */
static void kbd_wait_input(void)
{
    int timeout = 10000;
    while (timeout--) {
        uint8_t status;
        __asm__ __volatile__(
            "inb $0x64, %0"
            : "=a"(status)
            :
            : "memory"
        );
        if (!(status & 0x02))  /* bit 1 = 输入缓冲区满 */
            return;
    }
}

/*
 * kbd_wait_output — 等待 8042 控制器输出缓冲区满
 *
 * 读取数据前需要确认输出缓冲区有数据。
 */
static void kbd_wait_output(void)
{
    int timeout = 10000;
    while (timeout--) {
        uint8_t status;
        __asm__ __volatile__(
            "inb $0x64, %0"
            : "=a"(status)
            :
            : "memory"
        );
        if (status & 0x01)  /* bit 0 = 输出缓冲区满 */
            return;
    }
}

/*
 * kbd_send_cmd — 向 8042 控制器发送命令
 * @cmd: 命令字节
 *
 * 等待输入缓冲区空后发送命令到端口 0x64。
 */
static void kbd_send_cmd(uint8_t cmd)
{
    kbd_wait_input();
    __asm__ __volatile__(
        "outb %0, $0x64"
        :
        : "a"(cmd)
        : "memory"
    );
}

/*
 * kbd_send_data — 向键盘发送数据/命令
 * @data: 数据字节
 *
 * 等待输入缓冲区空后发送数据到端口 0x60。
 */
static void kbd_send_data(uint8_t data)
{
    kbd_wait_input();
    __asm__ __volatile__(
        "outb %0, $0x60"
        :
        : "a"(data)
        : "memory"
    );
}

/*
 * kbd_read_data — 从键盘读取数据
 * 返回: 数据字节
 *
 * 等待输出缓冲区满后读取端口 0x60。
 */
static uint8_t kbd_read_data(void)
{
    uint8_t data;
    kbd_wait_output();
    __asm__ __volatile__(
        "inb $0x60, %0"
        : "=a"(data)
        :
        : "memory"
    );
    return data;
}

/*
 * kbd_flush_output — 清空 8042 输出缓冲区
 *
 * 读取并丢弃所有待读取的数据, 防止残留数据干扰。
 */
static void kbd_flush_output(void)
{
    int timeout = 100;
    while (timeout--) {
        uint8_t status;
        __asm__ __volatile__(
            "inb $0x64, %0"
            : "=a"(status)
            :
            : "memory"
        );
        if (!(status & 0x01))  /* 输出缓冲区空 */
            return;
        /* 读取并丢弃 */
        __asm__ __volatile__(
            "inb $0x60, %%al"
            :
            :
            : "ax", "memory"
        );
    }
}

/*
 * 扫描码到 ASCII 转换表 (未按 Shift, Set 1)
 * 索引 = 扫描码, 值 = ASCII 字符
 * 0x00: 无映射  0x01: ESC  0x0E: Backspace  0x0F: Tab
 * 0x1C: Enter   0x01: ESC
 */
static const char scancode_table[128] = {
    0,   0x1B,'1', '2', '3', '4', '5', '6',  /* 0x00-0x07: NUL, ESC, 1-6 */
   '7', '8', '9', '0', '-', '=', '\b', '\t',  /* 0x08-0x0F: 7-0, -, =, BS, TAB */
   'q', 'w', 'e', 'r', 't', 'y', 'u', 'i',  /* 0x10-0x17 */
   'o', 'p', '[', ']', '\r',  0,  'a', 's',  /* 0x18-0x1F: o-p, [, ], Enter, LCtrl, a, s */
   'd', 'f', 'g', 'h', 'j', 'k', 'l', ';',  /* 0x20-0x27 */
   '\'', '`',  0, '\\', 'z', 'x', 'c', 'v',  /* 0x28-0x2F: ', `, LShift, \, z-v */
   'b', 'n', 'm', ',', '.', '/',  0,  '*',  /* 0x30-0x37: b-m, ,, ., /, RShift, * (KP) */
    0,  ' ',  0,   0,   0,   0,   0,   0,   /* 0x38-0x3F: LAlt, Space, CapsLock, F1-F5 */
    0,   0,   0,   0,   0,   0,   0,  '7', /* 0x40-0x47: F6-F10, NumLock, ScrollLock, KP 7 */
    '8', '9', '-', '4', '5', '6', '+', '1', /* 0x48-0x4F: KP 8, KP 9, KP -, KP 4-6, KP +, KP 1 */
    '2', '3', '0', '.',  0,   0,   0,   0,   /* 0x50-0x57: KP 2, KP 3, KP 0, KP ., F11, F12 */
    0,   0,   0,   0,   0,   0,   0,   0,   /* 0x58-0x5F */
    0,   0,   0,   0,   0,   0,   0,   0,   /* 0x60-0x67 */
    0,   0,   0,   0,   0,   0,   0,   0,   /* 0x68-0x6F */
    0,   0,   0,   0,   0,   0,   0,   0,   /* 0x70-0x77 */
    0,   0,   0,   0,   0,   0,   0,   0    /* 0x78-0x7F */
};

/*
 * 扫描码到 ASCII 转换表 (按住 Shift, Set 1)
 */
static const char scancode_shift_table[128] = {
    0,   0x1B,'!', '@', '#', '$', '%', '^',  /* 0x00-0x07 */
   '&', '*', '(', ')', '_', '+', '\b', '\t',  /* 0x08-0x0F */
   'Q', 'W', 'E', 'R', 'T', 'Y', 'U', 'I',  /* 0x10-0x17 */
   'O', 'P', '{', '}', '\r',  0,  'A', 'S',  /* 0x18-0x1F */
   'D', 'F', 'G', 'H', 'J', 'K', 'L', ':',  /* 0x20-0x27 */
   '"', '~',  0,  '|', 'Z', 'X', 'C', 'V',  /* 0x28-0x2F */
   'B', 'N', 'M', '<', '>', '?',  0,  '*',  /* 0x30-0x37 */
    0,  ' ',  0,   0,   0,   0,   0,   0,   /* 0x38-0x3F */
    0,   0,   0,   0,   0,   0,   0,  '7', /* 0x40-0x47: F6-F10, NumLock, ScrollLock, KP 7 */
    '8', '9', '-', '4', '5', '6', '+', '1', /* 0x48-0x4F: KP 8, KP 9, KP -, KP 4-6, KP +, KP 1 */
    '2', '3', '0', '.',  0,   0,   0,   0,   /* 0x50-0x57: KP 2, KP 3, KP 0, KP ., F11, F12 */
    0,   0,   0,   0,   0,   0,   0,   0,   /* 0x58-0x5F */
    0,   0,   0,   0,   0,   0,   0,   0,   /* 0x60-0x67 */
    0,   0,   0,   0,   0,   0,   0,   0,   /* 0x68-0x6F */
    0,   0,   0,   0,   0,   0,   0,   0,   /* 0x70-0x77 */
    0,   0,   0,   0,   0,   0,   0,   0    /* 0x78-0x7F */
};

/*
 * kbd_buffer_put — 向键盘缓冲区写入一个字符
 * @c: 要写入的字符
 *
 * 缓冲区满时丢弃字符。
 */
static void kbd_buffer_put(char c)
{
    if (kbd_count >= KBD_BUFFER_SIZE)
        return; /* 缓冲区满, 丢弃 */
    kbd_buffer[kbd_tail] = c;
    kbd_tail = (kbd_tail + 1) % KBD_BUFFER_SIZE;
    kbd_count++;
}

/*
 * kbd_buffer_get — 从键盘缓冲区读取一个字符
 * 返回: ASCII 字符, 缓冲区空时返回 -1
 */
static int kbd_buffer_get(void)
{
    char c;
    if (kbd_count <= 0)
        return -1; /* 缓冲区空 */
    c = kbd_buffer[kbd_head];
    kbd_head = (kbd_head + 1) % KBD_BUFFER_SIZE;
    kbd_count--;
    return (int)c;
}

/*
 * kbd_put_arrow — 将方向键作为转义序列写入缓冲区
 * @dir: 方向字符 ('A'=上, 'B'=下, 'C'=右, 'D'=左)
 *
 * 生成 VT100 转义序列: ESC [ dir
 */
static void kbd_put_arrow(char dir)
{
    kbd_buffer_put(0x1B);  /* ESC */
    kbd_buffer_put('[');
    kbd_buffer_put(dir);
}

/*
 * kbd_interrupt_handler — 键盘中断处理程序 (IRQ1, INT 0x21)
 *
 * 从键盘端口 0x60 读取 Set 1 扫描码, 转换为 ASCII 并存入缓冲区。
 * 处理 E0 扩展扫描码 (方向键、多媒体键等)。
 * 处理 Shift、Ctrl、Alt、Caps Lock 等修饰键。
 */
void kbd_interrupt_handler(void)
{
    uint8_t scancode;
    char ascii;

    /* 从键盘数据端口读取扫描码 */
    __asm__ __volatile__(
        "inb $0x60, %0"
        : "=a"(scancode)
        :
        : "memory"
    );

    /* E0 扩展前缀: 下一个字节是扩展扫描码 */
    if (scancode == 0xE0) {
        e0_prefix = 1;
        return;
    }

    /* 处理 E0 扩展扫描码 */
    if (e0_prefix) {
        e0_prefix = 0;

        /* 断码 (释放): 忽略 */
        if (scancode & 0x80)
            return;

        switch (scancode) {
        case 0x48: /* 上 */
            kbd_put_arrow(KBD_ESC_UP);
            return;
        case 0x50: /* 下 */
            kbd_put_arrow(KBD_ESC_DOWN);
            return;
        case 0x4B: /* 左 */
            kbd_put_arrow(KBD_ESC_LEFT);
            return;
        case 0x4D: /* 右 */
            kbd_put_arrow(KBD_ESC_RIGHT);
            return;
        case 0x52: /* Insert (静默) */
            return;
        case 0x53: /* Delete (E0) */
            kbd_buffer_put(0x7F);
            return;
        case 0x47: /* Home */
            kbd_buffer_put(0x1B);
            kbd_buffer_put('[');
            kbd_buffer_put('H');
            return;
        case 0x4F: /* End */
            kbd_buffer_put(0x1B);
            kbd_buffer_put('[');
            kbd_buffer_put('F');
            return;
        case 0x49: /* Page Up (静默) */
            return;
        case 0x51: /* Page Down (静默) */
            return;
        case 0x5B: /* Left Windows */
        case 0x5C: /* Right Windows */
        case 0x5D: /* Menu/App */
            return; /* 静默吸收 */
        default:
            return; /* 其他 E0 键忽略 */
        }
    }

    /* 最高位为 1 表示按键释放 (断码) */
    if (scancode & 0x80) {
        scancode &= 0x7F;
        switch (scancode) {
        case 0x2A: /* 左 Shift */
        case 0x36: /* 右 Shift */
            shift_pressed = 0;
            break;
        case 0x1D: /* 左 Ctrl */
            ctrl_pressed = 0;
            break;
        case 0x38: /* 左 Alt */
            alt_pressed = 0;
            break;
        default:
            break;
        }
        return;
    }

    /* 按键按下 */
    switch (scancode) {
    case 0x2A: /* 左 Shift */
    case 0x36: /* 右 Shift */
        shift_pressed = 1;
        return;
    case 0x1D: /* 左 Ctrl */
        ctrl_pressed = 1;
        return;
    case 0x38: /* 左 Alt */
        alt_pressed = 1;
        return;
    case 0x3A: /* Caps Lock */
        caps_lock = !caps_lock;
        return;
    case 0x3B: /* F1 (静默) */
    case 0x3C: /* F2 (静默) */
    case 0x3D: /* F3 (静默) */
    case 0x3E: /* F4 (静默) */
    case 0x3F: /* F5 (静默) */
    case 0x40: /* F6 (静默) */
    case 0x41: /* F7 (静默) */
    case 0x42: /* F8 (静默) */
    case 0x43: /* F9 (静默) */
    case 0x44: /* F10 (静默) */
    case 0x57: /* F11 (静默) */
    case 0x58: /* F12 (静默) */
        return;
    case 0x01: /* ESC */
        kbd_buffer_put(0x1B);
        return;
    default:
        break;
    }

    /* 查表转换扫描码到 ASCII */
    if (scancode >= 128)
        return;

    if (shift_pressed)
        ascii = scancode_shift_table[scancode];
    else
        ascii = scancode_table[scancode];

    /* Caps Lock 仅对字母键取反 */
    if (caps_lock && ascii >= 'a' && ascii <= 'z')
        ascii -= 32;
    else if (caps_lock && ascii >= 'A' && ascii <= 'Z')
        ascii += 32;

    if (ascii != 0)
        kbd_buffer_put(ascii);
}

/*
 * keyboard_init — 初始化 PS/2 键盘控制器和驱动
 *
 * 初始化 8042 PS/2 控制器:
 * 1. 禁用键盘/鼠标端口
 * 2. 清空输出缓冲区
 * 3. 读取并修改控制器配置字节 (启用 IRQ1)
 * 4. 控制器自检
 * 5. 键盘接口测试
 * 6. 启用键盘端口
 * 7. 复位键盘并等待自检结果
 * 8. 启用键盘扫描
 */
void keyboard_init(void)
{
    uint8_t config;
    uint8_t resp;

    /* 清空缓冲区和状态 */
    kbd_head = 0;
    kbd_tail = 0;
    kbd_count = 0;
    shift_pressed = 0;
    caps_lock = 0;
    ctrl_pressed = 0;
    alt_pressed = 0;
    e0_prefix = 0;

    /* 第 1 步: 禁用键盘和鼠标端口 */
    kbd_send_cmd(KBD_CMD_DISABLE_KBD);
    kbd_send_cmd(0xA7);  /* 禁用鼠标端口 (如果存在) */

    /* 第 2 步: 清空输出缓冲区 */
    kbd_flush_output();

    /* 第 3 步: 读取控制器配置字节 */
    kbd_send_cmd(KBD_CMD_READ_CONFIG);
    config = kbd_read_data();

    /* 修改配置: 启用键盘中断 (IRQ1), 禁用鼠标中断, 禁用翻译 */
    config |= KBD_CONFIG_IRQ1_EN;       /* 启用 IRQ1 */
    config &= ~KBD_CONFIG_IRQ12_EN;     /* 禁用 IRQ12 (鼠标) */
    config |= KBD_CONFIG_TRANSLATE;     /* 启用扫描码翻译 (Set 2 → Set 1) */
    config |= KBD_CONFIG_SYS_FLAG;      /* 设置系统标志 */

    /* 第 4 步: 写入新的配置字节 */
    kbd_send_cmd(KBD_CMD_WRITE_CONFIG);
    kbd_send_data(config);

    /* 第 5 步: 控制器自检 */
    kbd_send_cmd(KBD_CMD_SELF_TEST);
    resp = kbd_read_data();
    if (resp != 0x55) {
        screen_puts("[kbd] controller self-test failed.\n");
        return;
    }

    /* 第 6 步: 键盘接口测试 */
    kbd_send_cmd(KBD_CMD_IF_TEST);
    resp = kbd_read_data();
    if (resp != 0x00) {
        screen_puts("[kbd] interface test failed.\n");
        return;
    }

    /* 第 7 步: 启用键盘端口 */
    kbd_send_cmd(KBD_CMD_ENABLE_KBD);

    /* 第 8 步: 复位键盘 */
    kbd_send_data(KBD_RESET);
    resp = kbd_read_data();
    if (resp != KBD_ACK) {
        screen_puts("[kbd] reset ACK failed.\n");
        /* 继续尝试, 某些键盘不发 ACK */
    }

    /* 等待键盘自检完成 (0xAA = 通过) */
    resp = kbd_read_data();
    if (resp != KBD_SELF_TEST_OK) {
        screen_puts("[kbd] self-test failed.\n");
        /* 非致命错误, 继续 */
    }

    /* 第 9 步: 启用键盘扫描 */
    kbd_send_data(KBD_ENABLE_SCAN);
    resp = kbd_read_data();
    if (resp != KBD_ACK) {
        screen_puts("[kbd] enable scan failed.\n");
        return;
    }

    screen_puts("[kbd] PS/2 keyboard ready.\n");
}

/*
 * keyboard_getchar — 阻塞读取一个字符
 * 从键盘缓冲区读取字符。当缓冲区为空时,
 * 轮询串口输入, 使得通过 QEMU 终端 (-serial stdio) 输入的字符也能被接收。
 */
char keyboard_getchar(void)
{
    int c;
    while ((c = kbd_buffer_get()) == -1) {
        /* 轮询串口输入: 终端输入优先走此路径 */
        c = serial_getchar();
        if (c >= 0)
            return (char)c;
        /* 节能等待 */
        __asm__ __volatile__("hlt");
    }
    return (char)c;
}

/*
 * keyboard_available — 检查是否有待读取的按键
 * 返回: 1 = 有数据, 0 = 缓冲区空
 */
int keyboard_available(void)
{
    return (kbd_count > 0) ? 1 : 0;
}

/*
 * keyboard_read_line — 读取一行输入
 * @buf:     目标缓冲区
 * @max_len: 缓冲区最大长度 (含 null 终止符)
 *
 * 回显输入字符, 支持退格删除, 以 Enter 结束。
 * 返回: 读取的字符数 (不含 null)
 */
int keyboard_read_line(char *buf, int max_len)
{
    int len = 0;
    char c;

    while (1) {
        c = keyboard_getchar();

        if (c == '\r' || c == '\n') {
            /* Enter: 结束输入 */
            screen_putchar('\n');
            break;
        } else if (c == '\b') {
            /* 退格: 删除最后一个字符 */
            if (len > 0) {
                len--;
                screen_putchar('\b');
                screen_putchar(' ');
                screen_putchar('\b');
            }
        } else if (len < max_len - 1) {
            /* 普通字符: 存入缓冲区并回显 */
            buf[len++] = c;
            screen_putchar(c);
        }
        /* 缓冲区满时忽略多余输入 */
    }

    buf[len] = '\0';
    return len;
}
