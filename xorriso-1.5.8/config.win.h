/* ==========================================================================
 * config.win.h — Windows (MSVC) 平台兼容配置
 * 项目：xorriso 1.5.8 Windows 移植
 * 作者：Tinmc189623
 * 团队：Nexlyh
 * 品牌：Nexsteaduser
 * Copyright (C) 2026 Nexlyh, Tinmc189623 / Nexsteaduser
 *
 * 本文件替代 autotools 生成的 config.h，提供 MSVC 编译器所需的
 * 平台宏定义。基于 config.h.in 手工调整。
 * ========================================================================== */

#ifndef CONFIG_WIN_H
#define CONFIG_WIN_H

/* 禁用 MSVC 安全警告：POSIX 名称已弃用 */
#define _CRT_SECURE_NO_WARNINGS 1
#define _CRT_NONSTDC_NO_DEPRECATE 1

/* ===================================================================
 * 头文件可用性
 * MSVC 不提供以下 POSIX 头文件：
 *   dlfcn.h, poll.h, unistd.h, sys/select.h
 * stdint.h / inttypes.h 从 VS2010 起可用。
 * sys/stat.h, sys/types.h 从 VS 最早版本起可用。
 * =================================================================== */
#define HAVE_INTTYPES_H 1
#define HAVE_STDINT_H   1
#define HAVE_STDLIB_H   1
#define HAVE_STRINGS_H  0   /* MSVC 不提供 strings.h */
#define HAVE_STRING_H   1
#define HAVE_MEMORY_H   0   /* MSVC 不提供 memory.h */
#define HAVE_SYS_STAT_H 1
#define HAVE_SYS_TYPES_H 1
#define HAVE_UNISTD_H   0   /* MSVC 不提供 unistd.h */
#define HAVE_DLFCN_H    0   /* MSVC 不提供 dlfcn.h */
#define HAVE_POLL_H     0   /* MSVC 不提供 poll.h */
#define HAVE_SYS_SELECT_H 0 /* MSVC 不提供 sys/select.h */

/* ===================================================================
 * 函数可用性
 * timegm()       — MSVC 不提供，需要自行实现（_mkgmtime 替代方案）
 * tm.tm_gmtoff   — MSVC 不提供（用 _timezone 全局变量替代）
 * fseeko/ftello  — MSVC 2005+ 提供 _fseeki64/_ftelli64
 * eaccess        — MSVC 不提供（用 _access 替代）
 * =================================================================== */
#define HAVE_TIMEGM     0   /* MSVC 不提供 timegm */
/* #undef HAVE_TM_GMTOFF */   /* MSVC 不提供 tm_gmtoff */
#define HAVE_FSEEKO     1   /* 用 _fseeki64 模拟 */
#define HAVE_EACCESS    0   /* MSVC 不提供 eaccess */

/* ===================================================================
 * 库依赖
 * zlib — MSVC 可通过 vcpkg 或预编译 SDK 获得
 * libiconv — MSVC 不默认提供（禁用）
 * libreadline — MSVC 不提供（禁用）
 * libacl — Windows ACL API 不同（禁用）
 * libbz2 — 可选，用于 libjte（暂不启用）
 * =================================================================== */
#define HAVE_LIBZ        0   /* 暂不使用外部 zlib */
#define HAVE_LIBICONV    0
#define HAVE_LIBREADLINE 0
#define HAVE_LIBACL      0
#define HAVE_LIBCDIO     0
#define HAVE_LIBBZ2      0

/* ===================================================================
 * xorriso 特性开关
 * Xorriso_standalonE — 静态链接所有符号
 * Xorriso_allow_external_filterS — 允许外部过滤进程（由启动器提供）
 * Xorriso_allow_launch_frontenD — 允许启动前端
 * =================================================================== */
#define Xorriso_standalonE 1
#define Xorriso_allow_external_filterS 0  /* Windows 不 spawn 外部进程 */
#define Xorriso_allow_launch_frontenD 0
#define Xorriso_dvd_obs_default_64K 0
#define Xorriso_with_editlinE 0
#define Xorriso_with_readlinE 0
#define Xorriso_with_libjtE 0

/* ===================================================================
 * libisofs 特性开关
 * Libisofs_with_zliB — 内置 zlib 压缩（zisofs）
 * Libisofs_with_libjtE — libjte（禁用，需要 libbz2/zlib）
 * Libisofs_with_aaip_* — ACL/xattr（Windows 不适用，全部禁用）
 * =================================================================== */
#define Libisofs_with_zliB 1
#define Libisofs_with_libjtE 0
#define Libisofs_with_aaip_acL 0
#define Libisofs_with_aaip_xattR 0
#define Libisofs_with_aaip_lfa_flagS 0
#define Libisofs_with_aaip_projiD 0
#define Libisofs_with_freebsd_extattR 0
#define Libisofs_with_sys_xattR 0
#define Libisofs_dir_rec_size_checK 0

