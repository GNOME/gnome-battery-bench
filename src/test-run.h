/* -*- mode: C; c-file-style: "stroustrup"; indent-tabs-mode: nil; -*- */

#ifndef __TEST_RUN_H__
#define __TEST_RUN_H__

#include <gio/gio.h>

#include "battery-test.h"
#include "power-monitor.h"

typedef struct _GbbTestRun GbbTestRun;
typedef struct _GbbTestRunClass GbbTestRunClass;

#define GBB_TYPE_TEST_RUN         (gbb_test_run_get_type ())
#define GBB_TEST_RUN(o)           (G_TYPE_CHECK_INSTANCE_CAST ((o), GBB_TYPE_TEST_RUN, GbbTestRun))
#define GBB_TEST_RUN_CLASS(k)     (G_TYPE_CHECK_CLASS_CAST((k), GBB_TYPE_TEST_RUN, GbbTestRunClass))
#define GBB_IS_TEST_RUN(o)        (G_TYPE_CHECK_INSTANCE_TYPE ((o), GBB_TYPE_TEST_RUN))
#define GBB_IS_TEST_RUN_CLASS(k)  (G_TYPE_CHECK_CLASS_TYPE ((k), GBB_TYPE_TEST_RUN))
#define GBB_TEST_RUN_GET_CLASS(o) (G_TYPE_INSTANCE_GET_CLASS ((o), GBB_TYPE_TEST_RUN, GbbTestRunClass))

typedef enum {
    GBB_DURATION_TIME,
    GBB_DURATION_PERCENT
} GbbDurationType;

GType gbb_test_run_get_type(void);

GbbTestRun *gbb_test_run_new(GbbBatteryTest *test);

GbbTestRun *gbb_test_run_new_from_file(const char *filename,
                                       GError    **error);

GbbBatteryTest *gbb_test_run_get_test(GbbTestRun *run);

void   gbb_test_run_set_start_time(GbbTestRun *run,
                                   gint64      t);
gint64 gbb_test_run_get_start_time(GbbTestRun *run);

void            gbb_test_run_set_duration_time    (GbbTestRun *run,
                                                   double      duration_seconds);
void            gbb_test_run_set_duration_percent (GbbTestRun *run,
                                                   double      percent);

GbbDurationType gbb_test_run_get_duration_type    (GbbTestRun *run);
double          gbb_test_run_get_duration_time    (GbbTestRun *run);
double          gbb_test_run_get_duration_percent (GbbTestRun *run);

gboolean        gbb_test_run_is_done              (GbbTestRun *run);

void            gbb_test_run_set_screen_brightness (GbbTestRun *run,
                                                    int         screen_brightness);
int             gbb_test_run_get_screen_brightness (GbbTestRun *run);

void gbb_test_run_add(GbbTestRun          *run,
                      const GbbPowerState *state);

GbbBatteryTest *gbb_test_run_get_test      (GbbTestRun *run);
double          gbb_test_run_get_loop_time (GbbTestRun *run);
const char     *gbb_test_run_get_filename  (GbbTestRun *run);
const char     *gbb_test_run_get_name        (GbbTestRun *run);
const char     *gbb_test_run_get_description (GbbTestRun *run);

GQueue         *gbb_test_run_get_history(GbbTestRun *run);

const GbbPowerState *gbb_test_run_get_start_state (GbbTestRun *run);
const GbbPowerState *gbb_test_run_get_last_state  (GbbTestRun *run);

double          gbb_test_run_get_max_power        (GbbTestRun *run);
double          gbb_test_run_get_max_battery_life (GbbTestRun *run);

char *gbb_test_run_get_default_path(GbbTestRun *run,
                                    GFile      *folder);

gboolean gbb_test_run_write_to_file(GbbTestRun *run,
                                    const char *filename,
                                    GError    **error);

#endif /* __TEST_RUN_H__ */

