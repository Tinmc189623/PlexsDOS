/* iconv.h — Windows MSVC stub for xorriso */
#ifndef _ICONV_H_
#define _ICONV_H_
#include <stddef.h>
/* iconv is disabled via HAVE_LIBICONV=0 in config.win.h.
   Provide stub types so code that conditionally includes this still compiles. */
typedef void *iconv_t;
#define iconv_open(t, f) ((iconv_t)-1)
#define iconv(c, i, il, o, ol) ((size_t)-1)
#define iconv_close(c) (-1)
#endif
