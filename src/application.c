/* -*- mode: C; c-file-style: "stroustrup"; indent-tabs-mode: nil; -*- */

#include <config.h>

#include <stdlib.h>
#include <string.h>
#include <time.h>

#include <gio/gunixfdlist.h>

#include <gtk/gtk.h>

#include <gdk/gdkx.h>
#include <X11/keysymdef.h>

#include "application.h"
#include "battery-test.h"
#include "power-graphs.h"
#include "test-runner.h"
#include "util.h"

#define LOGIND_DBUS_NAME        "org.freedesktop.login1"
#define LOGIND_DBUS_PATH        "/org/freedesktop/login1"
#define LOGIND_DBUS_INTERFACE   "org.freedesktop.login1.Manager"

struct _GbbApplication {
    GtkApplication parent;
    GbbTestRunner *runner;
    GbbPowerMonitor *monitor;
    GbbEventPlayer *player;

    GFile *log_folder;

    GbbPowerState *current_state;
    GbbPowerState *previous_state;

    GtkBuilder *builder;
    GtkWidget *window;

    GtkWidget *headerbar;
    GtkWidget *start_button;
    GtkWidget *delete_button;

    GtkWidget *test_combo;
    GtkWidget *duration_combo;
    GtkWidget *backlight_combo;

    GtkWidget *log_view;
    GtkListStore *log_model;

    GtkWidget *test_graphs;
    GtkWidget *log_graphs;

    gboolean exit_requested;

    GbbBatteryTest *test;
    GbbTestRun *run;

    GDBusProxy *logind;
    guint       sleep_id;
    gint        inhibitor_fd;
};

struct _GbbApplicationClass {
    GtkApplicationClass parent_class;
};

static void application_stop(GbbApplication *application);
static void gbb_application_inhibitor_lock_release(GbbApplication *application);
static gboolean gbb_application_inhibitor_lock_take(GbbApplication *application);

G_DEFINE_TYPE(GbbApplication, gbb_application, GTK_TYPE_APPLICATION)

