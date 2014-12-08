/* -*- mode: C; c-file-style: "stroustrup"; indent-tabs-mode: nil; -*- */

#include <gtk/gtk.h>
#include "application.h"

int
main (gint   argc,
      char **argv)
{
    GbbApplication *app;

    app = gbb_application_new();
    return g_application_run(G_APPLICATION (app), argc, argv);
}
