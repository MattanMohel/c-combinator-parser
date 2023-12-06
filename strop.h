#ifndef _STROP_H_
#define _STROP_H_

#include <stdarg.h>

char *pc_strcpy  (const char *str);
char *pc_format  (const char *str, va_list fmt);
char *pc_formatf (const char *str, ...);

#endif
