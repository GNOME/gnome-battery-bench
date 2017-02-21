/* -*- mode: C; c-file-style: "stroustrup"; indent-tabs-mode: nil; -*- */

#include "system-info.h"

#include <glib.h>
#include <string.h>

#include "config.h"

struct _GbbSystemInfo {
    GObject parent;

    /* Hardware*/
    /*  Product */
    char *sys_vendor;
    char *product_version;
    char *product_name;

    /*  BIOS */
    char *bios_version;
    char *bios_date;
    char *bios_vendor;

    /*  CPU*/
    guint cpu_number;
    GStrv cpu_info;

    guint64 mem_total;

    /* GPU/Renderer */
    char *renderer;

    /* Software */

    /* OS */
    char *os_type;
    char *os_kernel;

    /*  GNOME */
    char *gnome_version;
    char *gnome_distributor;
    char *gnome_date;
};

enum {
    PROP_0,

    PROP_SYS_VENDOR,
    PROP_PRODUCT_VERSION,
    PROP_PRODUCT_NAME,

    PROP_BIOS_VERSION,
    PROP_BIOS_VENDOR,
    PROP_BIOS_DATE,

    PROP_CPU_NUMBER,
    PROP_CPU_INFO,
    PROP_MEM_TOTAL,

    PROP_RENDERER,

    PROP_OS_TYPE,
    PROP_OS_KERNEL,

    PROP_GNOME_VERSION,
    PROP_GNOME_DISTRIBUTOR,
    PROP_GNOME_DATE,

    PROP_LAST
};

G_DEFINE_TYPE (GbbSystemInfo, gbb_system_info, G_TYPE_OBJECT);

/* prototypes  */

static gboolean    load_gnome_version (char **version,
                                       char **distributor,
                                       char **date);
static char       *get_os_type        (void);
/*  */

static void
gbb_system_info_finalize(GbbSystemInfo *info)
{
    g_free(info->sys_vendor);
    g_free(info->product_version);
    g_free(info->product_name);

    g_free(info->bios_version);
    g_free(info->bios_date);
    g_free(info->bios_vendor);

    g_strfreev(info->cpu_info);

    g_free(info->os_type);
    g_free(info->os_kernel);

    g_free(info->gnome_version);
    g_free(info->gnome_distributor);
    g_free(info->gnome_date);

    G_OBJECT_CLASS(gbb_system_info_parent_class)->finalize(G_OBJECT (info));
}

static void
gbb_system_info_get_property (GObject *object, guint prop_id, GValue *value, GParamSpec *pspec)
{
    GbbSystemInfo *info = GBB_SYSTEM_INFO(object);

    switch (prop_id) {
    case PROP_SYS_VENDOR:
        g_value_set_string(value, info->sys_vendor);
        break;

    case PROP_PRODUCT_VERSION:
        g_value_set_string(value, info->product_version);
        break;

    case PROP_PRODUCT_NAME:
        g_value_set_string(value, info->product_name);
        break;

    case PROP_BIOS_VERSION:
        g_value_set_string(value, info->bios_version);
        break;

    case PROP_BIOS_DATE:
        g_value_set_string(value, info->bios_date);
        break;

    case PROP_BIOS_VENDOR:
        g_value_set_string(value, info->bios_vendor);
        break;

    case PROP_CPU_NUMBER:
        g_value_set_uint(value, info->cpu_number);
        break;

    case PROP_CPU_INFO:
        g_value_set_boxed(value, info->cpu_info);
        break;

    case PROP_MEM_TOTAL:
        g_value_set_uint64(value, info->mem_total);
        break;

    case PROP_RENDERER:
        g_value_set_string(value, info->renderer);
        break;

    case PROP_OS_TYPE:
        g_value_set_string(value, info->os_type);
        break;

    case PROP_OS_KERNEL:
        g_value_set_string(value, info->os_kernel);
        break;

    case PROP_GNOME_VERSION:
        g_value_set_string(value, info->gnome_version);
        break;

    case PROP_GNOME_DISTRIBUTOR:
        g_value_set_string(value, info->gnome_distributor);
        break;

    case PROP_GNOME_DATE:
        g_value_set_string(value, info->gnome_date);
        break;
    }

}