/* ===================================================================
 * libburn 相关开关
 * LIBISOFS_WITHOUT_LIBBURN — 嵌入 struct burn_source 定义，
 *   使 libisofs 不依赖 libburn 即可创建 ISO
 * =================================================================== */
/* 完整 xorriso 构建时 libburn 可用，libisofs 使用 libburn.h 的 burn_source。
 * 此处必须 #undef 而非 #define 0，因为 libisofs.h 用 #ifdef 检查。 */
/* #undef LIBISOFS_WITHOUT_LIBBURN */

/* ===================================================================
 * libjte 开关
 * =================================================================== */
#define LIBJTE_WITH_ZLIB 0
#define LIBJTE_WITH_LIBBZ2 0

/* ===================================================================
 * 编译目标名
 * =================================================================== */
#define PACKAGE "xorriso"
#define PACKAGE_BUGREPORT "http://libburnia-project.org"
#define PACKAGE_NAME "xorriso"
#define PACKAGE_STRING "xorriso 1.5.8"
#define PACKAGE_TARNAME "xorriso"
#define PACKAGE_URL ""
#define PACKAGE_VERSION "1.5.8"
#define VERSION "1.5.8"

/* ===================================================================
 * 基本 C 标准定义
 * =================================================================== */
#define STDC_HEADERS 1
#define LT_OBJDIR ".libs/"

/* ===================================================================
 * 字节序
 * x86/x64 都是小端，无需定义 WORDS_BIGENDIAN
 * =================================================================== */
/* #undef WORDS_BIGENDIAN */

/* ===================================================================
 * iconv const 限定符
 * =================================================================== */
#define ICONV_CONST /**/

/* ===================================================================
 * 大文件支持
 * Windows 上 _fseeki64/_ftelli64 原生支持 64 位偏移
 * =================================================================== */
#define _FILE_OFFSET_BITS 64

/* ===================================================================
 * 多线程（禁用，Win32 线程模型不同）
 * =================================================================== */
/* #undef THREADED_CHECKSUMS */

/* ===================================================================
 * libburn 平台开关（通过 LIBISOFS_WITHOUT_LIBBURN + dummy）
 * =================================================================== */
/* #undef Libburn_os_has_statvfS */
/* #undef Libburn_read_o_direcT */
/* #undef Libburn_use_libcdiO */
#define Libburn_use_sg_dummY 1
#define Libburnia_timezonE _timezone

/* ===================================================================
 * 可执行文件扩展名
 * Windows EXE 扩展名处理
 * =================================================================== */
#define Xorriso_exe_ext ".exe"

/* ===================================================================
 * MSVC 缺少的 POSIX 函数名
 * =================================================================== */
#define strcasecmp  _stricmp
#define strncasecmp _strnicmp
/* snprintf/vsnprintf — Windows SDK C11 标准已提供函数声明，
 * 此处不能定义宏，否则与 <stdio.h> 中 #error 检查冲突。 */
#define access      _access
#define chdir       _chdir
/* mkdir 宏移到 win-compat.h（在所有 SDK 头文件包含之后，变参宏避免冲突） */
#define rmdir       _rmdir
#define fileno      _fileno
#define fdopen      _fdopen
#define isatty      _isatty
#define hypot       _hypot
#define putenv       _putenv
#define strdup       _strdup
#define strrev       _strrev

/* ===================================================================
 * MSVC 缺少的 POSIX 常量
 * =================================================================== */
#ifndef S_ISREG
#define S_ISREG(m)  (((m) & S_IFMT) == S_IFREG)
#endif
#ifndef S_ISDIR
#define S_ISDIR(m)  (((m) & S_IFMT) == S_IFDIR)
#endif
#ifndef S_ISLNK
#define S_ISLNK(m)  (0)   /* Windows 没有符号链接 */
#endif
#ifndef S_ISBLK
#define S_ISBLK(m)  (0)
#endif
#ifndef S_ISCHR
#define S_ISCHR(m)  (((m) & S_IFMT) == S_IFCHR)
#endif
#ifndef S_ISFIFO
#define S_ISFIFO(m) (((m) & S_IFMT) == S_IFIFO)
#endif

/* ===================================================================
 * 缺失的 POSIX 类型
 * =================================================================== */
#ifndef mode_t
#define mode_t unsigned short
#endif
#ifndef pid_t
#define pid_t int
#endif
#ifndef ssize_t
#ifdef _WIN64
#define ssize_t __int64
#else
#define ssize_t int
#endif
#endif
/* off_t — 由 Windows SDK <sys/types.h> 提供（x64 上为 long），
 * 不要在此重定义，否则与 typedef _off_t off_t; 冲突。
 * 如需 64 位偏移，使用 _off64_t 或 __int64。 */

/* ===================================================================
 * inline 标记
 * MSVC 支持 __inline
 * =================================================================== */
#ifndef __cplusplus
#define inline __inline
#endif

#endif /* CONFIG_WIN_H */
