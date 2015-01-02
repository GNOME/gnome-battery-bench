/* -*- mode: C; c-file-style: "stroustrup"; indent-tabs-mode: nil; -*- */

#include <config.h>

#include <string.h>

#include <gio/gio.h>

#include "battery-test.h"

static GList *all_tests;
static GHashTable *tests_by_id;

static void
load_test(GFile *filename)
{
    GError *error = NULL;
    gboolean success = FALSE;

    GKeyFile *key_file = g_key_file_new();

    GbbBatteryTest *test = g_slice_new0(GbbBatteryTest);
    test->path = g_file_get_path(filename);

    char *base_path = g_strndup(test->path, strlen(test->path) - strlen(".batterytest"));


    test->id = g_path_get_basename(base_path);
    if (!g_key_file_load_from_file(key_file, test->path,
                                   G_KEY_FILE_NONE, &error)) {
        g_warning("Failed to load .batterytest file: %s\n", error->message);
        goto out;
    }

    test->name = g_key_file_get_value(key_file, "batterytest", "name", &error);
    if (error) {
        g_warning("No name key in [batterytest] section");
        goto out;
    }

    test->description = g_key_file_get_value(key_file, "batterytest", "description", &error);
    if (error) {
        g_warning("No name key in [batterytest] section");
        goto out;
    }

    test->loop_file = g_strconcat(base_path, ".loop", NULL);
    if (!g_file_test(test->loop_file, G_FILE_TEST_EXISTS)) {
        g_warning("%s doesn't exist", test->loop_file);
        goto out;
    }

    test->prologue_file = g_strconcat(base_path, ".prologue", NULL);
    if (!g_file_test(test->prologue_file, G_FILE_TEST_EXISTS))
        g_clear_pointer(&test->prologue_file, g_free);

    test->epilogue_file = g_strconcat(base_path, ".epilogue", NULL);
    if (!g_file_test(test->epilogue_file, G_FILE_TEST_EXISTS))
        g_clear_pointer(&test->epilogue_file, g_free);

    if (g_hash_table_lookup(tests_by_id, test->id)) {
        g_warning("Duplicate test ID %s\n", test->id);
        goto out;
    }

    g_hash_table_insert(tests_by_id, test->id, test);
    all_tests = g_list_prepend(all_tests, test);

    success = TRUE;

out:
    if (!success) {
        g_free(test->path);
        g_free(test->description);
        g_free(test->name);
        g_free(test->prologue_file);
        g_free(test->loop_file);
        g_free(test->epilogue_file);
        g_slice_free(GbbBatteryTest, test);
    }

    g_free(base_path);
    g_clear_error(&error);
    g_key_file_free(key_file);
}

static void
load_tests_from_directory(const char *path)
{
    GFile *file = g_file_new_for_path(path);
    GFileEnumerator *enumerator;
    GError *error = NULL;

    enumerator = g_file_enumerate_children (file,
                                            "standard::name,standard::type",
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

        if (g_file_info_get_file_type (info) != G_FILE_TYPE_REGULAR)
            goto next;

        const char *name = g_file_info_get_name (info);
        if (!g_str_has_suffix(name, ".batterytest"))
            goto next;

        child = g_file_enumerator_get_child (enumerator, info);
        load_test(child);

    next:
        g_clear_object (&child);
        g_clear_object (&info);
    }

out:
    if (error != NULL) {
        g_warning("Error loading tests: %s", error->message);
        g_clear_error(&error);
    }

    g_clear_object (&file);
    g_clear_object (&enumerator);
}

static int
compare_tests(gconstpointer a, gconstpointer b)
{
    const GbbBatteryTest *test_a = a;
    const GbbBatteryTest *test_b = b;

    return g_utf8_collate(test_a->name, test_b->name);
}

static void
tests_init(void)
{
    if (tests_by_id != NULL)
        return;

    tests_by_id = g_hash_table_new(g_str_hash,
                                     g_str_equal);
    load_tests_from_directory(PKGDATADIR "/tests");

    char *user_tests_path = g_build_filename (g_get_user_config_dir(),
                                              PACKAGE_NAME,
                                              "tests",
                                              NULL);
    if (g_file_test(user_tests_path, G_FILE_TEST_IS_DIR))
        load_tests_from_directory(user_tests_path);
    g_free(user_tests_path);

    all_tests = g_list_sort(all_tests, compare_tests);
}

GbbBatteryTest *
gbb_battery_test_get_for_id(const char *id)
{
    tests_init();

    return g_hash_table_lookup(tests_by_id, id);
}

GList *
gbb_battery_test_list_all(void)
{
    tests_init();

    return all_tests;
}
