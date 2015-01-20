/* -*- mode: C; c-file-style: "stroustrup"; indent-tabs-mode: nil; -*- */

#include "config.h"

#include <dirent.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/poll.h>
#include <unistd.h>

#include <X11/Xlib.h>

#include <glib.h>
#include <glib-unix.h>
#include <gio/gio.h>

#include "evdev-player.h"
#include "remote-player.h"
#include "event-recorder.h"
#include "power-monitor.h"
#include "test-runner.h"
#include "xinput-wait.h"
#include "util.h"

static GbbPowerState *start_state;

static void
on_power_monitor_changed(GbbPowerMonitor *monitor,
                         GbbTestRunner   *runner)
{
    const GbbPowerState *state = gbb_power_monitor_get_state(monitor);

    g_print("AC: %s\n", state->online ? "online" : "offline");
    if (state->energy_now >= 0)
        g_print("Energy: %.2f WH (%.2f%%)\n", state->energy_now, gbb_power_state_get_percent(state));
    else if (state->charge_now >= 0)
        g_print("Charge: %.2f AH (%.2f%%)\n", state->energy_now, gbb_power_state_get_percent(state));
    else if (state->capacity_now >= 0)
        g_print("Capacity: %.2f%%\n", gbb_power_state_get_percent(state));

    if (runner != NULL) {
        GbbTestRun *run = gbb_test_runner_get_run(runner);
        const GbbPowerState *tmp = gbb_test_run_get_start_state(run);
        if (tmp)
            start_state = gbb_power_state_copy(tmp);
    } else {
        if (start_state == NULL) {
            if (!state->online)
                start_state = gbb_power_state_copy(state);

            return;
        }
    }

    if (!start_state)
        return;

    GbbPowerStatistics *statistics = gbb_power_statistics_compute(start_state, state);

    if (statistics->power >= 0)
        g_print("Average power: %.2f W\n", statistics->power);
    if (statistics->current >= 0)
        g_print("Average current: %.2f A\n", statistics->current);
    if (statistics->battery_life >= 0) {
        int h, m, s;
        break_time(statistics->battery_life, &h, &m, &s);
        g_print("Predicted battery life: %.0fs (%d:%02d:%02d)\n",
                statistics->battery_life, h, m, s);
    }
    if (statistics->battery_life_design >= 0) {
        int h, m, s;
        break_time(statistics->battery_life_design, &h, &m, &s);
        g_print("Predicted battery life (design): %.0fs (%d:%02d:%02d)\n",
                statistics->battery_life_design, h, m, s);
    }

    gbb_power_statistics_free(statistics);

}

static GOptionEntry monitor_options[] =
{
    { NULL }
};

static int
monitor(int argc, char **argv)
{
    GbbPowerMonitor *monitor;
    GMainLoop *loop;

    monitor = gbb_power_monitor_new();

    g_signal_connect(monitor, "changed",
                     G_CALLBACK(on_power_monitor_changed), NULL);

    loop = g_main_loop_new (NULL, FALSE);
    g_main_loop_run (loop);

    return 0;
}

static void
on_player_finished(GbbEventPlayer *player,
                   GMainLoop      *loop)
{
    g_main_loop_quit(loop);
}

static void
on_player_ready(GObject      *source_object,
                GAsyncResult *result,
                gpointer      data)
{
    GError *error = NULL;
    GMainLoop *loop = data;

    if (!gbb_xinput_wait_finish(GBB_EVENT_PLAYER(source_object),
                                result,
                                &error)) {
        die("Could not wait for devices to appear: %s\n", error->message);
    }

    g_main_loop_quit(loop);
}

static int
do_play(GbbEventPlayer *player,
        int             argc,
        char          **argv)
{
    GMainLoop *loop = g_main_loop_new (NULL, FALSE);

    gbb_xinput_wait(player, NULL,
                    NULL, on_player_ready, loop);
    g_main_loop_run (loop);

    g_signal_connect(player, "finished",
                     G_CALLBACK(on_player_finished), loop);
    gbb_event_player_play_file(player, argv[1]);
    g_main_loop_run (loop);

    return 0;
}

static GOptionEntry play_options[] =
{
    { NULL }
};

static int
play(int argc, char **argv)
{
    GbbEventPlayer *player = GBB_EVENT_PLAYER(gbb_remote_player_new("Gnome Battery Bench Test"));
    return do_play(player, argc, argv);
}

static int
play_local(int argc, char **argv)
{
    GError *error = NULL;
    GbbEventPlayer *player = GBB_EVENT_PLAYER(gbb_evdev_player_new("Gnome Battery Bench Test", &error));
    if (player == NULL)
        die(error->message);

    return do_play(player, argc, argv);
}

