/* -*- mode: C; c-file-style: "stroustrup"; indent-tabs-mode: nil; -*- */

#include <stdlib.h>

#include <gio/gio.h>
#include <gio/gunixfdlist.h>
#include <polkit/polkit.h>

#include "evdev-player.h"
#include "introspection.h"
#include "util.h"

typedef struct _Player Player;

struct _Player {
    GDBusConnection *connection;
    char *name;
    char *path;
    char *creator;
    guint creator_changed_connection;
    guint registration_id;
    GbbEventPlayer *player;

    GDBusMethodInvocation *invocation;
    guint finished_connection;
};

int player_serial = 0;

static void
player_destroy(Player *player)
{
    if (player->invocation) {
        gbb_event_player_stop(player->player);
        g_dbus_method_invocation_return_error(player->invocation,
                                              G_DBUS_ERROR,
                                              G_DBUS_ERROR_FAILED,
                                              "Player destroyed during playback");
        g_signal_handler_disconnect(player->player, player->finished_connection);
    }

    g_clear_object(&player->player);

    g_dbus_connection_signal_unsubscribe(player->connection,
                                         player->creator_changed_connection);

    if (player->registration_id)
        g_dbus_connection_unregister_object(player->connection, player->registration_id);

    g_object_unref(player->connection);
    g_free(player->name);
    g_free(player->path);
    g_free(player->creator);
    g_slice_free(Player, player);
}

static void
on_player_finished(GbbEventPlayer *event_player,
                   Player         *player)
{
    g_signal_handler_disconnect(player->player, player->finished_connection);
    player->finished_connection = 0;

    g_dbus_method_invocation_return_value(player->invocation, NULL);
    player->invocation = NULL;
}

static void
player_handle_method_call(GDBusConnection       *connection,
                          const gchar           *sender,
                          const gchar           *object_path,
                          const gchar           *interface_name,
                          const gchar           *method_name,
                          GVariant              *parameters,
                          GDBusMethodInvocation *invocation,
                          gpointer               user_data)
{
    Player *player = user_data;

    if (g_strcmp0 (method_name, "Play") == 0) {
        gint32 fd_index = -1;

        GDBusMessage *message = g_dbus_method_invocation_get_message(invocation);
        GUnixFDList *fd_list = g_dbus_message_get_unix_fd_list(message);

        if (g_unix_fd_list_get_length(fd_list) != 1) {
            g_dbus_method_invocation_return_error (invocation,
                                                   G_DBUS_ERROR,
                                                   G_DBUS_ERROR_INVALID_ARGS,
                                                   "Exactly one file descriptor should be passed");
        }

        g_variant_get (parameters, "(h)", &fd_index);
        if (fd_index != 0) {
            g_dbus_method_invocation_return_error (invocation,
                                                   G_DBUS_ERROR,
                                                   G_DBUS_ERROR_INVALID_ARGS,
                                                   "Bad file descriptor index %d", fd_index);
        }

        if (player->invocation) {
            g_dbus_method_invocation_return_error (invocation,
                                                   G_DBUS_ERROR,
                                                   G_DBUS_ERROR_FAILED,
                                                   "Player already playing");
            return;
        }

        int n_fds;
        int *fds = g_unix_fd_list_steal_fds (fd_list, &n_fds);
        int fd = fds[0];
        g_free(fds);

        gbb_event_player_play_fd(player->player, fd);

        player->invocation = invocation;
        player->finished_connection = g_signal_connect(player->player, "finished",
                                                       G_CALLBACK(on_player_finished), player);

    } else if (g_strcmp0 (method_name, "Stop") == 0) {
        if (player->invocation == NULL) {
            g_dbus_method_invocation_return_error (invocation,
                                                   G_DBUS_ERROR,
                                                   G_DBUS_ERROR_FAILED,
                                                   "Player stopped");
            return;
        }
        gbb_event_player_stop(player->player);
        g_dbus_method_invocation_return_value(invocation, NULL);
    } else if (g_strcmp0 (method_name, "Destroy") == 0) {
        player_destroy(user_data);
        g_dbus_method_invocation_return_value(invocation, NULL);
    }
}

