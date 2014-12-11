/* -*- mode: C; c-file-style: "stroustrup"; indent-tabs-mode: nil; -*- */

#include <gtk/gtk.h>

#include "application.h"
#include "remote-player.h"
#include "power-monitor.h"
#include "system-state.h"
#include "util.h"

struct _GbbApplication {
    GtkApplication parent;
    GbbPowerMonitor *monitor;
    GbbEventPlayer *player;
    GbbSystemState *system_state;

    GbbPowerState *start_state;
    GbbPowerStatistics *statistics;

    GtkWidget *window;
    GtkWidget *test_combo;
    GtkWidget *start_button;
    GtkWidget *state_label;
    gboolean started;
    char *filename;
};

struct _GbbApplicationClass {
    GtkApplicationClass parent_class;
};

G_DEFINE_TYPE(GbbApplication, gbb_application, GTK_TYPE_APPLICATION)

static void
gbb_application_finalize(GObject *object)
{
}

static void
update_label(GbbApplication *application)
{
    GString *text = g_string_new (NULL);

    GbbPowerState *state = gbb_power_monitor_get_state(application->monitor);
    g_string_append_printf(text, "Online: %s\n", state->online ? "yes": "no");
    if (state->energy_now >= 0) {
        g_string_append_printf(text, "Energy: %g Wh", state->energy_now);
        if (state->energy_full > 0) {
            g_string_append_printf(text, " (%.1f%%)", 100. * state->energy_now / state->energy_full);
        }
        g_string_append(text, "\n");
        if (state->energy_full >= 0)
            g_string_append_printf(text, "Energy full: %.1f Wh\n", state->energy_full);
        if (state->energy_full_design >= 0)
            g_string_append_printf(text, "Energy full (design): %.1f Wh\n", state->energy_full_design);
    }

    if (application->started || application->statistics) {
        GbbPowerStatistics *statistics = application->statistics;

        g_string_append(text, "-------\n");
        if (statistics) {
            if (statistics->power >= 0)
                g_string_append_printf(text, "Power: %.2f W\n", statistics->power);
            if (statistics->current >= 0)
                g_string_append_printf(text, "Current: %.2f A\n", statistics->current);
            if (statistics->battery_life >= 0) {
                int h, m, s;
                break_time(statistics->battery_life, &h, &m, &s);
                g_string_append_printf(text, "Predicted battery life: %.0fs (%d:%02d:%02d)\n",
                                       statistics->battery_life, h, m, s);
            }
            if (statistics->battery_life_design >= 0) {
                int h, m, s;
                break_time(statistics->battery_life_design, &h, &m, &s);
                g_string_append_printf(text, "Predicted battery life (design): %.0fs (%d:%02d:%02d)\n",
                                       statistics->battery_life_design, h, m, s);
            }
        } else if (state->online) {
            g_string_append(text, "Disconnect from AC to begin\n");
        } else {
            g_string_append(text, "Waiting for data...\n");
        }
    }

    gtk_label_set_text(GTK_LABEL(application->state_label), text->str);
    g_string_free(text, TRUE);

    gbb_power_state_free(state);
}

static void
on_power_monitor_changed(GbbPowerMonitor *monitor,
                         GbbApplication  *application)
{
    if (application->started) {
        if (application->start_state == NULL) {
            GbbPowerState *state = gbb_power_monitor_get_state(monitor);
            if (!state->online) {
                application->start_state = state;
                gbb_event_player_play_file(application->player, application->filename);
            } else {
                gbb_power_state_free(state);
            }
        } else {
            if (application->statistics)
                gbb_power_statistics_free(application->statistics);

            GbbPowerState *state = gbb_power_monitor_get_state(monitor);
            application->statistics = gbb_power_monitor_compute_statistics(monitor, application->start_state, state);
            gbb_power_state_free(state);
        }
    }

    update_label(application);
}

static void
on_player_ready(GbbEventPlayer *player,
                GbbApplication *application)
{
    gtk_widget_set_sensitive(application->start_button, TRUE);
    update_label(application);
}

static void
on_player_finished(GbbEventPlayer *player,
                   GbbApplication *application)
{
    if (application->started) {
        gbb_event_player_play_file(player, application->filename);
    }
}

static void
on_start_button_clicked(GtkWidget      *button,
                        GbbApplication *application)
{
    if (application->started) {
        application->started = FALSE;
        g_free(application->filename);
        application->filename = NULL;

        if (application->start_state) {
            gbb_event_player_stop(application->player);
            gbb_power_state_free(application->start_state);
            application->start_state = NULL;
        }

        gbb_system_state_restore(application->system_state);

        g_object_set(G_OBJECT(application->start_button), "label", "Start", NULL);
        gtk_widget_set_sensitive(application->test_combo, TRUE);
    } else {
        gbb_system_state_save(application->system_state);
        gbb_system_state_set_default(application->system_state);
        application->started = TRUE;

        gtk_widget_set_sensitive(application->test_combo, FALSE);
        g_object_set(G_OBJECT(application->start_button), "label", "Stop", NULL);

        const char *test_id = gtk_combo_box_get_active_id(GTK_COMBO_BOX(application->test_combo));
        application->filename = g_strconcat(test_id, ".batterytest", NULL);
    }

    update_label(application);
}

static void
gbb_application_activate (GApplication *app)
{
    GbbApplication *application = GBB_APPLICATION (app);

    if (application->window) {
        gtk_window_present (GTK_WINDOW(application->window));
        return;
    }

    GtkBuilder *builder = gtk_builder_new();
    GError *error = NULL;
    gtk_builder_add_from_resource(builder, "/org/gnome/battery-bench/gnome-battery-bench.xml",
                                  &error);
    if (error)
        die("Cannot load user interface: %s\n", error->message);

    application->window = GTK_WIDGET(gtk_builder_get_object(builder, "window"));
    gtk_application_add_window(GTK_APPLICATION(app), GTK_WINDOW(application->window));

    application->test_combo = GTK_WIDGET(gtk_builder_get_object(builder, "test-combo"));

    application->start_button = GTK_WIDGET(gtk_builder_get_object(builder, "start-button"));

    gtk_widget_set_sensitive(application->start_button,
                             gbb_event_player_is_ready(application->player));

    g_signal_connect(application->start_button, "clicked",
                     G_CALLBACK(on_start_button_clicked), application);

    application->state_label = GTK_WIDGET(gtk_builder_get_object(builder, "state-label"));
    update_label(application);

    g_signal_connect(application->monitor, "changed",
                     G_CALLBACK(on_power_monitor_changed),
                     application);

    gtk_widget_show(application->window);
}

static void
gbb_application_class_init(GbbApplicationClass *class)
{
    GObjectClass *gobject_class = G_OBJECT_CLASS(class);
    GApplicationClass *application_class = G_APPLICATION_CLASS(class);

    application_class->activate = gbb_application_activate;

    gobject_class->finalize = gbb_application_finalize;
}

static void
gbb_application_init(GbbApplication *application)
{
    application->monitor = gbb_power_monitor_new();
    application->system_state = gbb_system_state_new();

    application->player = GBB_EVENT_PLAYER(gbb_remote_player_new("GNOME Battery Bench"));
    g_signal_connect(application->player, "ready",
                     G_CALLBACK(on_player_ready), application);
    g_signal_connect(application->player, "finished",
                     G_CALLBACK(on_player_finished), application);
}

GbbApplication *
gbb_application_new(void)
{
    return g_object_new (GBB_TYPE_APPLICATION,
                         "application-id", "org.gnome.BatteryBench",
                         "flags", G_APPLICATION_FLAGS_NONE,
                         NULL);
}
