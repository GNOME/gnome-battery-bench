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

    GtkBuilder *builder;
    GtkWidget *window;
    GtkWidget *headerbar;
    GtkWidget *test_combo;
    GtkWidget *duration_combo;
    GtkWidget *backlight_combo;
    GtkWidget *start_button;
    GtkWidget *power_area;
    GtkWidget *percentage_area;
    GtkWidget *life_area;

    gboolean started;
    char *filename;

    DurationType duration_type;
    union {
        double seconds;
        double percent;
    } duration;

    int backlight_level;

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
set_label(GbbApplication *application,
          const char     *label_name,
          const char     *format,
          ...) G_GNUC_PRINTF(3, 4);

static void
set_label(GbbApplication *application,
          const char     *label_name,
          const char     *format,
          ...)
{
    va_list args;

    va_start(args, format);
    char *text = g_strdup_vprintf(format, args);
    va_end(args);

    GtkLabel *label = GTK_LABEL(gtk_builder_get_object(application->builder, label_name));
    gtk_label_set_text(label, text);
    g_free(text);
}

static void
clear_label(GbbApplication *application,
            const char     *label_name)
{
    GtkLabel *label = GTK_LABEL(gtk_builder_get_object(application->builder, label_name));
    gtk_label_set_text(label, "");
}

