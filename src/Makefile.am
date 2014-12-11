bin_PROGRAMS=gnome-battery-bench gbb
libexec_PROGRAMS=gnome-battery-bench-replay-helper

base_sources =					\
	power-monitor.c				\
	power-monitor.h				\
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

x_sources =					\
	$(base_sources) 			\
	event-recorder.c			\
	event-recorder.h			\
	xinput-wait.c				\
	xinput-wait.h

gnome_battery_bench_CPPFLAGS = $(APPLICATION_CFLAGS)
gnome_battery_bench_LDADD = $(APPLICATION_LIBS)

gnome_battery_bench_SOURCES =			\
	$(x_sources)				\
	gui.c					\
	application.c				\
	system-state.c				\
	system-state.h				\
	gnome-battery-bench.gresource.c

gbb_CPPFLAGS = $(COMMANDLINE_CFLAGS)
gbb_LDADD = $(COMMANDLINE_LIBS)

gbb_SOURCES =					\
	$(x_sources)				\
	commandline.c

gnome_battery_bench_replay_helper_CPPFLAGS = $(HELPER_CFLAGS)
gnome_battery_bench_replay_helper_LDADD = $(HELPER_LIBS)

gnome_battery_bench_replay_helper_SOURCES =	\
	$(base_sources)				\
	replay-helper.c

OBJS = power-monitor.o system-state.o introspection.o

%.gresource.c : %.gresource.xml
	$(GLIB_COMPILE_RESOURCES) --generate --target $@ $<
