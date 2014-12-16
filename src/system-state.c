/* -*- mode: C; c-file-style: "stroustrup"; indent-tabs-mode: nil; -*- */

#include <gio/gio.h>

#include "system-state.h"
#include "util.h"

typedef struct _GbbSystemState      GbbSystemState;
typedef struct _GbbSystemStateClass GbbSystemStateClass;

struct _GbbSystemState {
    GObject parent;

    GDBusProxy *screen_proxy;
    GDBusProxy *keyboard_proxy;

    int saved_screen_brightness;
    int saved_keyboard_brightness;

    gboolean ready;
};

struct _GbbSystemStateClass {
    GObjectClass parent_class;
};

enum {
    READY,
    LAST_SIGNAL
};

static guint signals[LAST_SIGNAL];

static const gchar gsd_power_introspection_xml[] =
    "<node>"
    "  <interface name='org.gnome.SettingsDaemon.Power.Screen'>"
    "    <property name='Brightness' type='i' access='readwrite'/>"
    "    <method name='StepUp'>"
    "      <arg type='i' name='new_percentage' direction='out'/>"
    "    </method>"
    "    <method name='StepDown'>"
    "      <arg type='i' name='new_percentage' direction='out'/>"
    "    </method>"
    "  </interface>"
    "  <interface name='org.gnome.SettingsDaemon.Power.Keyboard'>"
    "    <property name='Brightness' type='i' access='readwrite'/>"
    "    <method name='StepUp'>"
    "      <arg type='i' name='new_percentage' direction='out'/>"
    "    </method>"
    "    <method name='StepDown'>"
    "      <arg type='i' name='new_percentage' direction='out'/>"
    "    </method>"
    "    <method name='Toggle'>"
    "      <arg type='i' name='new_percentage' direction='out'/>"
    "    </method>"
    "  </interface>"
    "</node>";

G_DEFINE_TYPE(GbbSystemState, gbb_system_state, G_TYPE_OBJECT)

static void
gbb_system_state_finalize(GObject *object)
{
    GbbSystemState *system_state = GBB_SYSTEM_STATE(object);

    g_object_ref(system_state->screen_proxy);
    g_object_ref(system_state->keyboard_proxy);

    G_OBJECT_CLASS(gbb_system_state_parent_class)->finalize(object);
}

static void
system_state_maybe_ready (GbbSystemState *system_state)
{
    if (!system_state->ready && system_state->screen_proxy && system_state->keyboard_proxy) {
        system_state->ready = TRUE;

        g_signal_emit(system_state, signals[READY], 0);
    }
}

static void
on_screen_interface_ready_cb (GObject      *source_object,
                              GAsyncResult *result,
                              gpointer      data)
{
    GError *error = NULL;
    GbbSystemState *system_state = data;

    system_state->screen_proxy = g_dbus_proxy_new_finish(result,
                                                         &error);
    if (system_state->screen_proxy == NULL)
        die("Can't get proxy object for screen brightness: %s", error->message);

    system_state_maybe_ready(system_state);
}

static void
on_keyboard_interface_ready_cb (GObject      *source_object,
                                GAsyncResult *result,
                                gpointer      data)
{
    GError *error = NULL;
    GbbSystemState *system_state = data;

    system_state->keyboard_proxy = g_dbus_proxy_new_finish(result,
                                                           &error);
    if (system_state->keyboard_proxy == NULL)
        die("Can't get proxy object for keyboard brightness: %s", error->message);

    system_state_maybe_ready(system_state);
}

