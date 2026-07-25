/*
 * Nexsteaduser — PlexsDOS
 * C++ 中断管理器实现
 * 作者: Tinmc189623 | 团队: Nexlyh
 *
 * 面向对象的中断管理和系统调用分发。
 * 与现有 C 代码通过 extern "C" 互操作。
 */

#include <plexsdos/interrupt.hpp>
#include <plexsdos/screen.h>
#include <plexsdos/keyboard.h>
#include <plexsdos/serial.h>
#include <plexsdos/config.h>
#include <plexsdos/pci.h>
#include <plexsdos/isa.h>
#include <plexsdos/fs.h>
#include <plexsdos/loader.h>
#include <plexsdos/syscall.h>
#include <plexsdos/shell.h>
#include <plexsdos/scheduler.h>

/* 外部 C 函数 */
extern "C" {
    void idt_init(void);
    void idt_set_gate(uint8_t vector, void (*handler)(void));
    void idt_set_gate_dpl(uint8_t vector, void (*handler)(void), uint8_t dpl);
    void pic_eoi(uint8_t irq);
}

/* 外部汇编符号 */
extern "C" {
    void isr_default(void);
    void isr_keyboard(void);
    void isr_syscall(void);
    void isr_fdc(void);
}

/* ==================== InterruptManager 单例 ==================== */

InterruptManager InterruptManager::s_instance;

/*
 * InterruptManager::InterruptManager — 构造函数
 * 初始化所有处理程序指针为 nullptr。
 */
InterruptManager::InterruptManager()
{
    for (int i = 0; i < 256; i++)
        m_handlers[i] = nullptr;
}

/*
 * InterruptManager::instance — 获取单例实例
 */
InterruptManager &InterruptManager::instance()
{
    return s_instance;
}

/*
 * InterruptManager::init — 初始化中断管理器
 *
 * IDT 和 PIC 已在 kernel_main 中由 idt_init() 完成初始化。
 * 此处仅需注册需要 C++ 管理器处理的中断。
 */
void InterruptManager::init()
{
    serial_puts("[InterruptManager] initialized.\n");
}

/*
 * InterruptManager::registerHandler — 注册中断处理程序
 * @vector:  中断向量号
 * @handler: 处理程序对象
 */
void InterruptManager::registerHandler(uint8_t vector,
                                       InterruptHandler *handler)
{
    m_handlers[vector] = handler;
}

/*
 * InterruptManager::unregisterHandler — 注销中断处理程序
 * @vector: 中断向量号
 */
void InterruptManager::unregisterHandler(uint8_t vector)
{
    m_handlers[vector] = nullptr;
}

/*
 * InterruptManager::dispatch — 中断分发
 * @vector:     中断向量号
 * @error_code: 错误码
 *
 * 由汇编桩调用, 根据向量号查找并调用对应的处理程序。
 * 如果没有注册处理程序, 仅发送 EOI。
 */
void InterruptManager::dispatch(uint32_t vector,
                                uint32_t error_code)
{
    InterruptHandler *handler = m_handlers[vector];

    if (handler != nullptr) {
        handler->handle(vector, error_code);
    }

    /* 发送 EOI (对于硬件中断) */
    if (vector >= 0x20 && vector <= 0x2F) {
        uint8_t irq = (uint8_t)(vector - 0x20);
        pic_eoi(irq);
    }
}

/* ==================== SyscallDispatcher ==================== */

/*
 * SyscallDispatcher::SyscallDispatcher — 构造函数
 */
SyscallDispatcher::SyscallDispatcher()
    : m_exit_flag(false)
{
}

/*
 * SyscallDispatcher::handle — 处理系统调用中断 (INT 0x22)
 * @vector:     中断向量号
 * @error_code: 错误码 (未使用)
 *
 * 系统调用的实际参数通过汇编桩传递给 dispatch_syscall()。
 * 此方法仅用于中断管理器的统一接口。
 */
void SyscallDispatcher::handle(uint32_t vector, uint32_t error_code)
{
    (void)vector;
    (void)error_code;
    /* 系统调用由汇编桩直接调用 dispatch_syscall, 不经过此路径 */
}

