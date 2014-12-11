#ifndef __APPLICATION_H__
#define __APPLICATION_H__

#include <glib.h>

typedef struct _GbbApplication      GbbApplication;
typedef struct _GbbApplicationClass GbbApplicationClass;

#define GBB_TYPE_APPLICATION         (gbb_application_get_type ())
#define GBB_APPLICATION(o)           (G_TYPE_CHECK_INSTANCE_CAST ((o), GBB_TYPE_APPLICATION, GbbApplication))
#define GBB_APPLICATION_CLASS(k)     (G_TYPE_CHECK_CLASS_CAST((k), GBB_TYPE_APPLICATION, GbbApplicationClass))
#define GBB_IS_APPLICATION(o)        (G_TYPE_CHECK_INSTANCE_TYPE ((o), GBB_TYPE_APPLICATION))
#define GBB_IS_APPLICATION_CLASS(k)  (G_TYPE_CHECK_CLASS_TYPE ((k), GBB_TYPE_APPLICATION))
#define GBB_APPLICATION_GET_CLASS(o) (G_TYPE_INSTANCE_GET_CLASS ((o), GBB_TYPE_APPLICATION, GbbApplicationClass))

GType              gbb_application_get_type   (void);

GbbApplication    *gbb_application_new        (void);

#endif /*__APPLICATION_H__ */
