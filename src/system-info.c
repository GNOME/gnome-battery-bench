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


static void gbb_system_info_init (GbbSystemInfo *info)
{
    read_dmi_info(info);
    load_gnome_version(&info->gnome_version,
                       &info->gnome_distributor,
                       &info->gnome_date);
    info->os_type = get_os_type();
    info->os_kernel = read_kernel_version();

}

GbbSystemInfo *
gbb_system_info_acquire ()
{
    GbbSystemInfo *info;
    info = (GbbSystemInfo *) g_object_new(GBB_TYPE_SYSTEM_INFO, NULL);
    return info;
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
    GHashTable *hashtable;
    gchar *buffer;

    hashtable = NULL;

    if (g_file_get_contents ("/etc/os-release", &buffer, NULL, NULL)) {
        gchar **lines;
        gint i;

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

        g_strfreev (lines);
        g_free (buffer);
    }

    return hashtable;
}

static char *
get_os_type (void)
{
    GHashTable *os_info;
    gchar *name, *result, *build_id;
    int bits;

    os_info = get_os_info ();

    if (!os_info)
        return NULL;

    name = g_hash_table_lookup (os_info, "PRETTY_NAME");
    build_id = g_hash_table_lookup (os_info, "BUILD_ID");

    if (GLIB_SIZEOF_VOID_P == 8)
        bits = 64;
    else
        bits = 32;

    if (build_id) {
        if (name)
            result = g_strdup_printf ("%s %d-bit (Build ID: %s)", name, bits, build_id);
        else
            result = g_strdup_printf ("%d-bit (Build ID: %s)", bits, build_id);
    } else {
      if (name)
        result = g_strdup_printf ("%s %d-bit", name, bits);
      else
        result = g_strdup_printf ("%d-bit", bits);
    }

    g_clear_pointer (&os_info, g_hash_table_destroy);

    return result;
}