static void
gbb_application_finalize(GObject *object)
{
    GbbApplication *application = GBB_APPLICATION (object);
    if (application->logind) {
        GDBusConnection *bus;

        bus = g_dbus_proxy_get_connection (application->logind);

        if (application->sleep_id)
            g_dbus_connection_signal_unsubscribe (bus, application->sleep_id);

        g_clear_object (&application->logind);
    }
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
    const GbbPowerState *current_state = application->current_state;

    set_label(application, "ac",
              "%s", current_state->online ? "online" : "offline");

    char *title = NULL;
    switch (gbb_test_runner_get_phase(application->runner)) {
    case GBB_TEST_PHASE_STOPPED:
        title = g_strdup("GNOME Battery Bench");
        break;
    case GBB_TEST_PHASE_PROLOGUE:
        title = g_strdup("GNOME Battery Bench - setting up");
        break;
    case GBB_TEST_PHASE_WAITING:
        if (current_state->online)
            title = g_strdup("GNOME Battery Bench - disconnect from AC to start");
        else
            title = g_strdup("GNOME Battery Bench - waiting for data");
        break;
    case GBB_TEST_PHASE_RUNNING:
    {
        int h, m, s;
        const GbbPowerState *start_state = gbb_test_run_get_start_state(application->run);
        break_time((current_state->time_us - start_state->time_us) / 1000000, &h, &m, &s);
        title = g_strdup_printf("GNOME Battery Bench - running (%d:%02d:%02d)", h, m, s);
        break;
    }
    case GBB_TEST_PHASE_STOPPING:
        title = g_strdup("GNOME Battery Bench - stopping");
        break;
    case GBB_TEST_PHASE_EPILOGUE:
        title = g_strdup("GNOME Battery Bench - cleaning up");
        break;
    }
    gtk_header_bar_set_title(GTK_HEADER_BAR(application->headerbar), title);
    g_free(title);

    if (current_state->energy_now >= 0)
        set_label(application, "energy-now", "%.1fWh", current_state->energy_now);
    else
        clear_label(application, "energy-now");

    if (current_state->energy_full >= 0)
        set_label(application, "energy-full", "%.1fWh", current_state->energy_full);
    else
        clear_label(application, "energy-full");

    if (current_state->energy_now >= 0 && current_state->energy_full >= 0)
        set_label(application, "percentage", "%.1f%%", 100. * current_state->energy_now / current_state->energy_full);
    else
        clear_label(application, "percentage");

    if (current_state->energy_full_design >= 0)
        set_label(application, "energy-full-design", "%.1fWh", current_state->energy_full_design);
    else
        clear_label(application, "energy-full-design");

    if (current_state->energy_now >= 0 && current_state->energy_full_design >= 0)
        set_label(application, "percentage-design", "%.1f%%", 100. * current_state->energy_now / current_state->energy_full_design);
    else
        clear_label(application, "percentage-design");

    GbbPowerStatistics *interval_statistics = NULL;
    if (application->previous_state)
        interval_statistics = gbb_power_statistics_compute(application->previous_state, current_state);

    GbbPowerStatistics *overall_statistics = NULL;
    if (application->run) {
        const GbbPowerState *start_state = gbb_test_run_get_start_state(application->run);
        if (start_state) {
            const GbbPowerState *end_state;
            if (gbb_test_runner_get_phase(application->runner) == GBB_TEST_PHASE_RUNNING)
                end_state = current_state;
            else
                end_state = gbb_test_run_get_last_state(application->run);

            overall_statistics = gbb_power_statistics_compute(start_state, end_state);
        }
    }

    if (overall_statistics && overall_statistics->power >= 0)
        set_label(application, "power-average", "%.2fW", overall_statistics->power);
    else
        clear_label(application, "power-average");

    if (interval_statistics && interval_statistics->power >= 0) {
        set_label(application, "power-instant", "%.2fW", interval_statistics->power);
    } else {
        clear_label(application, "power-instant");
    }

    if (overall_statistics && overall_statistics->battery_life >= 0) {
        int h, m, s;
        break_time(overall_statistics->battery_life, &h, &m, &s);
        set_label(application, "estimated-life", "%d:%02d:%02d", h, m, s);
    } else {
        clear_label(application, "estimated-life");
    }
    if (overall_statistics && overall_statistics->battery_life_design >= 0) {
        int h, m, s;
        break_time(overall_statistics->battery_life_design, &h, &m, &s);
        set_label(application, "estimated-life-design", "%d:%02d:%02d", h, m, s);
    } else {
        clear_label(application, "estimated-life-design");
    }

    if (overall_statistics)
        gbb_power_statistics_free(overall_statistics);
    if (interval_statistics)
        gbb_power_statistics_free(interval_statistics);
}

