/*
 * Nexsteaduser — PlexsDOS
 * 测试程序: 使用 INT 21h 系统调用
 * 作者: Tinmc189623 | 团队: Nexlyh
 *
 * 通过 INT 21h (向量 0x22) 系统调用输出消息。
 * 演示 AH=0x02 (写字符) 和 AH=0x09 (写字符串) 功能。
 * 此程序为 flat binary 格式, 入口在文件开头。
 * 由 Shell 的 run 命令加载到 0x50000 (USER_LOAD_ADDR) 并执行。
 */

.code32
.section .text
.global _start

_start:
    /* VGA 调试标记: 屏幕第 2 行显示 'U3' (Ring 3 user code started) */
    movl  $0xB8000, %eax
    movw  $0x2F55, 160(%eax)  /* 'U' 绿底白字, 第 2 行 */
    movw  $0x2F33, 162(%eax)  /* '3' */

    /* 保存调用者的栈帧 */
    pushl %ebp
    movl  %esp, %ebp

    /* INT 21h AH=0x09: 写字符串 (DS:EDX = 字符串地址, '$' 结尾) */
    leal  msg_hello, %edx
    movb  $0x09, %ah
    int   $0x22                /* INT 21h (向量 0x22) */

    /* INT 21h AH=0x02: 逐字符写 "OK!" */
    movb  $'O', %dl
    movb  $0x02, %ah
    int   $0x22

    movb  $'K', %dl
    movb  $0x02, %ah
    int   $0x22

    movb  $'!', %dl
    movb  $0x02, %ah
    int   $0x22

    movb  $'\n', %dl
    movb  $0x02, %ah
    int   $0x22

    /* 通过 INT 0x22 AH=0x4C 退出程序 (DOS 兼容) */
    xorl  %eax, %eax
    movb  $0x4C, %ah
    int   $0x22

.section .rodata
/* 以 '$' 结尾的字符串 (DOS 格式) */
msg_hello: .asciz "Hello from Nexsteaduser PlexsDOS via INT 21h!\n$"
