/* -*- mode: C; c-file-style: "stroustrup"; indent-tabs-mode: nil; -*- */

#include "remote-player.h"
#include "system-state.h"
#include "test-runner.h"

struct _GbbTestRunner {
    GObject parent;

    GbbPowerMonitor *monitor;
    GbbEventPlayer *player;
    GbbSystemState *system_state;

    GbbBatteryTest *test;
    GbbTestRun *run;

    GbbTestPhase phase;
    gboolean stop_requested;
    gboolean force_stop;
};

struct _GbbTestRunnerClass {
    GObjectClass parent_class;
};

enum {
    PHASE_CHANGED,
    LAST_SIGNAL
};

static guint signals[LAST_SIGNAL];

G_DEFINE_TYPE(GbbTestRunner, gbb_test_runner, G_TYPE_OBJECT)

static void
runner_set_phase(GbbTestRunner *runner,
                 GbbTestPhase   phase)
{
    if (runner->phase == phase)
        return;

    runner->phase = phase;
    g_signal_emit(runner, signals[PHASE_CHANGED], 0);
}

static void
runner_set_stopped(GbbTestRunner *runner)
{
    gbb_system_state_restore(runner->system_state);

    runner_set_phase(runner, GBB_TEST_PHASE_STOPPED);
}

static void
runner_set_epilogue(GbbTestRunner *runner)
{
    if (runner->test->epilogue_file) {
        gbb_event_player_play_file(runner->player, runner->test->epilogue_file);
        runner_set_phase(runner, GBB_TEST_PHASE_EPILOGUE);
    } else {
        runner_set_stopped(runner);
    }
}

static void
on_player_finished(GbbEventPlayer *player,
                   GbbTestRunner  *runner)
{
    if (runner->force_stop) {
        runner->force_stop = FALSE;
        runner_set_stopped(runner);
        return;
    }

    if (runner->phase == GBB_TEST_PHASE_PROLOGUE) {
        runner_set_phase(runner, GBB_TEST_PHASE_WAITING);

        if (runner->stop_requested) {
            runner->stop_requested = FALSE;
            gbb_test_runner_stop(runner);
        }
    } else if (runner->phase == GBB_TEST_PHASE_RUNNING) {
        if (gbb_test_run_is_done(runner->run))
            runner_set_epilogue(runner);
        else
            gbb_event_player_play_file(player, runner->test->loop_file);
    } else if (runner->phase == GBB_TEST_PHASE_STOPPING) {
        runner_set_epilogue(runner);
    } else if (runner->phase == GBB_TEST_PHASE_EPILOGUE) {
        runner_set_stopped(runner);
    }
}

static void
on_power_monitor_changed(GbbPowerMonitor *monitor,
                         GbbTestRunner   *runner)
{
    const GbbPowerState *current_state = gbb_power_monitor_get_state(monitor);

    if (runner->phase == GBB_TEST_PHASE_WAITING) {
        if (!current_state->online) {
            gbb_test_run_set_start_time(runner->run, time(NULL));
            gbb_test_run_add(runner->run, current_state);
            runner_set_phase(runner, GBB_TEST_PHASE_RUNNING);
            gbb_event_player_play_file(runner->player, runner->test->loop_file);
        }
    } else if (runner->phase == GBB_TEST_PHASE_RUNNING) {
        gbb_test_run_add(runner->run, current_state);
    }
}

static void
gbb_test_runner_finalize(GObject *object)
{
    GbbTestRunner *runner = GBB_TEST_RUNNER(object);

    g_clear_object(&runner->run);

    G_OBJECT_CLASS(gbb_test_runner_parent_class)->finalize(object);
}

static void
gbb_test_runner_init(GbbTestRunner *runner)
{
    runner->monitor = gbb_power_monitor_new();
    g_signal_connect(runner->monitor, "changed",
                     G_CALLBACK(on_power_monitor_changed),
                     runner);

    runner->system_state = gbb_system_state_new();

    runner->player = GBB_EVENT_PLAYER(gbb_remote_player_new("GNOME Battery Bench"));
    g_signal_connect(runner->player, "finished",
                     G_CALLBACK(on_player_finished), runner);
}

