/* regex.h — Windows MSVC stub for xorriso */
#ifndef _REGEX_H_
#define _REGEX_H_
#include <stdint.h>
/* Minimal stub — xorriso 在 HAVE_LIBICONV=0 时不会实际使用 regex */
typedef int regex_t;
typedef int regmatch_t;
#define REG_EXTENDED 0
#define REG_NOSUB    0
#define REG_NOMATCH 1
#define regcomp(a,b,c) (1)
#define regexec(a,b,c,d,e) (1)
#define regfree(a) ((void)0)
#endif
