/* -*- mode: C; c-file-style: "stroustrup"; indent-tabs-mode: nil; -*- */

#include <gtk/gtk.h>

#include "power-graphs.h"
#include "util-gtk.h"

struct _GbbPowerGraphs {
    GtkGrid parent;

    GtkWidget *power_area;
    GtkWidget *percentage_area;
    GtkWidget *life_area;

    GtkWidget *power_max;
    GtkWidget *power_min;
    GtkWidget *life_max;
    GtkWidget *life_min;
    GtkWidget *time_max;
    GtkWidget *time_min;

    GbbTestRun *run;

    double max_x;
    double max_y_power;
    double max_y_life;
};

struct _GbbPowerGraphsClass {
    GtkGridClass parent_class;
};

G_DEFINE_TYPE(GbbPowerGraphs, gbb_power_graphs, GTK_TYPE_GRID)

static double
round_up_time(double seconds)
{
    if (seconds <= 5 * 60)
        return 5 * 60;
    else if (seconds <= 6 * 60)
        return 6 * 60;
    else if (seconds <= 10 * 60)
        return 10 * 60;
    else if (seconds <= 12 * 60)
        return 12 * 60;
    else if (seconds <= 15 * 60)
        return 15 * 60;
    else if (seconds <= 20 * 60)
        return 20 * 60;
    else if (seconds <= 30 * 60)
        return 30 * 60;
    else if (seconds <= 40 * 60)
        return 40 * 60;
    else if (seconds <= 60 * 60)
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
redraw_graphs(GbbPowerGraphs *graphs)
{
    gtk_widget_queue_draw(graphs->power_area);
    gtk_widget_queue_draw(graphs->percentage_area);
    gtk_widget_queue_draw(graphs->life_area);
}

static void
update_chart_ranges(GbbPowerGraphs *graphs)
{
    double max_power = 0.0, max_life = 0.0;

    if (graphs->power_area == NULL) /* after destruction */
        return;

    if (graphs->run) {
        max_power = gbb_test_run_get_max_power(graphs->run);
        max_life = gbb_test_run_get_max_battery_life(graphs->run);
    }

    if (max_power == 0)
        max_power = 10;
    if (max_life == 0)
        max_life = 5 * 60 * 60;

    if (max_power <= 5)
        graphs->max_y_power = 5;
    else if (max_power <= 10)
        graphs->max_y_power = 10;
    else if (max_power <= 15)
        graphs->max_y_power = 15;
    else if (max_power <= 20)
        graphs->max_y_power = 20;
    else if (max_power <= 30)
        graphs->max_y_power = 30;
    else if (max_power <= 50)
        graphs->max_y_power = 50;
    else
        graphs->max_y_power = 100;

    label_set_textf(GTK_LABEL(graphs->power_max), "%.0fW", graphs->max_y_power);

    graphs->max_y_life = round_up_time(max_life);

    if (graphs->max_y_life >= 60 * 60)
        label_set_textf(GTK_LABEL(graphs->life_max), "%.0fh", graphs->max_y_life / (60 * 60));
    else
        label_set_textf(GTK_LABEL(graphs->life_max), "%.0fm", graphs->max_y_life / (60));

    GbbDurationType duration_type = GBB_DURATION_TIME;
    if (graphs->run)
        duration_type = gbb_test_run_get_duration_type(graphs->run);

    switch (duration_type) {
    case GBB_DURATION_TIME:
        graphs->max_x = 60 * 60;
        if (graphs->run)
            graphs->max_x = round_up_time(gbb_test_run_get_duration_time(graphs->run) +
                                          gbb_test_run_get_loop_time(graphs->run));
        break;
    case GBB_DURATION_PERCENT:
        graphs->max_x = 5 * 60 * 60;
        if (graphs->run) {
            const GbbPowerState *start_state = gbb_test_run_get_start_state(graphs->run);
            const GbbPowerState *last_state = gbb_test_run_get_last_state(graphs->run);

            if (start_state != last_state) {
                GbbPowerStatistics *stats = gbb_power_statistics_compute(start_state, last_state);
                graphs->max_x = round_up_time(stats->battery_life);
                gbb_power_statistics_free(stats);
            }
        }
        break;
    }

    if (graphs->max_x >= 60 * 60)
        label_set_textf(GTK_LABEL(graphs->time_max), "%.0f:00", graphs->max_x / (60 * 60));
    else
        label_set_textf(GTK_LABEL(graphs->time_max), "0:%02.0f", graphs->max_x / (60));

    redraw_graphs(graphs);
}

static void
on_chart_area_draw (GtkWidget      *chart_area,
                    cairo_t        *cr,
                    GbbPowerGraphs *graphs)
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
    if (chart_area == graphs->power_area) {
        if ((int)(0.5 + graphs->max_y_power) == 30)
            n_y_ticks = 6;
    } else if (chart_area == graphs->life_area) {
        int max_y_life = (int)(0.5 + graphs->max_y_life);
        switch (max_y_life) {
        case 6 * 60:
        case 12 * 60:
        case 60 * 60:
        case 2 * 60 * 60:
        case 24 * 60 * 60:
        case 48 * 60 * 60:
            n_y_ticks = 6;
            break;
        }
    }

    int graph_max_time = (int)(0.5 + graphs->max_x);
    int n_x_ticks;
    switch (graph_max_time) {
    case 15 * 60:
    case 15 * 60 * 60:
        n_x_ticks = 5;
        break;
    case 60 * 60:
        n_x_ticks = 6;
        break;
    case 5 * 60:
    case 10 * 60:
    case 20 * 60:
    case 30 * 60:
    case 40 * 60:
    case 5 * 60 * 60:
    case 10 * 60 * 60:
        n_x_ticks = 10;
        break;
    case 6 * 60:
    case 12 * 60:
    case 2 * 60 * 60:
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

    if (chart_area == graphs->percentage_area && graphs->run &&
        gbb_test_run_get_duration_type(graphs->run) == GBB_DURATION_PERCENT) {
        double percent = gbb_test_run_get_duration_percent(graphs->run);
        double y = (1 - percent / 100.) * allocation.height;
        cairo_move_to(cr, 1.0, y);
        cairo_line_to(cr, allocation.width - 1.0, y);
        cairo_set_source_rgb(cr, 1.0, 0.5, 0.3);
        cairo_stroke(cr);
    } else if (graphs->run &&
               gbb_test_run_get_duration_type(graphs->run) == GBB_DURATION_TIME) {
        double seconds = gbb_test_run_get_duration_time(graphs->run);
        double x = (seconds / graphs->max_x) * allocation.width;
        cairo_move_to(cr, x, 1.0);
        cairo_line_to(cr, x, allocation.width - 1.0);
        cairo_set_source_rgb(cr, 1.0, 0.5, 0.3);
        cairo_stroke(cr);
    }

    if (!graphs->run)
        return;

    GQueue *history = gbb_test_run_get_history(graphs->run);
    if (!history->head && history->head == history->tail)
        return;

    GbbPowerState *start_state = history->head->data;
    GbbPowerState *last_state = history->head->data;
    GList *l;
    for (l = history->head->next; l; l = l->next) {
        GbbPowerState *state = l->data;
        double v;

        if (chart_area == graphs->power_area) {
            GbbPowerStatistics *interval_stats = gbb_power_statistics_compute(last_state, state);
            v = interval_stats->power / graphs->max_y_power;
            gbb_power_statistics_free(interval_stats);
        } else if (chart_area == graphs->percentage_area) {
            v = gbb_power_state_get_percent(state) / 100;
        } else {
            GbbPowerStatistics *overall_stats = gbb_power_statistics_compute(start_state, state);
            v = overall_stats->battery_life / graphs->max_y_life;
            gbb_power_statistics_free(overall_stats);
        }

        double x = allocation.width * (state->time_us - start_state->time_us) / 1000000. / graphs->max_x;
        double y = (1 - v) * allocation.height;

        if (l == history->head->next)
            cairo_move_to(cr, x, y);
        else
            cairo_line_to(cr, x, y);

        last_state = state;
    }

    cairo_set_source_rgb(cr, 0, 0, 0.8);
    cairo_stroke(cr);
}

static void
gbb_power_graphs_finalize(GObject *object)
{
    GbbPowerGraphs *graphs = GBB_POWER_GRAPHS(object);

    gbb_power_graphs_set_test_run(graphs, NULL);

    G_OBJECT_CLASS(gbb_power_graphs_parent_class)->finalize(object);
}

static void
gbb_power_graphs_init(GbbPowerGraphs *graphs)
{
    gtk_widget_init_template(GTK_WIDGET (graphs));

    g_signal_connect(graphs->power_area, "draw",
                    G_CALLBACK(on_chart_area_draw),
                     graphs);
    g_signal_connect(graphs->percentage_area, "draw",
                     G_CALLBACK(on_chart_area_draw),
                     graphs);
    g_signal_connect(graphs->life_area, "draw",
                     G_CALLBACK(on_chart_area_draw),
                     graphs);
}

static void
gbb_power_graphs_class_init(GbbPowerGraphsClass *graphs_class)
{
    GObjectClass *gobject_class = G_OBJECT_CLASS(graphs_class);
    GtkWidgetClass *widget_class = GTK_WIDGET_CLASS(graphs_class);

    gobject_class->finalize = gbb_power_graphs_finalize;

    gtk_widget_class_set_template_from_resource(widget_class,
                                                 "/org/gnome/BatteryBench/power-graphs.ui");

    gtk_widget_class_bind_template_child(widget_class, GbbPowerGraphs, power_area);
    gtk_widget_class_bind_template_child(widget_class, GbbPowerGraphs, percentage_area);
    gtk_widget_class_bind_template_child(widget_class, GbbPowerGraphs, life_area);
    gtk_widget_class_bind_template_child(widget_class, GbbPowerGraphs, power_max);
    gtk_widget_class_bind_template_child(widget_class, GbbPowerGraphs, power_min);
    gtk_widget_class_bind_template_child(widget_class, GbbPowerGraphs, life_max);
    gtk_widget_class_bind_template_child(widget_class, GbbPowerGraphs, life_min);
    gtk_widget_class_bind_template_child(widget_class, GbbPowerGraphs, time_max);
    gtk_widget_class_bind_template_child(widget_class, GbbPowerGraphs, time_min);
}

GtkWidget *
gbb_power_graphs_new(void)
{
    return g_object_new(GBB_TYPE_POWER_GRAPHS, NULL);
}

static void
on_run_updated(GbbTestRun     *run,
               GbbPowerGraphs *graphs)
{
    update_chart_ranges(graphs);
}

void
gbb_power_graphs_set_test_run(GbbPowerGraphs *graphs,
                              GbbTestRun     *run)
{
    if (run == graphs->run)
        return;

    if (graphs->run) {
        g_signal_handlers_disconnect_by_func(graphs->run,
                                             (gpointer)on_run_updated,
                                             graphs);
        g_clear_object(&graphs->run);
    }

    if (run) {
        graphs->run = g_object_ref(run);
        g_signal_connect(graphs->run, "updated",
                         G_CALLBACK(on_run_updated), graphs);
    }

    update_chart_ranges(graphs);
}
