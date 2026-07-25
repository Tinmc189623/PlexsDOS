/*
 * Nexsteaduser — PlexsDOS
 * 系统配置常量
 * 作者: Tinmc189623 | 团队: Nexlyh
 */

#ifndef _PLXSDOS_CONFIG_H
#define _PLXSDOS_CONFIG_H

/* 版本信息 */
#define PLXSDOS_VERSION_MAJOR  0
#define PLXSDOS_VERSION_MINOR  2
#define PLXSDOS_VERSION_STRING "PlexsDOS 0.2"

/* 内存布局 */
#define KERNEL_LOAD_ADDR      0x30000  /* 内核加载地址 (ES=0x3000, BX=0x0000, 避免 BX 溢出) */
#define STACK_TOP             0x90000  /* 内核栈顶 */
#define INTERRUPT_STACK_TOP   0x91000  /* Ring 0 中断处理栈顶 */
#define USER_LOAD_ADDR        0x50000  /* 用户程序加载地址 */
#define USER_STACK_TOP        0x81000  /* 用户栈顶 (Ring 3) */
#define PAGE_DIR_ADDR         0x100000 /* 页目录物理地址 */
#define PAGE_TABLE_ADDR       0x101000 /* 页表 0 物理地址 */
#define VGA_TEXT_BUFFER       0xB8000  /* VGA 文本模式显存 */

/* 屏幕参数 */
#define SCREEN_COLS        80
#define SCREEN_ROWS        25
#define SCREEN_SIZE        (SCREEN_COLS * SCREEN_ROWS)

/* 默认颜色属性 (黑底白字) */
#define DEFAULT_COLOR      0x07

/* 键盘缓冲区大小 */
#define KBD_BUFFER_SIZE    64

/* Shell */
#define SHELL_PROMPT       "PLXSDOS> "
#define SHELL_CMD_MAX_LEN  128

/* 磁盘参数 (1.44MB 软盘) */
#define SECTOR_SIZE        512
#define SECTORS_PER_TRACK  18
#define NUM_HEADS          2
#define TOTAL_SECTORS      2880

/* 内核最大扇区数 (加载到内存的扇区数) */
#define KERNEL_SECTORS     80

#endif /* _PLXSDOS_CONFIG_H */