static void
update_labels(GbbApplication *application)
{
    GString *text = g_string_new (NULL);

    GbbPowerState *state = gbb_power_monitor_get_state(application->monitor);
    set_label(application, "ac",
              "%s", state->online ? "online" : "offline");

    char *title;
    if (!application->started) {
        title = g_strdup("GNOME Battery Bench");
    } else if (application->started && !application->statistics) {
        if (state->online)
            title = g_strdup("GNOME Battery Bench - disconnect from AC to start");
        else
            title = g_strdup("GNOME Battery Bench - waiting for data");
    } else {
        int h, m, s;
        break_time((state->time_us - application->start_state->time_us) / 1000000, &h, &m, &s);
        title = g_strdup_printf("GNOME Battery Bench - running (%d:%02d:%02d)", h, m, s);
    }

    gtk_header_bar_set_title(GTK_HEADER_BAR(application->headerbar), title);
    g_free(title);


    if (state->energy_now >= 0)
        set_label(application, "energy-now", "%.1fWH", state->energy_now);
    else
        clear_label(application, "energy-now");

    if (state->energy_full >= 0)
        set_label(application, "energy-full", "%.1fWH", state->energy_full);
    else
        clear_label(application, "energy-full");

    if (state->energy_now >= 0 && state->energy_full >= 0)
        set_label(application, "percentage", "%.1f%%", 100. * state->energy_now / state->energy_full);
    else
        clear_label(application, "percentage");

    if (state->energy_full_design >= 0)
        set_label(application, "energy-full-design", "%.1fWH", state->energy_full_design);
    else
        clear_label(application, "energy-full-design");

    if (state->energy_now >= 0 && state->energy_full_design >= 0)
        set_label(application, "percentage-design", "%.1f%%", 100. * state->energy_now / state->energy_full_design);
    else
        clear_label(application, "percentage-design");

    GbbPowerStatistics *statistics = application->statistics;
    if (statistics && statistics->power >= 0)
        set_label(application, "power-average", "%.1fW", statistics->power);
    else
        clear_label(application, "power-average");

    if (application->history && application->history->tail) {
        GbbPowerState *state = application->history->tail->data;
        GbbPowerState *last_state = application->history->tail->prev ? application->history->tail->prev->data : application->start_state;
        GbbPowerStatistics *interval_stats = gbb_power_monitor_compute_statistics(application->monitor,
                                                                                  last_state, state);
        set_label(application, "power-instant", "%.1fW", interval_stats->power);
        gbb_power_statistics_free(interval_stats);
    } else if (statistics && statistics->power >= 0) {
        set_label(application, "power-instant", "%.1fW", statistics->power);
    } else {
        clear_label(application, "power-instant");
    }

    if (statistics && statistics->battery_life >= 0) {
        int h, m, s;
        break_time(statistics->battery_life, &h, &m, &s);
        set_label(application, "estimated-life", "%d:%02d:%02d", h, m, s);
    } else {
        clear_label(application, "estimated-life");
    }
    if (statistics && statistics->battery_life_design >= 0) {
        int h, m, s;
        break_time(statistics->battery_life_design, &h, &m, &s);
        set_label(application, "estimated-life-design", "%d:%02d:%02d", h, m, s);
    } else {
        clear_label(application, "estimated-life-design");
    }

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
redraw_graphs(GbbApplication *application)
{
    gtk_widget_queue_draw(application->power_area);
    gtk_widget_queue_draw(application->percentage_area);
    gtk_widget_queue_draw(application->life_area);
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

            if ((last_percent - current_percent) / (start_percent - application->duration.percent) > 0.005)
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

    set_label(application, "power-max", "%.0fW", application->graph_max_power);

    GbbPowerStatistics *overall_stats = gbb_power_monitor_compute_statistics(application->monitor,
                                                                             application->start_state, state);
    application->max_life = MAX(overall_stats->battery_life, application->max_life);
    application->graph_max_life = round_up_time(application->max_life);

    if (application->graph_max_life >= 60 * 60)
        set_label(application, "life-max", "%.0fh", application->graph_max_life / (60 * 60));
    else
        set_label(application, "life-max", "%.0fm", application->graph_max_life / (60));

    switch (application->duration_type) {
    case DURATION_TIME:
        application->graph_max_time = application->duration.seconds;
        break;
    case DURATION_PERCENT:
        application->graph_max_time = round_up_time(overall_stats->battery_life);
        break;
    }

    if (application->graph_max_time >= 60 * 60)
        set_label(application, "time-max", "%.0f:00", application->graph_max_time / (60 * 60));
    else
        set_label(application, "time-max", "0:%02.0f", application->graph_max_time / (60));

    gbb_power_statistics_free(interval_stats);
    gbb_power_statistics_free(overall_stats);

    redraw_graphs(application);
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

    update_labels(application);
}

static void
on_player_ready(GbbEventPlayer *player,
                GbbApplication *application)
{
    gtk_widget_set_sensitive(application->start_button, TRUE);
    update_labels(application);
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

        gbb_system_state_restore(application->system_state);

        g_object_set(G_OBJECT(application->start_button), "label", "Start", NULL);
        gtk_widget_set_sensitive(application->test_combo, TRUE);
        gtk_widget_set_sensitive(application->duration_combo, TRUE);
        gtk_widget_set_sensitive(application->backlight_combo, TRUE);

        if (application->start_state)
            gbb_event_player_stop(application->player);
    } else {
        if (application->history) {
            g_queue_free_full(application->history, (GFreeFunc)gbb_power_state_free);
            application->history = NULL;
        }

        g_clear_pointer(&application->start_state, (GFreeFunc)gbb_power_state_free);
        g_clear_pointer(&application->statistics, (GFreeFunc)gbb_power_statistics_free);

        application->history = g_queue_new();
        application->max_power = 0;
        application->max_life = 0;

        gtk_widget_set_sensitive(application->test_combo, FALSE);
        gtk_widget_set_sensitive(application->duration_combo, FALSE);
        gtk_widget_set_sensitive(application->backlight_combo, FALSE);
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
        } else if (strcmp(duration_id, "until-percent-5") == 0) {
            application->duration_type = DURATION_PERCENT;
            application->duration.percent = 5;
        }

        const char *backlight_id = gtk_combo_box_get_active_id(GTK_COMBO_BOX(application->backlight_combo));
        application->backlight_level = atoi(backlight_id);
        gbb_system_state_save(application->system_state);
        gbb_system_state_set_brightnesses(application->system_state,
                                          application->backlight_level,
                                          0);
        application->started = TRUE;

    }

    update_labels(application);
    redraw_graphs(application);
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

    if (chart_area == application->percentage_area && application->duration_type == DURATION_PERCENT) {
        double y = (1 - application->duration.percent / 100.) * allocation.height;
        cairo_move_to(cr, 1.0, y);
        cairo_line_to(cr, allocation.width - 1.0, y);
        cairo_set_source_rgb(cr, 1.0, 0.0, 0.0);
        cairo_stroke(cr);
    }

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

    application->builder = gtk_builder_new();
    GError *error = NULL;
    gtk_builder_add_from_resource(application->builder,
                                  "/org/gnome/battery-bench/gnome-battery-bench.xml",
                                  &error);
    if (error)
        die("Cannot load user interface: %s\n", error->message);

    application->window = GTK_WIDGET(gtk_builder_get_object(application->builder, "window"));
    gtk_application_add_window(GTK_APPLICATION(app), GTK_WINDOW(application->window));

    application->headerbar = GTK_WIDGET(gtk_builder_get_object(application->builder, "headerbar"));

    application->test_combo = GTK_WIDGET(gtk_builder_get_object(application->builder, "test-combo"));
    application->duration_combo = GTK_WIDGET(gtk_builder_get_object(application->builder, "duration-combo"));
    application->backlight_combo = GTK_WIDGET(gtk_builder_get_object(application->builder, "backlight-combo"));

    application->start_button = GTK_WIDGET(gtk_builder_get_object(application->builder, "start-button"));

    gtk_widget_set_sensitive(application->start_button,
                             gbb_event_player_is_ready(application->player));

    g_signal_connect(application->start_button, "clicked",
                     G_CALLBACK(on_start_button_clicked), application);

    application->power_area = GTK_WIDGET(gtk_builder_get_object(application->builder, "power-area"));
    g_signal_connect(application->power_area, "draw",
                     G_CALLBACK(on_chart_area_draw),
                     application);
    application->percentage_area = GTK_WIDGET(gtk_builder_get_object(application->builder, "percentage-area"));
    g_signal_connect(application->percentage_area, "draw",
                     G_CALLBACK(on_chart_area_draw),
                     application);
    application->life_area = GTK_WIDGET(gtk_builder_get_object(application->builder, "life-area"));
    g_signal_connect(application->life_area, "draw",
                     G_CALLBACK(on_chart_area_draw),
                     application);

    update_labels(application);

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