/*
 * SyscallDispatcher::dispatch_syscall — 系统调用分发
 * @eax: AH=功能号, AL=子参数
 * @edx: DL/DX=数据参数 (字符或缓冲区指针)
 * @esi: 附加参数
 * 返回: 0=正常, 1=程序请求终止。
 *
 * 实现 DOS 兼容的 INT 21h 功能和 PlexsDOS 扩展功能。
 */
uint32_t SyscallDispatcher::dispatch_syscall(uint32_t eax,
                                             uint32_t edx,
                                             uint32_t esi)
{
    uint8_t func = (uint8_t)((eax >> 8) & 0xFF);
    uint8_t al = (uint8_t)(eax & 0xFF);
    uint8_t dl = (uint8_t)(edx & 0xFF);
    uint8_t dh = (uint8_t)((edx >> 8) & 0xFF);

    switch (func) {
    /* ===== DOS 兼容功能 ===== */
    case SYS_READ_CHAR: {
        char c = keyboard_getchar();
        return (uint32_t)(uint8_t)c;
    }

    case SYS_WRITE_CHAR:
        screen_putchar((char)dl);
        return 0;

    case SYS_WRITE_STR: {
        const char *str = (const char *)edx;
        while (*str && *str != '$') {
            screen_putchar(*str);
            str++;
        }
        return 0;
    }

    case SYS_READ_STR: {
        char *buf = (char *)edx;
        uint8_t max_len = (uint8_t)buf[0];
        if (max_len < 2)
            return 0;
        keyboard_read_line(buf + 2, max_len - 1);
        uint8_t len = 0;
        while (buf[2 + len] != '\0' && len < max_len - 1)
            len++;
        buf[1] = len;
        return 0;
    }

    case SYS_EXIT:
        m_exit_flag = true;
        return 1;

    /* ===== PlexsDOS 扩展: 屏幕控制 ===== */
    case SYS_CLEAR_SCREEN:
        screen_clear();
        return 0;

    case SYS_SET_COLOR:
        screen_set_color(dl, dh);
        return 0;

    case SYS_RESET_COLOR:
        screen_reset_color();
        return 0;

    case SYS_PUT_DEC:
        screen_put_dec(edx);
        return 0;

    case SYS_PUT_HEX:
        screen_put_hex(edx);
        return 0;

    /* ===== PlexsDOS 扩展: 进程/文件 ===== */
    case SYS_EXEC: {
        const char *filename = (const char *)edx;
        char name_buf[64];
        int i = 0;
        while (filename[i] && filename[i] != '$' && i < 63) {
            name_buf[i] = filename[i];
            i++;
        }
        name_buf[i] = '\0';
        loader_run(name_buf);
        return 0;
    }

    case SYS_FS_LIST:
        fs_list_root();
        return 0;

    case SYS_FS_READ: {
        const char *filename = (const char *)edx;
        uint8_t *buf = (uint8_t *)esi;
        char name_buf[64];
        int i = 0;
        while (filename[i] && filename[i] != '$' && i < 63) {
            name_buf[i] = filename[i];
            i++;
        }
        name_buf[i] = '\0';
        struct fs_entry *entry = fs_find_file(name_buf);
        if (!entry)
            return 0xFFFFFFFF;
        uint32_t size = fs_load_file(entry, (uint32_t)buf);
        return size;
    }

    case SYS_GET_DRIVE:
        return (uint32_t)fs_get_current_drive();

    case SYS_SET_DRIVE:
        fs_set_current_drive((char)dl);
        return 0;

    case SYS_REBOOT:
        __asm__ __volatile__("mov $0xFE, %%al\n\tout %%al, $0x64" : : : "ax", "memory");
        __asm__ __volatile__("cli\n\tlidt (%%eax)\n\tint $0x00" : : "a"(0) : "memory");
        while (1) __asm__ __volatile__("hlt");
        return 0;

    case SYS_GET_VERSION:
        return (PLXSDOS_VERSION_MAJOR << 8) | PLXSDOS_VERSION_MINOR;

    case SYS_SHELL_CMD: {
        const char *cmd = (const char *)edx;
        char cmd_buf[128];
        int i = 0;
        while (cmd[i] && cmd[i] != '$' && i < 127) {
            cmd_buf[i] = cmd[i];
            i++;
        }
        cmd_buf[i] = '\0';
        shell_exec_cmd(cmd_buf);
        return 0;
    }

    /* ===== PnP 设备枚举 ===== */
    case SYS_PNP_PCI_COUNT: {
        return (uint32_t)pci_device_count();
    }

    case SYS_PNP_PCI_GET: {
        int pci_idx = (int)edx;
        struct pci_device *buf = (struct pci_device *)esi;
        struct pci_device *dev = pci_get_device(pci_idx);
        if (!dev)
            return 0xFFFFFFFF;
        for (int i = 0; i < (int)sizeof(struct pci_device); i++)
            ((uint8_t *)buf)[i] = ((uint8_t *)dev)[i];
        return 0;
    }

    case SYS_PNP_ISA_COUNT: {
        return (uint32_t)isa_device_count();
    }

    case SYS_PNP_ISA_GET: {
        int isa_idx = (int)edx;
        struct isa_device *buf = (struct isa_device *)esi;
        struct isa_device *dev = isa_get_device(isa_idx);
        if (!dev)
            return 0xFFFFFFFF;
        for (int i = 0; i < (int)sizeof(struct isa_device); i++)
            ((uint8_t *)buf)[i] = ((uint8_t *)dev)[i];
        return 0;
    }

    default:
        screen_puts("[syscall] unknown: 0x");
        screen_put_hex((uint32_t)func);
        screen_putchar('\n');
        return 0;
    }
}