static void
gbb_system_info_class_init (GbbSystemInfoClass *klass)
{
    GObjectClass *gobject_class = G_OBJECT_CLASS(klass);
    gobject_class->finalize = (GObjectFinalizeFunc) gbb_system_info_finalize;
    gobject_class->get_property = gbb_system_info_get_property;

    g_object_class_install_property (gobject_class,
                                     PROP_SYS_VENDOR,
                                     g_param_spec_string ("sys-vendor",
                                                          NULL, NULL,
                                                          NULL,
                                                          G_PARAM_READABLE));
    g_object_class_install_property (gobject_class,
                                     PROP_PRODUCT_VERSION,
                                     g_param_spec_string ("product-version",
                                                          NULL, NULL,
                                                          NULL,
                                                          G_PARAM_READABLE));
    g_object_class_install_property (gobject_class,
                                     PROP_PRODUCT_NAME,
                                     g_param_spec_string ("product-name",
                                                          NULL, NULL,
                                                          NULL,
                                                          G_PARAM_READABLE));
    g_object_class_install_property (gobject_class,
                                     PROP_BIOS_VERSION,
                                     g_param_spec_string ("bios-version",
                                                          NULL, NULL,
                                                          NULL,
                                                          G_PARAM_READABLE));
    g_object_class_install_property (gobject_class,
                                     PROP_BIOS_DATE,
                                     g_param_spec_string ("bios-date",
                                                          NULL, NULL,
                                                          NULL,
                                                          G_PARAM_READABLE));
    g_object_class_install_property (gobject_class,
                                     PROP_BIOS_VENDOR,
                                     g_param_spec_string ("bios-vendor",
                                                          NULL, NULL,
                                                          NULL,
                                                          G_PARAM_READABLE));
    g_object_class_install_property (gobject_class,
                                     PROP_CPU_NUMBER,
                                     g_param_spec_uint ("cpu-number",
                                                        NULL, NULL,
                                                        0, G_MAXUINT, 0,
                                                        G_PARAM_READABLE));
    g_object_class_install_property (gobject_class,
                                     PROP_CPU_INFO,
                                     g_param_spec_boxed("cpu-info",
                                                        NULL, NULL,
                                                        G_TYPE_STRV,
                                                        G_PARAM_READABLE));
    g_object_class_install_property (gobject_class,
                                     PROP_MEM_TOTAL,
                                     g_param_spec_uint64 ("mem-total",
                                                          NULL, NULL,
                                                          0, G_MAXUINT64, 0,
                                                          G_PARAM_READABLE));
    g_object_class_install_property (gobject_class,
                                     PROP_RENDERER,
                                     g_param_spec_string ("renderer",
                                                          NULL, NULL,
                                                          NULL,
                                                          G_PARAM_READABLE));
    g_object_class_install_property (gobject_class,
                                     PROP_OS_KERNEL,
                                     g_param_spec_string ("os-kernel",
                                                          NULL, NULL,
                                                          NULL,
                                                          G_PARAM_READABLE));
    g_object_class_install_property (gobject_class,
                                     PROP_OS_TYPE,
                                     g_param_spec_string ("os-type",
                                                          NULL, NULL,
                                                          NULL,
                                                          G_PARAM_READABLE));
    g_object_class_install_property (gobject_class,
                                     PROP_GNOME_VERSION,
                                     g_param_spec_string ("gnome-version",
                                                          NULL, NULL,
                                                          NULL,
                                                          G_PARAM_READABLE));
    g_object_class_install_property (gobject_class,
                                     PROP_GNOME_DISTRIBUTOR,
                                     g_param_spec_string ("gnome-distributor",
                                                          NULL, NULL,
                                                          NULL,
                                                          G_PARAM_READABLE));
    g_object_class_install_property (gobject_class,
                                     PROP_GNOME_DATE,
                                     g_param_spec_string ("gnome-date",
                                                          NULL, NULL,
                                                          NULL,
                                                          G_PARAM_READABLE));
}

