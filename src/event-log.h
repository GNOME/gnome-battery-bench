#ifndef __EVENT_H__
#define __EVENT_H__

#include <gio/gio.h>

typedef struct {
    char *name;
    unsigned time;
    int x_root, y_root;
    int detail;
} GbbEvent;

void gbb_event_free(GbbEvent *event);

GbbEvent *gbb_event_read (GDataInputStream *input_stream,
                          GCancellable     *cancellable,
                          GError          **error);

int gbb_event_log_duration (GFile        *event_log,
                            GCancellable *cancellable,
                            GError      **error);

#endif /* __EVENT_H__ */

