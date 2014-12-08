#include "util.h"
#include <errno.h>
#include <stdarg.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>

void
die(const char *msg, ...)
{
    va_list vap;
    va_start(vap, msg);
    vfprintf(stderr, msg, vap);
    va_end(vap);

    fprintf(stderr, "\n");

    exit(1);
}

void
die_errno(const char *msg, ...)
{
    va_list vap;
    va_start(vap, msg);
    vfprintf(stderr, msg, vap);
    va_end(vap);

    fprintf(stderr, ": %s\n", strerror(errno));

    exit(1);
}

void
break_time(double span,
           int *h, int *m, int *s)
{
    int span_seconds = 0.5 + span;
    *h = span_seconds / 3600;
    *m = span_seconds / 60 - *h * 60;
    *s = span_seconds - *h *3600 - *m * 60;
}