static char *
read_sysfs_string(const char *node)
{
    gboolean ok;
    char *contents = NULL;
    GError *error = NULL;
    gsize len;

    ok = g_file_get_contents (node,
                              &contents,
                              &len,
                              &error);

    if (!ok) {
        g_log_structured (G_LOG_DOMAIN, G_LOG_LEVEL_WARNING,
                          "MESSAGE_ID", "3a2690a163c5465bb9ba0cab229bf3cf",
                          "MESSAGE", "Reading sys file '%s' failed: %s.",
                          node, error->message);
        g_error_free(error);
        return g_strdup("Unknown");
    }

    if (len > 1 && contents[len-1] == '\n') {
        contents[len-1] = '\0';
    }

    return contents;
}

static void
read_dmi_info(GbbSystemInfo *info)
{
    info->bios_version = read_sysfs_string("/sys/devices/virtual/dmi/id/bios_version");
    info->bios_date = read_sysfs_string("/sys/devices/virtual/dmi/id/bios_date");
    info->bios_vendor = read_sysfs_string("/sys/devices/virtual/dmi/id/bios_vendor");

    info->product_name = read_sysfs_string("/sys/devices/virtual/dmi/id/product_name");
    info->product_version = read_sysfs_string("/sys/devices/virtual/dmi/id/product_version");

    info->sys_vendor = read_sysfs_string("/sys/devices/virtual/dmi/id/sys_vendor");
}

static char *
read_kernel_version(void)
{
    g_autofree char *data = read_sysfs_string("/proc/version");
    g_auto(GStrv) comps = NULL;

    if (data == NULL) {
        return g_strdup("Unknown");
    }

    comps = g_strsplit(data, " ", 4);
    if (g_strv_length (comps) < 3) {
        char *tmp = data;
        data = NULL;
        return tmp;
    }

    return g_strdup(comps[2]);
}

static void
report_format_error(const char *filename, const char *message)
{
    g_log_structured (G_LOG_DOMAIN, G_LOG_LEVEL_WARNING,
                      "MESSAGE_ID", "3a2690a163c5465bb9ba0cab229bf3cf",
                      "MESSAGE", "Format error: while parsing '%s': %s.",
                      filename, message);
}

static GStrv
read_cpu_info(guint *ncpus)
{
    g_autofree char *data = read_sysfs_string("/proc/cpuinfo");
    g_autoptr(GHashTable) cpus = NULL;
    g_auto(GStrv) kv = NULL;
    GStrv models = NULL;
    gsize i, n = 0;
    gpointer key, val;
    GHashTableIter iter;

    if (data == NULL) {
        models = g_new(char *, 2);
        models[0] = g_strdup("Unknown");
        models[1] = NULL;
        *ncpus = 1;
        return models;
    }

    cpus = g_hash_table_new (g_str_hash, g_str_equal);

    kv = g_strsplit(data, "\n", -1);
    for (i = 0; i < g_strv_length(kv); i++) {
        const char *entry = kv[i];
        const char *pos;
        if (!g_str_has_prefix(entry, "model name")) {
            continue;
        }

        pos = g_strstr_len(entry, -1, ":");
        if (pos == NULL) {
            report_format_error("/proc/cpuinfo", "expected a ':'");
            continue;
        }
        pos++;

        while (*pos == ' ' && *pos != '\n') {
            pos++;
        }

        val = g_hash_table_lookup(cpus, pos);
        if (val == NULL) {
            g_hash_table_insert(cpus, (gpointer) pos, GINT_TO_POINTER(1));
        } else {
            val = GINT_TO_POINTER(GPOINTER_TO_INT(val) + 1);
            g_hash_table_replace(cpus, (gpointer) pos, val);
        }
    }

    n = i = 0;
    g_hash_table_iter_init (&iter, cpus);
    models = (GStrv) g_new(char *, g_hash_table_size(cpus) + 1);
    while (g_hash_table_iter_next (&iter, &key, &val)) {
        int k = GPOINTER_TO_INT(val);
        models[i] = g_strdup_printf("%s [%d]", (char *) key, k);
        n += k;
        i++;
    }
    models[i] = NULL;
    *ncpus = n;
    return models;
}

