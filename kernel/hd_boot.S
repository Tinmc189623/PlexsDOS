/*
 * Nexsteaduser — PlexsDOS
 * 硬盘引导扇区二进制嵌入
 * 作者: Tinmc189623 | 团队: Nexlyh
 *
 * 使用 .incbin 将编译好的 MBR 和 VBR 二进制嵌入内核。
 * 安装程序通过符号引用这些数据。
 */

.section .rodata

/* COFF 格式下 C 编译器会自动添加 _ 前缀, 所以汇编需要双下划线 */
.global __binary_hd_mbr_bin_start
.global __binary_hd_mbr_bin_end
.global __binary_hd_vbr_bin_start
.global __binary_hd_vbr_bin_end

.align 4
__binary_hd_mbr_bin_start:
    .incbin "build/hd_mbr.bin"
__binary_hd_mbr_bin_end:

.align 4
__binary_hd_vbr_bin_start:
    .incbin "build/hd_vbr.bin"
__binary_hd_vbr_bin_end:
