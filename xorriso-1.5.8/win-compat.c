/* ==========================================================================
 * win-compat.c — Windows POSIX 兼容层实现
 * 项目：xorriso 1.5.8 Windows 移植
 * 作者：Tinmc189623
 * 团队：Nexlyh
 * 品牌：Nexsteaduser
 * Copyright (C) 2026 Nexlyh, Tinmc189623 / Nexsteaduser
 *
 * POSIX 兼容函数在 Windows 上的实际实现。
 * ========================================================================== */

#include "win-compat.h"
#include <stdlib.h>
#include <string.h>
#include <wchar.h>

/* ===================================================================
 * dirent.h 实现
 * =================================================================== */

/* 将多字节字符串转换为宽字符串（堆分配） */
static wchar_t *mb_to_wcs(const char *src)
{
    int len;
    wchar_t *dst;

    len = MultiByteToWideChar(CP_UTF8, 0, src, -1, NULL, 0);
    if (len <= 0) return NULL;
    dst = (wchar_t *)malloc(len * sizeof(wchar_t));
    if (dst == NULL) return NULL;
    MultiByteToWideChar(CP_UTF8, 0, src, -1, dst, len);
    return dst;
}

DIR *opendir(const char *name)
{
    DIR *dirp;
    wchar_t *wpath;
    size_t len;

    if (name == NULL || name[0] == '\0')
        return NULL;

    dirp = (DIR *)calloc(1, sizeof(*dirp));
    if (dirp == NULL)
        return NULL;

    /* 构建搜索模式：path\* */
    len = strlen(name);
    if (len + 3 > MAX_PATH) {
        free(dirp);
        return NULL;
    }
    memcpy(dirp->searchPath, name, len);
    if (dirp->searchPath[len - 1] == '\\' || dirp->searchPath[len - 1] == '/')
        dirp->searchPath[len - 1] = '\0';
    strcat(dirp->searchPath, "\\*");

    dirp->hFind = INVALID_HANDLE_VALUE;
    dirp->firstCall = 1;
    dirp->inoCounter = 0;

    wpath = mb_to_wcs(dirp->searchPath);
    if (wpath == NULL) {
        free(dirp);
        return NULL;
    }

    dirp->hFind = FindFirstFileW(wpath, &dirp->findData);
    free(wpath);

    if (dirp->hFind == INVALID_HANDLE_VALUE) {
        free(dirp);
        return NULL;
    }
    return dirp;
}

struct dirent *readdir(DIR *dirp)
{
    WIN32_FIND_DATAW *fd;
    int len;

    if (dirp == NULL)
        return NULL;

    fd = &dirp->findData;

    if (dirp->firstCall) {
        dirp->firstCall = 0;
    } else {
        if (!FindNextFileW(dirp->hFind, fd)) {
            return NULL;
        }
    }

    /* 将文件名从宽字符转换为 UTF-8 */
    len = WideCharToMultiByte(CP_UTF8, 0, fd->cFileName, -1,
                              dirp->entry.d_name, NAME_MAX, NULL, NULL);
    if (len <= 0) {
        return NULL;
    }
    dirp->entry.d_name[len] = '\0';
    dirp->entry.d_ino = ++dirp->inoCounter;
    dirp->entry.d_reclen = (unsigned short)len;

    /* 文件类型 */
    if (fd->dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY)
        dirp->entry.d_type = DT_DIR;
    else
        dirp->entry.d_type = DT_REG;

    return &dirp->entry;
}

int closedir(DIR *dirp)
{
    if (dirp == NULL)
        return -1;
    if (dirp->hFind != INVALID_HANDLE_VALUE)
        FindClose(dirp->hFind);
    free(dirp);
    return 0;
}

void rewinddir(DIR *dirp)
{
    wchar_t *wpath;

    if (dirp == NULL)
        return;

    if (dirp->hFind != INVALID_HANDLE_VALUE)
        FindClose(dirp->hFind);

    dirp->firstCall = 1;
    dirp->inoCounter = 0;

    wpath = mb_to_wcs(dirp->searchPath);
    if (wpath != NULL) {
        dirp->hFind = FindFirstFileW(wpath, &dirp->findData);
        free(wpath);
    }
}

/* ===================================================================
 * strptime — 简单实现
 *
 * 支持格式：%Y %m %d %H %M %S %a %b %d（足够 libisofs 使用）
 * =================================================================== */
char *win_strptime(const char *buf, const char *fmt, struct tm *tm)
{
    int y, m, d, H, M, S;
    const char *p;

    if (buf == NULL || fmt == NULL || tm == NULL)
        return NULL;

    /* 先清除 tm */
    memset(tm, 0, sizeof(*tm));

    y = m = d = H = M = S = -1;

    while (*fmt && *buf) {
        if (*fmt == '%') {
            fmt++;
            switch (*fmt) {
            case 'Y':
                if (sscanf(buf, "%4d%n", &y, &p) >= 1)
                    buf = (const char *)p;
                break;
            case 'm':
                if (sscanf(buf, "%2d%n", &m, &p) >= 1)
                    buf = (const char *)p;
                break;
            case 'd':
                if (sscanf(buf, "%2d%n", &d, &p) >= 1)
                    buf = (const char *)p;
                break;
            case 'H':
                if (sscanf(buf, "%2d%n", &H, &p) >= 1)
                    buf = (const char *)p;
                break;
            case 'M':
                if (sscanf(buf, "%2d%n", &M, &p) >= 1)
                    buf = (const char *)p;
                break;
            case 'S':
                if (sscanf(buf, "%2d%n", &S, &p) >= 1)
                    buf = (const char *)p;
                break;
            case 'a': /* 简化：跳过 3 个字符的星期几缩写 */
            case 'A':
                while (*buf && *buf != ' ' && *buf != '-')
                    buf++;
                break;
            case 'b': /* 简化：跳过月份缩写 */
            case 'B':
                while (*buf && *buf != ' ' && *buf != '-')
                    buf++;
                break;
            case 'T': /* %T = %H:%M:%S */
                if (sscanf(buf, "%2d:%2d:%2d%n", &H, &M, &S, &p) >= 3)
                    buf = (const char *)p;
                break;
            case 'D': /* %D = %m/%d/%y */
                if (sscanf(buf, "%2d/%2d/%2d%n", &m, &d, &y, &p) >= 3)
                    buf = (const char *)p;
                break;
            case 'F': /* %F = %Y-%m-%d */
                if (sscanf(buf, "%4d-%2d-%2d%n", &y, &m, &d, &p) >= 3)
                    buf = (const char *)p;
                break;
            case 'R': /* %R = %H:%M */
                if (sscanf(buf, "%2d:%2d%n", &H, &M, &p) >= 2)
                    buf = (const char *)p;
                break;
            case 's': /* Unix timestamp */
            {
                time_t t;
                if (sscanf(buf, "%lld%n", (long long *)&t, &p) >= 1) {
#ifdef _MSC_VER
                    struct tm *lt = _localtime64(&t);
#else
                    struct tm *lt = localtime(&t);
#endif
                    if (lt) *tm = *lt;
                    buf = (const char *)p;
                }
                break;
            }
            case '%':
                if (*buf == '%') buf++;
                break;
            case '\0':
                goto done;
            default:
                if (*buf == *fmt) buf++;
                break;
            }
            if (*fmt) fmt++;
        } else if (*fmt == ' ' || *fmt == '\t') {
            /* 跳过空白 */
            while (*buf == ' ' || *buf == '\t')
                buf++;
            fmt++;
        } else {
            if (*buf == *fmt) {
                buf++;
                fmt++;
            } else {
                break;
            }
        }
    }

done:
    if (y >= 0) tm->tm_year = y - 1900;
    if (m >= 0) tm->tm_mon  = m - 1;
    if (d >= 0) tm->tm_mday = d;
    if (H >= 0) tm->tm_hour = H;
    if (M >= 0) tm->tm_min  = M;
    if (S >= 0) tm->tm_sec  = S;

    return (char *)buf;
}
