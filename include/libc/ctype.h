/*
 * Nexsteaduser — PlexsDOS
 * ctype.h — 字符分类和转换函数
 * 作者: Tinmc189623 | 团队: Nexlyh
 */

#ifndef _LIBC_CTYPE_H
#define _LIBC_CTYPE_H

#ifdef __cplusplus
extern "C" {
#endif

/*
 * isalpha — 是否为字母 (a-z, A-Z)
 */
int isalpha(int c);

/*
 * isdigit — 是否为数字 (0-9)
 */
int isdigit(int c);

/*
 * isalnum — 是否为字母或数字
 */
int isalnum(int c);

/*
 * isspace — 是否为空白字符 (空格, \t, \n, \r, \f, \v)
 */
int isspace(int c);

/*
 * isupper — 是否为大写字母
 */
int isupper(int c);

/*
 * islower — 是否为小写字母
 */
int islower(int c);

/*
 * isprint — 是否为可打印字符 (0x20-0x7E)
 */
int isprint(int c);

/*
 * isxdigit — 是否为十六进制数字
 */
int isxdigit(int c);

/*
 * ispunct — 是否为标点符号
 */
int ispunct(int c);

/*
 * iscntrl — 是否为控制字符 (0x00-0x1F, 0x7F)
 */
int iscntrl(int c);

/*
 * toupper — 转换为大写
 */
int toupper(int c);

/*
 * tolower — 转换为小写
 */
int tolower(int c);

#ifdef __cplusplus
}
#endif

#endif /* _LIBC_CTYPE_H */
