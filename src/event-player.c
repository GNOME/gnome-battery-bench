/* -*- mode: C; c-file-style: "stroustrup"; indent-tabs-mode: nil; -*- */

#include <errno.h>
#include <string.h>
#include <fcntl.h>

#include <gio/gio.h>
#include <gio/gunixinputstream.h>

#include <libevdev/libevdev.h>
#include <libevdev/libevdev-uinput.h>

#include "event-player.h"
#include "util.h"

enum {
    READY,
    FINISHED,
    LAST_SIGNAL
};

static guint signals[LAST_SIGNAL];

G_DEFINE_TYPE(GbbEventPlayer, gbb_event_player, G_TYPE_OBJECT)

static void
gbb_event_player_finalize(GObject *object)
{
    GbbEventPlayer *player = GBB_EVENT_PLAYER(object);

    g_free(player->keyboard_device_node);
    g_free(player->mouse_device_node);

    G_OBJECT_CLASS(gbb_event_player_parent_class)->finalize(object);
}

static void
gbb_event_player_init(GbbEventPlayer *player)
{
}

static void
gbb_event_player_class_init(GbbEventPlayerClass *player_class)
{
    GObjectClass *gobject_class = G_OBJECT_CLASS (player_class);

    gobject_class->finalize = gbb_event_player_finalize;

    signals[READY] =
        g_signal_new ("ready",
                      GBB_TYPE_EVENT_PLAYER,
                      G_SIGNAL_RUN_LAST,
                      0,
                      NULL, NULL, NULL,
                      G_TYPE_NONE, 0);
    signals[FINISHED] =
        g_signal_new ("finished",
                      GBB_TYPE_EVENT_PLAYER,
                      G_SIGNAL_RUN_LAST,
                      0,
                      NULL, NULL, NULL,
                      G_TYPE_NONE, 0);
}

gboolean
gbb_event_player_is_ready(GbbEventPlayer *player)
{
    return player->ready;
}

const char *
gbb_event_player_get_keyboard_device_node(GbbEventPlayer *player)
{
    return player->keyboard_device_node;
}

const char *
gbb_event_player_get_mouse_device_node(GbbEventPlayer *player)
{
    return player->mouse_device_node;
}

void
gbb_event_player_play_fd(GbbEventPlayer *player,
                         int             fd)
{
    GBB_EVENT_PLAYER_GET_CLASS(player)->play_fd(player, fd);
}

void
gbb_event_player_play_file(GbbEventPlayer *player,
                           const char     *filename)
{
    int fd = open(filename, O_RDONLY);
    if (fd == -1)
        die_errno("Can't open '%s'", filename);

    gbb_event_player_play_fd(player, fd);
}

void
gbb_event_player_stop(GbbEventPlayer *player)
{
    GBB_EVENT_PLAYER_GET_CLASS(player)->stop(player);
}

void
gbb_event_player_set_ready(GbbEventPlayer *player,
                           const char     *keyboard_device_node,
                           const char     *mouse_device_node)
{
    player->ready = TRUE;
    player->keyboard_device_node = g_strdup(keyboard_device_node);
    player->mouse_device_node = g_strdup(mouse_device_node);

    g_signal_emit(player, signals[READY], 0);
}

void
gbb_event_player_finished(GbbEventPlayer *player)
{
    g_signal_emit(player, signals[FINISHED], 0);
}
