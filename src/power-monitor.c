/* -*- mode: C; c-file-style: "stroustrup"; indent-tabs-mode: nil; -*- */

#include <stdlib.h>

#include <gio/gio.h>

#include "power-monitor.h"

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

typedef struct {
} BatteryState;

typedef struct  {
    GFile *directory;
    double energy_now;
    double energy_full;
    double energy_full_design;
    double charge_now;
    double charge_full;
    double charge_full_design;
    double capacity_now;
} Battery;

typedef struct  {
    GFile *directory;
    gboolean online;
} Adapter;

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
gbb_power_state_get_percent (GbbPowerState *state)
{
    if (state->energy_full >= 0)
        return 100 * state->energy_now / state->energy_full;
    else if (state->charge_full >= 0)
        return 100 * state->charge_now / state->charge_full;
    else if (state->capacity_now >= 0)
        return 100 * state->capacity_now;
    else
        return -1;
}

static gboolean
gbb_power_state_equal(GbbPowerState *a,
                      GbbPowerState *b)
{
    return (a->online == b->online &&
            a->energy_now == b->energy_now &&
            a->energy_full == b->energy_full &&
            a->energy_full_design == b->energy_full_design &&
            a->charge_now == b->charge_now &&
            a->charge_full == b->charge_full &&
            a->charge_full_design == b->charge_full_design &&
            a->capacity_now == b->capacity_now);
}

void
gbb_power_statistics_free (GbbPowerStatistics *statistics)
{
    g_slice_free(GbbPowerStatistics, statistics);
}

static char *
get_file_contents_string (GFile        *directory,
                          char         *child,
                          GCancellable *cancellable,
                          GError      **error)
{
    GFile *file = g_file_get_child(directory, child);
    char *contents;
    if (!g_file_load_contents(file, cancellable, &contents, NULL, NULL, error))
        contents = NULL;

    g_object_unref (file);
    return contents;
}

static gboolean
get_file_contents_int (GFile        *directory,
                       char         *child,
                       int          *result,
                       GCancellable *cancellable,
                       GError      **error)
{
    char *contents = get_file_contents_string(directory, child, cancellable, error);
    if (contents) {
        *result = atoi (contents);
        g_free (contents);
        return TRUE;
    } else {
        return FALSE;
    }
}

static gboolean
get_file_contents_double (GFile        *directory,
                          char         *child,
                          double       *result,
                          GCancellable *cancellable,
                          GError      **error)
{
    int result_int;
    if (get_file_contents_int (directory, child, &result_int, cancellable, error)) {
        *result = result_int / 1000000.;
        return TRUE;
    } else {
        return FALSE;
    }
}

static void
battery_poll(Battery *battery)
{
    battery->energy_now = -1.0;
    battery->energy_full = -1.0;
    battery->energy_full_design = -1.0;
    battery->charge_now = -1.0;
    battery->charge_full = -1.0;
    battery->charge_full_design = -1.0;
    battery->capacity_now = -1.0;

    get_file_contents_double (battery->directory, "energy_now", &battery->energy_now, NULL, NULL);
    if (battery->energy_now >= 0) {
        GError *error = NULL;
        get_file_contents_double (battery->directory, "energy_full", &battery->energy_full, NULL, NULL);
        get_file_contents_double (battery->directory, "energy_full_design", &battery->energy_full_design, NULL, &error);
        return;
    }
    get_file_contents_double (battery->directory, "charge_now", &battery->charge_now, NULL, NULL);
    if (battery->charge_now >= 0) {
        get_file_contents_double (battery->directory, "charge_full", &battery->charge_full, NULL, NULL);
        get_file_contents_double (battery->directory, "charge_full_design", &battery->charge_full_design, NULL, NULL);
        return;
    }

    int capacity;
    if (get_file_contents_int (battery->directory, "capacity_now", &capacity, NULL, NULL))
        battery->capacity_now = capacity / 100.;
}

static Battery *
battery_new (GFile *directory)
{
    Battery *battery = g_slice_new0(Battery);
    battery->directory = g_object_ref(directory);
    battery_poll(battery);

    return battery;
}

static void
battery_free (Battery *battery)
{
    g_object_unref(battery->directory);
    g_slice_free(Battery, battery);
}

static void
adapter_poll(Adapter *adapter)
{
    int online;

    if (get_file_contents_int (adapter->directory, "online", &online, NULL, NULL))
        adapter->online = online != 0;
    else
        adapter->online = FALSE;
}

static Adapter *
adapter_new(GFile *directory)
{
    Adapter *adapter = g_slice_new0(Adapter);
    adapter->directory = g_object_ref(directory);
    adapter_poll(adapter);

    return adapter;
}

static void
adapter_free (Adapter *adapter)
{
    g_object_unref(adapter->directory);
    g_slice_free(Adapter, adapter);
}

