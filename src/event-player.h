#ifndef __EVENT_PLAYER_H__
#define __EVENT_PLAYER_H__

#include <glib-object.h>

typedef struct _GbbEventPlayer      GbbEventPlayer;
typedef struct _GbbEventPlayerClass GbbEventPlayerClass;

#define GBB_TYPE_EVENT_PLAYER         (gbb_event_player_get_type ())
#define GBB_EVENT_PLAYER(o)           (G_TYPE_CHECK_INSTANCE_CAST ((o), GBB_TYPE_EVENT_PLAYER, GbbEventPlayer))
#define GBB_EVENT_PLAYER_CLASS(k)     (G_TYPE_CHECK_CLASS_CAST((k), GBB_TYPE_EVENT_PLAYER, GbbEventPlayerClass))
#define GBB_IS_EVENT_PLAYER(o)        (G_TYPE_CHECK_INSTANCE_TYPE ((o), GBB_TYPE_EVENT_PLAYER))
#define GBB_IS_EVENT_PLAYER_CLASS(k)  (G_TYPE_CHECK_CLASS_TYPE ((k), GBB_TYPE_EVENT_PLAYER))
#define GBB_EVENT_PLAYER_GET_CLASS(o) (G_TYPE_INSTANCE_GET_CLASS ((o), GBB_TYPE_EVENT_PLAYER, GbbEventPlayerClass))

struct _GbbEventPlayer {
    GObject parent;

    gboolean ready;
    char *keyboard_device_node;
    char *mouse_device_node;
};

struct _GbbEventPlayerClass {
    GObjectClass parent_class;

  void (*play_fd) (GbbEventPlayer *player,
                   int             fd);
  void (*stop)    (GbbEventPlayer *player);
};

gboolean gbb_event_player_is_ready(GbbEventPlayer *player);

const char *gbb_event_player_get_keyboard_device_node(GbbEventPlayer *player);
const char *gbb_event_player_get_mouse_device_node   (GbbEventPlayer *player);

void gbb_event_player_play_fd  (GbbEventPlayer *player,
                                int             fd);
void gbb_event_player_play_file(GbbEventPlayer *player,
                                const char     *filename);
void gbb_event_player_stop     (GbbEventPlayer *player);

GType gbb_event_player_get_type(void);

/* For implementations */
void gbb_event_player_set_ready (GbbEventPlayer *player,
                                 const char     *keyboard_device_node,
                                 const char     *mouse_device_node);
void gbb_event_player_finished  (GbbEventPlayer *player);

#endif /* __EVENT_PLAYER_H__*/
