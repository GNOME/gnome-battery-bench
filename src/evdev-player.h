#ifndef __EVDEV_PLAYER_H__
#define __EVDEV_PLAYER_H__

#include "event-player.h"

typedef struct _GbbEvdevPlayer GbbEvdevPlayer;

#define GBB_TYPE_EVDEV_PLAYER         (gbb_evdev_player_get_type ())
#define GBB_EVDEV_PLAYER(o)           (G_TYPE_CHECK_INSTANCE_CAST ((o), GBB_TYPE_EVDEV_PLAYER, GbbEvdevPlayer))
#define GBB_EVDEV_PLAYER_CLASS(k)     (G_TYPE_CHECK_CLASS_CAST((k), GBB_TYPE_EVDEV_PLAYER, GbbEvdevPlayerClass))
#define GBB_IS_EVDEV_PLAYER(o)        (G_TYPE_CHECK_INSTANCE_TYPE ((o), GBB_TYPE_EVDEV_PLAYER))
#define GBB_IS_EVDEV_PLAYER_CLASS(k)  (G_TYPE_CHECK_CLASS_TYPE ((k), GBB_TYPE_EVDEV_PLAYER))
#define GBB_EVDEV_PLAYER_GET_CLASS(o) (G_TYPE_INSTANCE_GET_CLASS ((o), GBB_TYPE_EVDEV_PLAYER, GbbEvdevPlayerClass))

GbbEvdevPlayer *gbb_evdev_player_new(const char *name,
                                     GError    **error);

GType gbb_evdev_player_get_type(void);

#endif /* __EVDEV_PLAYER_H__*/
