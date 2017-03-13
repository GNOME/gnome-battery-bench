#ifndef __UTIL_H__
#define __UTIL_H__

#include <glib.h>

void die(const char *msg, ...)
    __attribute__ ((noreturn))
    __attribute__ ((format (printf, 1, 2)));

void die_errno(const char *msg, ...)
    __attribute__ ((noreturn))
    __attribute__ ((format (printf, 1, 2)));

void break_time(double span,
                int *h, int *m, int *s);


gchar *uuid_gen_new(void);

#endif /* __UTIL_H__ */
