#ifndef __BATTERY_TEST_H__
#define __BATTERY_TEST_H__

#include <glib-object.h>

typedef struct _GbbBatteryTest GbbBatteryTest;

struct _GbbBatteryTest {
    char *id;
    char *name;
    char *path;
    char *description;
    char *prologue_file;
    char *loop_file;
    char *epilogue_file;
};

GbbBatteryTest *gbb_battery_test_get_for_id(const char *id);
GList          *gbb_battery_test_list_all  (void);

#endif /* __BATTERY_TEST_H__ */