static guint64
read_mem_info(void)
{
    g_autofree char *data = read_sysfs_string("/proc/meminfo");
    g_auto(GStrv) kv = NULL;
    const char *total = NULL;
    const char *pos;
    char *endptr;
    guint64 res;
    guint i;

    if (data == NULL) {
        return 0;
    }

    kv = g_strsplit(data, "\n", -1);
    for (i = 0; i < g_strv_length(kv); i++) {
        if (g_str_has_prefix(kv[i], "MemTotal:")) {
            total = kv[i];
            break;
        }
    }

    if (total == NULL) {
        report_format_error("/proc/meminfo", "'MemTotal' not found");
        return 0;
    }

    pos = g_strstr_len(total, -1, ":");
    if (pos == NULL) {
        report_format_error("/proc/meminfo", "MemTotal: expected a ':'");
        return 0;
    }

    do {
        pos++;
    } while (*pos != '\0' && *pos == ' ');

    res = g_ascii_strtoull(pos, &endptr, 10);

    if (pos == endptr) {
        report_format_error("/proc/meminfo", "MemTotal: could not parse number");
        return 0;
    }

    return res;
}

static char *
get_renderer_info (void)
{
    g_autoptr(GDBusProxy) proxy;
    g_autoptr(GVariant) var = NULL;
    g_autoptr(GError) error = NULL;
    const char *renderer = "Unknown";

    proxy = g_dbus_proxy_new_for_bus_sync(G_BUS_TYPE_SESSION,
                                          G_DBUS_PROXY_FLAGS_NONE,
                                          NULL,
                                          "org.gnome.SessionManager",
                                          "/org/gnome/SessionManager",
                                          "org.gnome.SessionManager",
                                          NULL, &error);

    if (error != NULL) {
        g_warning("Failed to connect to org.gnome.SessionManager: %s",
                  error->message);
        goto out;
    }

    var = g_dbus_proxy_get_cached_property(proxy, "Renderer");
    if (!var) {
        g_warning("Failed to obtain 'Renderer' property from org.gnome.SessionManager");
        goto out;
    }

    renderer = g_variant_get_string(var, NULL);

out:
    return g_strstrip(g_strdup(renderer));
}


static void gbb_system_info_init (GbbSystemInfo *info)
{
    read_dmi_info(info);
    load_gnome_version(&info->gnome_version,
                       &info->gnome_distributor,
                       &info->gnome_date);
    info->os_type = get_os_type();
    info->os_kernel = read_kernel_version();
    info->cpu_info = read_cpu_info(&info->cpu_number);
    info->mem_total = read_mem_info();
    info->renderer = get_renderer_info();
}

GbbSystemInfo *
gbb_system_info_acquire ()
{
    GbbSystemInfo *info;
    info = (GbbSystemInfo *) g_object_new(GBB_TYPE_SYSTEM_INFO, NULL);
    return info;
}

