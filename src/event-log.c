/* -*- mode: C; c-file-style: "stroustrup"; indent-tabs-mode: nil; -*- */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "event-log.h"

void
gbb_event_free(GbbEvent *event)
{
    g_free(event->name);
    g_slice_free(GbbEvent, event);
}

GbbEvent *
gbb_event_read (GDataInputStream *input_stream,
                GCancellable     *cancellable,
                GError          **error)
{
    GbbEvent *event = NULL;
    gboolean have_error = FALSE;

    while (event == NULL && !have_error) {
        GError *local_error = NULL;
        char **fields = NULL;

        char *line = g_data_input_stream_read_line (input_stream, NULL, NULL, &local_error);
        if (local_error) {
            g_propagate_error(error, local_error);
            have_error = TRUE;
            goto next;
        }

        if (!line)
            return NULL;

        char *hash = index(line, '#');
        if (hash)
            *hash = '\0';
        g_strstrip(line);

        fields = g_strsplit (line, ",", -1);

        if (!*line)
            goto next;
        if (g_strv_length (fields) != 5) {
            g_set_error(error,
                        G_IO_ERROR,
                        G_IO_ERROR_FAILED,
                        "Bad field count in '%s'", line);
            have_error = TRUE;
            goto next;
        }

        event = g_slice_new(GbbEvent);

        event->name = g_strdup(fields[0]);
        sscanf(fields[1], "%u", &event->time);
        event->x_root = atoi(fields[2]);
        event->y_root = atoi(fields[3]);
        event->detail = atoi(fields[4]);

    next:
        if (fields)
            g_strfreev(fields);
        g_free(line);
    }

    if (have_error) {
        if (event)
            gbb_event_free(event);
        return NULL;
    } else {
        return event;
    }
}

int
gbb_event_log_duration (GFile        *event_log,
                        GCancellable *cancellable,
                        GError      **error)
{
    GFileInputStream *input_raw = g_file_read(event_log, cancellable, error);
    GDataInputStream *input = g_data_input_stream_new (G_INPUT_STREAM (input_raw));
    g_object_unref(input_raw);
    int duration = 0;

    while (TRUE) {
        GError *local_error = NULL;
        GbbEvent *event = gbb_event_read(input, cancellable, &local_error);
        if (local_error) {
            g_propagate_error(error, local_error);
            duration = -1;
        }

        if (!event)
            break;

        duration = MAX(duration, event->time);

        gbb_event_free(event);
    }

    g_input_stream_close(G_INPUT_STREAM(input), cancellable,
                         !(error && *error) ? error : NULL);
    g_object_unref(input);

    return duration;
}
