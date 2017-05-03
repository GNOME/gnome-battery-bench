/* -*- mode: C; c-file-style: "stroustrup"; indent-tabs-mode: nil; -*- */

#define _XOPEN_SOURCE
#include <time.h>

#include <json-glib/json-glib.h>

#include "event-log.h"
#include "system-info.h"
#include "test-run.h"
#include "util.h"

struct _GbbTestRun {
    GObject parent;

    GbbBatteryTest *test;
    char *id;
    char *filename;
    char *name;
    char *description;

    GQueue *history;
    gint64 start_time;

    GbbDurationType duration_type;
    union {
        double seconds;
        double percent;
    } duration;

    int screen_brightness;

    double max_power;
    double max_life;
    double loop_time;
};

struct _GbbTestRunClass {
    GObjectClass parent_class;
};

enum {
    UPDATED,
    LAST_SIGNAL
};

static guint signals[LAST_SIGNAL];

G_DEFINE_TYPE(GbbTestRun, gbb_test_run, G_TYPE_OBJECT)

static void
gbb_test_run_finalize(GObject *object)
{
    GbbTestRun *run = GBB_TEST_RUN(object);

    g_queue_free_full(run->history, (GDestroyNotify)gbb_power_state_free);
    g_free(run->filename);
    g_free(run->name);
    g_free(run->description);

    G_OBJECT_CLASS(gbb_test_run_parent_class)->finalize(object);
}

static void
gbb_test_run_init(GbbTestRun *run)
{
    run->id = uuid_gen_new();
    run->history = g_queue_new();
}

static void
gbb_test_run_class_init(GbbTestRunClass *run_class)
{
    GObjectClass *gobject_class = G_OBJECT_CLASS (run_class);

    gobject_class->finalize = gbb_test_run_finalize;

    signals[UPDATED] =
        g_signal_new ("updated",
                      GBB_TYPE_TEST_RUN,
                      G_SIGNAL_RUN_LAST,
                      0,
                      NULL, NULL, NULL,
                      G_TYPE_NONE, 0);
}

GbbTestRun *
gbb_test_run_new(GbbBatteryTest *test)
{
    GbbTestRun *run = g_object_new(GBB_TYPE_TEST_RUN, NULL);

    run->test = test;
    run->name = g_strdup(test->name);
    run->description = g_strdup(test->description);

    GFile *loop_file = g_file_new_for_path(run->test->loop_file);
    GError *error = NULL;
    run->loop_time = gbb_event_log_duration(loop_file, NULL, &error) / 1000.;
    if (error)
        die("Can't get duration of .loop file: %s", error->message);
    g_object_unref(loop_file);

    return run;
}

void
gbb_test_run_set_start_time(GbbTestRun *run,
                            gint64      t)
{
    run->start_time = t;
}

gint64
gbb_test_run_get_start_time(GbbTestRun *run)
{
    return run->start_time;
}

void
gbb_test_run_set_duration_time (GbbTestRun *run,
                                double      duration_seconds)
{
    run->duration_type = GBB_DURATION_TIME;
    run->duration.seconds = duration_seconds;
}

void
gbb_test_run_set_duration_percent (GbbTestRun *run,
                                   double      percent)
{
    run->duration_type = GBB_DURATION_PERCENT;
    run->duration.percent = percent;
}

GbbDurationType
gbb_test_run_get_duration_type (GbbTestRun *run)
{
    return run->duration_type;
}

double
gbb_test_run_get_duration_time (GbbTestRun *run)
{
    g_return_val_if_fail(run->duration_type == GBB_DURATION_TIME, -1.0);
    return run->duration.seconds;
}

double
gbb_test_run_get_duration_percent (GbbTestRun *run)
{
    g_return_val_if_fail(run->duration_type == GBB_DURATION_PERCENT, -1.0);
    return run->duration.percent;
}


gboolean
gbb_test_run_is_done (GbbTestRun *run)
{
    const GbbPowerState *start_state = gbb_test_run_get_start_state(run);
    const GbbPowerState *last_state = gbb_test_run_get_last_state(run);

    if (run->duration_type == GBB_DURATION_TIME)
        return (last_state->time_us - start_state->time_us) / 1000000. > run->duration.seconds;
    else
        return gbb_power_state_get_percent(last_state) < run->duration.percent;
}

void
gbb_test_run_set_screen_brightness (GbbTestRun *run,
                                    int         screen_brightness)
{
    run->screen_brightness = screen_brightness;
}