void
gbb_system_info_to_json (const GbbSystemInfo *info, JsonBuilder *builder)
{
    json_builder_begin_object(builder);
    json_builder_set_member_name(builder, "hardware");
    {
        json_builder_begin_object(builder);
        json_builder_set_member_name(builder, "vendor");
        json_builder_add_string_value(builder, info->sys_vendor);
        json_builder_set_member_name(builder, "version");
        json_builder_add_string_value(builder, info->product_version);
        json_builder_set_member_name(builder, "name");
        json_builder_add_string_value(builder, info->product_name);

        json_builder_set_member_name(builder, "bios");
        {
            json_builder_begin_object(builder);
            json_builder_set_member_name(builder, "version");
            json_builder_add_string_value(builder, info->bios_version);
            json_builder_set_member_name(builder, "date");
            json_builder_add_string_value(builder, info->bios_date);
            json_builder_set_member_name(builder, "vendor");
            json_builder_add_string_value(builder, info->bios_vendor);
            json_builder_end_object(builder);
        }

        json_builder_set_member_name(builder, "cpu");
        {
            json_builder_begin_object(builder);
            json_builder_set_member_name(builder, "number");
            json_builder_add_int_value(builder, info->cpu_number);
            json_builder_set_member_name(builder, "info");
            json_builder_begin_array(builder);
            for (int i = 0; i < g_strv_length(info->cpu_info); i++) {
                json_builder_add_string_value(builder, info->cpu_info[i]);
            }
            json_builder_end_array(builder);
            json_builder_end_object(builder);
        }

        json_builder_set_member_name(builder, "renderer");
        json_builder_add_string_value(builder, info->renderer);

        json_builder_set_member_name(builder, "memory");
        {
            json_builder_begin_object(builder);
            json_builder_set_member_name(builder, "total");
            json_builder_add_int_value(builder, info->mem_total);
            json_builder_end_object(builder);
        }

        json_builder_end_object(builder);
    }
    json_builder_set_member_name(builder, "software");
    {
        json_builder_begin_object(builder);
        json_builder_set_member_name(builder, "os");
        {
            json_builder_begin_object(builder);
            json_builder_set_member_name(builder, "type");
            json_builder_add_string_value(builder, info->os_type);
            json_builder_set_member_name(builder, "kernel");
            json_builder_add_string_value(builder, info->os_kernel);
            json_builder_end_object(builder);
        }
        json_builder_set_member_name(builder, "gnome");
        {
            json_builder_begin_object(builder);
            json_builder_set_member_name(builder, "version");
            json_builder_add_string_value(builder, info->gnome_version);
            json_builder_set_member_name(builder, "distributor");
            json_builder_add_string_value(builder, info->gnome_distributor);
            json_builder_set_member_name(builder, "date");
            json_builder_add_string_value(builder, info->gnome_date);
            json_builder_end_object(builder);
        }
        json_builder_end_object(builder);
    }
    json_builder_end_object(builder);
}

/* GNOME system info */
/* Everything below has been mostly borrowed from
 * gnome-control-center/panels/info/cc-info-panel.c
 * with some minor modifications, and reformatting.
 * License: GPLv2+
 * Copyright (C) 2010 Red Hat, Inc
 * Copyright (C) 2008 William Jon McCann <jmccann@redhat.com>
 */
typedef struct {
    char *major;
    char *minor;
    char *micro;
    char *distributor;
    char *date;
    char **current;
} VersionData;

static void
version_start_element_handler (GMarkupParseContext      *ctx,
                               const char               *element_name,
                               const char              **attr_names,
                               const char              **attr_values,
                               gpointer                  user_data,
                               GError                  **error)
{
    VersionData *data = user_data;

    if (g_str_equal (element_name, "platform"))
        data->current = &data->major;
    else if (g_str_equal (element_name, "minor"))
        data->current = &data->minor;
    else if (g_str_equal (element_name, "micro"))
        data->current = &data->micro;
    else if (g_str_equal (element_name, "distributor"))
        data->current = &data->distributor;
    else if (g_str_equal (element_name, "date"))
        data->current = &data->date;
    else
        data->current = NULL;
}

static void
version_end_element_handler (GMarkupParseContext      *ctx,
                             const char               *element_name,
                             gpointer                  user_data,
                             GError                  **error)
{
    VersionData *data = user_data;
    data->current = NULL;
}

static void
version_text_handler (GMarkupParseContext *ctx,
                      const char          *text,
                      gsize                text_len,
                      gpointer             user_data,
                      GError             **error)
{
    VersionData *data = user_data;
    if (data->current != NULL)
        *data->current = g_strstrip (g_strdup (text));
}

