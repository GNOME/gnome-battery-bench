#ifndef __SYSTEM_STATE_H__
#define __SYSTEM_STATE_H__

#include <glib-object.h>

typedef struct _GbbSystemState GbbSystemState;

#define GBB_TYPE_SYSTEM_STATE         (gbb_system_state_get_type ())
#define GBB_SYSTEM_STATE(o)           (G_TYPE_CHECK_INSTANCE_CAST ((o), GBB_TYPE_SYSTEM_STATE, GbbSystemState))
#define GBB_SYSTEM_STATE_CLASS(k)     (G_TYPE_CHECK_CLASS_CAST((k), GBB_TYPE_SYSTEM_STATE, GbbSystemStateClass))
#define GBB_IS_SYSTEM_STATE(o)        (G_TYPE_CHECK_INSTANCE_TYPE ((o), GBB_TYPE_SYSTEM_STATE))
#define GBB_IS_SYSTEM_STATE_CLASS(k)  (G_TYPE_CHECK_CLASS_TYPE ((k), GBB_TYPE_SYSTEM_STATE))
#define GBB_SYSTEM_STATE_GET_CLASS(o) (G_TYPE_INSTANCE_GET_CLASS ((o), GBB_TYPE_SYSTEM_STATE, GbbSystemStateClass))

GbbSystemState *gbb_system_state_new(void);

gboolean gbb_system_state_is_ready (GbbSystemState *system_state);

void gbb_system_state_save        (GbbSystemState *system_state);
void gbb_system_state_restore     (GbbSystemState *system_state);

void gbb_system_state_set_brightnesses (GbbSystemState *system_state,
                                        int             screen_brightness,
                                        int             keyboard_brightness);

GType gbb_system_state_get_type(void);

#endif /* __SYSTEM_STATE_H__ */

