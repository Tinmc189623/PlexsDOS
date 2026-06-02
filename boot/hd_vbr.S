/*
 * Nexsteaduser — PlexsDOS
 * FAT32 卷引导记录 (VBR)
 * 作者: Tinmc189623 | 团队: Nexlyh
 *
 * FAT32 引导扇区, 从 FAT32 根目录加载 KERNEL.BIN:
 * 1. 读取 FAT 表到临时缓冲区
 * 2. 扫描根目录簇链查找 "KERNEL  BIN"
 * 3. 沿 FAT 链逐簇加载到 0x30000
 * 4. 切换到 32-bit 保护模式
 * 5. 跳转到内核
 *
 * 必须在 512 字节内完成 (含 BPB)。
 * 汇编语法: GAS AT&T
 */

.code16
.section .text
.global _start

/* ==================== FAT32 BPB (偏移 0x00-0x5A) ==================== */
_start:
    jmp  boot_code
    nop

/* 标准 BPB (偏移 0x03-0x3F) */
bpb_oem:           .ascii "PLXSDOS "   /* 8 字节 OEM 标识 */
bpb_bytes_per_sec: .word 512
bpb_sec_per_clust: .byte 8
bpb_reserved_secs: .word 32
bpb_num_fats:      .byte 2
bpb_root_entries:  .word 0             /* FAT32 为 0 */
bpb_total_secs16:  .word 0             /* FAT32 使用 32-bit 字段 */
bpb_media_type:    .byte 0xF8          /* 固定磁盘 */
bpb_fat_size16:    .word 0             /* FAT32 使用 32-bit 字段 */
bpb_sec_per_track: .word 63
bpb_num_heads:     .word 16
bpb_hidden_secs:   .long 2048          /* 分区起始 LBA */
bpb_total_secs32:  .long 0             /* 由安装程序填充 */

/* FAT32 扩展 BPB (偏移 0x40-0x52) */
bpb_fat_size32:    .long 0             /* 由安装程序填充 */
bpb_ext_flags:     .word 0
bpb_fs_version:    .word 0
bpb_root_cluster:  .long 2             /* 根目录起始簇 */
bpb_fs_info_sec:   .word 1             /* FSINFO 扇区号 */
bpb_backup_boot:   .word 6             /* 备份引导扇区 */
bpb_reserved:      .space 12, 0
bpb_drive_num:     .byte 0x80          /* 硬盘 */
bpb_reserved2:     .byte 0
bpb_boot_sig:      .byte 0x29          /* 扩展引导签名 */
bpb_serial:        .long 0x12345678    /* 卷序列号 */
bpb_label:         .ascii "PLXSDOS    " /* 11 字节卷标 */
bpb_fs_type:       .ascii "FAT32   "   /* 8 字节文件系统类型 */

/* ==================== 引导代码 (偏移 0x5A) ==================== */
boot_code:
    cli
    xor  %ax, %ax
    mov  %ax, %ds
    mov  %ax, %es
    mov  %ax, %ss
    mov  $0x7C00, %sp
    sti

    /* SI 指向数据区基址 (16-bit 寻址, 避免 32-bit 前缀) */
    mov  $data_area, %si

    /* 保存启动驱动器号 */
    mov  %dl, (%si)

    /* 计算数据区起始 LBA = reserved + num_fats * fat_size32 */
    mov  bpb_reserved_secs, %ax
    mov  %ax, 4(%si)              /* fat_lba (16-bit) */
    movzbl bpb_num_fats, %ecx
    mov  bpb_fat_size32, %eax
    imul %ecx, %eax
    add  4(%si), %eax
    mov  %eax, 6(%si)             /* data_lba (32-bit) */

    /* 读取 FAT 表前 32 扇区到 0x2000 */
    mov  4(%si), %ax              /* fat_lba → EAX */
    movzwl %ax, %eax
    mov  $0x2000, %bx
    movb $32, %cl
    call read_sectors
    jc   die

    /* 读取根目录第一个簇到 0x4000 */
    /* 根目录 LBA = data_lba + (root_cluster - 2) * sec_per_clust */
    mov  bpb_root_cluster, %eax
    sub  $2, %eax
    movzbl bpb_sec_per_clust, %ecx
    imul %ecx, %eax
    add  6(%si), %eax             /* + data_lba */
    mov  $0x4000, %bx
    movb bpb_sec_per_clust, %cl
    call read_sectors
    jc   die

    /* 扫描根目录查找 "KERNEL  BIN" */
    mov  $0x4000, %di

find_loop:
    cmpb $0x00, (%di)             /* 空条目 = 目录结束 */
    je   die
    cmpb $0xE5, (%di)             /* 已删除 */
    je   find_next
    cmpb $0x0F, 11(%di)           /* LFN */
    je   find_next

    /* 比较前 8 字节 (主文件名) */
    mov  $kernel_name, %si
    push %di
    mov  $8, %cx
    repe cmpsb
    pop  %di
    jne  find_next

    /* 匹配! 读取簇号和文件大小 */
    mov  20(%di), %ax             /* cluster_hi */
    shl  $16, %eax
    mov  26(%di), %ax             /* cluster_lo */
    mov  %eax, cur_cluster

    mov  28(%di), %eax            /* file_size */
    mov  %eax, file_size

    jmp  load_setup

