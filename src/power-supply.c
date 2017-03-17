/* -*- mode: C; c-file-style: "stroustrup"; indent-tabs-mode: nil; -*- */

#include <glib.h>
#include <gudev/gudev.h>

#define _ISOC99_SOURCE //for NAN
#include <math.h>
#include <limits.h>

#include "power-supply.h"

struct _GbbBattery {
    GObject parent;

    GUdevDevice *udevice;
    char *vendor;
    char *model;

    double voltage_desgin;

    double energy;
    double energy_full;
    double energy_full_design;
    gboolean use_charge;
};

enum {
    PROP_BAT_0,
    PROP_UDEV_DEVICE,

    PROP_VENDOR,
    PROP_MODEL,

    PROP_VOLTAGE_DESIGN,

    PROP_ENERGY,
    PROP_ENERGY_FULL,
    PROP_ENERGY_FULL_DESIGN,

    PROP_BAT_LAST
};

static GParamSpec *battery_props[PROP_BAT_LAST] = { NULL, };

G_DEFINE_TYPE(GbbBattery, gbb_battery, G_TYPE_OBJECT);


static char *   sysfs_read_string_cached             (GUdevDevice *device,
                                               const char  *name);
static double   sysfs_read_double_scaled      (GUdevDevice *device,
                                               const char  *name);

static void     voltage_design_initialize     (GbbBattery *battery);
static void     energy_design_initialize      (GbbBattery *battery);

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

    case PROP_VOLTAGE_DESIGN:
        g_value_set_double(value, bat->voltage_desgin);
        break;

    case PROP_ENERGY:
        g_value_set_double(value, bat->energy);
        break;

    case PROP_ENERGY_FULL:
        g_value_set_double(value, bat->energy_full);
        break;

    case PROP_ENERGY_FULL_DESIGN:
        g_value_set_double(value, bat->energy_full_design);
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

    bat->vendor = sysfs_read_string_cached(device, "manufacturer");
    bat->model = sysfs_read_string_cached(device, "model_name");

    voltage_design_initialize(bat);
    energy_design_initialize(bat);

    gbb_battery_poll(bat);

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

    battery_props[PROP_VOLTAGE_DESIGN] =
        g_param_spec_double("voltage-design",
                            NULL, NULL,
                            0, G_MAXDOUBLE, 0,
                            G_PARAM_READABLE |
                            G_PARAM_STATIC_NAME);

    battery_props[PROP_ENERGY] =
        g_param_spec_double("energy",
                            NULL, NULL,
                            0, G_MAXDOUBLE, 0,
                            G_PARAM_READABLE |
                            G_PARAM_STATIC_NAME);

    battery_props[PROP_ENERGY_FULL] =
        g_param_spec_double("energy-full",
                            NULL, NULL,
                            0, G_MAXDOUBLE, 0,
                            G_PARAM_READABLE |
                            G_PARAM_STATIC_NAME);

    battery_props[PROP_ENERGY_FULL_DESIGN] =
        g_param_spec_double("energy-full-design",
                            NULL, NULL,
                            0, G_MAXDOUBLE, 0,
                            G_PARAM_READABLE |
                            G_PARAM_STATIC_NAME);

    g_object_class_install_properties(gobject_class,
                                      PROP_BAT_LAST,
                                      battery_props);
}


static char *
sysfs_read_string_cached(GUdevDevice *device, const char *name)
{
    const char *value;

    value = g_udev_device_get_sysfs_attr(device, name);
    if (value == NULL) {
        value = "Unknown";
    }

    return g_strdup(value);
}

static double
sysfs_read_double_scaled(GUdevDevice *device, const char *name)
{
    g_autofree char *buffer = NULL;
    char filename[PATH_MAX];
    const char *path;
    guint64 value;
    double dv;
    gboolean ok;
    char *end;

    path = g_udev_device_get_sysfs_path(device);

    g_snprintf(filename, sizeof(filename), "%s/%s", path, name);
    ok = g_file_get_contents(filename, &buffer, NULL, NULL);
    if (!ok) {
        return NAN;
    }

    value = g_ascii_strtoull(buffer, &end, 10);
    if (end != buffer) {
        dv = value / 1000000.;
    } else {
        dv = NAN;
    }

    return dv;
}

static const char *voltage_sources[] = {
    "voltage_min_design",
    "voltage_max_design",
    "voltage_present",
    "voltage_now",
    NULL
};

static void
voltage_design_initialize (GbbBattery *bat)
{
    const char **source;

    for (source = voltage_sources; *source != NULL; source++) {
        const char *name = *source;
        double val = sysfs_read_double_scaled(bat->udevice, name);

        if (val > 1.0) {
            g_debug("Using '%s' as design voltage", name);
            bat->voltage_desgin = val;
            return;
        }
    }

    g_warning("Could not get design voltage, estimating 12V.");
    bat->voltage_desgin = 12;
}

static void
energy_design_initialize(GbbBattery *bat)
{
    GUdevDevice *dev = bat->udevice;
    double val;

    val = sysfs_read_double_scaled(dev, "energy_now");
    if (val > 1.0f) {
        val = sysfs_read_double_scaled(dev, "energy_full");
        bat->energy_full = val;

        val = sysfs_read_double_scaled(dev, "energy_full_design");
        bat->energy_full_design = val;
        return;
    }

    val = sysfs_read_double_scaled(dev, "charge_now");
    if (val > 1.0f) {
        const double voltage_design = bat->voltage_desgin;
        val = sysfs_read_double_scaled(dev, "charge_full");
        bat->energy_full = val * voltage_design;

        val = sysfs_read_double_scaled(dev, "charge_full_design");
        bat->energy_full_design = val * voltage_design;

        bat->use_charge = TRUE;
    }

    if (bat->energy_full < 1.0f ||
        bat->energy_full_design < 1.0f) {
        /* We actually should report that and give up working at all */
        g_warning("Could not get energy full (design) for battery");
    }
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

double
gbb_battery_poll(GbbBattery *bat)
{
    GUdevDevice *dev = bat->udevice;
    double new_value;

    if (bat->use_charge) {
        new_value = sysfs_read_double_scaled(dev, "charge_now");
        new_value *= bat->voltage_desgin;
    } else {
        new_value = sysfs_read_double_scaled(dev, "energy_now");
    }

    bat->energy = new_value;
    return new_value;
}