int
gbb_test_run_get_screen_brightness (GbbTestRun *run)
{
    return run->screen_brightness;
}

static void
test_run_add_internal(GbbTestRun    *run,
                      GbbPowerState *state)
{
    GbbPowerState *start_state = run->history->head ? run->history->head->data : NULL;
    GbbPowerState *last_state = run->history->tail ? run->history->tail->data : NULL;
    gboolean use_this_state = FALSE;

    if (!start_state) {
        use_this_state = TRUE;
    } else {
        switch (run->duration_type) {
        case GBB_DURATION_TIME:
            if (state->time_us - last_state->time_us > run->duration.seconds * 1000000. / 100.)
                use_this_state = TRUE;
            break;
        case GBB_DURATION_PERCENT:
        {
            double start_percent = gbb_power_state_get_percent(start_state);
            double last_percent = gbb_power_state_get_percent(last_state);
            double current_percent = gbb_power_state_get_percent(state);

            if ((last_percent - current_percent) / (start_percent - run->duration.percent) > 0.005)
                use_this_state = TRUE;
            break;
        }
        }
    }

    if (!use_this_state) {
        gbb_power_state_free(state);
        return;
    }

    g_queue_push_tail(run->history, state);

    if (start_state) {
        GbbPowerStatistics *overall_stats = gbb_power_statistics_compute(start_state, state);
        run->max_life = MAX(overall_stats->battery_life, run->max_life);

        GbbPowerStatistics *interval_stats = gbb_power_statistics_compute(last_state, state);
        run->max_power = MAX(interval_stats->power, run->max_power);

        gbb_power_statistics_free(interval_stats);
        gbb_power_statistics_free(overall_stats);
    }

    g_signal_emit(run, signals[UPDATED], 0);
}

void
gbb_test_run_add(GbbTestRun          *run,
                 const GbbPowerState *state)
{
    test_run_add_internal(run, gbb_power_state_copy(state));
    g_signal_emit(run, signals[UPDATED], 0);
}

GbbBatteryTest *
gbb_test_run_get_test(GbbTestRun *run)
{
    return run->test;
}

double
gbb_test_run_get_loop_time(GbbTestRun *run)
{
    return run->loop_time;
}

const char *
gbb_test_run_get_filename(GbbTestRun *run)
{
    return run->filename;
}

const char *
gbb_test_run_get_name (GbbTestRun *run)
{
    return run->name;
}

const char *
gbb_test_run_get_description (GbbTestRun *run)
{
    return run->description;
}

GQueue *
gbb_test_run_get_history(GbbTestRun *run)
{
    return run->history;
}


const GbbPowerState *
gbb_test_run_get_start_state (GbbTestRun *run)
{
    return run->history->head ? run->history->head->data : NULL;
}

const GbbPowerState *
gbb_test_run_get_last_state (GbbTestRun *run)
{
    return run->history->tail ? run->history->tail->data : NULL;
}

double
gbb_test_run_get_max_power(GbbTestRun *run)
{
    return run->max_power;
}

double
gbb_test_run_get_max_battery_life(GbbTestRun *run)
{
    return run->max_life;
}

static void
add_int_value_1e6(JsonBuilder *builder,
                  double       value)
{
    json_builder_add_int_value(builder, (gint64)(0.5 + 1e6 * value));
}

