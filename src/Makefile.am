bin_PROGRAMS=gnome-battery-bench gbb
libexec_PROGRAMS=gnome-battery-bench-helper

base_sources =					\
	power-monitor.c				\
	power-monitor.h				\
	battery-test.c				\
	battery-test.h				\
	evdev-player.c				\
	evdev-player.h				\
	event-log.c				\
	event-log.h				\
	event-player.c				\
	event-player.h				\
	introspection.c				\
	introspection.h				\
	remote-player.c				\
	remote-player.h				\
	util.c					\
	util.h

x_sources =					\
	$(base_sources) 			\
	event-recorder.c			\
	event-recorder.h			\
	xinput-wait.c				\
	xinput-wait.h

gnome_battery_bench_CPPFLAGS = $(APPLICATION_CFLAGS) -DPKGDATADIR=\"$(pkgdatadir)\"
gnome_battery_bench_LDADD = $(APPLICATION_LIBS)

gnome_battery_bench_SOURCES =			\
	$(x_sources)				\
	gui.c					\
	application.c				\
	system-state.c				\
	system-state.h				\
	gnome-battery-bench.gresource.c

gbb_CPPFLAGS = $(COMMANDLINE_CFLAGS) -DPKGDATADIR=\"$(pkgdatadir)\"
gbb_LDADD = $(COMMANDLINE_LIBS)

gbb_SOURCES =					\
	$(x_sources)				\
	commandline.c

gnome_battery_bench_helper_CPPFLAGS = $(HELPER_CFLAGS) -DPKGDATADIR=\"$(pkgdatadir)\"
gnome_battery_bench_helper_LDADD = $(HELPER_LIBS)

gnome_battery_bench_helper_SOURCES =		\
	$(base_sources)				\
	replay-helper.c

OBJS = power-monitor.o system-state.o introspection.o

gnome-battery-bench.gresource.c: gnome-battery-bench.xml

%.gresource.c : %.gresource.xml
	$(GLIB_COMPILE_RESOURCES) --generate --target $@ $<