/*
 * SyscallDispatcher::shouldExit — 检查退出标志
 */
bool SyscallDispatcher::shouldExit() const
{
    return m_exit_flag;
}

/*
 * SyscallDispatcher::resetExit — 重置退出标志
 */
void SyscallDispatcher::resetExit()
{
    m_exit_flag = false;
}

/* ==================== 全局系统调用分发器 ==================== */

static SyscallDispatcher g_syscall_dispatcher;

/* ==================== C 兼容接口 ==================== */

/*
 * interrupt_manager_init — 初始化 C++ 中断管理器
 * 供 kernel_main.c 调用。
 */
extern "C" void interrupt_manager_init(void)
{
    InterruptManager::instance().init();

    /* 注册系统调用分发器到 INT 0x22 */
    InterruptManager::instance().registerHandler(0x22, &g_syscall_dispatcher);
    idt_set_gate_dpl(0x22, isr_syscall, 3);

    serial_puts("[interrupt] C++ InterruptManager ready.\n");
}

/*
 * interrupt_register_cpp — 注册中断处理程序 (C 兼容)
 * @vector:  中断向量号
 * @handler: InterruptHandler 指针 (void* 用于 C 兼容)
 */
extern "C" void interrupt_register_cpp(uint8_t vector, void *handler)
{
    InterruptManager::instance().registerHandler(
        vector, static_cast<InterruptHandler *>(handler));
}

/* ==================== 汇编桩调用的 C++ 函数 ==================== */

/*
 * cpp_syscall_dispatch — 汇编桩调用的系统调用分发
 * 由 interrupt.S 中的 _isr_syscall 调用。
 */
extern "C" uint32_t cpp_syscall_dispatch(uint32_t eax, uint32_t edx,
                                         uint32_t esi)
{
    return g_syscall_dispatcher.dispatch_syscall(eax, edx, esi);
}

/*
 * cpp_syscall_should_exit — 检查系统调用退出标志
 * 由 interrupt.S 中的 _isr_syscall 调用。
 */
extern "C" uint32_t cpp_syscall_should_exit(void)
{
    return g_syscall_dispatcher.shouldExit() ? 1 : 0;
}

/*
 * cpp_syscall_reset_exit — 重置系统调用退出标志
 */
extern "C" void cpp_syscall_reset_exit(void)
{
    g_syscall_dispatcher.resetExit();
}
