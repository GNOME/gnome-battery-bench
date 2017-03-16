/* -*- mode: C; c-file-style: "stroustrup"; indent-tabs-mode: nil; -*- */

#include <glib.h>
#include <gudev/gudev.h>

#include "power-supply.h"

struct _GbbBattery {
    GObject parent;

    GUdevDevice *udevice;
    char *vendor;
    char *model;
};

enum {
    PROP_BAT_0,
    PROP_UDEV_DEVICE,

    PROP_VENDOR,
    PROP_MODEL,

    PROP_BAT_LAST
};

static GParamSpec *battery_props[PROP_BAT_LAST] = { NULL, };

G_DEFINE_TYPE(GbbBattery, gbb_battery, G_TYPE_OBJECT);

static void
gbb_battery_finalize(GObject *obj)
{
    GbbBattery *bat = GBB_BATTERY(obj);

    g_free(bat->vendor);
    g_free(bat->model);

    g_clear_object(&bat->udevice);

    G_OBJECT_CLASS(gbb_battery_parent_class)->finalize(obj);
}

static void
gbb_battery_get_property(GObject    *object,
                         guint       prop_id,
                         GValue     *value,
                         GParamSpec *pspec)
{
    GbbBattery *bat = GBB_BATTERY(object);

    switch (prop_id) {
    case PROP_UDEV_DEVICE:
        g_value_set_object(value, bat->udevice);
        break;

    case PROP_VENDOR:
        g_value_set_string(value, bat->vendor);
        break;

    case PROP_MODEL:
        g_value_set_string(value, bat->model);
        break;
    }
}

static void
gbb_battery_set_property(GObject     *object,
                         guint        prop_id,
                         const GValue *value,
                         GParamSpec   *pspec)
{
    GbbBattery *bat = GBB_BATTERY(object);

    switch (prop_id) {
    case PROP_UDEV_DEVICE:
        bat->udevice = g_value_dup_object(value);
        break;

    default:
        g_assert_not_reached();
    }
}

static void
gbb_battery_constructed(GObject *obj)
{
    GbbBattery *bat = GBB_BATTERY(obj);
    GUdevDevice *device = bat->udevice;
    const gchar *value;

    value = g_udev_device_get_sysfs_attr(device, "manufacturer");
    bat->vendor = g_strdup(value);

    value = g_udev_device_get_sysfs_attr(device, "model_name");
    bat->model = g_strdup(value);

    G_OBJECT_CLASS(gbb_battery_parent_class)->constructed(obj);
}

static void
gbb_battery_init(GbbBattery *bat)
{
}

static void
gbb_battery_class_init(GbbBatteryClass *klass)
{
    GObjectClass *gobject_class = G_OBJECT_CLASS(klass);

    gobject_class->finalize     = gbb_battery_finalize;
    gobject_class->get_property = gbb_battery_get_property;
    gobject_class->set_property = gbb_battery_set_property;
    gobject_class->constructed  = gbb_battery_constructed;

    battery_props[PROP_UDEV_DEVICE] =
        g_param_spec_object("udev-device",
                            NULL, NULL,
                            G_UDEV_TYPE_DEVICE,
                            G_PARAM_READWRITE |
                            G_PARAM_CONSTRUCT_ONLY |
                            G_PARAM_STATIC_NAME);

    battery_props[PROP_VENDOR] =
        g_param_spec_string("vendor",
                            NULL, NULL,
                            "",
                            G_PARAM_READABLE |
                            G_PARAM_STATIC_NAME);

    battery_props[PROP_MODEL] =
        g_param_spec_string("model",
                            NULL, NULL,
                            "",
                            G_PARAM_READABLE |
                            G_PARAM_STATIC_NAME);

    g_object_class_install_properties(gobject_class,
                                      PROP_BAT_LAST,
                                      battery_props);
}



GList *
gbb_battery_discover()
{
    GUdevClient *client;
    GList *devices;
    GList *l;
    GList *supplies = NULL;

    client = g_udev_client_new(NULL);

    devices = g_udev_client_query_by_subsystem(client, "power_supply");

    for (l = devices; l != NULL; l = l->next) {
        GUdevDevice *device = l->data;
        const gchar *dev_type;

        dev_type = g_udev_device_get_sysfs_attr(device,
                                                "type");
        if (dev_type == NULL) {
            continue;
        }

        g_print("Type: %s\n", dev_type);
        if (g_str_equal(dev_type, "Battery")) {
            GObject *bat = g_object_new(GBB_TYPE_BATTERY,
                                        "udev-device", device,
                                        NULL);
            supplies = g_list_prepend(supplies, bat);
        }
    }

    g_list_free_full(devices, (GDestroyNotify) g_object_unref);
    return supplies;
}
