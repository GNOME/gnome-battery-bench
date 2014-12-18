/* -*- mode: C; c-file-style: "stroustrup"; indent-tabs-mode: nil; -*- */

#include <stdlib.h>
#include <string.h>

#include <gtk/gtk.h>

#include <gdk/gdkx.h>
#include <X11/keysymdef.h>

#include "application.h"
#include "event-log.h"
#include "battery-test.h"
#include "remote-player.h"
#include "power-monitor.h"
#include "system-state.h"
#include "util.h"

typedef enum {
    DURATION_TIME,
    DURATION_PERCENT
} DurationType;

typedef enum {
    STATE_STOPPED,
    STATE_PROLOGUE,
    STATE_WAITING,
    STATE_RUNNING,
    STATE_STOPPING,
    STATE_EPILOGUE
} State;

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

    State state;
    gboolean stop_requested;
    gboolean exit_requested;
    BatteryTest *test;
    double loop_duration;

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

static void application_stop(GbbApplication *application);

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
    GbbPowerState *state = gbb_power_monitor_get_state(application->monitor);
    set_label(application, "ac",
              "%s", state->online ? "online" : "offline");

    char *title = NULL;
    switch (application->state) {
    case STATE_STOPPED:
        title = g_strdup("GNOME Battery Bench");
        break;
    case STATE_PROLOGUE:
        title = g_strdup("GNOME Battery Bench - setting up");
        break;
    case STATE_WAITING:
        if (state->online)
            title = g_strdup("GNOME Battery Bench - disconnect from AC to start");
        else
            title = g_strdup("GNOME Battery Bench - waiting for data");
        break;
    case STATE_RUNNING:
    {
        int h, m, s;
        break_time((state->time_us - application->start_state->time_us) / 1000000, &h, &m, &s);
        title = g_strdup_printf("GNOME Battery Bench - running (%d:%02d:%02d)", h, m, s);
        break;
    }
    case STATE_STOPPING:
        title = g_strdup("GNOME Battery Bench - stopping");
        break;
    case STATE_EPILOGUE:
        title = g_strdup("GNOME Battery Bench - cleaning up");
        break;
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
redraw_graphs(GbbApplication *application)
{
    gtk_widget_queue_draw(application->power_area);
    gtk_widget_queue_draw(application->percentage_area);
    gtk_widget_queue_draw(application->life_area);
}

static void
update_chart_ranges(GbbApplication *application)
{
    double max_power = application->max_power > 0 ? application->max_power : 10;
    double max_life = application->max_life > 0 ? application->max_life : 5 * 60 * 60;

    if (max_power <= 5)
        application->graph_max_power = 5;
    else if (max_power <= 10)
        application->graph_max_power = 10;
    else if (max_power <= 15)
        application->graph_max_power = 15;
    else if (max_power <= 20)
        application->graph_max_power = 20;
    else if (max_power <= 30)
        application->graph_max_power = 30;
    else if (max_power <= 50)
        application->graph_max_power = 50;
    else
        application->graph_max_power = 100;

    set_label(application, "power-max", "%.0fW", application->graph_max_power);

    application->graph_max_life = round_up_time(max_life);

    if (application->graph_max_life >= 60 * 60)
        set_label(application, "life-max", "%.0fh", application->graph_max_life / (60 * 60));
    else
        set_label(application, "life-max", "%.0fm", application->graph_max_life / (60));

    switch (application->duration_type) {
    case DURATION_TIME:
        {
            if (application->test)
                application->graph_max_time = round_up_time(application->duration.seconds + application->loop_duration);
            else
                application->graph_max_time = application->duration.seconds;
        }
        break;
    case DURATION_PERCENT:
        if (application->statistics)
            application->graph_max_time = round_up_time(application->statistics->battery_life);
        else
            application->graph_max_time = round_up_time(5 * 60 * 60);
        break;
    }

    if (application->graph_max_time >= 60 * 60)
        set_label(application, "time-max", "%.0f:00", application->graph_max_time / (60 * 60));
    else
        set_label(application, "time-max", "0:%02.0f", application->graph_max_time / (60));

    redraw_graphs(application);
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

    GbbPowerStatistics *overall_stats = gbb_power_monitor_compute_statistics(application->monitor,
                                                                             application->start_state, state);
    application->max_life = MAX(overall_stats->battery_life, application->max_life);

    GbbPowerStatistics *interval_stats = gbb_power_monitor_compute_statistics(application->monitor,
                                                                              last_state, state);
    application->max_power = MAX(interval_stats->power, application->max_power);

    gbb_power_statistics_free(interval_stats);
    gbb_power_statistics_free(overall_stats);

    update_chart_ranges(application);
}

static void
update_sensitive(GbbApplication *application)
{
    gboolean start_sensitive = FALSE;
    gboolean controls_sensitive = FALSE;

    switch (application->state) {
    case STATE_STOPPED:
        start_sensitive = gbb_event_player_is_ready(application->player);
        controls_sensitive = TRUE;
        break;
    case STATE_PROLOGUE:
    case STATE_WAITING:
    case STATE_RUNNING:
        start_sensitive = !application->stop_requested;
        controls_sensitive = FALSE;
        break;
    case STATE_STOPPING:
    case STATE_EPILOGUE:
        start_sensitive = FALSE;
        controls_sensitive = FALSE;
        break;
    }

    gtk_widget_set_sensitive(application->start_button, start_sensitive);
    gtk_widget_set_sensitive(application->test_combo, controls_sensitive);
    gtk_widget_set_sensitive(application->duration_combo, controls_sensitive);
    gtk_widget_set_sensitive(application->backlight_combo, controls_sensitive);
}

static void
application_set_state(GbbApplication *application,
                      State           state)
{
    application->state = state;
    update_sensitive(application);
    update_labels(application);
    redraw_graphs(application);
}

static void
on_power_monitor_changed(GbbPowerMonitor *monitor,
                         GbbApplication  *application)
{
    if (application->state == STATE_WAITING) {
        GbbPowerState *state = gbb_power_monitor_get_state(monitor);
        if (!state->online) {
            application->start_state = state;
            application_set_state(application, STATE_RUNNING);
            gbb_event_player_play_file(application->player, application->test->loop_file);
        } else {
            gbb_power_state_free(state);
        }
    } else if (application->state == STATE_RUNNING) {
        if (application->statistics)
            gbb_power_statistics_free(application->statistics);

        GbbPowerState *state = gbb_power_monitor_get_state(monitor);
        application->statistics = gbb_power_monitor_compute_statistics(monitor, application->start_state, state);
        add_to_history(application, state);
        update_labels(application);
    }
}

static void
on_player_ready(GbbEventPlayer *player,
                GbbApplication *application)
{
    update_sensitive(application);
    update_labels(application);
}

static GdkFilterReturn
on_root_event (GdkXEvent *xevent,
               GdkEvent  *event,
               gpointer   data)
{
    GbbApplication *application = data;
    XEvent *xev = (XEvent *)xevent;
    if (xev->xany.type == KeyPress) {
        application_stop(application);
        return GDK_FILTER_REMOVE;
    } else {
        return GDK_FILTER_CONTINUE;
    }
}

/* As always, when we XGrabKey, we need to grab with different combinations
 * of ignored modifiers like CapsLock, NumLock; this function figures that
 * out.
 */
static GList *
get_grab_modifiers(GbbApplication *application)
{
    GdkScreen *screen = gtk_widget_get_screen(application->window);
    Display *xdisplay = gdk_x11_display_get_xdisplay(gdk_screen_get_display(screen));
    gboolean used[8] = { FALSE };
    gint super = -1;

    /* Figure out what modifiers are used, and what modifier is Super */
    XModifierKeymap *modmap =  XGetModifierMapping(xdisplay);
    int i, j;
    for (i = 0; i < 8; i++) {
        for (j = 0; j < modmap->max_keypermod; j++) {
            if (modmap->modifiermap[i * modmap->max_keypermod + j]) {
                used[i] = TRUE;
            }
            if (modmap->modifiermap[i * modmap->max_keypermod + j] == XKeysymToKeycode(xdisplay, XK_Super_L))
                super = i;
        }
    }
    XFree(modmap);

    /* We want to effectively grab only if Shift/Control/Mod1/Super
     * are in the same state we expect.
     */
    guint32 to_ignore = ShiftMask | ControlMask | Mod1Mask;
    if (super >= 0)
        to_ignore |= (1 << super);

    for (i = 0; i < 8; i++) {
        if (!used[i])
            to_ignore |= 1 << i;
    }

    /* quick-and-dirty way to find out all the combinations of other
     * modifiers; since the total number of modifier combinations is
     * small, works fine */
    GList *result = NULL;
    guint32 mask = 0;
    for (mask = 0; mask < 255; mask++) {
        if ((mask & to_ignore) == 0)
            result = g_list_prepend(result, GUINT_TO_POINTER(mask));
    }

    return result;
}

static void
setup_stop_shortcut(GbbApplication *application)
{
    GdkScreen *screen = gtk_widget_get_screen(application->window);
    GdkWindow *root = gdk_screen_get_root_window(screen);
    Window xroot = gdk_x11_window_get_xid(root);
    Display *xdisplay = gdk_x11_display_get_xdisplay(gdk_screen_get_display(screen));

    GList *modifiers = get_grab_modifiers(application);
    GList *l;
    for (l = modifiers; l; l = l->next)
        XGrabKey(xdisplay,
                 XKeysymToKeycode(xdisplay, 'q'),
                 ControlMask | Mod1Mask | GPOINTER_TO_UINT(l->data),
                 xroot, False /* owner_events */,
                 GrabModeAsync /* pointer_mode */, GrabModeAsync /* keyboard_mode */);
    g_list_free(modifiers);

    gdk_window_add_filter(root, on_root_event, application);
}

static void
remove_stop_shortcut(GbbApplication *application)
{
    GdkScreen *screen = gtk_widget_get_screen(application->window);
    GdkWindow *root = gdk_screen_get_root_window(screen);
    Window xroot = gdk_x11_window_get_xid(root);
    Display *xdisplay = gdk_x11_display_get_xdisplay(gdk_screen_get_display(screen));

    GList *modifiers = get_grab_modifiers(application);
    GList *l;
    for (l = modifiers; l; l = l->next)
        XUngrabKey(xdisplay,
                   XKeysymToKeycode(xdisplay, 'q'),
                   ControlMask | Mod1Mask | GPOINTER_TO_UINT(l->data),
                   xroot);
    g_list_free(modifiers);

    gdk_window_remove_filter(root, on_root_event, application);
}

static void
application_set_stopped(GbbApplication *application)
{
    application_set_state(application, STATE_STOPPED);
    application->test = NULL;

    remove_stop_shortcut(application);

    gbb_system_state_restore(application->system_state);

    g_object_set(G_OBJECT(application->start_button), "label", "Start", NULL);

    if (application->exit_requested)
        gtk_widget_destroy(application->window);
}

static void
application_set_epilogue(GbbApplication *application)
{
    if (application->test->epilogue_file) {
        gbb_event_player_play_file(application->player, application->test->epilogue_file);
        application_set_state(application, STATE_EPILOGUE);
    } else {
        application_set_stopped(application);
    }
}

static void
on_player_finished(GbbEventPlayer *player,
                   GbbApplication *application)
{
    if (application->state == STATE_PROLOGUE) {
        application_set_state(application, STATE_WAITING);

        if (application->stop_requested) {
            application->stop_requested = FALSE;
            application_stop(application);
        }
    } else if (application->state == STATE_RUNNING) {
        GbbPowerState *last_state = application->history->tail ? application->history->tail->data : application->start_state;
        gboolean done;

        if (application->duration_type == DURATION_TIME)
            done = (last_state->time_us - application->start_state->time_us) / 1000000. > application->duration.seconds;
        else
            done = gbb_power_state_get_percent(last_state) < application->duration.percent;

        if (done)
            application_set_epilogue(application);
        else
            gbb_event_player_play_file(player, application->test->loop_file);
    } else if (application->state == STATE_STOPPING) {
        application_set_epilogue(application);
    } else if (application->state == STATE_EPILOGUE) {
        application_set_stopped(application);
    }
}

static void
application_start(GbbApplication *application)
{
    if (application->state != STATE_STOPPED)
        return;

    if (application->history) {
        g_queue_free_full(application->history, (GFreeFunc)gbb_power_state_free);
        application->history = NULL;
    }

    g_clear_pointer(&application->start_state, (GFreeFunc)gbb_power_state_free);
    g_clear_pointer(&application->statistics, (GFreeFunc)gbb_power_statistics_free);

    application->history = g_queue_new();
    application->max_power = 0;
    application->max_life = 0;
    g_object_set(G_OBJECT(application->start_button), "label", "Stop", NULL);

    const char *test_id = gtk_combo_box_get_active_id(GTK_COMBO_BOX(application->test_combo));
    application->test = battery_test_get_for_id(test_id);

    GFile *loop_file = g_file_new_for_path(application->test->loop_file);
    GError *error = NULL;
    application->loop_duration = gbb_event_log_duration(loop_file, NULL, &error) / 1000.;
    if (error)
        die("Can't get duration of .loop file: %s", error->message);
    g_object_unref(loop_file);

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

    setup_stop_shortcut(application);

    update_chart_ranges(application);

    if (application->test->prologue_file) {
        gbb_event_player_play_file(application->player, application->test->prologue_file);
        application_set_state(application, STATE_PROLOGUE);
    } else {
        application_set_state(application, STATE_WAITING);
    }
}

static void
application_stop(GbbApplication *application)
{
    if ((application->state == STATE_WAITING || application->state == STATE_RUNNING)) {
        if (application->state == STATE_RUNNING) {
            gbb_event_player_stop(application->player);
            application_set_state(application, STATE_STOPPING);
        } else {
            application_set_epilogue(application);
        }
    } else if (application->state == STATE_PROLOGUE) {
        application->stop_requested = TRUE;
        update_sensitive(application);
    }
}

static void
on_start_button_clicked(GtkWidget      *button,
                        GbbApplication *application)
{
    if (application->state == STATE_STOPPED) {
        application_start(application);
    } else if (application->state == STATE_PROLOGUE ||
               application->state == STATE_WAITING ||
               application->state == STATE_RUNNING) {
        application_stop(application);
    }
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
        if ((int)(0.5 + application->graph_max_power) == 30)
            n_y_ticks = 6;
    } else if (chart_area == application->life_area) {
        int graph_max_life = (int)(0.5 + application->graph_max_life);
        switch (graph_max_life) {
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

    int graph_max_time = (int)(0.5 + application->graph_max_time);
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

    if (application->test && chart_area == application->percentage_area && application->duration_type == DURATION_PERCENT) {
        double y = (1 - application->duration.percent / 100.) * allocation.height;
        cairo_move_to(cr, 1.0, y);
        cairo_line_to(cr, allocation.width - 1.0, y);
        cairo_set_source_rgb(cr, 1.0, 0.5, 0.3);
        cairo_stroke(cr);
    } else if (application->test && application->duration_type == DURATION_TIME) {
        double x = (application->duration.seconds / application->graph_max_time) * allocation.width;
        cairo_move_to(cr, x, 1.0);
        cairo_line_to(cr, x, allocation.width - 1.0);
        cairo_set_source_rgb(cr, 1.0, 0.5, 0.3);
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

static gboolean
on_delete_event(GtkWidget      *window,
                GdkEventAny    *event,
                GbbApplication *application)
{
    if (application->state == STATE_STOPPED) {
        return FALSE;
    } else {
        application->exit_requested = TRUE;
        application_stop(application);
        return TRUE;
    }
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
    g_signal_connect(application->window, "delete-event",
                     G_CALLBACK(on_delete_event), application);

    gtk_application_add_window(GTK_APPLICATION(app), GTK_WINDOW(application->window));

    application->headerbar = GTK_WIDGET(gtk_builder_get_object(application->builder, "headerbar"));

    application->test_combo = GTK_WIDGET(gtk_builder_get_object(application->builder, "test-combo"));
    GList *tests = battery_test_list_all();
    GList *l;
    for (l = tests; l; l = l->next) {
        BatteryTest *test = l->data;
        gtk_combo_box_text_append(GTK_COMBO_BOX_TEXT(application->test_combo),
                                  test->id, test->name);
    }
    gtk_combo_box_set_active_id(GTK_COMBO_BOX(application->test_combo), "lightduty");

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
    application->graph_max_power = 10;
    application->graph_max_time = 60 * 60;
    application->graph_max_life = 5 * 60 * 60;

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
