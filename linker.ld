/* Nexsteaduser — PlexsDOS 链接器脚本 (32-bit 保护模式) */
/* 作者: Tinmc189623 | 团队: Nexlyh */
/* 内核加载到 0x30000, 所有段连续排列 (无间隙) */

ENTRY(_start)

SECTIONS
{
    . = 0x30000;

    .text : {
        *(.text)
        *(.rdata)
        *(.rdata*)
        *(.rodata*)
        *(.data*)
    }

    .bss : {
        _bss_start = .;
        *(.bss)
        *(COMMON)
    }

    _bss_end = .;
    _end = .;

    /DISCARD/ : {
        *(.reloc)
        *(.eh_frame)
        *(.eh_frame*)
    }
}
