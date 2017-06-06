NULL =

EXTRA_DIST = integration-test

AM_CPPFLAGS =	                                                \
	-DPKGDATADIR=\"$(pkgdatadir)\"                          \
	-DDATADIR=\"$(datadir)\"                                \
	-DGNOME_SESSION_DIR="\"$(gnome_session_libexecdir)\""	\
	$(NULL)

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
	power-supply.h				\
	power-supply.c				\
	system-info.h				\
	system-info.c				\
	system-state.c				\
	system-state.h				\
	test-run.c				\
	test-run.h				\
	test-runner.c				\
	test-runner.h				\
	xinput-wait.c				\
	xinput-wait.h				\
	util-sysfs.h				\
	util-sysfs.c

gnome_battery_bench_CPPFLAGS =  $(AM_CPPFLAGS) $(APPLICATION_CFLAGS)
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

gbb_CPPFLAGS =  $(AM_CPPFLAGS) $(COMMANDLINE_CFLAGS)
gbb_LDADD = $(COMMANDLINE_LIBS)

gbb_SOURCES =					\
	$(client_sources)			\
	commandline.c

gnome_battery_bench_helper_CPPFLAGS =  $(AM_CPPFLAGS) $(HELPER_CFLAGS)
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
	$(AM_V_GEN) $(GLIB_COMPILE_RESOURCES) \
        --sourcedir="$(srcdir)"               \
        --generate                            \
        --target $@ $<

check-local: gbb
	env top_builddir=$(top_builddir) $(srcdir)/integration-test -v
