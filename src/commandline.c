/* -*- mode: C; c-file-style: "stroustrup"; indent-tabs-mode: nil; -*- */

#include <dirent.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/poll.h>
#include <unistd.h>

#include <X11/Xlib.h>

#include <glib.h>
#include <gio/gio.h>

#include "evdev-player.h"
#include "remote-player.h"
#include "event-recorder.h"
#include "power-monitor.h"
#include "xinput-wait.h"
#include "util.h"

static GbbPowerState *start_state;

static void
on_power_monitor_changed(GbbPowerMonitor *monitor)
{
    if (start_state == NULL) {
        start_state = gbb_power_state_copy(gbb_power_monitor_get_state(monitor));
        return;
    }

    const GbbPowerState *state = gbb_power_monitor_get_state(monitor);
    GbbPowerStatistics *statistics = gbb_power_statistics_compute(start_state, state);

    if (statistics->power >= 0)
        g_print("Power: %.2f W\n", statistics->power);
    if (statistics->current >= 0)
        g_print("Current: %.2f A\n", statistics->current);
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
    GbbEventPlayer *player = GBB_EVENT_PLAYER(gbb_evdev_player_new("Gnome Battery Bench Test"));
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

typedef struct {
    const char *command;
    const GOptionEntry *options;
    int (*run) (int argc, char **argv);
    int min_args;
    int max_args;
    const char *param_string;
} Subcommand;

Subcommand subcommands[] = {
    { "monitor",      monitor_options, monitor, 0, 0 },
    { "play",         play_options, play, 1, 1, "FILENAME" },
    { "play-local",   play_options, play_local, 1, 1, "FILENAME" },
    { "record",       record_options, record, 0, 0 },
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
