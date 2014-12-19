/* -*- mode: C; c-file-style: "stroustrup"; indent-tabs-mode: nil; -*- */

#include <stdarg.h>

#include "util-gtk.h"

void
label_set_textf(GtkLabel       *label,
                const char     *format,
                ...)
{
    va_list args;

    va_start(args, format);
    char *text = g_strdup_vprintf(format, args);
    va_end(args);

    gtk_label_set_text(label, text);
    g_free(text);
}
