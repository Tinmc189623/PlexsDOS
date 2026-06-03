/*
 * Nexsteaduser — PlexsDOS
 * PS/2 鼠标驱动
 * 作者: Tinmc189623 | 团队: Nexlyh
 *
 * 通过 IRQ12 (INT 0x2C) 中断处理 PS/2 鼠标输入。
 * 使用 8042 PS/2 控制器辅助通道, 标准 3 字节数据包格式。
 */

#include <plexsdos/types.h>
#include <plexsdos/mouse.h>
#include <plexsdos/screen.h>
#include <plexsdos/interrupt.h>
#include <plexsdos/serial.h>

/* 8042 PS/2 控制器端口 */
#define MOUSE_DATA_PORT   0x60    /* 数据端口 */
#define MOUSE_CMD_PORT    0x64    /* 命令/状态端口 */

/* 8042 控制器命令 */
#define MOUSE_CMD_ENABLE_AUX    0xA8    /* 启用辅助设备 (鼠标) */
#define MOUSE_CMD_DISABLE_AUX   0xA7    /* 禁用辅助设备 */
#define MOUSE_CMD_READ_CONFIG   0x20    /* 读取配置字节 */
#define MOUSE_CMD_WRITE_CONFIG  0x60    /* 写入配置字节 */
#define MOUSE_CMD_SEND_TO_AUX   0xD4    /* 发送数据到辅助设备 */

/* 鼠标命令 (通过 0xD4 发送) */
#define MOUSE_CMD_RESET         0xFF    /* 复位 */
#define MOUSE_CMD_RESEND        0xFE    /* 重发 */
#define MOUSE_CMD_SET_DEFAULTS  0xF6    /* 设置默认值 */
#define MOUSE_CMD_DISABLE       0xF5    /* 禁用数据报告 */
#define MOUSE_CMD_ENABLE        0xF4    /* 启用数据报告 */
#define MOUSE_CMD_SET_SAMPLE    0xF3    /* 设置采样率 */
#define MOUSE_CMD_SET_REMOTE    0xF0    /* 设置远程模式 */
#define MOUSE_CMD_READ_DATA     0xEB    /* 读取数据 (远程模式) */
#define MOUSE_CMD_STATUS_REQ    0xE9    /* 状态请求 */

/* 鼠标应答 */
#define MOUSE_ACK               0xFA    /* 应答 */
#define MOUSE_SELF_TEST_OK      0xAA    /* 自检通过 */
#define MOUSE_SELF_TEST_FAIL1   0xFC    /* 自检失败 */
#define MOUSE_SELF_TEST_FAIL2   0xFD    /* 自检失败 */

/* 鼠标中断向量 */
#define MOUSE_IRQ       12
#define MOUSE_INT_VEC   0x2C    /* IRQ12 → INT 0x2C */

/* 鼠标数据包解析状态机 */
#define MOUSE_PACKET_WAIT_SYNC  0   /* 等待同步字节 */
#define MOUSE_PACKET_HAVE_BYTE0 1   /* 已接收字节 0 */
#define MOUSE_PACKET_HAVE_BYTE1 2   /* 已接收字节 1 */

/* 鼠标事件环形缓冲区 */
static struct mouse_event event_buf[MOUSE_EVENT_BUF];
static int  event_head = 0;     /* 读指针 */
static int  event_tail = 0;     /* 写指针 */
static int  event_count = 0;    /* 事件数 */

/* 数据包解析状态 */
static int  packet_state = MOUSE_PACKET_WAIT_SYNC;
static uint8_t packet_byte0 = 0;

/* 鼠标是否存在 */
static int mouse_present = 0;

/* ===== 8042 控制器低级操作 ===== */

/*
 * mouse_wait_input — 等待输入缓冲区空
 */
static void mouse_wait_input(void)
{
    int timeout = 10000;
    while (timeout--) {
        uint8_t status;
        __asm__ __volatile__("inb $0x64, %0" : "=a"(status) :: "memory");
        if (!(status & 0x02))
            return;
    }
}

/*
 * mouse_wait_output — 等待输出缓冲区满
 */
static void mouse_wait_output(void)
{
    int timeout = 10000;
    while (timeout--) {
        uint8_t status;
        __asm__ __volatile__("inb $0x64, %0" : "=a"(status) :: "memory");
        if (status & 0x01)
            return;
    }
}

