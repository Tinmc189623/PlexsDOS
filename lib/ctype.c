/*
 * Nexsteaduser — PlexsDOS
 * ctype.h 实现 — 字符分类和转换函数
 * 作者: Tinmc189623 | 团队: Nexlyh
 */

#include <libc/ctype.h>

/*
 * isalpha — 是否为字母 (a-z, A-Z)
 * @c: 字符 (int)
 * 返回: 非零 = 是字母, 0 = 不是。
 */
int isalpha(int c)
{
    return (c >= 'A' && c <= 'Z') || (c >= 'a' && c <= 'z');
}

/*
 * isdigit — 是否为数字 (0-9)
 * @c: 字符
 * 返回: 非零 = 是数字, 0 = 不是。
 */
int isdigit(int c)
{
    return c >= '0' && c <= '9';
}

/*
 * isalnum — 是否为字母或数字
 * @c: 字符
 * 返回: 非零 = 是字母或数字。
 */
int isalnum(int c)
{
    return isalpha(c) || isdigit(c);
}

/*
 * isspace — 是否为空白字符
 * @c: 字符
 * 返回: 非零 = 是空白 (空格, \t, \n, \r, \f, \v)。
 */
int isspace(int c)
{
    return c == ' ' || c == '\t' || c == '\n' ||
           c == '\r' || c == '\f' || c == '\v';
}

/*
 * isupper — 是否为大写字母
 * @c: 字符
 * 返回: 非零 = 是大写。
 */
int isupper(int c)
{
    return c >= 'A' && c <= 'Z';
}

/*
 * islower — 是否为小写字母
 * @c: 字符
 * 返回: 非零 = 是小写。
 */
int islower(int c)
{
    return c >= 'a' && c <= 'z';
}

/*
 * isprint — 是否为可打印字符 (0x20-0x7E)
 * @c: 字符
 * 返回: 非零 = 可打印。
 */
int isprint(int c)
{
    return c >= 0x20 && c <= 0x7E;
}

/*
 * isxdigit — 是否为十六进制数字 (0-9, a-f, A-F)
 * @c: 字符
 * 返回: 非零 = 是十六进制数字。
 */
int isxdigit(int c)
{
    return isdigit(c) || (c >= 'A' && c <= 'F') ||
           (c >= 'a' && c <= 'f');
}

/*
 * ispunct — 是否为标点符号
 * @c: 字符
 * 返回: 非零 = 是标点。
 */
int ispunct(int c)
{
    return isprint(c) && !isalnum(c) && !isspace(c);
}

/*
 * iscntrl — 是否为控制字符 (0x00-0x1F, 0x7F)
 * @c: 字符
 * 返回: 非零 = 是控制字符。
 */
int iscntrl(int c)
{
    return (c >= 0x00 && c <= 0x1F) || c == 0x7F;
}

/*
 * toupper — 转换为大写
 * @c: 字符
 * 返回: 大写字符 (非小写字符原样返回)。
 */
int toupper(int c)
{
    if (c >= 'a' && c <= 'z')
        return c - 'a' + 'A';
    return c;
}

/*
 * tolower — 转换为小写
 * @c: 字符
 * 返回: 小写字符 (非大写字符原样返回)。
 */
int tolower(int c)
{
    if (c >= 'A' && c <= 'Z')
        return c - 'A' + 'a';
    return c;
}
