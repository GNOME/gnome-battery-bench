/* -*- mode: C; c-file-style: "stroustrup"; indent-tabs-mode: nil; -*- */

#include "util.h"
#include <errno.h>
#include <stdarg.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <string.h>

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

gchar *
uuid_gen_new(void)
{
    guint8 bytes[32];
    int i;

    for (i = 0; i < 4; i++) {
        guint32 n = g_random_int();
        memcpy(bytes + i * sizeof(guint32), &n, sizeof(n));
    }

    bytes[6] &= 0x0f;
    bytes[6] |= 4 << 4;
    bytes[8] &= 0x3f;
    bytes[8] |= 0x80;

    return g_strdup_printf("%02x%02x%02x%02x-%02x%02x"
                           "-%02x%02x-%02x%02x"
                           "-%02x%02x%02x%02x%02x%02x",
                           bytes[0],  bytes[1],  bytes[2],  bytes[3],
                           bytes[4],  bytes[5],  bytes[6],  bytes[7],
                           bytes[8],  bytes[9],  bytes[10], bytes[11],
                           bytes[12], bytes[13], bytes[14], bytes[15]);
}