static void
update_sensitive(GbbApplication *application)
{
    gboolean start_sensitive = FALSE;
    gboolean controls_sensitive = FALSE;

    switch (gbb_test_runner_get_phase(application->runner)) {
    case GBB_TEST_PHASE_STOPPED:
        start_sensitive = gbb_event_player_is_ready(application->player);
        controls_sensitive = TRUE;
        break;
    case GBB_TEST_PHASE_PROLOGUE:
    case GBB_TEST_PHASE_WAITING:
    case GBB_TEST_PHASE_RUNNING:
        start_sensitive = !gbb_test_runner_get_stop_requested(application->runner);
        controls_sensitive = FALSE;
        break;
    case GBB_TEST_PHASE_STOPPING:
    case GBB_TEST_PHASE_EPILOGUE:
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
on_power_monitor_changed(GbbPowerMonitor *monitor,
                         GbbApplication  *application)
{
    if (application->previous_state)
        gbb_power_state_free(application->previous_state);

    application->previous_state = application->current_state;
    application->current_state = gbb_power_state_copy(gbb_power_monitor_get_state(monitor));

    update_labels(application);
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

enum {
    COLUMN_RUN,
    COLUMN_NAME,
    COLUMN_DURATION,
    COLUMN_DATE
};

static char *
make_duration_string(GbbTestRun *run)
{
    switch (gbb_test_run_get_duration_type(run)) {
    case GBB_DURATION_TIME:
        return g_strdup_printf("%.0f Minutes", gbb_test_run_get_duration_time(run) / 60);
    case GBB_DURATION_PERCENT:
        return g_strdup_printf("Until %.0f%% battery", gbb_test_run_get_duration_percent(run));
    default:
        g_assert_not_reached();
    }
}

static char *
make_date_string(GbbTestRun *run)
{
    GDateTime *start = g_date_time_new_from_unix_local(gbb_test_run_get_start_time(run));
    GDateTime *now = g_date_time_new_now_local();

    char *result;

    gint64 difference = g_date_time_difference(now, start);
    if (difference < G_TIME_SPAN_DAY)
        result = g_date_time_format(start, "%H:%M");
    else if (difference < 7 * G_TIME_SPAN_DAY &&
             g_date_time_get_day_of_week(now) != g_date_time_get_day_of_week(start))
        result = g_date_time_format(start, "%a %H:%M");
    else if (g_date_time_get_year(now) == g_date_time_get_year(start))
        result = g_date_time_format(start, "%m-%d %H:%M");
    else
        result = g_date_time_format(start, "%Y-%m-%d %H:%M");

    g_date_time_unref(start);
    g_date_time_unref(now);

    return result;
}

static void
add_run_to_logs(GbbApplication *application,
                GbbTestRun     *run)
{
    GtkTreeIter iter;

    char *duration = make_duration_string(run);
    char *date = make_date_string(run);

    gtk_list_store_append(application->log_model, &iter);
    gtk_list_store_set(application->log_model, &iter,
                       COLUMN_RUN, run,
                       COLUMN_DURATION, duration,
                       COLUMN_DATE, date,
                       COLUMN_NAME, gbb_test_run_get_name(run),
                       -1);
    g_free(duration);
    g_free(date);

    GtkTreeSelection *selection = gtk_tree_view_get_selection(GTK_TREE_VIEW(application->log_view));
    if (!gtk_tree_selection_get_selected(selection, NULL, NULL))
        gtk_tree_selection_select_iter(selection, &iter);
}

static void
write_run_to_disk(GbbApplication *application,
                  GbbTestRun     *run)
{
    GError *error = NULL;

    g_debug("Writing %s to disk", gbb_test_run_get_name(application->run));

    if (!g_file_query_exists(application->log_folder, NULL)) {
        if (!g_file_make_directory_with_parents(application->log_folder, NULL, &error)) {
            g_warning("Cannot create log directory: %s\n", error->message);
            g_clear_error(&error);
            return;
        }
    }

    char *path = gbb_test_run_get_default_path(run, application->log_folder);
    if (!gbb_test_run_write_to_file(application->run, path, &error)) {
        g_warning("Can't write test run to disk: %s\n", error->message);
        g_clear_error(&error);
    }
    g_free(path);
}

static void
application_start(GbbApplication *application)
{
    if (gbb_test_runner_get_phase(application->runner) != GBB_TEST_PHASE_STOPPED)
        return;

    if (application->run) {
        gbb_power_graphs_set_test_run(GBB_POWER_GRAPHS(application->test_graphs), NULL);
        g_clear_object(&application->run);
    }

    g_object_set(G_OBJECT(application->start_button), "label", "Stop", NULL);

    const char *test_id = gtk_combo_box_get_active_id(GTK_COMBO_BOX(application->test_combo));
    application->test = gbb_battery_test_get_for_id(test_id);

    application->run = gbb_test_run_new(application->test);

    const char *duration_id = gtk_combo_box_get_active_id(GTK_COMBO_BOX(application->duration_combo));
    if (strcmp(duration_id, "minutes-5") == 0) {
        gbb_test_run_set_duration_time(application->run, 5 * 60);
    } else if (strcmp(duration_id, "minutes-10") == 0) {
        gbb_test_run_set_duration_time(application->run, 10 * 60);
    } else if (strcmp(duration_id, "minutes-30") == 0) {
        gbb_test_run_set_duration_time(application->run, 30 * 60);
    } else if (strcmp(duration_id, "until-percent-5") == 0) {
        gbb_test_run_set_duration_percent(application->run, 5);
    }

    const char *backlight_id = gtk_combo_box_get_active_id(GTK_COMBO_BOX(application->backlight_combo));
    int screen_brightness = atoi(backlight_id);

    gbb_test_run_set_screen_brightness(application->run, screen_brightness);

    gbb_power_graphs_set_test_run(GBB_POWER_GRAPHS(application->test_graphs), application->run);

    if (GDK_IS_X11_DISPLAY(gtk_widget_get_display(application->window)))
        setup_stop_shortcut(application);

    gbb_application_inhibitor_lock_take(application);
    gbb_test_runner_set_run(application->runner, application->run);
    gbb_test_runner_start(application->runner);
}

static void
application_stop(GbbApplication *application)
{
    gbb_test_runner_stop(application->runner);
}

static void
on_main_stack_notify_visible_child(GtkStack       *stack,
                                   GParamSpec     *pspec,
                                   GbbApplication *application)
{
    const gchar *visible = gtk_stack_get_visible_child_name(stack);
    gtk_widget_set_visible(application->start_button, g_strcmp0(visible, "test") == 0);
    gtk_widget_set_visible(application->delete_button, g_strcmp0(visible, "logs") == 0);
}

static void
on_start_button_clicked(GtkWidget      *button,
                        GbbApplication *application)
{
    GbbTestPhase phase = gbb_test_runner_get_phase(application->runner);

    if (phase == GBB_TEST_PHASE_STOPPED) {
        application_start(application);
    } else if (phase == GBB_TEST_PHASE_PROLOGUE ||
               phase == GBB_TEST_PHASE_WAITING ||
               phase == GBB_TEST_PHASE_RUNNING) {
        application_stop(application);
    }
}

static void
on_delete_button_clicked(GtkWidget      *button,
                        GbbApplication *application)
{
    GtkTreeModel *model;
    GtkTreeIter iter;

    GtkTreeSelection *selection = gtk_tree_view_get_selection(GTK_TREE_VIEW(application->log_view));
    if (gtk_tree_selection_get_selected(selection, &model, &iter)) {
        GbbTestRun *run;
        const char *name;
        const char *date;
        gtk_tree_model_get(model, &iter,
                           COLUMN_RUN, &run,
                           COLUMN_NAME, &name,
                           COLUMN_DATE, &date,
                           -1);

        GtkWidget *dialog = gtk_message_dialog_new(GTK_WINDOW(application->window),
                                                   GTK_DIALOG_MODAL,
                                                   GTK_MESSAGE_QUESTION,
                                                   GTK_BUTTONS_NONE,
                                                   "Delete log?");

        gtk_message_dialog_format_secondary_text(GTK_MESSAGE_DIALOG(dialog),
                                                 "Permanently delete log of test '%s' from %s?",
                                                 name, date);

        gtk_dialog_add_buttons(GTK_DIALOG(dialog),
                               "Cancel", GTK_RESPONSE_CANCEL,
                               "Delete", GTK_RESPONSE_OK,
                               NULL);
        gtk_dialog_set_default_response(GTK_DIALOG(dialog), GTK_RESPONSE_OK);

        int response = gtk_dialog_run(GTK_DIALOG(dialog));
        if (response == GTK_RESPONSE_OK) {
            const char *filename = gbb_test_run_get_filename(run);
            GFile *file = g_file_new_for_path(filename);
            GError *error = NULL;

            if (g_file_delete(file, NULL, &error)) {
                if (gtk_list_store_remove(application->log_model, &iter))
                    gtk_tree_selection_select_iter(selection, &iter);
            } else {
                g_warning("Failed to delete log: %s\n", error->message);
                g_clear_error(&error);
            }
        }

        gtk_widget_destroy(dialog);
    }
}

static gboolean
on_delete_event(GtkWidget      *window,
                GdkEventAny    *event,
                GbbApplication *application)
{
    if (gbb_test_runner_get_phase(application->runner) == GBB_TEST_PHASE_STOPPED) {
        return FALSE;
    } else {
        application->exit_requested = TRUE;
        application_stop(application);
        return TRUE;
    }
}

static void
fill_log_from_run(GbbApplication *application,
                  GbbTestRun     *run)
{
    set_label(application, "test-log", "%s", gbb_test_run_get_name(run));
    char *duration = make_duration_string(run);
    set_label(application, "duration-log", "%s", duration);
    g_free(duration);
    set_label(application, "backlight-log", "%d%%",
              gbb_test_run_get_screen_brightness(run));
    const GbbPowerState *start_state = gbb_test_run_get_start_state(run);
    const GbbPowerState *last_state = gbb_test_run_get_last_state(run);
    if (last_state != start_state) {
        GbbPowerStatistics *statistics = gbb_power_statistics_compute(start_state, last_state);
        if (statistics->power >= 0)
            set_label(application, "power-average-log", "%.2fW", statistics->power);
        else
            clear_label(application, "power-average-log");

        if (last_state->energy_full >= 0)
            set_label(application, "energy-full-log", "%.2fW", last_state->energy_full);
        else
            clear_label(application, "energy-full-log");

        if (last_state->energy_full_design >= 0)
            set_label(application, "energy-full-design-log", "%.1fWh", last_state->energy_full_design);
        else
            clear_label(application, "energy-full-design-log");

        if (statistics->battery_life >= 0) {
            int h, m, s;
            break_time(statistics->battery_life, &h, &m, &s);
            set_label(application, "estimated-life-log", "%d:%02d:%02d", h, m, s);
        } else {
            clear_label(application, "estimated-life-log");
        }

        if (statistics->battery_life_design >= 0) {
            int h, m, s;
            break_time(statistics->battery_life_design, &h, &m, &s);
            set_label(application, "estimated-life-design-log", "%d:%02d:%02d", h, m, s);
        } else {
            clear_label(application, "estimated-life-design-log");
        }
    }

    gbb_power_graphs_set_test_run(GBB_POWER_GRAPHS(application->log_graphs), run);
}

static int
compare_runs(gconstpointer a,
             gconstpointer b)
{
    gint64 time_a = gbb_test_run_get_start_time((GbbTestRun *)a);
    gint64 time_b = gbb_test_run_get_start_time((GbbTestRun *)b);

    return time_a < time_b ? -1 : (time_a == time_b ? 0 : 1);
}

static void
read_logs(GbbApplication *application)
{
    GError *error = NULL;
    GFileEnumerator *enumerator;
    GList *runs = NULL;

    enumerator = g_file_enumerate_children (application->log_folder,
                                            "standard::name",
                                            G_FILE_QUERY_INFO_NONE,
                                            NULL, &error);
    if (!enumerator)
        goto out;

    while (error == NULL) {
        GFileInfo *info = g_file_enumerator_next_file (enumerator, NULL, &error);
        GFile *child = NULL;
        if (error != NULL)
            goto out;
        else if (!info)
            break;

        const char *name = g_file_info_get_name (info);
        if (!g_str_has_suffix(name, ".json"))
            goto next;

        child = g_file_enumerator_get_child (enumerator, info);
        char *child_path = g_file_get_path(child);
        GbbTestRun *run = gbb_test_run_new_from_file(child_path, &error);
        if (run) {
            runs = g_list_prepend(runs, run);
        } else {
            g_warning("Can't read test log '%s': %s", child_path, error->message);
            g_clear_error(&error);
        }

    next:
        g_clear_object (&child);
        g_clear_object (&info);
    }

out:
    if (error != NULL) {
        g_warning("Error reading logs: %s", error->message);
        g_clear_error(&error);
    }

    g_clear_object (&enumerator);

    runs = g_list_sort(runs, compare_runs);

    GList *l;
    for (l = runs; l; l = l->next) {
        add_run_to_logs(application, l->data);
        g_object_unref(l->data);
    }

    g_list_free(runs);
}

static void
on_log_selection_changed (GtkTreeSelection *selection,
                          GbbApplication   *application)
{
    GtkTreeModel *model;
    GtkTreeIter iter;

    gboolean have_selected = gtk_tree_selection_get_selected(selection, &model, &iter);

    if (have_selected) {
        GbbTestRun *run;
        gtk_tree_model_get(model, &iter, 0, &run, -1);
        fill_log_from_run(application, run);
    }

    gtk_widget_set_sensitive(application->delete_button, have_selected);
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
                                  "/org/gnome/BatteryBench/application.ui",
                                  &error);
    if (error)
        die("Cannot load user interface: %s\n", error->message);

    application->window = GTK_WIDGET(gtk_builder_get_object(application->builder, "window"));
    g_signal_connect(application->window, "delete-event",
                     G_CALLBACK(on_delete_event), application);

    gtk_application_add_window(GTK_APPLICATION(app), GTK_WINDOW(application->window));

    application->headerbar = GTK_WIDGET(gtk_builder_get_object(application->builder, "headerbar"));

    application->test_combo = GTK_WIDGET(gtk_builder_get_object(application->builder, "test-combo"));
    GList *tests = gbb_battery_test_list_all();
    GList *l;
    for (l = tests; l; l = l->next) {
        GbbBatteryTest *test = l->data;
        gtk_combo_box_text_append(GTK_COMBO_BOX_TEXT(application->test_combo),
                                  test->id, test->name);
    }
    gtk_combo_box_set_active_id(GTK_COMBO_BOX(application->test_combo), "idle");

    application->duration_combo = GTK_WIDGET(gtk_builder_get_object(application->builder, "duration-combo"));
    application->backlight_combo = GTK_WIDGET(gtk_builder_get_object(application->builder, "backlight-combo"));

    application->start_button = GTK_WIDGET(gtk_builder_get_object(application->builder, "start-button"));
    application->delete_button = GTK_WIDGET(gtk_builder_get_object(application->builder, "delete-button"));

    GtkWidget *test_graphs_parent = GTK_WIDGET(gtk_builder_get_object(application->builder, "test-graphs-parent"));
    application->test_graphs = gbb_power_graphs_new();
    gtk_box_pack_start(GTK_BOX(test_graphs_parent), application->test_graphs, TRUE, TRUE, 0);

    GtkWidget *log_graphs_parent = GTK_WIDGET(gtk_builder_get_object(application->builder, "log-graphs-parent"));
    application->log_graphs = gbb_power_graphs_new();
    gtk_box_pack_start(GTK_BOX(log_graphs_parent), application->log_graphs, TRUE, TRUE, 0);

    gtk_widget_set_sensitive(application->start_button,
                             gbb_event_player_is_ready(application->player));

    g_signal_connect(application->start_button, "clicked",
                     G_CALLBACK(on_start_button_clicked), application);
    g_signal_connect(application->delete_button, "clicked",
                     G_CALLBACK(on_delete_button_clicked), application);

    /****************************************/

    application->log_view = GTK_WIDGET(gtk_builder_get_object(application->builder, "log-view"));
    GtkTreeSelection *selection = gtk_tree_view_get_selection(GTK_TREE_VIEW(application->log_view));
    gtk_tree_selection_set_mode(selection, GTK_SELECTION_BROWSE);

    g_signal_connect(selection, "changed",
                     G_CALLBACK(on_log_selection_changed), application);
    on_log_selection_changed(selection, application);

    GtkTreeViewColumn *column;
    GtkCellRenderer *renderer;

    renderer = gtk_cell_renderer_text_new();
    column = gtk_tree_view_column_new_with_attributes("Date", renderer, "text", COLUMN_DATE, NULL);
    gtk_tree_view_column_set_expand(column, TRUE);
    gtk_tree_view_append_column(GTK_TREE_VIEW(application->log_view), column);

    renderer = gtk_cell_renderer_text_new();
    column = gtk_tree_view_column_new_with_attributes("Name", renderer, "text", COLUMN_NAME, NULL);
    gtk_tree_view_append_column(GTK_TREE_VIEW(application->log_view), column);

    renderer = gtk_cell_renderer_text_new();
    column = gtk_tree_view_column_new_with_attributes("Duration", renderer, "text", COLUMN_DURATION, NULL);
    gtk_tree_view_append_column(GTK_TREE_VIEW(application->log_view), column);

    application->log_model = GTK_LIST_STORE(gtk_builder_get_object(application->builder, "log-model"));

    read_logs(application);

    /****************************************/

    GtkWidget *main_stack = GTK_WIDGET(gtk_builder_get_object(application->builder, "main-stack"));
    g_signal_connect(main_stack, "notify::visible-child",
                     G_CALLBACK(on_main_stack_notify_visible_child), application);
    on_main_stack_notify_visible_child(GTK_STACK(main_stack), NULL, application);

    application->current_state = gbb_power_state_copy(gbb_power_monitor_get_state(application->monitor));
    g_signal_connect(application->monitor, "changed",
                     G_CALLBACK(on_power_monitor_changed),
                     application);

    update_labels(application);

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
on_runner_phase_changed(GbbTestRunner  *runner,
                        GbbApplication *application)
{

    GbbTestPhase phase = gbb_test_runner_get_phase(runner);

    if (phase == GBB_TEST_PHASE_STOPPED) {
        const GbbPowerState *start_state = gbb_test_run_get_start_state(application->run);
        const GbbPowerState *last_state = gbb_test_run_get_last_state(application->run);
        if (last_state != start_state) {
            write_run_to_disk(application, application->run);
            add_run_to_logs(application, application->run);
        }

        application->test = NULL;

        if (GDK_IS_X11_DISPLAY(gtk_widget_get_display(application->window)))
            remove_stop_shortcut(application);

        g_object_set(G_OBJECT(application->start_button), "label", "Start", NULL);

        if (application->exit_requested)
            gtk_widget_destroy(application->window);

        gbb_application_inhibitor_lock_release(application);
    }

    update_sensitive(application);
    update_labels(application);
}

static void
gbb_application_inhibitor_lock_release(GbbApplication *application)
{
    if (application->inhibitor_fd == -1) {
        return;
    }

    close (application->inhibitor_fd);
    application->inhibitor_fd = -1;

    g_debug ("Released inhibitor lock");
}

static gboolean
gbb_application_inhibitor_lock_take(GbbApplication *application)
{
    g_autoptr(GVariant) out = NULL;
    g_autoptr(GUnixFDList) fds = NULL;
    g_autoptr(GError) error = NULL;
    GVariant *input;

    if (application->logind == NULL) {
        return FALSE;
    }

    if (application->inhibitor_fd > -1) {
        return TRUE;
    }

    input = g_variant_new ("(ssss)",
                           "sleep",                /* what */
                           "GNOME Battery Bench",  /* who */
                           "Battery test ongoing", /* why */
                           "delay");               /* mode */

    out = g_dbus_proxy_call_with_unix_fd_list_sync (application->logind,
                                                    "Inhibit",
                                                    input,
                                                    G_DBUS_CALL_FLAGS_NONE,
                                                    -1,
                                                    NULL,
                                                    &fds,
                                                    NULL,
                                                    &error);
    if (out == NULL) {
        g_warning ("Could not acquire inhibitor lock: %s", error->message);
        return FALSE;
    }

    if (g_unix_fd_list_get_length (fds) != 1) {
        g_warning ("Unexpected values returned by logind's 'Inhibit'");
        return FALSE;
    }

    application->inhibitor_fd = g_unix_fd_list_get (fds, 0, NULL);

    g_debug ("Acquired inhibitor lock (%i)", application->inhibitor_fd);

    return TRUE;
}

static void
gbb_application_prepare_for_sleep (GDBusConnection *connection,
                                   const gchar     *sender_name,
                                   const gchar     *object_path,
                                   const gchar     *interface_name,
                                   const gchar     *signal_name,
                                   GVariant        *parameters,
                                   gpointer         user_data)
{
    GbbApplication *application = GBB_APPLICATION (user_data);
    GbbTestRunner *runner = application->runner;
    GbbTestRun *run;
    GbbTestPhase phase;
    gboolean will_sleep;

    if (!g_variant_is_of_type (parameters, G_VARIANT_TYPE ("(b)"))) {
        g_warning ("logind PrepareForSleep has unexpected parameter(s)");
        return;
    }

    g_variant_get (parameters, "(b)", &will_sleep);

    if (!will_sleep) {
        /* not interesting for now */
        return;
    }

    g_debug("Preparing for sleep");

    phase = gbb_test_runner_get_phase(runner);
    if (phase == GBB_TEST_PHASE_STOPPED) {
        /* if we are stopped, we don't have a inhibitor lock */
        return;
    }

    run = gbb_test_runner_get_run(runner);
    if (run && phase == GBB_TEST_PHASE_RUNNING) {
        /* if we are not stopped, we should have a run,
         * but let's be sure.
         * This should also be called again by the the callback
         * to on_runner_phase_changed, but we want to make sure
         * we have the data on disc */
        write_run_to_disk(application, run);
    }

    /* We force stop the test runner, so the epilogue does
     * not get played, because it is very likely that the
     * screen has already been locked by gnome-shell and
     * we would play the epilogue on the lock-screen.
     *
     * stopping the run via gbb_test_runner_stop()
     *   -> on_runner_phase_changed() callback
     *   -> release inhibitor lock
     */
    gbb_test_runner_force_stop(runner);
}

static void
initialize_logind_proxy(GbbApplication *application)
{
    GDBusConnection *bus;
    GError *error = NULL;
    guint sleep_id;

    application->inhibitor_fd = -1;

    application->logind = g_dbus_proxy_new_for_bus_sync(G_BUS_TYPE_SYSTEM,
                                                        0,
                                                        NULL,
                                                        LOGIND_DBUS_NAME,
                                                        LOGIND_DBUS_PATH,
                                                        LOGIND_DBUS_INTERFACE,
                                                        NULL,
                                                        &error);
    if (application->logind == NULL) {
        g_warning("Could not create logind proxy. Sleep/Resume signals not enabled. (%s)",
                  error->message);
        g_error_free(error);
        return;
    }
    /* the following code needs to have a valid logind proxy */
    bus = g_dbus_proxy_get_connection(application->logind);
    sleep_id = g_dbus_connection_signal_subscribe(bus,
                                                  LOGIND_DBUS_NAME,
                                                  LOGIND_DBUS_INTERFACE,
                                                  "PrepareForSleep",
                                                  LOGIND_DBUS_PATH,
                                                  NULL,
                                                  G_DBUS_SIGNAL_FLAGS_NONE,
                                                  gbb_application_prepare_for_sleep,
                                                  application,
                                                  NULL);
    application->sleep_id = sleep_id;
}

static void
gbb_application_init(GbbApplication *application)
{
    application->runner = gbb_test_runner_new();
    g_signal_connect(application->runner, "phase-changed",
                     G_CALLBACK(on_runner_phase_changed), application);

    application->monitor = gbb_test_runner_get_power_monitor(application->runner);

    char *folder_path = g_build_filename(g_get_user_data_dir(), PACKAGE_NAME, "logs", NULL);
    application->log_folder = g_file_new_for_path(folder_path);
    g_free(folder_path);

    application->player = gbb_test_runner_get_event_player(application->runner);
    g_signal_connect(application->player, "ready",
                     G_CALLBACK(on_player_ready), application);

    initialize_logind_proxy(application);

    g_debug("Gbb initialized");
}

GbbApplication *
gbb_application_new(void)
{
    return g_object_new (GBB_TYPE_APPLICATION,
                         "application-id", "org.gnome.BatteryBench",
                         "flags", G_APPLICATION_FLAGS_NONE,
                         NULL);
}
