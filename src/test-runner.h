/* -*- mode: C; c-file-style: "stroustrup"; indent-tabs-mode: nil; -*- */

#ifndef __TEST_RUNNER_H__
#define __TEST_RUNNER_H__

#include "event-player.h"
#include "power-monitor.h"
#include "test-run.h"

typedef struct _GbbTestRunner GbbTestRunner;
typedef struct _GbbTestRunnerClass GbbTestRunnerClass;

#define GBB_TYPE_TEST_RUNNER         (gbb_test_runner_get_type ())
#define GBB_TEST_RUNNER(o)           (G_TYPE_CHECK_INSTANCE_CAST ((o), GBB_TYPE_TEST_RUNNER, GbbTestRunner))
#define GBB_TEST_RUNNER_CLASS(k)     (G_TYPE_CHECK_CLASS_CAST((k), GBB_TYPE_TEST_RUNNER, GbbTestRunnerClass))
#define GBB_IS_TEST_RUNNER(o)        (G_TYPE_CHECK_INSTANCE_TYPE ((o), GBB_TYPE_TEST_RUNNER))
#define GBB_IS_TEST_RUNNER_CLASS(k)  (G_TYPE_CHECK_CLASS_TYPE ((k), GBB_TYPE_TEST_RUNNER))
#define GBB_TEST_RUNNER_GET_CLASS(o) (G_TYPE_INSTANCE_GET_CLASS ((o), GBB_TYPE_TEST_RUNNER, GbbTestRunnerClass))

typedef enum {
    GBB_TEST_PHASE_STOPPED,
    GBB_TEST_PHASE_PROLOGUE,
    GBB_TEST_PHASE_WAITING,
    GBB_TEST_PHASE_RUNNING,
    GBB_TEST_PHASE_STOPPING,
    GBB_TEST_PHASE_EPILOGUE
} GbbTestPhase;

GType gbb_test_runner_get_type(void);

GbbTestRunner *gbb_test_runner_new(void);

GbbPowerMonitor *gbb_test_runner_get_power_monitor(GbbTestRunner *runner);
GbbEventPlayer  *gbb_test_runner_get_event_player (GbbTestRunner *runner);

GbbTestPhase gbb_test_runner_get_phase         (GbbTestRunner *runner);
gboolean     gbb_test_runner_get_stop_requested(GbbTestRunner *runner);

void gbb_test_runner_set_run(GbbTestRunner *runner,
                             GbbTestRun    *run);
GbbTestRun *gbb_test_runner_get_run(GbbTestRunner *runner);

void gbb_test_runner_start(GbbTestRunner *runner);
void gbb_test_runner_stop (GbbTestRunner *runner);
void gbb_test_runner_force_stop(GbbTestRunner *runner);

#endif /* __TEST_RUNNER_H__ */

