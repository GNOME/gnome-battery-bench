#include "util-sysfs.h"

#define _ISOC99_SOURCE //for NAN
#include <math.h>

char *
sysfs_read_string_cached(GUdevDevice *device, const char *name)
{
    const char *value;

    value = g_udev_device_get_sysfs_attr(device, name);
    if (value == NULL) {
        value = "Unknown";
    }

    return g_strdup(value);
}

gboolean
sysfs_read_guint64(GUdevDevice *device, const char *name, guint64 *res)
{
    g_autofree char *buffer = NULL;
    char filename[PATH_MAX];
    const char *path;
    guint64 value;
    gboolean ok;
    char *end;

    path = g_udev_device_get_sysfs_path(device);

    g_snprintf(filename, sizeof(filename), "%s/%s", path, name);
    ok = g_file_get_contents(filename, &buffer, NULL, NULL);
    if (!ok) {
        return FALSE;
    }

    value = g_ascii_strtoull(buffer, &end, 0);
    if (end == buffer) {
        return FALSE;
    }

    *res = value;
    return TRUE;
}

double
sysfs_read_double_scaled(GUdevDevice *device, const char *name)
{
    guint64 value;
    gboolean ok;

    ok = sysfs_read_guint64(device, name, &value);
    if (!ok) {
        return NAN;
    }

    return value / 1000000.;
}
