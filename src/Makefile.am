EXTRA_DIST =

bin_PROGRAMS=gnome-battery-bench gbb
libexec_PROGRAMS=gnome-battery-bench-helper

base_sources =					\
	event-log.c				\
	event-log.h				\
	evdev-player.c				\
	evdev-player.h				\
	event-player.c				\
	event-player.h				\
	introspection.c				\
	introspection.h				\
	remote-player.c				\
	remote-player.h				\
	util.c					\
	util.h

client_sources =				\
	$(base_sources) 			\
	battery-test.c				\
	battery-test.h				\
	event-recorder.c			\
	event-recorder.h			\
	power-monitor.c				\
	power-monitor.h				\
	system-state.c				\
	system-state.h				\
	test-run.c				\
	test-run.h				\
	test-runner.c				\
	test-runner.h				\
	xinput-wait.c				\
	xinput-wait.h

gnome_battery_bench_CPPFLAGS = $(APPLICATION_CFLAGS) -DPKGDATADIR=\"$(pkgdatadir)\"
gnome_battery_bench_LDADD = $(APPLICATION_LIBS)

gnome_battery_bench_SOURCES =			\
	$(client_sources)			\
	gui.c					\
	application.c				\
	application.h				\
	power-graphs.c				\
	power-graphs.h				\
	util-gtk.c				\
	util-gtk.h				\
	gnome-battery-bench.gresource.c

gbb_CPPFLAGS = $(COMMANDLINE_CFLAGS) -DPKGDATADIR=\"$(pkgdatadir)\"
gbb_LDADD = $(COMMANDLINE_LIBS)

gbb_SOURCES =					\
	$(client_sources)			\
	commandline.c

gnome_battery_bench_helper_CPPFLAGS = $(HELPER_CFLAGS) -DPKGDATADIR=\"$(pkgdatadir)\"
gnome_battery_bench_helper_LDADD = $(HELPER_LIBS)

gnome_battery_bench_helper_SOURCES =		\
	$(base_sources)				\
	replay-helper.c

ui_files =					\
	application.ui				\
	power-graphs.ui

EXTRA_DIST += $(ui_files)

gnome-battery-bench.gresource.c: $(ui_files)

%.gresource.c : %.gresource.xml
	$(GLIB_COMPILE_RESOURCES) --generate --target $@ $<
