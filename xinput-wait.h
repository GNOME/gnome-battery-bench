#ifndef __XINPUT_WAIT_H__
#define __XINPUT_WAIT_H__

#include <gio/gio.h>
#include "event-player.h"

void gbb_xinput_wait (GbbEventPlayer      *player,
                      const char          *display_name,
                      GCancellable        *cancellable,
                      GAsyncReadyCallback  callback,
                      gpointer             user_data);

gboolean gbb_xinput_wait_finish (GbbEventPlayer *player,
                                 GAsyncResult   *result,
                                 GError        **error);

#endif /* __XINPUT_WAIT_H__*/
