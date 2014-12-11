#ifndef __INTROSPECTION_H__
#define __INTROSPECTION_H__

#include <gio/gio.h>

#define GBB_DBUS_NAME_HELPER "org.gnome.BatteryBench.Helper"
#define GBB_DBUS_PATH_HELPER "/org/gnome/BatteryBench/Helper"
#define GBB_DBUS_PATH_PLAYER_BASE "/org/gnome/BatteryBench/Player"
#define GBB_DBUS_INTERFACE_HELPER "org.gnome.BatteryBench.Helper"
#define GBB_DBUS_INTERFACE_PLAYER "org.gnome.BatteryBench.Player"

GDBusNodeInfo      *gbb_get_introspection_info     (void);
GDBusInterfaceInfo *gbb_get_introspection_interface(const char *interface_name);

#endif /* __INTROSPECTION_H__ */