static GVariant *
player_handle_get_property(GDBusConnection *connection,
                           const gchar     *sender,
                           const gchar     *object_path,
                           const gchar     *interface_name,
                           const gchar     *property_name,
                           GError         **error,
                           gpointer         user_data)
{
    Player *player = user_data;
    GVariant *ret = NULL;

    if (g_strcmp0 (property_name, "KeyboardDeviceNode") == 0) {
        return g_variant_new_string(gbb_event_player_get_keyboard_device_node(player->player));
    } else if (g_strcmp0 (property_name, "MouseDeviceNode") == 0) {
        return g_variant_new_string(gbb_event_player_get_mouse_device_node(player->player));
    }

    return ret;
}

static gboolean
player_handle_set_property(GDBusConnection *connection,
                           const gchar     *sender,
                           const gchar     *object_path,
                           const gchar     *interface_name,
                           const gchar     *property_name,
                           GVariant        *value,
                           GError         **error,
                           gpointer         user_data)
{
    return FALSE;
}

static const GDBusInterfaceVTable player_interface_vtable =
{
    player_handle_method_call,
    player_handle_get_property,
    player_handle_set_property
};

static void
on_name_owner_changed (GDBusConnection *connection,
                       const gchar     *sender_name,
                       const gchar     *object_path,
                       const gchar     *interface_name,
                       const gchar     *signal_name,
                       GVariant        *parameters,
                       gpointer         user_data)
{
    player_destroy(user_data);
}

static void
on_checked_authorization(GObject      *source_object,
                         GAsyncResult *res,
                         gpointer      data)
{
    PolkitAuthority *authority = POLKIT_AUTHORITY(source_object);
    GDBusMethodInvocation *invocation = data;
    GError *error = NULL;

    PolkitAuthorizationResult *result = polkit_authority_check_authorization_finish(authority,
                                                                                    res, &error);
    if (error) {
        g_dbus_method_invocation_return_gerror (invocation, error);
        g_clear_error(&error);
        return;
    }

    if (!polkit_authorization_result_get_is_authorized(result)) {
        g_dbus_method_invocation_return_error (invocation,
                                               G_DBUS_ERROR,
                                               G_DBUS_ERROR_ACCESS_DENIED,
                                               "Not allowed to simulate events");
        g_object_unref(result);
        return;
    }

    g_object_unref(result);

    GVariant *parameters = g_dbus_method_invocation_get_parameters(invocation);
    GDBusConnection *connection = g_dbus_method_invocation_get_connection(invocation);

    const gchar *name;
    g_variant_get (parameters, "(&s)", &name);

    Player *player = g_slice_new0(Player);

    player->connection = g_object_ref(connection);
    player->name = g_strdup(name);
    player->path = g_strdup_printf(GBB_DBUS_PATH_PLAYER_BASE "/%d", ++player_serial);

    player->creator = g_strdup(g_dbus_method_invocation_get_sender(invocation));
    player->creator_changed_connection = g_dbus_connection_signal_subscribe (connection,
                                                                             "org.freedesktop.DBus", /* bus name */
                                                                             "org.freedesktop.DBus", /* interface */
                                                                             "NameOwnerChanged", /* member */
                                                                             "/org/freedesktop/DBus", /* path */
                                                                             player->creator, /* arg0 */
                                                                             G_DBUS_SIGNAL_FLAGS_NONE,
                                                                             on_name_owner_changed,
                                                                             player, NULL);

    GbbEvdevPlayer *evdev_player = gbb_evdev_player_new(player->name, &error);
    if (evdev_player == NULL) {
        g_dbus_method_invocation_take_error(invocation, error);
        player_destroy(player);
        return;
    }
    player->player = GBB_EVENT_PLAYER(evdev_player);

    player->registration_id = g_dbus_connection_register_object(connection,
                                                                player->path,
                                                                gbb_get_introspection_interface(GBB_DBUS_INTERFACE_PLAYER),
                                                                &player_interface_vtable,
                                                                player, NULL,
                                                                &error);
    if (error)
        die("Cannot register player: %s\n", error->message);

    g_dbus_method_invocation_return_value(invocation,
                                          g_variant_new ("(o)", player->path));
}

