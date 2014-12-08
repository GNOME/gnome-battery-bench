

#include <glib.h>
#include <X11/Xlib.h>

typedef struct _GbbEventRecorder GbbEventRecorder;

GbbEventRecorder *gbb_event_recorder_new(Display    *display,
                                         const char *filename);

void gbb_event_recorder_record(GbbEventRecorder *recorder);
void gbb_event_recorder_free (GbbEventRecorder *recorder);
