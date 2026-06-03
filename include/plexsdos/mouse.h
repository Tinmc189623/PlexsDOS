/*
 * Nexsteaduser — PlexsDOS
 * PS/2 鼠标驱动接口
 * 作者: Tinmc189623 | 团队: Nexlyh
 *
 * 通过 IRQ12 (INT 0x2C) 处理 PS/2 鼠标输入。
 * 维护环形事件缓冲区, 提供按钮和相对位移数据。
 */

#ifndef _PLXSDOS_MOUSE_H
#define _PLXSDOS_MOUSE_H

#include <plexsdos/types.h>

#ifdef __cplusplus
extern "C" {
#endif

/* 鼠标按钮掩码 */
#define MOUSE_LBUTTON   (1 << 0)   /* 左键 */
#define MOUSE_RBUTTON   (1 << 1)   /* 右键 */
#define MOUSE_MBUTTON   (1 << 2)   /* 中键 */

/* 鼠标事件结构 */
struct mouse_event {
    uint8_t buttons;    /* 按钮状态 */
    int8_t  dx;         /* X 相对位移 */
    int8_t  dy;         /* Y 相对位移 (负值=向上) */
};

/* 鼠标事件缓冲区大小 */
#define MOUSE_EVENT_BUF  16

/*
 * mouse_init — 初始化 PS/2 鼠标
 * 启用鼠标端口, 设置采样率, 注册中断。
 */
void mouse_init(void);

/*
 * mouse_get_event — 读取鼠标事件 (非阻塞)
 * @ev: 输出事件结构
 * 返回: 1 = 有事件, 0 = 缓冲区空
 */
int mouse_get_event(struct mouse_event *ev);

/*
 * mouse_available — 检查是否有待读取的鼠标事件
 * 返回: 等待的事件数
 */
int mouse_available(void);

#ifdef __cplusplus
}
#endif

#endif /* _PLXSDOS_MOUSE_H */
