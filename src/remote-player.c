/* -*- mode: C; c-file-style: "stroustrup"; indent-tabs-mode: nil; -*- */

#include <fcntl.h>

#include <gio/gio.h>
#include <gio/gunixfdlist.h>

#include "introspection.h"
#include "remote-player.h"
#include "util.h"

typedef struct _GbbRemotePlayerClass GbbRemotePlayerClass;

struct _GbbRemotePlayer {
    GbbEventPlayer parent;

    char *name;
    GCancellable *cancellable;

    GDBusProxy *player_proxy;
    int pending_fd;
    gboolean started;
};

struct _GbbRemotePlayerClass {
    GbbEventPlayerClass parent_class;
};

G_DEFINE_TYPE(GbbRemotePlayer, gbb_remote_player, GBB_TYPE_EVENT_PLAYER);

static void
close_pending_fd(GbbRemotePlayer *player)
{
    if (player->pending_fd != -1) {
        if (close(player->pending_fd) != 0)
            die_errno("Error closing file");
        player->pending_fd = -1;
    }
}

static void
gbb_remote_player_finalize(GObject *object)
{
    GbbRemotePlayer *player = GBB_REMOTE_PLAYER(object);

    close_pending_fd(player);

    g_cancellable_cancel(player->cancellable);
    g_clear_object(&player->cancellable);

    g_free(player->name);
    g_clear_object(&player->player_proxy);

    G_OBJECT_CLASS(gbb_remote_player_parent_class)->finalize(object);
}

static void
gbb_remote_player_init(GbbRemotePlayer *player)
{
    player->cancellable = g_cancellable_new();
    player->pending_fd = -1;
}

static void
on_play_reply(GObject      *source_object,
              GAsyncResult *result,
              gpointer      user_data)
{
    GError *error = NULL;
    GVariant *retval = g_dbus_proxy_call_with_unix_fd_list_finish(G_DBUS_PROXY(source_object),
                                                                  NULL, result, &error);
    if (error) {
        if (g_error_matches (error, G_IO_ERROR, G_IO_ERROR_CANCELLED)) {
            g_clear_error(&error);
            return;
        }

        g_dbus_error_strip_remote_error(error);
        g_warning("Failed to play:s %s", error->message);
        g_clear_error(&error);
    } else {
        g_variant_unref(retval);
    }

    GbbRemotePlayer *player = user_data;
    player->started = FALSE;
    gbb_event_player_finished(GBB_EVENT_PLAYER(player));
}

static void
remote_player_maybe_start(GbbRemotePlayer *player)
{
    if (gbb_event_player_is_ready(GBB_EVENT_PLAYER(player)) &&
        player->player_proxy && player->pending_fd != -1)
    {
        GUnixFDList *fd_list = g_unix_fd_list_new_from_array(&player->pending_fd, 1);
        player->pending_fd = -1;

        g_dbus_proxy_call_with_unix_fd_list(player->player_proxy, "Play",
                                            g_variant_new("(h)", 0),
                                            G_DBUS_CALL_FLAGS_NONE,
                                            G_MAXINT,
                                            fd_list,
                                            player->cancellable,
                                            on_play_reply,
                                            player);
        g_object_unref(fd_list);
        player->started = TRUE;
    }
}

static void
gbb_remote_player_play_fd(GbbEventPlayer *event_player,
                          int             fd)
{
    GbbRemotePlayer *player = GBB_REMOTE_PLAYER(event_player);

    if (player->pending_fd != -1 || player->started) {
        g_critical("Player is already playing");
        return;
    }

    player->pending_fd = fd;

    remote_player_maybe_start(player);
}

static void
gbb_remote_player_stop(GbbEventPlayer *event_player)
{
    GbbRemotePlayer *player = GBB_REMOTE_PLAYER(event_player);

    if (player->pending_fd != -1) {
        close_pending_fd(player);
    } else {
        GError *error = NULL;

        GVariant *retval = g_dbus_proxy_call_sync(player->player_proxy,
                                                  "Stop",
                                                  NULL,
                                                  G_DBUS_CALL_FLAGS_NONE,
                                                  -1,
                                                  NULL,
                                                  &error);
        if (error) {
            g_warning("Error stopping remote player: %s\n", error->message);
            g_clear_error(&error);
        } else {
            g_variant_unref(retval);
        }
    }
}

