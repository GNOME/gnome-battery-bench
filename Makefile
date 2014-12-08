PKG_CONFIG_PACKAGES = libevdev x11 xi xtst glib-2.0 gio-unix-2.0

CFLAGS := $(shell pkg-config --cflags $(PKG_CONFIG_PACKAGES) gtk+-3.0)  -Wall -g
LIBS := $(shell pkg-config --libs $(PKG_CONFIG_PACKAGES))
GBB_LIBS := $(shell pkg-config --libs $(PKG_CONFIG_PACKAGES) gtk+-3.0)

OBJS = event-player.o event-recorder.o power-monitor.o evdev-player.o remote-player.o system-state.o util.o introspection.o xinput-wait.o

HEADERS = event-player.h event-recorder.h power-monitor.h remote-player.h util.h

all: powertest gnome-battery-bench gnome-battery-bench-replay-helper

%.gresource.c : %.gresource.xml
	glib-compile-resources --generate --target $@ $<

gnome-battery-bench.gresource.c: gnome-battery-bench.xml

$(OBJS) gui.o application.o: $(HEADERS)

powertest: $(OBJS) powertest.o
	$(CC) -o $@ $(LIBS) $(OBJS) powertest.o

gnome-battery-bench: gui.o application.o gnome-battery-bench.gresource.o $(OBJS)
	$(CC) -o $@ $(GBB_LIBS) $(OBJS) gui.o application.o gnome-battery-bench.gresource.o

gnome-battery-bench-replay-helper: $(OBJS) replay-helper.o
	$(CC) -o $@ $(GBB_LIBS) $(OBJS) replay-helper.o