gboolean
gbb_test_run_write_to_file(GbbTestRun *run,
                           const char *filename,
                           GError    **error)
{
    JsonBuilder *builder = json_builder_new();

    json_builder_begin_object(builder);
    json_builder_set_member_name(builder, "id");
    json_builder_add_string_value(builder, run->id);
    json_builder_set_member_name(builder, "test-id");
    json_builder_add_string_value(builder, run->test->id);
    json_builder_set_member_name(builder, "test-name");
    json_builder_add_string_value(builder, run->name);
    if (run->description) {
        json_builder_set_member_name(builder, "test-description");
        json_builder_add_string_value(builder, run->description);
    }
    if (run->duration_type == GBB_DURATION_TIME) {
        json_builder_set_member_name(builder, "duration-seconds");
        json_builder_add_double_value(builder, run->duration.seconds);
    } else  {
        json_builder_set_member_name(builder, "until-percent");
        json_builder_add_double_value(builder, run->duration.percent);
    }
    json_builder_set_member_name(builder, "screen-brightness");
    json_builder_add_int_value(builder, run->screen_brightness);

    if (run->start_time != 0) {
        GDateTime *start = g_date_time_new_from_unix_utc(run->start_time);
        char *start_string = g_date_time_format (start, "%F %T");
        json_builder_set_member_name(builder, "start-time");
        json_builder_add_string_value(builder, start_string);
        g_date_time_unref(start);
        g_free(start_string);
    }

    /* probably better to do that when we create the run */
    g_autoptr(GbbSystemInfo) info = gbb_system_info_acquire();
    json_builder_set_member_name(builder, "system-info");
    gbb_system_info_to_json(info, builder);

    const GbbPowerState *start_state = gbb_test_run_get_start_state(run);
    const GbbPowerState *end_state = gbb_test_run_get_last_state(run);
    if (end_state != start_state) {
        /* The statistics aren't needed for reading the data back into the UI,
         * but are useful if the ouput files are going to be read by some other
         * consumer.
         */
        GbbPowerStatistics *statistics = gbb_power_statistics_compute(start_state, end_state);
        if (statistics->power > 0) {
            json_builder_set_member_name(builder, "power");
            json_builder_add_double_value(builder, statistics->power);
        }
        if (statistics->current > 0) {
            json_builder_set_member_name(builder, "current");
            json_builder_add_double_value(builder, statistics->current);
        }
        if (statistics->battery_life > 0) {
            json_builder_set_member_name(builder, "estimated-life");
            json_builder_add_double_value(builder, statistics->battery_life);
        }
        if (statistics->battery_life_design > 0) {
            json_builder_set_member_name(builder, "estimated-life-design");
            json_builder_add_double_value(builder, statistics->battery_life_design);
        }

        gbb_power_statistics_free(statistics);
    }

    json_builder_set_member_name(builder, "log");
    json_builder_begin_array(builder);
    GList *l;

    const GbbPowerState *last_state = NULL;
    for (l = run->history->head; l; l = l->next) {
        const GbbPowerState *state = l->data;

        json_builder_begin_object(builder);
        json_builder_set_member_name(builder, "time-ms");
        json_builder_add_int_value(builder, (500 + state->time_us - start_state->time_us) / 1000);
        if (!last_state || state->online != last_state->online) {
            json_builder_set_member_name(builder, "online");
            json_builder_add_boolean_value(builder, state->online);
        }
        if (state->energy_now >= 0) {
            json_builder_set_member_name(builder, "energy");
            add_int_value_1e6(builder, state->energy_now);
        }
        if (state->energy_full >= 0 && (!last_state || state->energy_full != last_state->energy_full)) {
            json_builder_set_member_name(builder, "energy-full");
            add_int_value_1e6(builder, state->energy_full);
        }
        if (state->energy_full_design >= 0 && (!last_state || state->energy_full_design != last_state->energy_full_design)) {
            json_builder_set_member_name(builder, "energy-full-design");
            add_int_value_1e6(builder, state->energy_full_design);
        }

        json_builder_end_object(builder);
        last_state = state;
    }

    json_builder_end_array(builder);

    json_builder_end_object(builder);

    JsonNode *root = json_builder_get_root(builder);

    JsonGenerator *generator = json_generator_new();
    json_generator_set_pretty(generator, TRUE);
    json_generator_set_root(generator, root);

    /* Small hack to force a fsync() on the file: pre-create the file
     * with non-zero content which will make g_file_set_contents()
     * call fsync() since this is now an atomic replace. We don't
     * really care this first g_file_set_contents fails.
     */
    g_file_set_contents(filename, "gnome-battery-bench", 20, NULL);
    gsize len;
    gchar *buffer = json_generator_to_data (generator, &len);
    gboolean success = g_file_set_contents(filename, buffer, len, error);
    g_free(buffer);

    g_object_unref(generator);
    json_node_free(root);
    g_object_unref(builder);

    if (success) {
        g_free(run->filename);
        run->filename = g_strdup(filename);
    }

    return success;
}

char *
gbb_test_run_get_default_path(GbbTestRun *run,
                              GFile      *folder)
{
    g_return_val_if_fail(run->start_time != 0, NULL);
    g_return_val_if_fail(run->test != NULL, NULL);

    GDateTime *start_datetime = g_date_time_new_from_unix_utc(run->start_time);
    char *start_string = g_date_time_format(start_datetime, "%F-%T");
    char *file_name = g_strdup_printf("%s-%s.json", start_string, run->test->id);
    GFile *file = g_file_get_child(folder, file_name);
    g_free(file_name);
    g_free(start_string);
    g_date_time_unref(start_datetime);

    char *file_path = g_file_get_path(file);

    g_object_unref(file);

    return file_path;
}

