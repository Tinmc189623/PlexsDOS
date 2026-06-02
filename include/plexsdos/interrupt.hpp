/*
 * Nexsteaduser — PlexsDOS
 * C++ 中断管理器 — 面向对象的中断处理框架
 * 作者: Tinmc189623 | 团队: Nexlyh
 *
 * 类似 Win32 子系统的设计:
 * - InterruptHandler: 中断处理程序抽象基类
 * - InterruptManager: 中断管理器单例
 * - SyscallDispatcher: 系统调用分发器
 *
 * 此头文件可同时被 C 和 C++ 代码包含。
 */

#ifndef _PLXSDOS_INTERRUPT_HPP
#define _PLXSDOS_INTERRUPT_HPP

#include <plexsdos/types.h>

#ifdef __cplusplus

/* 中断处理程序抽象基类 */
class InterruptHandler {
public:
    virtual ~InterruptHandler() = default;

    /*
     * handle — 处理中断
     * @vector:     中断向量号
     * @error_code: 错误码 (部分中断无, 传 0)
     */
    virtual void handle(uint32_t vector, uint32_t error_code) = 0;
};

/* 中断管理器 (单例) */
class InterruptManager {
private:
    static InterruptManager s_instance;
    InterruptHandler *m_handlers[256];

    InterruptManager();

public:
    /*
     * instance — 获取单例实例
     * 返回: InterruptManager 引用。
     */
    static InterruptManager &instance();

    /*
     * init — 初始化中断管理器
     * 设置 PIC、IDT, 注册默认处理程序。
     */
    void init();

    /*
     * registerHandler — 注册中断处理程序
     * @vector:  中断向量号 (0-255)
     * @handler: 处理程序对象指针
     */
    void registerHandler(uint8_t vector, InterruptHandler *handler);

    /*
     * unregisterHandler — 注销中断处理程序
     * @vector: 中断向量号
     */
    void unregisterHandler(uint8_t vector);

    /*
     * dispatch — 中断分发 (由汇编桩调用)
     * @vector:     中断向量号
     * @error_code: 错误码
     */
    void dispatch(uint32_t vector, uint32_t error_code);
};

/* 系统调用分发器 (INT 0x22) */
class SyscallDispatcher : public InterruptHandler {
private:
    volatile bool m_exit_flag;

public:
    SyscallDispatcher();

    void handle(uint32_t vector, uint32_t error_code) override;

    /*
     * dispatch_syscall — 系统调用分发 (由汇编桩调用)
     * @eax: AH=功能号, AL=子参数
     * @edx: DL/DX=数据参数
     * @esi: 附加参数
     * 返回: 0=正常, 1=程序请求终止。
     */
    uint32_t dispatch_syscall(uint32_t eax, uint32_t edx,
                              uint32_t esi);

    /*
     * shouldExit — 检查程序是否请求退出
     * 返回: true = 程序请求终止。
     */
    bool shouldExit() const;

    /*
     * resetExit — 重置退出标志
     */
    void resetExit();
};

#endif /* __cplusplus */

/* C 兼容接口 (供 C 和汇编代码调用) */
#ifdef __cplusplus
extern "C" {
#endif

    /*
     * interrupt_manager_init — 初始化 C++ 中断管理器
     * 由 kernel_main() 调用。
     */
    void interrupt_manager_init(void);

    /*
     * interrupt_register_cpp — 注册 C++ 中断处理程序
     * @vector:  中断向量号
     * @handler: InterruptHandler 对象指针 (void* 用于 C 兼容)
     */
    void interrupt_register_cpp(uint8_t vector, void *handler);

#ifdef __cplusplus
}
#endif

#endif /* _PLXSDOS_INTERRUPT_HPP */