static void
gbb_test_runner_class_init(GbbTestRunnerClass *run_class)
{
    GObjectClass *gobject_class = G_OBJECT_CLASS (run_class);

    gobject_class->finalize = gbb_test_runner_finalize;

    signals[PHASE_CHANGED] =
        g_signal_new ("phase-changed",
                      GBB_TYPE_TEST_RUNNER,
                      G_SIGNAL_RUN_LAST,
                      0,
                      NULL, NULL, NULL,
                      G_TYPE_NONE, 0);
}

GbbTestRunner *
gbb_test_runner_new(void)
{
    GbbTestRunner *runner = g_object_new(GBB_TYPE_TEST_RUNNER, NULL);

    return runner;
}

GbbPowerMonitor *
gbb_test_runner_get_power_monitor(GbbTestRunner *runner)
{
    return runner->monitor;
}

GbbEventPlayer *
gbb_test_runner_get_event_player(GbbTestRunner *runner)
{
    return runner->player;
}

GbbTestPhase
gbb_test_runner_get_phase(GbbTestRunner *runner)
{
    return runner->phase;
}

gboolean
gbb_test_runner_get_stop_requested(GbbTestRunner *runner)
{
    return runner->stop_requested;
}

void
gbb_test_runner_set_run(GbbTestRunner *runner,
                        GbbTestRun    *run)
{
    g_return_if_fail(runner->phase == GBB_TEST_PHASE_STOPPED);

    g_clear_object(&runner->run);

    runner->run = g_object_ref(run);
    runner->test = gbb_test_run_get_test(run);
}

GbbTestRun *
gbb_test_runner_get_run(GbbTestRunner *runner)
{
    return runner->run;
}

void
gbb_test_runner_start(GbbTestRunner *runner)
{
    g_return_if_fail(runner->phase == GBB_TEST_PHASE_STOPPED);
    g_return_if_fail(runner->run != NULL);

    gbb_system_state_save(runner->system_state);
    gbb_system_state_set_brightnesses(runner->system_state,
                                      gbb_test_run_get_screen_brightness(runner->run),
                                      0);

    if (runner->test->prologue_file) {
        gbb_event_player_play_file(runner->player, runner->test->prologue_file);
        runner_set_phase(runner, GBB_TEST_PHASE_PROLOGUE);
    } else {
        runner_set_phase(runner, GBB_TEST_PHASE_WAITING);
    }
}

void
gbb_test_runner_stop(GbbTestRunner *runner)
{
    if ((runner->phase == GBB_TEST_PHASE_WAITING || runner->phase == GBB_TEST_PHASE_RUNNING)) {
        if (runner->phase == GBB_TEST_PHASE_RUNNING) {
            gbb_event_player_stop(runner->player);
            runner_set_phase(runner, GBB_TEST_PHASE_STOPPING);
        } else {
            runner_set_epilogue(runner);
        }
    } else if (runner->phase == GBB_TEST_PHASE_PROLOGUE) {
        runner->stop_requested = TRUE;
    }
}

void
gbb_test_runner_force_stop(GbbTestRunner *runner)
{
    switch (runner->phase) {
    case GBB_TEST_PHASE_STOPPED:
        /* Nothing to do here */
        return;

    case GBB_TEST_PHASE_PROLOGUE:
    case GBB_TEST_PHASE_RUNNING:
    case GBB_TEST_PHASE_EPILOGUE:
        /* Player is active in these phases, needs to
         * be stopped. */
        gbb_event_player_stop(runner->player);
        runner_set_phase(runner, GBB_TEST_PHASE_STOPPING);

        /* FALLTHROUGH to set runner->force_stop */
    case GBB_TEST_PHASE_STOPPING:
        /* We are stopping the player already,
         * but we must not play the epilogue */
        runner->force_stop = TRUE;
        break;

    case GBB_TEST_PHASE_WAITING:
        /* No active player, transition directly to the
         * STOPPED phase. */
        runner_set_phase(runner, GBB_TEST_PHASE_STOPPED);
        break;
    }
}
