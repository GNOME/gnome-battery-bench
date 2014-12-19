#ifndef __POWER_GRAPHS_H__
#define __POWER_GRAPHS_H__

#include <glib-object.h>

#include "test-run.h"

typedef struct _GbbPowerGraphs      GbbPowerGraphs;
typedef struct _GbbPowerGraphsClass GbbPowerGraphsClass;

#define GBB_TYPE_POWER_GRAPHS         (gbb_power_graphs_get_type ())
#define GBB_POWER_GRAPHS(o)           (G_TYPE_CHECK_INSTANCE_CAST ((o), GBB_TYPE_POWER_GRAPHS, GbbPowerGraphs))
#define GBB_POWER_GRAPHS_CLASS(k)     (G_TYPE_CHECK_CLASS_CAST((k), GBB_TYPE_POWER_GRAPHS, GbbPowerGraphsClass))
#define GBB_IS_POWER_GRAPHS(o)        (G_TYPE_CHECK_INSTANCE_TYPE ((o), GBB_TYPE_POWER_GRAPHS))
#define GBB_IS_POWER_GRAPHS_CLASS(k)  (G_TYPE_CHECK_CLASS_TYPE ((k), GBB_TYPE_POWER_GRAPHS))
#define GBB_POWER_GRAPHS_GET_CLASS(o) (G_TYPE_INSTANCE_GET_CLASS ((o), GBB_TYPE_POWER_GRAPHS, GbbPowerGraphsClass))

GType gbb_power_graphs_get_type(void);

GtkWidget *gbb_power_graphs_new(void);

void gbb_power_graphs_set_test_run(GbbPowerGraphs *graphs,
                                   GbbTestRun     *run);

#endif /* __POWER_GRAPHS_H__*/
