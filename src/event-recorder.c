/* -*- mode: C; c-file-style: "stroustrup"; indent-tabs-mode: nil; -*- */

#include <stdio.h>
#include <string.h>

#include <X11/Xlib.h>
#include <X11/extensions/record.h>
#include <X11/Xproto.h>

#include "event-recorder.h"
#include <libevdev/libevdev.h>
#include "util.h"

struct _GbbEventRecorder {
    Display *control_display;
    Display *data_display;
    XRecordContext context;

    char *filename;
    FILE *out;

    Time start_time;
    GList *pressed_keys;
    GList *pressed_buttons;
};

static void
dump_event(GbbEventRecorder *recorder,
           const char       *event_name,
           xEvent           *xevent,
           int               detail)
{
    const char *comment = NULL;
    if (strcmp(event_name, "KeyPress") == 0 ||
        strcmp(event_name, "KeyRelease") == 0)
        comment = libevdev_event_code_get_name(EV_KEY, detail);

    fprintf(recorder->out,
            "%s,%u,%d,%d,%d%s%s\n",
            event_name,
            (unsigned)(xevent->u.keyButtonPointer.time - recorder->start_time),
            xevent->u.keyButtonPointer.rootX,
            xevent->u.keyButtonPointer.rootY,
            detail,
            comment ? " # " : "",
            comment ? comment : "");
}

static void
xrecord_callback (XPointer              closure,
                  XRecordInterceptData *recorded_data)
{
    GbbEventRecorder *recorder = (GbbEventRecorder *)closure;

    if (recorded_data->category == XRecordStartOfData)
        recorder->start_time = recorded_data->server_time;
    else if (recorded_data->category == XRecordFromServer) {
        int pos = 0;
        while (pos < recorded_data->data_len) {
            xEvent *xevent = (xEvent *)(recorded_data->data + pos);

            switch (xevent->u.u.type) {
            case KeyPress:
            {
                int key = xevent->u.u.detail - 8;
                void *detailp = GUINT_TO_POINTER(key);

                if (g_list_find(recorder->pressed_keys, GUINT_TO_POINTER(KEY_LEFTMETA)) &&
                    key == KEY_Q)
                {
                    GList *l;
                    for (l = recorder->pressed_keys; l; l = l->next)
                        dump_event(recorder, "KeyRelease", xevent, GPOINTER_TO_UINT(l->data));
                    for (l = recorder->pressed_buttons; l; l = l->next)
                        dump_event(recorder, "ButtonRelease", xevent, GPOINTER_TO_UINT(l->data));

                    XRecordDisableContext(recorder->control_display, recorder->context);
                    XFlush(recorder->control_display);
                }

                if (g_list_find(recorder->pressed_keys, detailp))
                    goto next;
                recorder->pressed_keys = g_list_prepend(recorder->pressed_keys, detailp);
                dump_event(recorder, "KeyPress", xevent, key);
                break;
            }
            case KeyRelease:
            {
                int key = xevent->u.u.detail - 8;
                void *detailp = GUINT_TO_POINTER(key);

                if (!g_list_find(recorder->pressed_keys, detailp))
                    goto next;
                recorder->pressed_keys = g_list_remove(recorder->pressed_keys, detailp);
                dump_event(recorder, "KeyRelease", xevent, key);
                break;
            }
            case ButtonPress:
            {
                int button = xevent->u.u.detail;
                void *detailp = GUINT_TO_POINTER(button);

                if (button == 4 || button == 5) {
                    dump_event(recorder, "Wheel", xevent, button == 4 ? 1 : -1);
                    goto next;
                }

                if (g_list_find(recorder->pressed_buttons, detailp))
                    goto next;
                recorder->pressed_buttons = g_list_prepend(recorder->pressed_buttons, detailp);
                dump_event(recorder, "ButtonPress", xevent, button);
                break;
            }
            case ButtonRelease:
            {
                int button = xevent->u.u.detail;
                void *detailp = GUINT_TO_POINTER(button);

                if (button == 4 || button == 5)
                    goto next;

                if (!g_list_find(recorder->pressed_buttons, detailp))
                    goto next;
                recorder->pressed_buttons = g_list_remove(recorder->pressed_buttons, detailp);
                dump_event(recorder, "ButtonRelease", xevent, button);
                break;
            }
            case MotionNotify:
                dump_event(recorder, "MotionNotify", xevent, 0);
                break;
            default:
                return;
            }
        next:
            pos += sizeof(xEvent);
        }
    }
}

void
gbb_event_recorder_record(GbbEventRecorder *recorder)
{
    XRecordEnableContext(recorder->data_display,
                         recorder->context,
                         xrecord_callback,
                         (XPointer)recorder);
}

GbbEventRecorder *
gbb_event_recorder_new(Display    *display,
                       const char *filename)
{
    GbbEventRecorder *recorder;

    XRecordRange range = { { 0, 0 }, };
    XRecordRange *ranges = &range;

    int record_major, record_minor;
    XRecordClientSpec client_spec = XRecordAllClients;

    recorder = g_slice_new0(GbbEventRecorder);

    if (!XRecordQueryVersion(display, &record_major, &record_minor))
        die("Record extension is not present\n");

    recorder->control_display = display;
    recorder->data_display = XOpenDisplay(DisplayString(recorder->control_display));
    if (!recorder->data_display)
        die("Can't open X display %s", XDisplayName(DisplayString(recorder->control_display)));

    range.device_events.first = KeyPress;
    range.device_events.last = MotionNotify;
    recorder->context = XRecordCreateContext(recorder->control_display,
                                             XRecordFromServerTime,
                                             &client_spec, 1, &ranges, 1);
    /* Make sure the context is visible to the data display */
    XSync(recorder->control_display, False);


    recorder->filename = g_strdup(filename);

    if (filename) {
        recorder->out = fopen(filename, "r");
        if (!recorder->out)
            die_errno("Can't open output file");
    } else {
        recorder->out = stdout;
    }

    return recorder;
}

void
gbb_event_recorder_free(GbbEventRecorder *recorder)
{
    XRecordFreeContext(recorder->control_display,
                       recorder->context);
    XCloseDisplay(recorder->data_display);

    if (recorder->filename) {
        fclose(recorder->out);
        g_free(recorder->filename);
    }
}