static void
on_got_polkit_authority(GObject      *source_object,
                        GAsyncResult *res,
                        gpointer      data)
{
    GDBusMethodInvocation *invocation = data;
    GError *error = NULL;

    PolkitAuthority *authority = polkit_authority_get_finish(res, &error);
    if (error) {
        g_dbus_method_invocation_return_gerror (invocation, error);
        g_clear_error(&error);
        return;
    }

    PolkitSubject *subject = polkit_system_bus_name_new(g_dbus_method_invocation_get_sender(invocation));
    polkit_authority_check_authorization(authority,
                                         subject,
                                         "org.gnome.BatteryBench.Helper.SimulateEvents",
                                         NULL, /* PolkitDetails */
                                         POLKIT_CHECK_AUTHORIZATION_FLAGS_ALLOW_USER_INTERACTION,
                                         NULL,
                                         (GAsyncReadyCallback)on_checked_authorization,
                                         invocation);
    g_object_unref(subject);
}

static void
helper_handle_method_call(GDBusConnection       *connection,
                          const gchar           *sender,
                          const gchar           *object_path,
                          const gchar           *interface_name,
                          const gchar           *method_name,
                          GVariant              *parameters,
                          GDBusMethodInvocation *invocation,
                          gpointer               user_data)
{
    if (g_strcmp0 (method_name, "CreatePlayer") == 0) {
        polkit_authority_get_async (NULL, on_got_polkit_authority, invocation);
    }
}

static GVariant *
helper_handle_get_property(GDBusConnection *connection,
                           const gchar     *sender,
                           const gchar     *object_path,
                           const gchar     *interface_name,
                           const gchar     *property_name,
                           GError         **error,
                           gpointer         user_data)
{
    GVariant *ret = NULL;

    return ret;
}

static gboolean
helper_handle_set_property(GDBusConnection *connection,
                           const gchar     *sender,
                           const gchar     *object_path,
                           const gchar     *interface_name,
                           const gchar     *property_name,
                           GVariant        *value,
                           GError         **error,
                           gpointer         user_data)
{
    return FALSE;
}

static const GDBusInterfaceVTable helper_interface_vtable =
{
    helper_handle_method_call,
    helper_handle_get_property,
    helper_handle_set_property
};

static void
on_bus_acquired(GDBusConnection *connection,
                const gchar     *name,
                gpointer         user_data)
{
    GError *error = NULL;

    g_dbus_connection_register_object(connection,
                                      GBB_DBUS_PATH_HELPER,
                                      gbb_get_introspection_interface(GBB_DBUS_INTERFACE_HELPER),
                                      &helper_interface_vtable,
                                      NULL, /* user_data */
                                      NULL, /* user_data_free_func */
                                      &error);
    if (error)
        die("Cannot register object: %s\n", error->message);

}

static void
on_name_acquired(GDBusConnection *connection,
                 const gchar     *name,
                 gpointer         user_data)
{
}

static void
on_name_lost(GDBusConnection *connection,
             const gchar     *name,
             gpointer         user_data)
{
    exit(1);
}

int main(int argc, char **argv)
{
    guint owner_id;
    GMainLoop *loop;
    GError *error = NULL;

    owner_id = g_bus_own_name(G_BUS_TYPE_SYSTEM,
                              GBB_DBUS_NAME_HELPER,
                              G_BUS_NAME_OWNER_FLAGS_NONE,
                              on_bus_acquired,
                              on_name_acquired,
                              on_name_lost,
                              NULL,
                              NULL);
    if (error)
        die("Cannot get bus name: %s\n", error->message);

    loop = g_main_loop_new(NULL, FALSE);
    g_main_loop_run(loop);
    g_bus_unown_name(owner_id);

    return 0;
}
