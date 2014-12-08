/* -*- mode: C; c-file-style: "stroustrup"; indent-tabs-mode: nil; -*- */

#include "introspection.h"
#include "util.h"

static const char gbb_introspection_xml[] =
    "<node>"
    " <interface name='org.gnome.BatteryBench.Helper'>"
    "   <method name='CreatePlayer'>"
    "     <arg type='s' name='name' direction='in'/>"
    "     <arg type='o' name='path' direction='out'/>"
    "    </method>"
    " </interface>"
    " <interface name='org.gnome.BatteryBench.Player'>"
    "    <property name='KeyboardDeviceNode' type='s' access='read'/>"
    "    <property name='MouseDeviceNode' type='s' access='read'/>"
    "    <method name='Play'>"
    "      <arg type='h' name='eventfd' direction='in'/>"
    "    </method>"
    "    <method name='Stop'>"
    "    </method>"
    "    <method name='Destroy'>"
    "    </method>"
    " </interface>"
    "</node>";

GDBusNodeInfo *
gbb_get_introspection_info(void)
{
    static GDBusNodeInfo *info;

    if (!info) {
        GError *error = NULL;
        info = g_dbus_node_info_new_for_xml(gbb_introspection_xml, &error);
        if (error)
            die("Cannot parse introspection XML: %s\n", error->message);
    }

    return info;
}

GDBusInterfaceInfo *
gbb_get_introspection_interface(const char *interface_name)
{
    return g_dbus_node_info_lookup_interface(gbb_get_introspection_info(),
                                             interface_name);
}
