#ifndef __POWER_SUPPLY__
#define __POWER_SUPPLY__

#include <glib-object.h>

#define GBB_TYPE_POWER_SUPPLY gbb_power_supply_get_type()
G_DECLARE_DERIVABLE_TYPE(GbbPowerSupply, gbb_power_supply, GBB, POWER_SUPPLY, GObject)

struct _GbbPowerSupplyClass
{
  GObjectClass parent_class;

  gpointer padding[13];
};

/* ************************************************************************** */

#define GBB_TYPE_BATTERY gbb_battery_get_type()
G_DECLARE_FINAL_TYPE(GbbBattery, gbb_battery, GBB, BATTERY, GbbPowerSupply)

GList *     gbb_battery_discover    (void);
double      gbb_battery_poll        (GbbBattery *);
#endif