static gboolean
load_gnome_version (char **version,
                    char **distributor,
                    char **date)
{
    GMarkupParser version_parser = {
        version_start_element_handler,
        version_end_element_handler,
        version_text_handler,
        NULL,
        NULL,
    };
    GError              *error;
    GMarkupParseContext *ctx;
    char                *contents;
    gsize                length;
    VersionData         *data;
    gboolean             ret;

    ret = FALSE;

    error = NULL;
    if (!g_file_get_contents (DATADIR "/gnome/gnome-version.xml",
                              &contents,
                              &length,
                              &error))
        return FALSE;

    data = g_new0 (VersionData, 1);
    ctx = g_markup_parse_context_new (&version_parser, 0, data, NULL);

    if (!g_markup_parse_context_parse (ctx, contents, length, &error)) {
        g_warning ("Invalid version file: '%s'", error->message);
    } else {
        if (version != NULL)
            *version = g_strdup_printf ("%s.%s.%s", data->major, data->minor, data->micro);
        if (distributor != NULL)
            *distributor = g_strdup (data->distributor);
        if (date != NULL)
            *date = g_strdup (data->date);

        ret = TRUE;
    }

    g_markup_parse_context_free (ctx);
    g_free (data->major);
    g_free (data->minor);
    g_free (data->micro);
    g_free (data->distributor);
    g_free (data->date);
    g_free (data);
    g_free (contents);

    return ret;
};

static GHashTable*
get_os_info (void)
{
    GHashTable *hashtable = NULL;
    g_autofree gchar *buffer = NULL;
    g_auto(GStrv) lines = NULL;
    g_autoptr(GError) error = NULL;
    gint i;

    if (! g_file_get_contents ("/etc/os-release", &buffer, NULL, &error)) {
        g_warning("Failed to read '/etc/os-release': %s", error->message);
        return NULL;
    }

    lines = g_strsplit (buffer, "\n", -1);

    for (i = 0; lines[i] != NULL; i++) {
        gchar *delimiter, *key, *value;

        /* Initialize the hash table if needed */
        if (!hashtable)
            hashtable = g_hash_table_new_full (g_str_hash, g_str_equal, g_free, g_free);

        delimiter = strstr (lines[i], "=");
        value = NULL;
        key = NULL;

        if (delimiter != NULL) {
            gint size;

            key = g_strndup (lines[i], delimiter - lines[i]);

            /* Jump the '=' */
            delimiter += strlen ("=");

            /* Eventually jump the ' " ' character */
            if (g_str_has_prefix (delimiter, "\""))
                delimiter += strlen ("\"");

            size = strlen (delimiter);

            /* Don't consider the last ' " ' too */
            if (g_str_has_suffix (delimiter, "\""))
                size -= strlen ("\"");

            value = g_strndup (delimiter, size);

            g_hash_table_insert (hashtable, key, value);
        }
    }

    return hashtable;
}

static char *
get_os_type (void)
{
    GHashTable *os_info;
    gchar *name, *result, *build_id;
    char base[255];
    int bits;

    os_info = get_os_info ();

    if (!os_info)
        return NULL;

    name = g_hash_table_lookup (os_info, "PRETTY_NAME");
    build_id = g_hash_table_lookup (os_info, "BUILD_ID");

    bits = GLIB_SIZEOF_VOID_P == 8 ? 64 : 32;

    if (name)
        g_snprintf(base, sizeof(base), "%s %d-bit", name, bits);
    else
        g_snprintf(base, sizeof(base), "%d-bit", bits);

    if (build_id) {
        char idstr[255];
        g_snprintf(idstr, sizeof(idstr), " (Build ID: %s)", build_id);
        result = g_strconcat(base, idstr, NULL);
    } else {
        result = g_strdup(base);
    }

    g_clear_pointer (&os_info, g_hash_table_destroy);

    return result;
}
