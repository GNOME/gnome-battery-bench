/* -*- mode: C; c-file-style: "stroustrup"; indent-tabs-mode: nil; -*- */

#include <stdlib.h>
#include <string.h>
#include <X11/Xlib.h>
#include <X11/extensions/XInput2.h>
#include <X11/Xatom.h>

#include "util.h"
#include "xinput-wait.h"

typedef struct {
    GbbEventPlayer *player;
    char *display_name;

    char *keyboard_device_node;
    char *mouse_device_node;

    guint ready_connection;

    GCancellable *cancellable;
    gulong cancelled_connection;

    Display *display;
    Window window;
} WaitData;

static void
wait_data_free (WaitData *wait_data)
{
    if (wait_data->cancellable) {
        g_cancellable_disconnect(wait_data->cancellable,
                                 wait_data->cancelled_connection);
        g_object_unref(wait_data->cancellable);
    }

    g_object_unref(wait_data->player);
    g_free(wait_data->display_name);
    g_free(wait_data->keyboard_device_node);
    g_free(wait_data->mouse_device_node);
    g_slice_free(WaitData, wait_data);
}

static gboolean
check_for_devices(WaitData *wait_data)
{
    XIDeviceInfo* devices;
    int n_devices;
    gboolean found_keyboard = FALSE;
    gboolean found_mouse = FALSE;
    Atom device_node_atom = XInternAtom(wait_data->display, 
                                        "Device Node", False);
    int i;

    devices = XIQueryDevice(wait_data->display,
                            XIAllDevices, &n_devices);

    for (i = 0; i < n_devices; i++) {
        Atom type;
        int format;
        gulong num_items, bytes_after;
        guchar *data;

        XIGetProperty(wait_data->display,
                      devices[i].deviceid,
                      device_node_atom,
                      0, G_MAXLONG, False, XA_STRING,
                      &type, &format, &num_items, &bytes_after, &data);

        if (type == XA_STRING) {
            if (format == 8) {
                char *device_node = (char *)data;
                if (strcmp (device_node, wait_data->keyboard_device_node) == 0)
                    found_keyboard = TRUE;
                else if (strcmp (device_node, wait_data->mouse_device_node) == 0)
                    found_mouse = TRUE;
            }
            XFree(data);
        }
    }

    XIFreeDeviceInfo(devices);

    return found_keyboard && found_mouse;
}

static void
do_thread (GTask        *task,
           gpointer      source_object,
           gpointer      task_data,
           GCancellable *cancellable)
{
    WaitData *wait_data = task_data;
    XIEventMask masks[1];
    int opcode, first_event, first_error;
    int major = 2, minor = 2;

    if (!XQueryExtension (wait_data->display,
                          "XInputExtension",
                          &opcode, &first_event, &first_error))
        die("No XInput extension");

    XIQueryVersion (wait_data->display, &major, &minor);
    if (major < 2 || (major == 2 && minor < 2))
        die("Too old XInput version");

    masks[0].deviceid = XIAllDevices;
    masks[0].mask_len = XIMaskLen(XI_LASTEVENT);
    masks[0].mask = g_new0(guchar, masks[0].mask_len);
    XISetMask(masks[0].mask, XI_HierarchyChanged);
    XISelectEvents(wait_data->display, wait_data->window,
                   masks, 1);
    g_free(masks[0].mask);

    if (check_for_devices(wait_data)) {
        g_task_return_boolean(task, TRUE);
        return;
    }

    while (TRUE) {
        XEvent xev;
        XNextEvent(wait_data->display, &xev);
        if (xev.xany.type == DestroyNotify) { /* cancelled */
            g_task_return_boolean(task, FALSE);
            return;
        } else if (xev.xany.type == GenericEvent &&
                   xev.xgeneric.extension == opcode &&
                   xev.xgeneric.evtype == XI_HierarchyChanged) {
            if (check_for_devices(wait_data)) {
                g_task_return_boolean(task, TRUE);
                return;
            }
        }
    }
}

static void
on_thread_done(GObject      *source_object,
               GAsyncResult *result,
               gpointer      user_data)
{
    GTask *task = user_data;
    WaitData *wait_data = g_task_get_task_data(task);
    GTask *subtask = G_TASK(result);
    GError *error = NULL;

    XCloseDisplay(wait_data->display);

    if (!g_task_propagate_boolean(subtask, &error))
        g_task_return_error(task, error);

    g_task_return_boolean(task, TRUE);
}

static void
start_thread(GTask *task)
{
    WaitData *wait_data = g_task_get_task_data(task);

    wait_data->keyboard_device_node = g_strdup(gbb_event_player_get_keyboard_device_node(wait_data->player));
    wait_data->mouse_device_node = g_strdup(gbb_event_player_get_mouse_device_node(wait_data->player));

    wait_data->display = XOpenDisplay(wait_data->display_name);
    if (!wait_data->display)
        die("Can't open X display %s", XDisplayName(wait_data->display_name));

    XSetWindowAttributes attrs;
    attrs.event_mask = StructureNotifyMask;
    wait_data->window = XCreateWindow(wait_data->display,
                                      DefaultRootWindow(wait_data->display),
                                      0, 0, 1, 1, /* x, y, width, height */
                                      0, CopyFromParent, /* border_width, depth */
                                      InputOnly,
                                      CopyFromParent,
                                      CWEventMask, &attrs);

    GTask *subtask = g_task_new(NULL, g_task_get_cancellable(task), on_thread_done, task);
    g_task_set_task_data (subtask, wait_data, NULL);
    g_task_run_in_thread(subtask, do_thread);
    g_object_unref(subtask);
}

static void
on_ready(GbbEventPlayer *player,
         GTask          *task)
{
    WaitData *wait_data = g_task_get_task_data(task);

    if (wait_data->ready_connection) {
        g_signal_handler_disconnect(wait_data->player,
                                    wait_data->ready_connection);
        wait_data->ready_connection = 0;
    }

    start_thread(task);
}

static void
on_cancelled(GCancellable *cancellable,
             GTask        *task)
{
    WaitData *wait_data = g_task_get_task_data(task);

    if (wait_data->ready_connection) {
        g_signal_handler_disconnect(wait_data->player,
                                    wait_data->ready_connection);
        wait_data->ready_connection = 0;
    }

    if (wait_data->window) {
        XDestroyWindow(wait_data->display, wait_data->window);
        wait_data->window = None;
        XFlush(wait_data->display);
    } else {
        g_task_return_error_if_cancelled(task);
    }
}

void
gbb_xinput_wait (GbbEventPlayer      *player,
                 const char          *display_name,
                 GCancellable        *cancellable,
                 GAsyncReadyCallback  callback,
                 gpointer             user_data)
{
    GTask *task = g_task_new(player, cancellable, callback, user_data);
    WaitData *wait_data = g_slice_new0(WaitData);

    wait_data->player = g_object_ref(player);
    wait_data->display_name = g_strdup(display_name);

    g_task_set_task_data (task, wait_data, (GDestroyNotify) wait_data_free);

    if (cancellable) {
        wait_data->cancellable = g_object_ref(cancellable);
        wait_data->cancelled_connection = g_cancellable_connect(cancellable,
                                                                G_CALLBACK(on_cancelled), task, NULL);
    }

    wait_data->ready_connection = g_signal_connect(player, "ready",
                                                   G_CALLBACK(on_ready), task);

    if (gbb_event_player_is_ready (player))
        start_thread(task);
}

gboolean
gbb_xinput_wait_finish (GbbEventPlayer *player,
                        GAsyncResult   *result,
                        GError        **error)
{
    return g_task_propagate_boolean(G_TASK(result), error);
}