static void
gbb_remote_player_class_init(GbbRemotePlayerClass *player_class)
{
    GObjectClass *gobject_class = G_OBJECT_CLASS (player_class);
    gobject_class->finalize = gbb_remote_player_finalize;

    GbbEventPlayerClass *event_player_class = GBB_EVENT_PLAYER_CLASS (player_class);
    event_player_class->play_fd = gbb_remote_player_play_fd;
    event_player_class->stop = gbb_remote_player_stop;

}

static void
on_player_proxy_ready_cb(GObject      *source_object,
                         GAsyncResult *result,
                         gpointer      data)
{
    GError *error = NULL;

    GDBusProxy *player_proxy = g_dbus_proxy_new_finish(result,
                                                       &error);
    if (error) {
        if (g_error_matches (error, G_IO_ERROR, G_IO_ERROR_CANCELLED)) {
            g_clear_error(&error);
            return;
        }

        die("Can't get proxy object for Player object %s", error->message);
    }

    GbbRemotePlayer *player = data;

    player->player_proxy = player_proxy;

    GVariant *keyboard_node_variant = g_dbus_proxy_get_cached_property(player->player_proxy,
                                                                       "KeyboardDeviceNode");
    GVariant *mouse_node_variant = g_dbus_proxy_get_cached_property(player->player_proxy,
                                                                    "MouseDeviceNode");
    gbb_event_player_set_ready(GBB_EVENT_PLAYER(player),
                               g_variant_get_string(keyboard_node_variant, NULL),
                               g_variant_get_string(mouse_node_variant, NULL));

    g_variant_unref(keyboard_node_variant);
    g_variant_unref(mouse_node_variant);

    remote_player_maybe_start(player);
}

static void
on_create_player_reply(GObject      *source_object,
                        GAsyncResult *result,
                        gpointer      data)
{
    GError *error = NULL;
    GVariant *retval = g_dbus_proxy_call_finish (G_DBUS_PROXY(source_object),
                                                 result,
                                                 &error);

    if (error) {
        if (g_error_matches (error, G_IO_ERROR, G_IO_ERROR_CANCELLED)) {
            g_clear_error(&error);
            return;
        }

        g_dbus_error_strip_remote_error(error);
        die("Error calling CreatePlayer: %s", error->message);
    }

    GbbRemotePlayer *player = data;
    const char *player_path = NULL;

    g_variant_get(retval, "(o)", &player_path);

    g_dbus_proxy_new_for_bus(G_BUS_TYPE_SYSTEM,
                             G_DBUS_PROXY_FLAGS_NONE,
                             gbb_get_introspection_interface(GBB_DBUS_INTERFACE_PLAYER),
                             GBB_DBUS_NAME_HELPER,
                             player_path,
                             GBB_DBUS_INTERFACE_PLAYER,
                             player->cancellable,
                             on_player_proxy_ready_cb,
                             player);

    g_variant_unref(retval);
}


static void
on_helper_proxy_ready_cb(GObject      *source_object,
                         GAsyncResult *result,
                         gpointer      data)
{
    GError *error = NULL;
    GDBusProxy *helper_proxy = g_dbus_proxy_new_finish(result,
                                                       &error);
    if (error) {
        if (g_error_matches (error, G_IO_ERROR, G_IO_ERROR_CANCELLED)) {
            g_clear_error(&error);
            return;
        }

        die("Can't get proxy object for gnome-battery-bench-replay-helper %s", error->message);
    }

    GbbRemotePlayer *player = data;

    g_dbus_proxy_call(helper_proxy, "CreatePlayer",
                      g_variant_new("(s)", player->name),
                      G_DBUS_CALL_FLAGS_NONE,
                      -1,
                      player->cancellable,
                      on_create_player_reply,
                      player);
    g_object_unref(helper_proxy);
}


GbbRemotePlayer *
gbb_remote_player_new(const char *name)
{
    GbbRemotePlayer *player = g_object_new(GBB_TYPE_REMOTE_PLAYER, NULL);

    player->name = g_strdup(name);

    g_dbus_proxy_new_for_bus(G_BUS_TYPE_SYSTEM,
                             G_DBUS_PROXY_FLAGS_NONE,
                             gbb_get_introspection_interface(GBB_DBUS_INTERFACE_HELPER),
                             GBB_DBUS_NAME_HELPER,
                             GBB_DBUS_PATH_HELPER,
                             GBB_DBUS_INTERFACE_HELPER,
                             player->cancellable,
                             on_helper_proxy_ready_cb,
                             player);

    return player;
}

