#ifndef __REMOTE_PLAYER_H__
#define __REMOTE_PLAYER_H__

#include "event-player.h"

typedef struct _GbbRemotePlayer GbbRemotePlayer;

#define GBB_TYPE_REMOTE_PLAYER         (gbb_remote_player_get_type ())
#define GBB_REMOTE_PLAYER(o)           (G_TYPE_CHECK_INSTANCE_CAST ((o), GBB_TYPE_REMOTE_PLAYER, GbbRemotePlayer))
#define GBB_REMOTE_PLAYER_CLASS(k)     (G_TYPE_CHECK_CLASS_CAST((k), GBB_TYPE_REMOTE_PLAYER, GbbRemotePlayerClass))
#define GBB_IS_REMOTE_PLAYER(o)        (G_TYPE_CHECK_INSTANCE_TYPE ((o), GBB_TYPE_REMOTE_PLAYER))
#define GBB_IS_REMOTE_PLAYER_CLASS(k)  (G_TYPE_CHECK_CLASS_TYPE ((k), GBB_TYPE_REMOTE_PLAYER))
#define GBB_REMOTE_PLAYER_GET_CLASS(o) (G_TYPE_INSTANCE_GET_CLASS ((o), GBB_TYPE_REMOTE_PLAYER, GbbRemotePlayerClass))

GbbRemotePlayer *gbb_remote_player_new(const char *name);

GType gbb_remote_player_get_type(void);

#endif /* __REMOTE_PLAYER_H__ */