find_next:
    add  $32, %di
    mov  %di, %ax
    sub  $0x4000, %ax
    movzbw bpb_sec_per_clust, %cx
    shl  $9, %cx                  /* * 512 */
    cmp  %ax, %cx
    ja   find_loop
    /* 第一个簇搜索完毕, 未找到 → halt */

die:
    cli
    hlt
    jmp  die

/* ==================== 内核加载 ==================== */
load_setup:
    mov  $0x3000, %ax
    mov  %ax, %es                 /* ES = 0x3000 → 物理地址 0x30000 */
    xor  %di, %di                 /* DI = 0x0000 */
    mov  $data_area, %si          /* 恢复 SI 指向数据区 */

load_loop:
    /* 检查是否加载完成 */
    mov  bytes_loaded, %eax
    cmp  file_size, %eax
    jae  load_done

    /* 计算当前簇的 LBA */
    mov  cur_cluster, %eax
    sub  $2, %eax
    movzbl bpb_sec_per_clust, %ecx
    imul %ecx, %eax
    add  6(%si), %eax             /* + data_lba */

    /* 读取簇到目标地址 */
    movb bpb_sec_per_clust, %cl
    mov  %di, %bx
    call read_sectors
    jc   die

    /* 更新字节数和目标地址 */
    movzbl bpb_sec_per_clust, %eax
    shl  $9, %eax                 /* * 512 */
    add  %eax, bytes_loaded
    add  %ax, %di                 /* 16-bit add 省前缀 */

    /* FAT 链: 下一簇 = FAT[cluster] & 0x0FFFFFFF */
    mov  cur_cluster, %eax
    shl  $2, %eax
    add  $0x2000, %eax
    mov  (%eax), %eax
    and  $0x0FFFFFFF, %eax
    mov  %eax, cur_cluster
    cmp  $0x0FFFFFF8, %eax
    jb   load_loop

load_done:
    /* 切换到 32-bit 保护模式 */
    cli

    /* 开启 A20 (快速方法, 端口 0x92) */
    in   $0x92, %al
    or   $2, %al
    out  %al, $0x92

    /* 加载 GDT */
    lgdt gdt_desc

    /* 设置 CR0.PE */
    mov  %cr0, %eax
    or   $1, %eax
    mov  %eax, %cr0

    /* 在 16-bit 保护模式下预设段选择子 (节省 32-bit 代码空间) */
    mov  $0x10, %ax
    mov  %ax, %ds
    mov  %ax, %es
    mov  %ax, %ss

    /* 远跳到 32-bit 代码段 */
    ljmp $0x08, $pm_entry

/* ==================== 辅助函数 ==================== */

/*
 * read_sectors — INT 13h 扩展读取扇区
 * 输入: EAX = LBA, CL = 扇区数, ES:BX = 缓冲区
 * 输出: CF = 成功/失败
 */
read_sectors:
    add  bpb_hidden_secs, %eax   /* 分区相对 LBA → 绝对 LBA */
    pusha
    pushw $0x0010          /* DAP 大小 */
    pushw %cx              /* 扇区数 */
    pushw %es              /* 段 */
    pushw %bx              /* 偏移 */
    pushl $0               /* LBA 高 32 */
    pushl %eax             /* LBA 低 32 */
    mov  $0x42, %ah
    mov  data_area, %dl    /* boot_drive */
    mov  %sp, %si
    int  $0x13
    add  $16, %sp
    popa
    ret

/* ==================== 数据区 (16-bit 可寻址) ==================== */
.align 2
data_area:
boot_drive:  .byte 0               /* 偏移 0 */
fat_lba:     .word 0               /* 偏移 1 (16-bit) */
             .word 0               /* padding */
data_lba:    .long 0               /* 偏移 4 */
cur_cluster: .long 0               /* 偏移 8 */
file_size:   .long 0               /* 偏移 12 */
bytes_loaded:.long 0               /* 偏移 16 */

kernel_name: .ascii "KERNEL  "     /* 8 字节, 只比较主文件名 */

/* ==================== GDT ==================== */
gdt_null:
    .quad 0
gdt_code:
    .word 0xFFFF, 0x0000
    .byte 0x00, 0x9A, 0xCF, 0x00
gdt_data:
    .word 0xFFFF, 0x0000
    .byte 0x00, 0x92, 0xCF, 0x00
gdt_end:

gdt_desc:
    .word gdt_end - gdt_null - 1
    .long gdt_null

/* ==================== 32-bit 保护模式入口 ==================== */
.code32
pm_entry:
    /* ESP 由 kernel_entry.S 设置, 此处无需重复设置 */
    ljmp $0x08, $0x30000

/* 填充到 510 字节, 之后是引导签名 */
.fill 510 - (. - _start), 1, 0
.word 0xAA55
