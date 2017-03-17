#ifndef __POWER_SUPPLY__
#define __POWER_SUPPLY__

#include <glib-object.h>

#define GBB_TYPE_BATTERY gbb_battery_get_type()
G_DECLARE_FINAL_TYPE(GbbBattery, gbb_battery, GBB, BATTERY, GObject)

GList *     gbb_battery_discover    (void);
double      gbb_battery_poll        (GbbBattery *);
#endif
