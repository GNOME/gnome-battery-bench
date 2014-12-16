/* -*- mode: C; c-file-style: "stroustrup"; indent-tabs-mode: nil; -*- */

#include <gtk/gtk.h>

#include "application.h"
#include "remote-player.h"
#include "power-monitor.h"
#include "system-state.h"
#include "util.h"

typedef enum {
    DURATION_TIME,
    DURATION_PERCENT
} DurationType;

struct _GbbApplication {
    GtkApplication parent;
    GbbPowerMonitor *monitor;
    GbbEventPlayer *player;
    GbbSystemState *system_state;

    GbbPowerState *start_state;
    GQueue *history;
    GbbPowerStatistics *statistics;

    GtkWidget *window;
    GtkWidget *test_combo;
    GtkWidget *duration_combo;
    GtkWidget *start_button;
    GtkWidget *state_label;
    GtkWidget *power_area;
    GtkWidget *percentage_area;
    GtkWidget *life_area;
    GtkWidget *power_max_label;
    GtkWidget *life_max_label;
    GtkWidget *time_max_label;

    gboolean started;
    char *filename;

    DurationType duration_type;
    union {
        double seconds;
        double percent;
    } duration;

    double max_power;
    double graph_max_power;
    double max_life;
    double graph_max_life;
    double graph_max_time;
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

static double
round_up_time(double seconds)
{
    if (seconds <= 60 * 60)
        return 60 * 60;
    else if (seconds <= 2 * 60 * 60)
        return 2 * 60 * 60;
    else if (seconds <= 5 * 60 * 60)
        return 5 * 60 * 60;
    else if (seconds <= 10 * 60 * 60)
        return 10 * 60 * 60;
    else if (seconds <= 15 * 60 * 60)
        return 15 * 60 * 60;
    else if (seconds <= 24 * 60 * 60)
        return 24 * 60 * 60;
    else
        return 48 * 60 * 60;
}

static void
add_to_history(GbbApplication *application,
               GbbPowerState  *state)
{
    GbbPowerState *last_state = application->history->tail ? application->history->tail->data : application->start_state;
    gboolean use_this_state = FALSE;

    switch (application->duration_type) {
    case DURATION_TIME:
        if (state->time_us - last_state->time_us > application->duration.seconds * 1000000. / 100.)
            use_this_state = TRUE;
        break;
    case DURATION_PERCENT:
        {
            double start_percent = gbb_power_state_get_percent(application->start_state);
            double last_percent = gbb_power_state_get_percent(last_state);
            double current_percent = gbb_power_state_get_percent(state);

            if ((current_percent - last_percent) / (start_percent - application->duration.percent) > 0.005)
                use_this_state = TRUE;
            break;
        }
    }

    if (!use_this_state) {
        gbb_power_state_free(state);
        return;
    }

    g_queue_push_tail(application->history, state);

    GbbPowerStatistics *interval_stats = gbb_power_monitor_compute_statistics(application->monitor,
                                                                              last_state, state);
    application->max_power = MAX(interval_stats->power, application->max_power);
    if (application->max_power <= 5)
        application->graph_max_power = 5;
    else if (application->max_power <= 10)
        application->graph_max_power = 10;
    else if (application->max_power <= 15)
        application->graph_max_power = 15;
    else if (application->max_power <= 20)
        application->graph_max_power = 20;
    else if (application->max_power <= 30)
        application->graph_max_power = 30;
    else if (application->max_power <= 50)
        application->graph_max_power = 50;
    else
        application->graph_max_power = 100;

    char *text;

    text = g_strdup_printf("%.0fW", application->graph_max_power);
    gtk_label_set_text(GTK_LABEL(application->power_max_label), text);
    g_free(text);

    GbbPowerStatistics *overall_stats = gbb_power_monitor_compute_statistics(application->monitor,
                                                                             application->start_state, state);
    application->max_life = MAX(overall_stats->battery_life, application->max_life);
    application->graph_max_life = round_up_time(application->max_life);

    if (application->graph_max_life >= 60 * 60)
        text = g_strdup_printf("%.0fh", application->graph_max_life / (60 * 60));
    else
        text = g_strdup_printf("%.0fm", application->graph_max_life / (60));
    gtk_label_set_text(GTK_LABEL(application->life_max_label), text);
    g_free(text);

    switch (application->duration_type) {
    case DURATION_TIME:
        application->graph_max_time = application->duration.seconds;
        break;
    case DURATION_PERCENT:
        application->graph_max_time = round_up_time(overall_stats->battery_life);
        break;
    }

    if (application->graph_max_time >= 60 * 60)
        text = g_strdup_printf("%.0f:00", application->graph_max_time / (60 * 60));
    else
        text = g_strdup_printf("0:%02.0f", application->graph_max_time / (60));
    gtk_label_set_text(GTK_LABEL(application->time_max_label), text);
    g_free(text);

    gbb_power_statistics_free(interval_stats);
    gbb_power_statistics_free(overall_stats);

    gtk_widget_queue_draw(application->power_area);
    gtk_widget_queue_draw(application->percentage_area);
    gtk_widget_queue_draw(application->life_area);
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
            add_to_history(application, state);
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
        gtk_widget_set_sensitive(application->duration_combo, TRUE);
    } else {
        if (application->history) {
            g_queue_free_full(application->history, (GFreeFunc)gbb_power_state_free);
            application->history = NULL;
        }

        application->history = g_queue_new();
        application->max_power = 0;
        application->max_life = 0;

        gbb_system_state_save(application->system_state);
        gbb_system_state_set_default(application->system_state);
        application->started = TRUE;

        gtk_widget_set_sensitive(application->test_combo, FALSE);
        gtk_widget_set_sensitive(application->duration_combo, FALSE);
        g_object_set(G_OBJECT(application->start_button), "label", "Stop", NULL);

        const char *test_id = gtk_combo_box_get_active_id(GTK_COMBO_BOX(application->test_combo));
        char *testfile = g_strconcat(test_id, ".batterytest", NULL);
        application->filename = g_build_filename(PKGDATADIR, "tests", testfile, NULL);
        g_free(testfile);

        const char *duration_id = gtk_combo_box_get_active_id(GTK_COMBO_BOX(application->duration_combo));
        if (strcmp(duration_id, "minutes-5") == 0) {
            application->duration_type = DURATION_TIME;
            application->duration.seconds = 5 * 60;
        } else if (strcmp(duration_id, "minutes-10") == 0) {
            application->duration_type = DURATION_TIME;
            application->duration.seconds = 10 * 60;
        } else if (strcmp(duration_id, "minutes-30") == 0) {
            application->duration_type = DURATION_TIME;
            application->duration.seconds = 30 * 60;
        } else if (strcmp(duration_id, "percent-5") == 0) {
            application->duration_type = DURATION_PERCENT;
            application->duration.percent = 5;
        }
    }

