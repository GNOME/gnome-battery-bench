#ifndef __POWER_SUPPLY__
#define __POWER_SUPPLY__

#include <glib-object.h>

G_BEGIN_DECLS

#define GBB_TYPE_POWER_SUPPLY gbb_power_supply_get_type()
G_DECLARE_DERIVABLE_TYPE(GbbPowerSupply, gbb_power_supply, GBB, POWER_SUPPLY, GObject)

struct _GbbPowerSupplyClass
{
  GObjectClass parent_class;

  gpointer padding[13];
};

GList *     gbb_power_supply_discover    (void);

/* ************************************************************************** */

#define GBB_TYPE_BATTERY gbb_battery_get_type()
G_DECLARE_FINAL_TYPE(GbbBattery, gbb_battery, GBB, BATTERY, GbbPowerSupply)


double      gbb_battery_poll        (GbbBattery *);

/* ************************************************************************** */

#define GBB_TYPE_MAINS gbb_mains_get_type()
G_DECLARE_FINAL_TYPE(GbbMains, gbb_mains, GBB, MAINS, GbbPowerSupply)

gboolean    gbb_mains_poll          (GbbMains *);

G_END_DECLS

#endif
