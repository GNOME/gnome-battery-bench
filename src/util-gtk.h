#ifndef __UTIL_GTK_H__
#define __UTIL_GTK_H__

#include <gtk/gtk.h>

void
label_set_textf(GtkLabel   *label,
                const char *format,
                ...) G_GNUC_PRINTF(2, 3);

#endif /* __UTIL_GTK_H__ */