static char *record_output;

static GOptionEntry record_options[] =
{
    { "output", 'o', 0, G_OPTION_ARG_FILENAME, &record_output, "Output file", "FILENAME" },
    { NULL }
};

static int
record(int argc, char **argv)
{
    GbbEventRecorder *recorder;

    Display *display = XOpenDisplay(NULL);
    if (!display)
        die("Can't open X display %s", XDisplayName(NULL));

    recorder = gbb_event_recorder_new(display, record_output);
    gbb_event_recorder_record(recorder);
    gbb_event_recorder_free(recorder);

    return 0;
}

static char *test_duration;
static int test_min_battery = -42;
static int test_screen_brightness = 50;
static char *test_output;
static gboolean test_verbose;

static GOptionEntry test_options[] =
{
    { "duration", 'd', 0, G_OPTION_ARG_STRING, &test_duration, "Duration (1h, 10m, etc.)", "DURATION" },
    { "min-battery", 'm', 0, G_OPTION_ARG_INT, &test_duration, "", "PERCENT" },
    { "screen-brightness", 0, 0, G_OPTION_ARG_INT, &test_screen_brightness, "screen backlight brightness (0-100)", "PERCENT" },
    { "output", 'o', 0, G_OPTION_ARG_FILENAME, &test_output, "Output filename", "FILENAME" },
    { "verbose", 'v', 0, G_OPTION_ARG_NONE, &test_verbose, "Show verbose statistics" },
    { NULL }
};

static void
test_on_player_ready(GbbEventPlayer *player,
                     GbbTestRunner  *runner)
{
    gbb_test_runner_start(runner);
}

static char *
make_default_filename(GbbTestRunner *runner)
{
    char *folder_path = g_build_filename(g_get_user_data_dir(), PACKAGE_NAME, "logs", NULL);
    GFile *log_folder = g_file_new_for_path(folder_path);
    g_free(folder_path);

    if (!g_file_query_exists(log_folder, NULL)) {
        GError *error = NULL;
        if (!g_file_make_directory_with_parents(log_folder, NULL, &error))
            die("Cannot create log directory: %s\n", error->message);
    }

    GbbTestRun *run = gbb_test_runner_get_run(runner);
    char *filename = gbb_test_run_get_default_path(run, log_folder);
    g_object_unref(log_folder);
    return filename;
}

static void
on_runner_phase_changed(GbbTestRunner *runner,
                        GMainLoop     *loop)
{
    switch (gbb_test_runner_get_phase(runner)) {
    case GBB_TEST_PHASE_RUNNING:
        if (test_output == NULL)
            test_output = make_default_filename(runner);
        fprintf(stderr, "Running; will write output to %s\n", test_output);

        break;
    case GBB_TEST_PHASE_STOPPED: {
        GbbTestRun *run = gbb_test_runner_get_run(runner);
        GError *error = NULL;
        if (!gbb_test_run_write_to_file(run, test_output, &error))
            die("Can't write test run to disk: %s", error->message);
        g_main_loop_quit(loop);
        break;
    }
    default:
        break;
    }
}

static char *
test_string(void)
{
    GString *s = g_string_new(NULL);

    g_string_append(s, "Available tests:\n");
    GList *tests = gbb_battery_test_list_all();
    GList *l;
    for (l = tests; l; l = l->next) {
        GbbBatteryTest *t = l->data;
        g_string_append_printf(s, "  %s%s%s",
                               t->id,
                               t->description ? " - " : "",
                               t->description ? t->description : "");
        if (l->next)
            g_string_append(s, "\n");
    }
    return g_string_free(s, FALSE);
}

static void
test_prepare_context(GOptionContext *context)
{
    char *description = test_string();
    g_option_context_set_description(context, description);
}

static gboolean
add_unit(char unit, int value, int *total)
{
    switch (unit) {
    case 'h':
        *total += 3600 * value;
        return TRUE;
    case 'm':
        *total += 60 * value;
        return TRUE;
    case 's':
        *total += value;
        return TRUE;
    default:
        return FALSE;
    }
}

static int
parse_duration(const char *duration)
{
    char after;
    int v1, v2, v3;
    char u1, u2, u3;
    int total = 0;

    int count = sscanf(duration, "%d%c%d%c%d%c%c",
                       &v1, &u1, &v2, &u2, &v3, &u3, &after);
    if (count < 2 || count > 6)
        goto fail;
    if (count %2 != 0)
        goto fail;
    if (count >= 2 && !add_unit(u1, v1, &total))
        goto fail;
    if (count >= 4 && !add_unit(u2, v2, &total))
        goto fail;
    if (count >= 6 && !add_unit(u3, v3, &total))
        goto fail;

    return total;

fail:
    die("Can't parse duration string '%s'", duration);
}

