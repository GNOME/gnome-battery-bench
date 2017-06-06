#ifndef __UTIL_SYSFS_H__
#define __UTIL_SYSFS_H__

#include <glib.h>
#include <gudev/gudev.h>

char *   sysfs_read_string_cached      (GUdevDevice *device,
					const char  *name);
gboolean sysfs_read_guint64            (GUdevDevice *device,
					const char  *name,
					guint64     *res);
double   sysfs_read_double_scaled      (GUdevDevice *device,
					const char  *name);

#endif /* __UTIL_SYSFS_H__ */
