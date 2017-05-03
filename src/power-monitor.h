#ifndef __POWER_MONITOR_H__
#define __POWER_MONITOR_H__

#include <glib.h>

typedef struct _GbbPowerMonitor      GbbPowerMonitor;
typedef struct _GbbPowerMonitorClass GbbPowerMonitorClass;
typedef struct _GbbPowerState        GbbPowerState;
typedef struct _GbbPowerStatistics   GbbPowerStatistics;

#define GBB_TYPE_POWER_MONITOR         (gbb_power_monitor_get_type ())
#define GBB_POWER_MONITOR(o)           (G_TYPE_CHECK_INSTANCE_CAST ((o), GBB_TYPE_POWER_MONITOR, GbbPowerMonitor))
#define GBB_POWER_MONITOR_CLASS(k)     (G_TYPE_CHECK_CLASS_CAST((k), GBB_TYPE_POWER_MONITOR, GbbPowerMonitorClass))
#define GBB_IS_POWER_MONITOR(o)        (G_TYPE_CHECK_INSTANCE_TYPE ((o), GBB_TYPE_POWER_MONITOR))
#define GBB_IS_POWER_MONITOR_CLASS(k)  (G_TYPE_CHECK_CLASS_TYPE ((k), GBB_TYPE_POWER_MONITOR))
#define GBB_POWER_MONITOR_GET_CLASS(o) (G_TYPE_INSTANCE_GET_CLASS ((o), GBB_TYPE_POWER_MONITOR, GbbPowerMonitorClass))

struct _GbbPowerState {
    gint64 time_us;
    gboolean online;
    double energy_now; /* WH */
    double energy_full;
    double energy_full_design;
    double voltage_now;
};

struct _GbbPowerStatistics {
    /* At most one (posibly none) of these will be set */
    double power; /* W */
    double current; /* A */

    double battery_life;
    double battery_life_design;
};

GType               gbb_power_monitor_get_type(void);

GbbPowerMonitor    *gbb_power_monitor_new        (void);

const GbbPowerState *gbb_power_monitor_get_state (GbbPowerMonitor *monitor);

GbbPowerState      *gbb_power_state_new          (void);
GbbPowerState      *gbb_power_state_copy         (const GbbPowerState   *state);
void                gbb_power_state_free         (GbbPowerState         *state);

double              gbb_power_state_get_percent  (const GbbPowerState   *state);

GbbPowerStatistics *gbb_power_statistics_compute (const GbbPowerState   *base,
                                                  const GbbPowerState   *current);
void                gbb_power_statistics_free    (GbbPowerStatistics *statistics);

#endif /*__POWER_MONITOR_H__ */
