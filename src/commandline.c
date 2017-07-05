/* -*- mode: C; c-file-style: "stroustrup"; indent-tabs-mode: nil; -*- */

#include "config.h"

#include <dirent.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/poll.h>
#include <time.h>
#include <unistd.h>

#include <X11/Xlib.h>
#include <gdk/gdk.h>

#include <glib.h>
#include <glib-unix.h>
#include <gio/gio.h>

#include "evdev-player.h"
#include "remote-player.h"
#include "event-recorder.h"
#include "power-monitor.h"
#include "power-supply.h"
#include "system-info.h"
#include "test-runner.h"
#include "xinput-wait.h"
#include "util.h"

static gboolean info_usejson = FALSE;
static GOptionEntry info_options[] =
{
    { "json", 'j', 0, G_OPTION_ARG_NONE, &info_usejson, "Show data in json" },
    { NULL }
};

static void
info_txt_battery(GbbBattery *bat, const char *prefix)
{
    g_autofree char *name = NULL;
    g_autofree char *vendor = NULL;
    g_autofree char *model = NULL;
    double volt_design;
    double energy_full;
    double energy_full_design;

    g_object_get(bat,
                 "name", &name,
                 "vendor", &vendor,
                 "model", &model,
                 "voltage-design", &volt_design,
                 "energy-full", &energy_full,
                 "energy-full-design", &energy_full_design,
                 NULL);

    g_print("%s Battery:\n", prefix);
    g_print("%s   Name: %s\n", prefix, name);
    g_print("%s   Vendor: %s\n", prefix, vendor);
    g_print("%s   Model: %s\n", prefix, model);
    g_print("%s   Voltage Design: %5.2f V\n", prefix, volt_design);
    g_print("%s   Energy Full: %5.2f Wh\n", prefix, energy_full);
    g_print("%s   Energy Full Design: %5.2f Wh\n", prefix, energy_full_design);
}

static void
info_txt_pci_device(GbbPciDevice *dev, const char *prefix)
{
    g_autofree char *vendor_name = NULL;
    g_autofree char *device_name = NULL;
    guint            vendor_id;
    guint            device_id;
    guint            revision;
    gboolean         enabled;

    g_object_get(dev,
                 "vendor", &vendor_id,
                 "vendor-name", &vendor_name,
                 "device", &device_id,
                 "device-name", &device_name,
                 "revision", &revision,
                 "enabled", &enabled,
                 NULL);

    g_print("%s %s [0x%x] (%s [0x%x]) rev. 0x%x%s\n",
            prefix,
            device_name != NULL ? device_name : "Unknown",
            device_id,
            vendor_name != NULL ? vendor_name : "Unknown",
            vendor_id,
            revision,
            enabled ? "" : " [disabled]");
}

static void
info_txt_cpu(GbbCpu *cpu, const char *prefix)
{
    g_autofree char *model_name = NULL;
    g_autofree char *arch = NULL;
    g_autofree char *vendor = NULL;
    g_autofree char *vendor_name = NULL;
    guint number, threads, cores, packages;

    g_print("%sCPU:\n", prefix);

    if (cpu == NULL) {
        g_print("%s Unknown\n", prefix);
        return;
    }

    g_object_get(cpu,
                 "model-name", &model_name,
                 "architecture", &arch,
                 "vendor", &vendor,
                 "vendor-name", &vendor_name,
                 "number", &number,
                 "threads", &threads,
                 "cores", &cores,
                 "packages", &packages,
                 NULL);


    g_print("%s  Model: %s\n", prefix, model_name);
    g_print("%s  Vendor: %s (%s)\n", prefix, vendor_name, vendor);
    g_print("%s  Number: %u\n", prefix, number);
    g_print("%s   Packages: %u\n", prefix, packages);
    g_print("%s   Cores per Package: %u\n", prefix, cores);
    g_print("%s   Threads per Core: %u\n", prefix, threads);
}

