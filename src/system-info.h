#ifndef __SYSTEM_INFO_H__
#define __SYSTEM_INFO_H__

#include <glib-object.h>

#define GBB_TYPE_SYSTEM_INFO gbb_system_info_get_type()
G_DECLARE_FINAL_TYPE(GbbSystemInfo, gbb_system_info, GBB, SYSTEM_INFO, GObject)

GbbSystemInfo *gbb_system_info_acquire (void);

#endif /* __SYSTEM_INFO_H__ */