/*
 * mouse_send_cmd — 向 8042 控制器发送命令
 * @cmd: 命令字节
 */
static void mouse_send_cmd(uint8_t cmd)
{
    mouse_wait_input();
    __asm__ __volatile__("outb %0, $0x64" : : "a"(cmd) : "memory");
}

/*
 * mouse_read_data — 从数据端口读取字节
 * 返回: 数据字节
 */
static uint8_t mouse_read_data(void)
{
    uint8_t data;
    mouse_wait_output();
    __asm__ __volatile__("inb $0x60, %0" : "=a"(data) :: "memory");
    return data;
}

/*
 * mouse_write_data — 向数据端口写入字节
 * @data: 数据字节
 */
static void mouse_write_data(uint8_t data)
{
    mouse_wait_input();
    __asm__ __volatile__("outb %0, $0x60" : : "a"(data) : "memory");
}

/*
 * mouse_send_aux — 向鼠标发送命令
 * @cmd: 命令字节
 * 返回: 应答字节, 超时返回 0
 *
 * 通过 8042 的辅助通道发送命令:
 * 1. 向命令端口写 0xD4 (发送到辅助设备)
 * 2. 向数据端口写命令
 * 3. 读取应答
 */
static uint8_t mouse_send_aux(uint8_t cmd)
{
    mouse_send_cmd(MOUSE_CMD_SEND_TO_AUX);
    mouse_write_data(cmd);
    return mouse_read_data();
}

/*
 * mouse_flush_output — 清空输出缓冲区
 */
static void mouse_flush_output(void)
{
    int timeout = 100;
    while (timeout--) {
        uint8_t status;
        __asm__ __volatile__("inb $0x64, %0" : "=a"(status) :: "memory");
        if (!(status & 0x01))
            return;
        __asm__ __volatile__("inb $0x60, %%al" : : : "ax", "memory");
    }
}

/* ===== 事件缓冲区 ===== */

/*
 * mouse_event_put — 向事件队列写入事件
 * @ev: 鼠标事件
 */
static void mouse_event_put(struct mouse_event *ev)
{
    if (event_count >= MOUSE_EVENT_BUF)
        return;
    event_buf[event_tail] = *ev;
    event_tail = (event_tail + 1) % MOUSE_EVENT_BUF;
    event_count++;
}

/*
 * mouse_event_get — 从事件队列读取事件 (非阻塞)
 * @ev: 输出事件结构
 * 返回: 1 = 有事件, 0 = 缓冲区空
 */
static int mouse_event_get(struct mouse_event *ev)
{
    if (event_count <= 0)
        return 0;
    *ev = event_buf[event_head];
    event_head = (event_head + 1) % MOUSE_EVENT_BUF;
    event_count--;
    return 1;
}

/* ===== 中断处理 ===== */

/*
 * mouse_interrupt_handler — 鼠标中断处理程序 (IRQ12, INT 0x2C)
 *
 * 从数据端口读取字节并组装 3 字节数据包。
 * 标准 PS/2 数据包格式:
 *   字节 0: YOvfl XOvfl Ysign Xsign 1 MBtn RBtn LBttn
 *   字节 1: X 位移 (有符号)
 *   字节 2: Y 位移 (有符号, 负值=向上)
 *
 * 同步条件: 字节 0 的 bit 3 必须为 1。
 */
void mouse_interrupt_handler(void)
{
    uint8_t data;

    /* 读取数据 */
    __asm__ __volatile__("inb $0x60, %0" : "=a"(data) :: "memory");

    if (!mouse_present)
        return;

    switch (packet_state) {
    case MOUSE_PACKET_WAIT_SYNC:
        /* 等待同步字节: bit 3 = 1, bit 0-2 = 按钮 */
        packet_byte0 = data;
        packet_state = MOUSE_PACKET_HAVE_BYTE0;
        break;

    case MOUSE_PACKET_HAVE_BYTE0: {
        int8_t dx = (int8_t)data;
        packet_state = MOUSE_PACKET_HAVE_BYTE1;
        /* 保存 dx, 等 dy 到来 */
        struct mouse_event *ev = &event_buf[event_tail];
        ev->buttons = packet_byte0 & 0x07;
        ev->dx = dx;
        /* dy 暂存于 dx 字段, 等待字节 2 */
        break;
    }

    case MOUSE_PACKET_HAVE_BYTE1: {
        int8_t dy = (int8_t)data;
        /* 组装完整事件 */
        struct mouse_event ev;
        ev.buttons = packet_byte0 & 0x07;
        ev.dx = (int8_t)event_buf[event_tail].dx;
        ev.dy = dy;

        /* 推入队列 */
        mouse_event_put(&ev);

        packet_state = MOUSE_PACKET_WAIT_SYNC;
        break;
    }

    default:
        packet_state = MOUSE_PACKET_WAIT_SYNC;
        break;
    }
}