static gboolean
find_power_supplies(GbbPowerMonitor *monitor,
                    GCancellable *cancellable,
                    GError      **error)
{
    GFile *file = g_file_new_for_path ("/sys/class/power_supply");
    GFileEnumerator *enumerator = NULL;

    enumerator = g_file_enumerate_children (file,
                                            "standard::name,standard::type",
                                            G_FILE_QUERY_INFO_NONE,
                                            cancellable, error);
    if (!enumerator)
        goto out;

    while (*error == NULL) {
        GFileInfo *info = g_file_enumerator_next_file (enumerator, cancellable, error);
        GFile *child = NULL;
        if (*error != NULL)
            goto out;
        else if (!info)
            break;

        if (g_file_info_get_file_type (info) != G_FILE_TYPE_DIRECTORY)
            goto next;

        child = g_file_enumerator_get_child (enumerator, info);

        const char *basename = g_file_info_get_name (info);
        if (g_str_has_prefix (basename, "BAT"))
            monitor->batteries = g_list_prepend (monitor->batteries, battery_new (child));
        else if (g_str_has_prefix (basename, "AC"))
            monitor->adapters = g_list_prepend (monitor->adapters, adapter_new (child));
    next:
        g_clear_object (&child);
        g_clear_object (&info);
    }

out:
    g_clear_object (&file);
    g_clear_object (&enumerator);

    return *error == NULL;
}

static void
gbb_power_monitor_finalize(GObject *object)
{
    GbbPowerMonitor *monitor = GBB_POWER_MONITOR(object);

    g_list_foreach(monitor->batteries, (GFunc)battery_free, NULL);
    g_list_foreach(monitor->adapters, (GFunc)adapter_free, NULL);

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

static GbbPowerState *
read_state(GbbPowerMonitor *monitor,
           GbbPowerState   *state)
{
    GList *l;
    int n_batteries = 0;

    state->time_us = g_get_monotonic_time();

    g_list_foreach (monitor->adapters, (GFunc)adapter_poll, NULL);
    g_list_foreach (monitor->batteries, (GFunc)battery_poll, NULL);

    state->online = FALSE;
    state->energy_now = -1.0;
    state->energy_full = -1.0;
    state->energy_full_design = -1.0;
    state->charge_now = -1.0;
    state->charge_full = -1.0;
    state->charge_full_design = -1.0;
    state->capacity_now = -1.0;

    for (l = monitor->adapters; l; l = l->next) {
        Adapter *adapter = l->data;
        if (adapter->online)
            state->online = TRUE;
    }

    for (l = monitor->batteries; l; l = l->next) {
        Battery *battery = l->data;
        if (battery->energy_now >= 0) {
            add_to (&state->energy_now, battery->energy_now);
            add_to (&state->energy_full, battery->energy_full);

            if (battery->energy_full_design >= 0) {
                if (n_batteries == 0 || state->energy_full_design >= 0)
                    add_to (&state->energy_full_design, battery->energy_full_design);
            } else {
                state->energy_full_design = -1;
            }
        } else if (battery->charge_now >= 0) {
            add_to (&state->charge_now, battery->charge_now);
            add_to (&state->charge_full, battery->charge_full);

            if (battery->charge_full_design >= 0) {
                if (n_batteries == 0 || state->charge_full_design >= 0)
                    add_to (&state->charge_full_design, battery->charge_full_design);
            } else {
                state->charge_full_design = -1;
            }
        } else if (battery->capacity_now >= 0) {
            add_to (&state->capacity_now, battery->capacity_now);
        }

        n_batteries += 1;
    }

    if ((state->energy_now >= 0 ? 1 : 0) +
        (state->charge_now >= 0 ? 1 : 0) +
        (state->capacity_now >= 0 ? 1 : 0) > 1) {
        g_error ("Different batteries have different accounting methods");
    }

    if (state->capacity_now >= 0)
        state->capacity_now /= n_batteries;

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

GbbPowerState *
gbb_power_monitor_get_state (GbbPowerMonitor *monitor)
{
    return g_slice_dup(GbbPowerState, &monitor->current_state);
}

GbbPowerStatistics *
gbb_power_monitor_compute_statistics (GbbPowerMonitor *monitor,
                                      GbbPowerState   *base,
                                      GbbPowerState   *current)
{
    GbbPowerStatistics *statistics = g_slice_new(GbbPowerStatistics);
    statistics->power = -1;
    statistics->current = -1;
    statistics->battery_life = -1;
    statistics->battery_life_design = -1;

    double time_elapsed = (current->time_us - base->time_us) / 1000000.;

    if (current->energy_now >= 0) {
        double energy_used = base->energy_now - current->energy_now;
        if (energy_used > 0) {
            statistics->power = 3600 * (energy_used) / time_elapsed;
            if (base->energy_full >= 0)
                statistics->battery_life = 3600 * base->energy_full / statistics->power;
            if (base->energy_full_design >= 0)
                statistics->battery_life_design = 3600 * base->energy_full_design / statistics->power;
        }
    } else if (current->capacity_now >= 0) {
        double charge_used = base->charge_now - current->charge_now;
        if (charge_used > 0) {
            statistics->current = 3600 * (charge_used) / time_elapsed;
            if (base->charge_full >= 0)
                statistics->battery_life = 3600 * base->charge_full / statistics->current;
            if (base->charge_full_design >= 0)
                statistics->battery_life_design = 3600 * base->charge_full_design / statistics->current;
        }
    } else if (current->capacity_now >= 0) {
        double capacity_used = base->capacity_now - current->capacity_now;
        if (capacity_used > 0)
            statistics->battery_life = 3600 * time_elapsed / capacity_used;
    }

    return statistics;
}
