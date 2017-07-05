#ifndef __SYSTEM_INFO_H__
#define __SYSTEM_INFO_H__

#include <glib-object.h>
#include <json-glib/json-glib.h>


typedef struct GbbPciClass {
    guint8 code;
    guint8 sub;
    guint8 progif;
} GbbPciClass;

#define GBB_TYPE_PCI_CLASS (gbb_pci_class_get_type())

GType            gbb_pci_class_get_type     (void);
GbbPciClass *    gbb_pci_class_copy         (const GbbPciClass *klass);
void             gbb_pci_class_free         (GbbPciClass       *klass);


#define GBB_TYPE_PCI_DEVICE gbb_pci_device_get_type()
G_DECLARE_DERIVABLE_TYPE(GbbPciDevice, gbb_pci_device, GBB, PCI_DEVICE, GObject)

#define GBB_TYPE_CPU gbb_cpu_get_type()
G_DECLARE_FINAL_TYPE(GbbCpu, gbb_cpu, GBB, CPU, GObject)

#define GBB_TYPE_SYSTEM_INFO gbb_system_info_get_type()
G_DECLARE_FINAL_TYPE(GbbSystemInfo, gbb_system_info, GBB, SYSTEM_INFO, GObject)

GbbSystemInfo *gbb_system_info_acquire       (void);
void           gbb_system_info_to_json       (const GbbSystemInfo *info,
                                              JsonBuilder         *builder);


#endif /* __SYSTEM_INFO_H__ */