typedef enum {
    MISSING, ERROR, OK
} GetResult;

static GetResult
get_double(JsonObject *object,
           const char *member_name,
           double     *v_double,
           GError    **error)
{
    JsonNode *member = json_object_get_member(object, member_name);
    if (member == NULL)
        return MISSING;

    GType value_type = json_node_get_value_type(member);
    if (value_type == G_TYPE_INT64) {
        *v_double = json_node_get_int(member);
        return OK;
    } else if (value_type == G_TYPE_DOUBLE) {
        *v_double = json_node_get_double(member);
        return OK;
    } else {
        g_set_error(error, G_IO_ERROR, G_IO_ERROR_FAILED,
                    "value for '%s' is not a double", member_name);
        return ERROR;
    }
}

static GetResult
get_int(JsonObject *object,
        const char *member_name,
        gint64     *v_int,
        GError    **error)
{
    JsonNode *member = json_object_get_member(object, member_name);
    if (member == NULL)
        return MISSING;

    GType value_type = json_node_get_value_type(member);
    if (value_type == G_TYPE_INT64) {
        *v_int = json_node_get_int(member);
        return OK;
    } else {
        g_set_error(error, G_IO_ERROR, G_IO_ERROR_FAILED,
                    "value for '%s' is not a integer", member_name);
        return ERROR;
    }
}

static GetResult
get_int_1e6(JsonObject *object,
            const char *member_name,
            double     *v_double,
            GError    **error)
{
    JsonNode *member = json_object_get_member(object, member_name);
    if (member == NULL)
        return MISSING;

    GType value_type = json_node_get_value_type(member);
    if (value_type == G_TYPE_INT64) {
        *v_double = json_node_get_int(member) / 1e6;
        return OK;
    } else {
        g_set_error(error, G_IO_ERROR, G_IO_ERROR_FAILED,
                    "value for '%s' is not a integer", member_name);
        return ERROR;
    }
}

static GetResult
get_string(JsonObject  *object,
           const char  *member_name,
           const char **v_string,
           GError     **error)
{
    JsonNode *member = json_object_get_member(object, member_name);
    if (member == NULL)
        return MISSING;

    GType value_type = json_node_get_value_type(member);
    if (value_type == G_TYPE_STRING) {
        *v_string = json_node_get_string(member);
        return OK;
    } else {
        g_set_error(error, G_IO_ERROR, G_IO_ERROR_FAILED,
                    "value for '%s' is not a string", member_name);
        return ERROR;
    }
}

static GetResult
get_boolean(JsonObject  *object,
            const char  *member_name,
            gboolean    *v_boolean,
            GError     **error)
{
    JsonNode *member = json_object_get_member(object, member_name);
    if (member == NULL)
        return MISSING;

    GType value_type = json_node_get_value_type(member);
    if (value_type == G_TYPE_BOOLEAN) {
        *v_boolean = json_node_get_boolean(member);
        return OK;
    } else {
        g_set_error(error, G_IO_ERROR, G_IO_ERROR_FAILED,
                    "value for '%s' is not a boolean", member_name);
        return ERROR;
    }
}

static GetResult
get_array(JsonObject  *object,
          const char  *member_name,
          JsonArray  **v_array,
          GError     **error)
{
    JsonNode *member = json_object_get_member(object, member_name);
    if (member == NULL)
        return MISSING;

    if (JSON_NODE_HOLDS_ARRAY(member)) {
        *v_array = json_node_get_array(member);
        return OK;
    } else {
        g_set_error(error, G_IO_ERROR, G_IO_ERROR_FAILED,
                    "value for '%s' is not an array", member_name);
        return ERROR;
    }
}