static gboolean
on_sigint(gpointer data)
{
    GbbTestRunner *runner = data;
    gbb_test_runner_stop(runner);
    return TRUE;
}

static int
test(int argc, char **argv)
{
    if (test_duration != NULL && test_min_battery != -42)
        die("Only one of --min-battery and --duration can be specified");
    if (test_min_battery != -42 && (test_min_battery < 0 || test_min_battery > 100))
        die("--min-battery argument must be between 0 and 100");
    if (test_screen_brightness < 0 || test_screen_brightness > 100)
        die("--screen-brightness argument must be between 0 and 100");

    const char *test_id = argv[1];
    GbbBatteryTest *test = gbb_battery_test_get_for_id(test_id);
    if (test == NULL) {
        fprintf(stderr, "Unknown test %s\n", test_id);
        fprintf(stderr, "%s\n", test_string());
        exit(1);
    }

    GbbTestRun *run = gbb_test_run_new(test);

    if (test_min_battery != -42) {
    } else if (test_duration != NULL) {
        int seconds = parse_duration(test_duration);
        gbb_test_run_set_duration_time(run, seconds);
    } else {
        gbb_test_run_set_duration_time(run, 10 * 60);
    }

    gbb_test_run_set_screen_brightness(run, test_screen_brightness);

    GbbTestRunner *runner = gbb_test_runner_new();
    gbb_test_runner_set_run(runner, run);

    GbbEventPlayer *player = gbb_test_runner_get_event_player(runner);
    if (gbb_event_player_is_ready(player)) {
        test_on_player_ready(player, runner);
    } else {
        g_signal_connect(player, "ready",
                         G_CALLBACK(test_on_player_ready), runner);
    }

    if (test_verbose) {
        GbbPowerMonitor *monitor = gbb_test_runner_get_power_monitor(runner);
        g_signal_connect(monitor, "changed",
                         G_CALLBACK(on_power_monitor_changed), runner);
        on_power_monitor_changed(monitor, runner);
    }

    g_unix_signal_add(SIGINT, on_sigint, runner);

    GMainLoop *loop = g_main_loop_new (NULL, FALSE);
    g_signal_connect(runner, "phase-changed",
                         G_CALLBACK(on_runner_phase_changed), loop);

    g_main_loop_run (loop);

    return 0;
}

typedef struct {
    const char *command;
    const GOptionEntry *options;
    void (*prepare_context) (GOptionContext *context);
    int (*run) (int argc, char **argv);
    int min_args;
    int max_args;
    const char *param_string;
} Subcommand;

Subcommand subcommands[] = {
    { "monitor",      monitor_options, NULL, monitor, 0, 0 },
    { "play",         play_options, NULL, play, 1, 1, "FILENAME" },
    { "play-local",   play_options, NULL, play_local, 1, 1, "FILENAME" },
    { "record",       record_options, NULL, record, 0, 0 },
    { "test",         test_options, test_prepare_context, test, 1, 1, "TEST_ID" },
    { NULL }
};

static void global_usage(void) G_GNUC_NORETURN;

static void
global_usage(void)
{
    int i;
    fprintf(stderr, "Usage: [");
    for (i = 0; subcommands[i].command; i++) {
        fprintf(stderr,
                "%s%s", i == 0 ? "" : "|",
                subcommands[i].command);
    }
    fprintf(stderr, "]\n");
    exit(1);
}

int
main(int argc, char **argv)
{
    int i;

    if (argc < 2)
        global_usage();

    for (i = 0; subcommands[i].command; i++) {
        Subcommand *subcommand = &subcommands[i];

        if (strcmp(subcommand->command, argv[1]) == 0) {
            char *command = g_strconcat("gbb ", subcommand->command, NULL);
            argv[1] = command;
            argv += 1;
            argc -= 1;

            GOptionContext *context = g_option_context_new(subcommand->param_string);
            GError *error = NULL;

            if (subcommand->prepare_context)
                subcommand->prepare_context(context);

            g_option_context_add_main_entries(context, subcommand->options, NULL);
            if (!g_option_context_parse (context, &argc, &argv, &error))
                die("option parsing failed: %s", error->message);

            if (argc < 1 + subcommand->min_args ||
                argc > 1 + subcommand->max_args) {
                char *help = g_option_context_get_help (context, TRUE, NULL);
                fprintf(stderr, "%s", help);
                exit(1);
            }

            return subcommand->run(argc, argv);
        }
    }

    global_usage();
}
