/*
 * Nexsteaduser — PlexsDOS
 * hal.hpp — 硬件抽象层 C++ 接口
 * 作者: Tinmc189623 | 团队: Nexlyh
 *
 * 基于 hal.h C 接口的 C++ RAII 封装:
 *   - IoPort: 端口 I/O 操作对象
 *   - InterruptGuard: RAII 中断禁用/恢复
 *   - SpinLock: 自旋锁 RAII 封装
 */

#ifndef _PLXSDOS_HAL_HPP
#define _PLXSDOS_HAL_HPP

#include <plexsdos/hal.h>

/*
 * IoPort — I/O 端口操作封装
 *
 * 将端口号与 I/O 操作绑定, 提供类型安全的端口访问。
 * 用法:
 *   IoPort ata_data(0x1F0);
 *   uint16_t val = ata_data.read16();
 *   ata_data.write16(0x1234);
 */
class IoPort {
public:
    /*
     * 构造函数 — 绑定 I/O 端口地址
     * @port: I/O 端口地址
     */
    explicit IoPort(uint16_t port) : m_port(port) {}

    /* 8-bit I/O */
    uint8_t  read8()  const { return hal_inb(m_port); }
    void     write8(uint8_t val)  const { hal_outb(m_port, val); }

    /* 16-bit I/O */
    uint16_t read16() const { return hal_inw(m_port); }
    void     write16(uint16_t val) const { hal_outw(m_port, val); }

    /* 32-bit I/O */
    uint32_t read32() const { return hal_inl(m_port); }
    void     write32(uint32_t val) const { hal_outl(m_port, val); }

    /* 串行 I/O */
    void readsw(void *buf, uint32_t count) const {
        hal_insw(m_port, buf, count);
    }
    void writesw(const void *buf, uint32_t count) const {
        hal_outsw(m_port, buf, count);
    }

    /* 获取端口号 */
    uint16_t port() const { return m_port; }

private:
    uint16_t m_port;
};

/*
 * InterruptGuard — RAII 中断禁用/恢复
 *
 * 构造时保存中断状态并禁用中断, 析构时恢复。
 * 用于保护临界区代码。
 * 用法:
 *   {
 *       InterruptGuard guard;
 *       // 临界区代码 (中断已禁用)
 *   } // 离开作用域时自动恢复中断状态
 */
class InterruptGuard {
public:
    InterruptGuard() : m_flags(hal_irq_save()) {}
    ~InterruptGuard() { hal_irq_restore(m_flags); }

    /* 禁止复制 */
    InterruptGuard(const InterruptGuard &) = delete;
    InterruptGuard &operator=(const InterruptGuard &) = delete;

private:
    uint32_t m_flags;
};

/*
 * SpinLock — 自旋锁 RAII 封装
 *
 * 使用 hal_spin_lock/hal_spin_unlock 实现。
 * 配合 LockGuard 使用。
 */
class SpinLock {
public:
    SpinLock() : m_lock(0) {}

    void lock()   { hal_spin_lock(&m_lock); }
    void unlock() { hal_spin_unlock(&m_lock); }

    /* 禁止复制 */
    SpinLock(const SpinLock &) = delete;
    SpinLock &operator=(const SpinLock &) = delete;

private:
    volatile uint32_t m_lock;
};

/*
 * LockGuard — RAII 锁守卫
 *
 * 构造时获取锁, 析构时释放。
 * 用法:
 *   SpinLock my_lock;
 *   {
 *       LockGuard guard(my_lock);
 *       // 受保护的代码
 *   } // 自动释放锁
 */
class LockGuard {
public:
    explicit LockGuard(SpinLock &lock) : m_lock(lock) {
        m_lock.lock();
    }
    ~LockGuard() { m_lock.unlock(); }

    /* 禁止复制 */
    LockGuard(const LockGuard &) = delete;
    LockGuard &operator=(const LockGuard &) = delete;

private:
    SpinLock &m_lock;
};

#endif /* _PLXSDOS_HAL_HPP */
