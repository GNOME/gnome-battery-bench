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

void usage(void)
    __attribute__ ((noreturn));

void
usage(void)
{
    die("Usage: gbb [--record|--playback|--remote-playback|--test]");
}

static void
record(void)
{
    GbbEventRecorder *recorder;

    Display *display = XOpenDisplay(NULL);
    if (!display)
        die("Can't open X display %s", XDisplayName(NULL));

    recorder = gbb_event_recorder_new(display, NULL);
    gbb_event_recorder_record(recorder);
    gbb_event_recorder_free(recorder);
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

static void
do_playback(GbbEventPlayer *player,
            const char     *filename)
{
    GMainLoop *loop = g_main_loop_new (NULL, FALSE);

    gbb_xinput_wait(player, NULL,
                    NULL, on_player_ready, loop);
    g_main_loop_run (loop);

    g_signal_connect(player, "finished",
                     G_CALLBACK(on_player_finished), loop);
    gbb_event_player_play_file(player, filename);
    g_main_loop_run (loop);
}

static void
playback(const char *filename)
{
    GbbEventPlayer *player = GBB_EVENT_PLAYER(gbb_evdev_player_new("Gnome Battery Bench Test"));
    do_playback(player, filename);
}

static void
remote_playback(const char *filename)
{
    GbbEventPlayer *player = GBB_EVENT_PLAYER(gbb_remote_player_new("Gnome Battery Bench Test"));
    do_playback(player, filename);
}

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

static void
test(void)
{
    GbbPowerMonitor *monitor;
    GMainLoop *loop;

    monitor = gbb_power_monitor_new();

    g_signal_connect(monitor, "changed",
                     G_CALLBACK(on_power_monitor_changed), NULL);

    loop = g_main_loop_new (NULL, FALSE);
    g_main_loop_run (loop);
}

int
main(int argc, char **argv)
{
    if (argc < 2)
        usage();

    if (strcmp(argv[1], "--record") == 0) {
        if (argc != 2)
            usage();

        record();
    } else if (strcmp(argv[1], "--playback") == 0) {
        if (argc != 3)
            usage();

        playback(argv[2]);
    } else if (strcmp(argv[1], "--remote-playback") == 0) {
        if (argc != 3)
            usage();

        remote_playback(argv[2]);
    } else if (strcmp(argv[1], "--test") == 0) {
        if (argc != 2)
            usage();

        test();
    } else {
        usage();
    }

    return 0;
}
