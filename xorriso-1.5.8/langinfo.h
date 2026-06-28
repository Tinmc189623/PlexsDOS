/* langinfo.h — Windows MSVC stub */
#ifndef _LANGINFO_H_
#define _LANGINFO_H_
/* Minimal stub — nl_langinfo not available on Windows */
#define CODESET 0
#define D_T_FMT 1
#define D_FMT 2
#define T_FMT 3
#define T_FMT_AMPM 4
#define AM_STR 5
#define PM_STR 6
#define DAY_1 7
#define ABDAY_1 8
#define MON_1 9
#define ABMON_1 10
#define RADIXCHAR 11
#define THOUSEP 12
#define YESEXPR 13
#define NOEXPR 14
#define CRNCYSTR 15
static __inline char *nl_langinfo(int item) { (void)item; return ""; }
#endif
