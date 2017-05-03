/* -*- mode: C; c-file-style: "stroustrup"; indent-tabs-mode: nil; -*- */

#include <stdlib.h>

#include <gio/gio.h>

#include "power-monitor.h"
#include "power-supply.h"

/* Time between reading values out of proc (ms) */
#define UPDATE_FREQUENCY 250

struct _GbbPowerMonitor {
    GObject parent;
    GList *batteries;
    GList *adapters;
    GbbPowerState current_state;
    guint update_timeout;
};

struct _GbbPowerMonitorClass {
    GObjectClass parent_class;
};

enum {
    CHANGED,
    LAST_SIGNAL
};

static guint signals[LAST_SIGNAL];

G_DEFINE_TYPE(GbbPowerMonitor, gbb_power_monitor, G_TYPE_OBJECT)

void
gbb_power_state_free(GbbPowerState   *state)
{
    g_slice_free(GbbPowerState, state);
}

double
gbb_power_state_get_percent (const GbbPowerState *state)
{
    return 100 * state->energy_now / state->energy_full;
}

static gboolean
gbb_power_state_equal(GbbPowerState *a,
                      GbbPowerState *b)
{
    return (a->online == b->online &&
            a->energy_now == b->energy_now &&
            a->energy_full == b->energy_full &&
            a->energy_full_design == b->energy_full_design);
}

void
gbb_power_statistics_free (GbbPowerStatistics *statistics)
{
    g_slice_free(GbbPowerStatistics, statistics);
}


static gboolean
find_power_supplies(GbbPowerMonitor *monitor,
                    GCancellable *cancellable,
                    GError      **error)
{

    GList *supplies;
    GList *l;

    supplies = gbb_power_supply_discover();

    for (l = supplies; l != NULL; l = l->next) {
        if (GBB_IS_BATTERY(l->data)) {
            monitor->batteries = g_list_prepend(monitor->batteries, l->data);
        } else if (GBB_IS_MAINS(l->data)) {
            monitor->adapters = g_list_prepend(monitor->adapters, l->data);
        } else {
            g_assert_not_reached();
        }
    }

    g_list_free(supplies);

    if (monitor->batteries == NULL) {
        g_set_error_literal(error, G_IO_ERROR, G_IO_ERROR_FAILED,
                            "No batteries found!");
    } else if (monitor->adapters == NULL) {
        g_set_error_literal(error, G_IO_ERROR, G_IO_ERROR_FAILED,
                            "No power adapter found!");
    }

    return *error == NULL;
}

static void
gbb_power_monitor_finalize(GObject *object)
{
    GbbPowerMonitor *monitor = GBB_POWER_MONITOR(object);

    g_list_foreach(monitor->batteries, (GFunc)g_object_unref, NULL);
    g_list_foreach(monitor->adapters, (GFunc)g_object_unref, NULL);

    G_OBJECT_CLASS(gbb_power_monitor_parent_class)->finalize(object);
}

static void
gbb_power_monitor_init(GbbPowerMonitor *monitor)
{
}

static void
gbb_power_monitor_class_init(GbbPowerMonitorClass *monitor_class)
{
    GObjectClass *gobject_class = G_OBJECT_CLASS(monitor_class);

    gobject_class->finalize = gbb_power_monitor_finalize;

    signals[CHANGED] =
        g_signal_new ("changed",
                      GBB_TYPE_POWER_MONITOR,
                      G_SIGNAL_RUN_LAST,
                      0,
                      NULL, NULL, NULL,
                      G_TYPE_NONE, 0);
}

static void
add_to (double *total, double  increment)
{
    if (*total >= 0)
        *total += increment;
    else
        *total = increment;
}

static void
gbb_power_state_init(GbbPowerState *state)
{
    state->time_us = 0;
    state->online = FALSE;
    state->energy_now = -1.0;
    state->energy_full = -1.0;
    state->energy_full_design = -1.0;
    state->voltage_now = -1.0;
}

GbbPowerState *
gbb_power_state_new(void)
{
    GbbPowerState *state = g_slice_new(GbbPowerState);
    gbb_power_state_init(state);
    return state;
}

GbbPowerState *
gbb_power_state_copy(const GbbPowerState *state)
{
    return g_slice_dup(GbbPowerState, state);
}

static GbbPowerState *
read_state(GbbPowerMonitor *monitor,
           GbbPowerState   *state)
{
    GList *l;
    int n_batteries = 0;

    gbb_power_state_init(state);
    state->time_us = g_get_monotonic_time();

    for (l = monitor->adapters; l; l = l->next) {
        GbbMains *mains = l->data;
        gboolean online = gbb_mains_poll(mains);

        if (online)
            state->online = TRUE;
    }

    for (l = monitor->batteries; l; l = l->next) {
        GbbBattery *battery = l->data;
        double energy_now = gbb_battery_poll(battery);
        double energy_full = -1.0;
        double energy_full_design = -1.0;

        g_object_get(battery,
                     "energy-full", &energy_full,
                     "energy-full-design", &energy_full_design,
                     NULL);

        add_to (&state->energy_now, energy_now);
        add_to (&state->energy_full, energy_full);

        if (energy_full_design >= 0) {
            if (n_batteries == 0 || state->energy_full_design >= 0)
                add_to (&state->energy_full_design, energy_full_design);
        } else {
            state->energy_full_design = -1;
        }

        state->voltage_now = -1.0;
        n_batteries += 1;
    }

    return state;
}

static gboolean
update_timeout(gpointer data)
{
    GbbPowerMonitor *monitor = data;
    GbbPowerState state;
    read_state(monitor, &state);

    if (!gbb_power_state_equal(&monitor->current_state, &state)) {
        monitor->current_state = state;
        g_signal_emit(monitor, signals[CHANGED], 0);
    }

    return G_SOURCE_CONTINUE;
}

GbbPowerMonitor *
gbb_power_monitor_new(void)
{
    GbbPowerMonitor *monitor = g_object_new(GBB_TYPE_POWER_MONITOR, NULL);
    GError *error = NULL;

    if (!find_power_supplies(monitor, NULL, &error))
        g_error("%s\n", error->message);

    read_state(monitor, &monitor->current_state);
    monitor->update_timeout = g_timeout_add(UPDATE_FREQUENCY, update_timeout, monitor);

    return monitor;
}

const GbbPowerState *
gbb_power_monitor_get_state (GbbPowerMonitor *monitor)
{
    return &monitor->current_state;
}

GbbPowerStatistics *
gbb_power_statistics_compute (const GbbPowerState   *base,
                              const GbbPowerState   *current)
{
    GbbPowerStatistics *statistics = g_slice_new(GbbPowerStatistics);
    statistics->power = -1;
    statistics->current = -1;
    statistics->battery_life = -1;
    statistics->battery_life_design = -1;

    double time_elapsed = (current->time_us - base->time_us) / 1000000.;

    if (time_elapsed < (UPDATE_FREQUENCY / 1000.)) {
        return statistics;
    }

    double energy_used = base->energy_now - current->energy_now;
    if (energy_used > 0) {
        statistics->power = 3600 * (energy_used) / time_elapsed;
        if (base->energy_full >= 0)
            statistics->battery_life = 3600 * base->energy_full / statistics->power;
        if (base->energy_full_design >= 0)
            statistics->battery_life_design = 3600 * base->energy_full_design / statistics->power;
    }

    return statistics;
}