static int
info_txt(int argc, char **argv)
{
    g_autoptr(GbbSystemInfo) info = NULL;
    g_autofree char *sys_vendor;
    g_autofree char *product_version;
    g_autofree char *product_name;
    g_autofree char *bios_vendor;
    g_autofree char *bios_version;
    g_autofree char *bios_date;
    g_autoptr(GbbCpu) cpu = NULL;
    guint64          mem_total;
    g_autofree char *renderer;
    int monitor_x;
    int monitor_y;
    int monitor_width;
    int monitor_height;
    float monitor_refresh;
    float monitor_scale;
    g_autofree char *os_type;
    g_autofree char *os_kernel;
    g_autofree char *display_proto;
    g_autofree char *gnome_version;
    g_autofree char *gnome_distributor;
    g_autofree char *gnome_date;
    g_autoptr(GPtrArray) batteries = NULL;
    g_autoptr(GPtrArray) gpus = NULL;

    info = gbb_system_info_acquire();

    g_object_get(info,
                 "sys-vendor", &sys_vendor,
                 "product-version", &product_version,
                 "product-name", &product_name,
                 "bios-date", &bios_date,
                 "bios-version", &bios_version,
                 "bios_vendor", &bios_vendor,
                 "cpu", &cpu,
                 "mem-total", &mem_total,
                 "batteries", &batteries,
                 "gpus", &gpus,
                 "renderer", &renderer,
                 "monitor-x", &monitor_x,
                 "monitor-y", &monitor_y,
                 "monitor-width", &monitor_width,
                 "monitor-height", &monitor_height,
                 "monitor-refresh", &monitor_refresh,
                 "monitor-scale", &monitor_scale,
                 "os-type", &os_type,
                 "os-kernel", &os_kernel,
                 "display-proto", &display_proto,
                 "gnome-version", &gnome_version,
                 "gnome-distributor", &gnome_distributor,
                 "gnome-date", &gnome_date,
                 NULL);

    g_print("System information:\n");
    g_print(" Hardware:\n");
    g_print("  Vendor: %s\n", sys_vendor);
    g_print("  Version: %s\n", product_version);
    g_print("  Name: %s\n", product_name);
    info_txt_cpu(cpu, "  ");
    g_print("  GPUs:\n");
    g_ptr_array_foreach(gpus, (GFunc) info_txt_pci_device, "   ");
    g_print("  Batteries:\n");
    g_ptr_array_foreach(batteries, (GFunc) info_txt_battery, "  ");
    g_print("  Monitor:\n");
    g_print("    Resolution: %d x %d (px)\n", monitor_x, monitor_y);
    g_print("    Size: %d x %d (mm)\n", monitor_width, monitor_height);
    g_print("    Refresh %2.3f\n", monitor_refresh);
    g_print("    Scale-Factor: %1.1f\n", monitor_scale);
    g_print("  Memory:\n");
    g_print("   Total: %" G_GUINT64_FORMAT " kB\n", mem_total);
    g_print("  Bios:\n");
    g_print("   Version: %s\n", bios_version);
    g_print("   Date: %s\n", bios_date);
    g_print("   Vendor: %s\n", bios_vendor);
    g_print(" Renderer: %s\n", renderer);
    g_print(" Software:\n");
    g_print("  OS:\n");
    g_print("   Type: %s\n", os_type);
    g_print("   Kernel: %s\n", os_kernel);
    g_print("  Display Protocol: %s\n", display_proto);
    g_print("  GNOME:\n");
    g_print("   Version: %s\n", gnome_version);
    g_print("   Distributor: %s\n", gnome_distributor);
    g_print("   Date: %s\n", gnome_date);

    return 0;
}

static int
info_json(int argc, char **argv)
{
    JsonBuilder *builder = json_builder_new();
    g_autoptr(GbbSystemInfo) info = NULL;
    char *data = NULL;

    info = gbb_system_info_acquire();
    gbb_system_info_to_json(info, builder);

    JsonNode *root = json_builder_get_root(builder);
    JsonGenerator *generator = json_generator_new();
    json_generator_set_pretty(generator, TRUE);
    json_generator_set_root(generator, root);

    data = json_generator_to_data (generator, NULL);

    g_object_unref(generator);
    json_node_free(root);
    g_object_unref(builder);

    if (data == NULL) {
        return -1;
    }

    g_print("%s", data);
    g_free(data);
    return 0;
}

static int
info(int argc, char **argv)
{
    gdk_init_check(&argc, &argv);
    if (info_usejson) {
        return info_json(argc, argv);
    } else {
        return info_txt(argc, argv);
    }
}
static GbbPowerState *start_state;

static void
on_power_monitor_changed(GbbPowerMonitor *monitor,
                         GbbTestRunner   *runner)
{
    const GbbPowerState *state = gbb_power_monitor_get_state(monitor);
    char time_str[256] = { 0 };
    time_t now;

    time (&now);
    strftime (time_str, sizeof (time_str), "%H:%M:%S ", localtime (&now));

    g_print("%s", time_str);
    g_print("AC: %s\n", state->online ? "online" : "offline");

    g_print("%s", time_str);
    g_print("Energy: %.2f WH (%.2f%%)\n", state->energy_now, gbb_power_state_get_percent(state));

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

    if (statistics->power >= 0) {
        g_print("%s", time_str);
        g_print("Average power: %.2f W\n", statistics->power);
    }
    if (statistics->current >= 0) {
        g_print("%s", time_str);
        g_print("Average current: %.2f A\n", statistics->current);
    }
    if (statistics->battery_life >= 0) {
        int h, m, s;
        break_time(statistics->battery_life, &h, &m, &s);
        g_print("%s", time_str);
        g_print("Predicted battery life: %.0fs (%d:%02d:%02d)\n",
                statistics->battery_life, h, m, s);
    }
    if (statistics->battery_life_design >= 0) {
        int h, m, s;
        break_time(statistics->battery_life_design, &h, &m, &s);
        g_print("%s", time_str);
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
    char time_str[256] = { 0 };
    time_t now;

    time (&now);
    strftime (time_str, sizeof (time_str), "%Y-%m-%d %H:%M:%S ", localtime (&now));

    g_print ("%s", time_str);
    g_print("Monitoring power events. Press Ctrl+C to cancel\n");
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
        die("%s", error->message);

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
    { "info",         info_options, NULL, info, 0, 0},
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