static gboolean
read_from_file(GbbTestRun *run,
               const char *filename,
               GError    **error)
{
    JsonParser *parser = json_parser_new();
    gboolean success = FALSE;
    GbbPowerState *state = NULL;

    if (!json_parser_load_from_file(parser, filename, error))
        goto out;

    JsonNode *root = json_parser_get_root(parser);
    if (!JSON_NODE_HOLDS_OBJECT(root)) {
        g_set_error(error, G_IO_ERROR, G_IO_ERROR_FAILED,
                    "Root node is not an object");
        goto out;
    }
    JsonObject *root_object = json_node_get_object(root);

    double v_double;
    gint64 v_int;
    const char *v_string;
    gboolean v_boolean;
    JsonArray *v_array;

    /* We could save it, but it's not really useful for a historical log */
    run->loop_time = 0.0;

    switch (get_string(root_object, "test-name", &v_string, error)) {
    case MISSING: break;
    case ERROR: goto out;
    case OK: run->name = g_strdup(v_string); break;
    }

    switch (get_string(root_object, "test-description", &v_string, error)) {
    case MISSING: break;
    case ERROR: goto out;
    case OK: run->description = g_strdup(v_string); break;
    }

    switch (get_double(root_object, "duration-seconds", &v_double, error)) {
    case MISSING: break;
    case ERROR: goto out;
    case OK: gbb_test_run_set_duration_time(run, v_double); break;
    }

    switch (get_double(root_object, "until-percent", &v_double, error)) {
    case MISSING: break;
    case ERROR: goto out;
    case OK: gbb_test_run_set_duration_percent(run, v_double); break;
    }

    switch (get_int(root_object, "screen-brightness", &v_int, error)) {
    case MISSING: break;
    case ERROR: goto out;
    case OK: gbb_test_run_set_screen_brightness(run, v_int); break;
    }

    switch (get_string(root_object, "start-time", &v_string, error)) {
    case MISSING: break;
    case ERROR: goto out;
    case OK: {
        char *stripped = g_strstrip(g_strdup(v_string));

        struct tm tm;
        char *result = strptime(stripped, "%F %T", &tm);
        if (result == NULL || *result != '\0') {
            g_set_error(error, G_IO_ERROR, G_IO_ERROR_FAILED,
                        "Cannot parse start-time");
            g_free(stripped);
            goto out;
        }
        g_free(stripped);

        GDateTime *datetime = g_date_time_new_utc(1900 + tm.tm_year, tm.tm_mon + 1, tm.tm_mday,
                                                  tm.tm_hour, tm.tm_min, tm.tm_sec);
        if (datetime == NULL) {
            g_set_error(error, G_IO_ERROR, G_IO_ERROR_FAILED,
                        "Cannot convert start-time to time");
            goto out;
        }

        gbb_test_run_set_start_time(run, g_date_time_to_unix(datetime));
        g_date_time_unref(datetime);
    }}

    switch (get_array(root_object, "log", &v_array, error)) {
    case MISSING: break;
    case ERROR: goto out;
    case OK: {
        int count = json_array_get_length(v_array);
        GbbPowerState *last_state = NULL;

        int i;
        for (i = 0; i < count; i++) {
            state = gbb_power_state_new();
            if (last_state)
                *state = *last_state;

            JsonNode *node = json_array_get_element(v_array, i);
            if (!JSON_NODE_HOLDS_OBJECT(node)) {
                g_set_error(error, G_IO_ERROR, G_IO_ERROR_FAILED,
                            "Log element isn't an object");
                goto out;
            }

            JsonObject *node_object = json_node_get_object(node);

            switch (get_int(node_object, "time-ms", &v_int, error)) {
            case MISSING: break;
            case ERROR: goto out;
            case OK: state->time_us = v_int * 1000; break;
            }

            switch (get_boolean(node_object, "online", &v_boolean, error)) {
            case MISSING: break;
            case ERROR: goto out;
            case OK: state->online = v_boolean;
            }

            if (get_int_1e6(node_object, "energy", &state->energy_now, error) == ERROR)
                goto out;
            if (get_int_1e6(node_object, "energy-full", &state->energy_full, error) == ERROR)
                goto out;
            if (get_int_1e6(node_object, "energy-full-design", &state->energy_full_design, error) == ERROR)
                goto out;

            test_run_add_internal(run, state);
            last_state = state;

            state = NULL;
        }
    }}

    run->filename = g_strdup(filename);

    success = TRUE;
out:
    if (state)
        gbb_power_state_free(state);
    g_object_unref(parser);
    return success;
}

GbbTestRun *
gbb_test_run_new_from_file(const char *filename,
                           GError    **error)
{
    GbbTestRun *run = g_object_new(GBB_TYPE_TEST_RUN, NULL);
    if (read_from_file(run, filename, error)) {
        return run;
    } else {
        g_object_unref(run);
        return NULL;
    }
}


