EXTRA_DIST =
CLEANFILES =

applicationsdir = $(datadir)/applications
applications_DATA = org.gnome.BatteryBench.desktop
EXTRA_DIST += $(applications_DATA)

sessionbusdir= $(datadir)/dbus-1/services
sessionbus_DATA = org.gnome.BatteryBench.service
EXTRA_DIST += org.gnome.BatteryBench.service.in
CLEANFILES += org.gnome.BatteryBench.service

systembusdir= $(datadir)/dbus-1/system-services
systembus_DATA = org.gnome.BatteryBench.Helper.service
EXTRA_DIST += org.gnome.BatteryBench.Helper.service.in
CLEANFILES += org.gnome.BatteryBench.Helper.service

systembusconfdir= $(sysconfdir)/dbus-1/system.d
systembusconf_DATA = org.gnome.BatteryBench.Helper.conf
EXTRA_DIST += $(systembusconf_DATA)

polkitdir = $(datadir)/polkit-1/actions
polkit_DATA = org.gnome.BatteryBench.Helper.policy
EXTRA_DIST += $(polkit_DATA)

%.service: %.service.in Makefile
	$(AM_V_GEN) sed -e "s%@LIBEXECDIR@%$(libexecdir)%" -e "s%@BINDIR@%$(bindir)%" < $< > $@ || rm $@