/* ===== 初始化 ===== */

/*
 * mouse_init — 初始化 PS/2 鼠标
 *
 * 步骤:
 * 1. 禁用鼠标端口
 * 2. 清空输出缓冲区
 * 3. 启用鼠标端口
 * 4. 配置控制器: 启用 IRQ12
 * 5. 复位鼠标
 * 6. 设置采样率 (100 样本/秒)
 * 7. 启用数据报告
 * 8. 注册 IDT 门 (INT 0x2C)
 */
void mouse_init(void)
{
    uint8_t config, resp;

    /* 初始化状态 */
    event_head = 0;
    event_tail = 0;
    event_count = 0;
    packet_state = MOUSE_PACKET_WAIT_SYNC;

    /* 第 1 步: 禁用鼠标端口 */
    mouse_send_cmd(MOUSE_CMD_DISABLE_AUX);

    /* 第 2 步: 清空输出缓冲区 */
    mouse_flush_output();

    /* 第 3 步: 读取并修改控制器配置字节 */
    mouse_send_cmd(MOUSE_CMD_READ_CONFIG);
    config = mouse_read_data();

    /* 启用 IRQ12 和时钟 */
    config |= 0x02;     /* bit 1: 启用鼠标中断 (IRQ12) */
    config &= ~0x20;    /* bit 5: 启用时钟 (清除禁用位) */

    mouse_send_cmd(MOUSE_CMD_WRITE_CONFIG);
    mouse_write_data(config);

    /* 第 4 步: 启用鼠标端口 */
    mouse_send_cmd(MOUSE_CMD_ENABLE_AUX);
    mouse_flush_output();

    /* 第 5 步: 复位鼠标 */
    resp = mouse_send_aux(MOUSE_CMD_RESET);
    if (resp != MOUSE_ACK) {
        screen_puts("[mouse] reset NAK.\n");
        serial_puts("[mouse] reset NAK\n");
        return;
    }

    /* 等待自检完成 */
    resp = mouse_read_data();
    if (resp != MOUSE_SELF_TEST_OK) {
        screen_puts("[mouse] self-test failed.\n");
        serial_puts("[mouse] self-test failed\n");
        return;
    }

    /* 第 6 步: 设置默认值 */
    resp = mouse_send_aux(MOUSE_CMD_SET_DEFAULTS);
    if (resp != MOUSE_ACK) {
        screen_puts("[mouse] set defaults failed.\n");
        serial_puts("[mouse] set defaults failed\n");
        return;
    }

    /* 第 7 步: 设置采样率 (100 样本/秒) */
    resp = mouse_send_aux(MOUSE_CMD_SET_SAMPLE);
    if (resp == MOUSE_ACK) {
        mouse_write_data(100);
        mouse_read_data();  /* 丢弃 ACK */
    }

    /* 第 8 步: 启用数据报告 */
    resp = mouse_send_aux(MOUSE_CMD_ENABLE);
    if (resp != MOUSE_ACK) {
        screen_puts("[mouse] enable failed.\n");
        serial_puts("[mouse] enable failed\n");
        return;
    }

    /* 第 9 步: 注册中断 */
    /* IRQ12 → INT 0x2C */
    {
        extern void isr_mouse(void);
        idt_set_gate(MOUSE_INT_VEC, isr_mouse);
    }

    mouse_present = 1;

    screen_puts("[mouse] PS/2 mouse ready.\n");
    serial_puts("[mouse] PS/2 mouse ready\n");
}

/* ===== 公共接口 ===== */

/*
 * mouse_get_event — 读取鼠标事件 (非阻塞)
 * @ev: 输出事件结构
 * 返回: 1 = 有事件, 0 = 缓冲区空
 */
int mouse_get_event(struct mouse_event *ev)
{
    return mouse_event_get(ev);
}

/*
 * mouse_available — 检查待读取的鼠标事件数
 * 返回: 事件数
 */
int mouse_available(void)
{
    return event_count;
}
