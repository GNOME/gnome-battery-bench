/* -*- mode: C; c-file-style: "stroustrup"; indent-tabs-mode: nil; -*- */

#include <errno.h>
#include <stdlib.h>
#include <string.h>
#include <fcntl.h>

#include <gio/gio.h>
#include <gio/gunixinputstream.h>

#include <libevdev/libevdev.h>
#include <libevdev/libevdev-uinput.h>

#include "event-log.h"
#include "evdev-player.h"
#include "util.h"

typedef struct _GbbEvdevPlayerClass GbbEvdevPlayerClass;

static void queue_event(GbbEvdevPlayer *player);

struct _GbbEvdevPlayer {
    GbbEventPlayer parent;

    char *filename;
    gint64 start_time;
    int uinput_fd_keyboard;
    struct libevdev_uinput *uidev_keyboard;
    int uinput_fd_mouse;
    struct libevdev_uinput *uidev_mouse;
    GDataInputStream *input;

    guint ready_timeout;
    gboolean ready;

    GbbEvent *next_event;
    guint next_event_timeout;
};

struct _GbbEvdevPlayerClass {
    GbbEvdevPlayer parent;

    GObjectClass parent_class;
};

G_DEFINE_TYPE(GbbEvdevPlayer, gbb_evdev_player, GBB_TYPE_EVENT_PLAYER)

static void
write_event(const struct libevdev_uinput *uinput_dev,
            unsigned int type,
            unsigned int code,
            int value)
{
    int rc = libevdev_uinput_write_event(uinput_dev, type, code, value);
    if (rc != 0)
        die("Can't write event (%u %u %i): %s", type, code, value, strerror(-rc));
}

static gboolean
next_event_timeout(void *data)
{
    GbbEvdevPlayer *player = data;
    GbbEvent *event = player->next_event;

    player->next_event = NULL;
    player->next_event_timeout = 0;

    if (!event) {
        gbb_event_player_stop(GBB_EVENT_PLAYER(player));
        return FALSE;
    }

    if (strcmp (event->name, "KeyPress") == 0) {
        write_event(player->uidev_keyboard, EV_KEY, event->detail, 1);
        write_event(player->uidev_keyboard, EV_SYN, SYN_REPORT, 0);
    } else if (strcmp (event->name, "KeyRelease") == 0) {
        write_event(player->uidev_keyboard, EV_KEY, event->detail, 0);
        write_event(player->uidev_keyboard, EV_SYN, SYN_REPORT, 0);
    } else if (strcmp (event->name, "ButtonPress") == 0) {
        int button = event->detail == 1 ? BTN_LEFT : (event->detail == 2 ? BTN_MIDDLE : BTN_RIGHT);

        write_event(player->uidev_mouse, EV_KEY, button, 1);
        write_event(player->uidev_mouse, EV_SYN, SYN_REPORT, 0);
    } else if (strcmp (event->name, "ButtonRelease") == 0) {
        int button = event->detail == 1 ? BTN_LEFT : (event->detail == 2 ? BTN_MIDDLE : BTN_RIGHT);

        write_event(player->uidev_mouse, EV_KEY, button, 0);
        write_event(player->uidev_mouse, EV_SYN, SYN_REPORT, 0);
    } else if (strcmp (event->name, "Wheel") == 0) {
        write_event(player->uidev_mouse, EV_REL, REL_WHEEL, event->detail);
        write_event(player->uidev_mouse, EV_SYN, SYN_REPORT, 0);
    } else if (strcmp (event->name, "MotionNotify") == 0) {
        write_event(player->uidev_mouse, EV_ABS, ABS_X, event->x_root);
        write_event(player->uidev_mouse, EV_ABS, ABS_Y, event->y_root);
        write_event(player->uidev_mouse, EV_SYN, SYN_REPORT, 0);
    }

    gbb_event_free(event);

    queue_event(player);

    return FALSE;
}

static void
queue_event(GbbEvdevPlayer *player)
{
    GError *error = NULL;

    g_return_if_fail(player->next_event == NULL);

    player->next_event = gbb_event_read(player->input,
                                        NULL, &error);
    if (error)
        die("Error reading event log: %s\n", error->message);

    gint64 remaining;
    if (player->next_event) {
        gint64 now = g_get_monotonic_time();
        gint64 next_event_time = player->start_time + 1000 * player->next_event->time;
        remaining = (next_event_time - now) / 1000;
    } else {
        remaining = 0;
    }

    player->next_event_timeout = g_timeout_add(remaining > 0 ? remaining : 0,
                                               next_event_timeout,
                                               player);
}

static void
gbb_evdev_player_finalize(GObject *object)
{
    GbbEvdevPlayer *player = GBB_EVDEV_PLAYER(object);

    if (player->uinput_fd_keyboard >= 0)
        close(player->uinput_fd_keyboard);
    if (player->uidev_keyboard)
        libevdev_uinput_destroy(player->uidev_keyboard);
    if (player->uinput_fd_mouse >= 0)
        close(player->uinput_fd_keyboard);
    if (player->uidev_mouse)
        libevdev_uinput_destroy(player->uidev_mouse);

    g_clear_object(&player->input);

    if (player->ready_timeout) {
        g_source_remove(player->ready_timeout);
        player->ready_timeout = 0;
    }

    G_OBJECT_CLASS(gbb_evdev_player_parent_class)->finalize(object);
}


static void
gbb_evdev_player_play_fd(GbbEventPlayer *event_player,
                         int             fd)
{
    GbbEvdevPlayer *player = GBB_EVDEV_PLAYER(event_player);
    GInputStream *input_raw;

    input_raw = g_unix_input_stream_new (fd, TRUE);
    player->input = g_data_input_stream_new (input_raw);
    g_object_unref(input_raw);

    player->start_time = g_get_monotonic_time ();
    queue_event(player);
}