static void
gbb_system_state_init(GbbSystemState *system_state)
{
    GError *error = NULL;
    GDBusNodeInfo *introspection_data = g_dbus_node_info_new_for_xml(gsd_power_introspection_xml,
                                                                     &error);
    if (!introspection_data)
        die("Can't load introspection_data: %s\n", error->message);

    g_dbus_proxy_new_for_bus(G_BUS_TYPE_SESSION,
                             G_DBUS_PROXY_FLAGS_NONE,
                             g_dbus_node_info_lookup_interface(introspection_data,
                                                               "org.gnome.SettingsDaemon.Power.Screen"),
                             "org.gnome.SettingsDaemon.Power",
                             "/org/gnome/SettingsDaemon/Power",
                             "org.gnome.SettingsDaemon.Power.Screen",
                             NULL,
                             on_screen_interface_ready_cb,
                              system_state);
    g_dbus_proxy_new_for_bus(G_BUS_TYPE_SESSION,
                             G_DBUS_PROXY_FLAGS_NONE,
                             g_dbus_node_info_lookup_interface(introspection_data,
                                                               "org.gnome.SettingsDaemon.Power.Keyboard"),
                             "org.gnome.SettingsDaemon.Power",
                             "/org/gnome/SettingsDaemon/Power",
                             "org.gnome.SettingsDaemon.Power.Keyboard",
                             NULL,
                             on_keyboard_interface_ready_cb,
                             system_state);

    g_dbus_node_info_unref(introspection_data);
}

static void
gbb_system_state_class_init(GbbSystemStateClass *monitor_class)
{
    GObjectClass *gobject_class = G_OBJECT_CLASS(monitor_class);

    gobject_class->finalize = gbb_system_state_finalize;

    signals[READY] =
        g_signal_new ("ready",
                      GBB_TYPE_SYSTEM_STATE,
                      G_SIGNAL_RUN_LAST,
                      0,
                      NULL, NULL, NULL,
                      G_TYPE_NONE, 0);
}

GbbSystemState *
gbb_system_state_new(void)
{
    return g_object_new(GBB_TYPE_SYSTEM_STATE, NULL);
}

gboolean
gbb_system_state_is_ready (GbbSystemState *system_state)
{
    return system_state->ready;
}

static void
set_int32_property (GDBusProxy *proxy,
                    const char *interface,
                    const char *name,
                    gint32      value)
{
    GError *error = NULL;
    GVariant *retval = g_dbus_connection_call_sync (g_dbus_proxy_get_connection (proxy),
                                                    g_dbus_proxy_get_name (proxy),
                                                    g_dbus_proxy_get_object_path (proxy),
                                                    "org.freedesktop.DBus.Properties",
                                                    "Set",
                                                    g_variant_new ("(ssv)",
                                                                   interface,
                                                                   name,
                                                                   g_variant_new_int32((int) value)),
                                                    NULL,
                                                    G_DBUS_CALL_FLAGS_NONE,
                                                    -1, NULL, &error);
    if (error) {
        g_warning("Error setting property %s: %s\n", name, error->message);
        g_clear_error(&error);
    }

    if (retval)
        g_variant_unref(retval);
}

void
gbb_system_state_save (GbbSystemState *system_state)
{
    GVariant *variant;

    variant = g_dbus_proxy_get_cached_property(system_state->screen_proxy,
                                               "Brightness");
    system_state->saved_screen_brightness = g_variant_get_int32(variant);
    g_variant_unref(variant);

    variant = g_dbus_proxy_get_cached_property(system_state->keyboard_proxy,
                                               "Brightness");
    system_state->saved_keyboard_brightness = g_variant_get_int32(variant);
    g_variant_unref(variant);
}

void
gbb_system_state_set_brightnesses (GbbSystemState *system_state,
                                   int             screen_brightness,
                                   int             keyboard_brightness)
{
    set_int32_property(system_state->screen_proxy,
                       g_dbus_proxy_get_interface_name(system_state->screen_proxy),
                       "Brightness", screen_brightness);
    set_int32_property(system_state->keyboard_proxy,
                       g_dbus_proxy_get_interface_name(system_state->keyboard_proxy),
                       "Brightness", keyboard_brightness);
}

void
gbb_system_state_restore (GbbSystemState *system_state)
{
    gbb_system_state_set_brightnesses(system_state,
                                      system_state->saved_screen_brightness,
                                      system_state->saved_keyboard_brightness);
}