    update_label(application);
}

static void
on_chart_area_draw (GtkWidget      *chart_area,
                    cairo_t        *cr,
                    GbbApplication *application)
{
    cairo_rectangle_int_t allocation;

    cairo_set_source_rgb(cr, 1, 1, 1);
    cairo_paint(cr);

    gtk_widget_get_allocation(chart_area, &allocation);
    cairo_rectangle(cr,
                    0.5, 0.5,
                    allocation.width - 1, allocation.height - 1);
    cairo_set_source_rgb(cr, 0, 0, 0);
    cairo_set_line_width(cr, 1.0);
    cairo_stroke(cr);

    int n_y_ticks = 5;
    if (chart_area == application->power_area) {
        if (application->history && application->history->tail && application->graph_max_power == 30)
            n_y_ticks = 6;
    } else if (chart_area == application->life_area) {
        if (application->history && application->history->tail) {
            int graph_max_life = (int)(0.5 + application->graph_max_life);
            switch (graph_max_life) {
            case 60 * 60:
            case 2 * 60 * 60:
            case 24 * 60 * 60:
            case 48 * 60 * 60:
                n_y_ticks = 6;
                break;
            }
        }
    }

    int graph_max_time = 60 * 60;
    if (application->history && application->history->tail)
        graph_max_time = (int)(0.5 + application->graph_max_time);
    int n_x_ticks;
    switch (graph_max_time) {
    case 5 * 60:
    case 10 * 60:
    case 30 * 60:
        n_x_ticks = 10;
        break;
    case 60 * 60:
        n_x_ticks = 6;
        break;
    case 2 * 60 * 60:
        n_x_ticks = 12;
        break;
    case 5 * 60 * 60:
    case 10 * 60 * 60:
        n_x_ticks = 10;
        break;
    case 15 * 60 * 60:
        n_x_ticks = 5;
        break;
    case 24 * 60 * 60:
    case 48 * 60 * 60:
        n_x_ticks = 12;
        break;
    default:
        n_x_ticks = 10;
        break;
    }

    int i;
    for (i = 1; i < n_y_ticks; i++) {
        int y = (int)(0.5 + i * (double)allocation.height / n_y_ticks) - 0.5;
        cairo_move_to(cr, 1.0, y);
        cairo_line_to(cr, allocation.width - 1.0, y);
    }

    for (i = 1; i < n_x_ticks; i++) {
        int x = (int)(0.5 + i * (double)allocation.width / n_x_ticks) - 0.5;
        cairo_move_to(cr, x, 1.0);
        cairo_line_to(cr, x, allocation.height - 1.0);
    }

    cairo_set_source_rgb(cr, 0.8, 0.8, 0.8);
    cairo_stroke(cr);

    if (!application->history || !application->history->head)
        return;

    GbbPowerState *start_state = application->start_state;
    GbbPowerState *last_state = application->start_state;
    GList *l;
    for (l = application->history->head; l; l = l->next) {
        GbbPowerState *state = l->data;
        double v;

        if (chart_area == application->power_area) {
            GbbPowerStatistics *interval_stats = gbb_power_monitor_compute_statistics(application->monitor,
                                                                                      last_state, state);
            v = interval_stats->power / application->graph_max_power;
            gbb_power_statistics_free(interval_stats);
        } else if (chart_area == application->percentage_area) {
            v = gbb_power_state_get_percent(state) / 100;
        } else {
            GbbPowerStatistics *overall_stats = gbb_power_monitor_compute_statistics(application->monitor,
                                                                                     start_state, state);
            v = overall_stats->battery_life / application->graph_max_life;
            gbb_power_statistics_free(overall_stats);
        }

        double x = allocation.width * (state->time_us - start_state->time_us) / 1000000. / application->graph_max_time;
        double y = (1 - v) * allocation.height;

        if (l == application->history->head)
            cairo_move_to(cr, x, y);
        else
            cairo_line_to(cr, x, y);

        last_state = state;
    }

    cairo_set_source_rgb(cr, 0, 0, 0.8);
    cairo_stroke(cr);
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
    application->duration_combo = GTK_WIDGET(gtk_builder_get_object(builder, "duration-combo"));

    application->start_button = GTK_WIDGET(gtk_builder_get_object(builder, "start-button"));

    gtk_widget_set_sensitive(application->start_button,
                             gbb_event_player_is_ready(application->player));

    g_signal_connect(application->start_button, "clicked",
                     G_CALLBACK(on_start_button_clicked), application);

    application->state_label = GTK_WIDGET(gtk_builder_get_object(builder, "state-label"));
    application->power_area = GTK_WIDGET(gtk_builder_get_object(builder, "power-area"));
    g_signal_connect(application->power_area, "draw",
                     G_CALLBACK(on_chart_area_draw),
                     application);
    application->percentage_area = GTK_WIDGET(gtk_builder_get_object(builder, "percentage-area"));
    g_signal_connect(application->percentage_area, "draw",
                     G_CALLBACK(on_chart_area_draw),
                     application);
    application->life_area = GTK_WIDGET(gtk_builder_get_object(builder, "life-area"));
    g_signal_connect(application->life_area, "draw",
                     G_CALLBACK(on_chart_area_draw),
                     application);

    application->power_max_label = GTK_WIDGET(gtk_builder_get_object(builder, "power-max"));
    application->life_max_label = GTK_WIDGET(gtk_builder_get_object(builder, "life-max"));
    application->time_max_label = GTK_WIDGET(gtk_builder_get_object(builder, "time-max"));

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