static void
gbb_evdev_player_stop(GbbEventPlayer *event_player)
{
    GbbEvdevPlayer *player = GBB_EVDEV_PLAYER(event_player);
    GError *error = NULL;

    g_clear_pointer(&player->next_event, gbb_event_free);

    if (player->next_event_timeout) {
        g_source_remove(player->next_event_timeout);
        player->next_event_timeout = 0;
    }

    if (!g_input_stream_close(G_INPUT_STREAM(player->input), NULL, &error))
        die("Error closing input: %s\n", error->message);

    g_clear_object(&player->input);

    gbb_event_player_finished(GBB_EVENT_PLAYER(player));
}

static void
gbb_evdev_player_init(GbbEvdevPlayer *player)
{
    player->uinput_fd_keyboard = -1;
    player->uinput_fd_mouse = -1;
}

static void
gbb_evdev_player_class_init(GbbEvdevPlayerClass *player_class)
{
    GObjectClass *gobject_class = G_OBJECT_CLASS (player_class);
    gobject_class->finalize = gbb_evdev_player_finalize;

    GbbEventPlayerClass *event_player_class = GBB_EVENT_PLAYER_CLASS (player_class);
    event_player_class->play_fd = gbb_evdev_player_play_fd;
    event_player_class->stop = gbb_evdev_player_stop;
}

static struct libevdev_uinput *
create_uinput(struct libevdev *dev,
              int             *fd_return,
              GError         **error)
{
    /* Since we don't need to access the FD directly, we could use
     * LIBEVDEV_UINPUT_OPEN_MANAGED instead of opening /dev/input directly.
     * However, a bug in libevdev 1.2.2 and earlier means that we don't get the
     * right return code if we use that option - -EBADF is always returned.
     * We open directly so that the error message will be useful.
     */
    int fd = open("/dev/uinput", O_CLOEXEC | O_RDWR);
    if (fd < 0) {
        if (errno == EACCES)
            g_set_error(error, G_IO_ERROR, g_io_error_from_errno(errno),
                        "Need to be root to simulate events");
        else if (errno == ENOENT)
            g_set_error(error, G_IO_ERROR, g_io_error_from_errno(errno),
                        "Can't open /dev/uinput: %s. The kernel may not be compiled with uinput support.", strerror(errno));
        else
            g_set_error(error, G_IO_ERROR, g_io_error_from_errno(errno),
                        "Can't open /dev/uinput: %s", strerror(errno));

        return NULL;
    }

    static struct libevdev_uinput *uinput;
    int rc = libevdev_uinput_create_from_device(dev, fd, &uinput);
    if (rc != 0) {
        g_set_error(error, G_IO_ERROR, g_io_error_from_errno(-rc),
                    "Can't create uinput device: %s", strerror(-rc));
        close(fd);
        return NULL;
    }

    *fd_return = fd;
    return uinput;
}

GbbEvdevPlayer *
gbb_evdev_player_new(const char *name,
                     GError    **error)
{
    GbbEvdevPlayer *player;
    struct libevdev *dev;
    int i;

    player = g_object_new(GBB_TYPE_EVDEV_PLAYER, NULL);

    struct input_absinfo absinfo;
    absinfo.value = 0;
    absinfo.minimum = 0;
    absinfo.maximum = 0;
    absinfo.fuzz = 0;
    absinfo.flat = 0;
    absinfo.resolution = 3; /* 3 units per mm */

    dev = libevdev_new();
    char *keyboard_name = g_strconcat(name, " - simulated keyboard", NULL);
    libevdev_set_name(dev, keyboard_name);
    g_free(keyboard_name);
    for (i = 1; i <= 255 - 8; i++)
        libevdev_enable_event_code(dev, EV_KEY, i, NULL);
    libevdev_enable_event_type(dev, EV_KEY);

    player->uidev_keyboard = create_uinput(dev, &player->uinput_fd_keyboard, error);
    libevdev_free(dev);
    if (!player->uidev_keyboard)
        goto error;

    dev = libevdev_new();
    char *mouse_name = g_strconcat(name, " - simulated mouse", NULL);
    libevdev_set_name(dev, mouse_name);
    g_free(mouse_name);
    libevdev_enable_event_type(dev, EV_KEY);
    libevdev_enable_event_code(dev, EV_KEY, BTN_LEFT, NULL);
    libevdev_enable_event_code(dev, EV_KEY, BTN_RIGHT, NULL);
    libevdev_enable_event_code(dev, EV_KEY, BTN_MIDDLE, NULL);
    absinfo.maximum = 2560;
    libevdev_enable_event_type(dev, EV_ABS);
    libevdev_enable_event_code(dev, EV_ABS, ABS_X, &absinfo);
    absinfo.maximum = 1440;
    libevdev_enable_event_code(dev, EV_ABS, ABS_Y, &absinfo);
    libevdev_enable_event_type(dev, EV_REL);
    libevdev_enable_event_code(dev, EV_REL, REL_WHEEL, NULL);

    player->uidev_mouse = create_uinput(dev, &player->uinput_fd_mouse, error);
    libevdev_free(dev);
    if (!player->uidev_mouse)
        goto error;

    gbb_event_player_set_ready (GBB_EVENT_PLAYER(player),
                                libevdev_uinput_get_devnode(player->uidev_keyboard),
                                libevdev_uinput_get_devnode(player->uidev_mouse));

    return player;

error:
    g_object_unref(player);

    return NULL;
}
