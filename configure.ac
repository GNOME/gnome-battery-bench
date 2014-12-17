AC_PREREQ(2.63)
AC_INIT([gnome-battery-bench],[3.15.0],
	[https://bugzilla.gnome.org/enter_bug.cgi?product=gnome-hwtest&component=simulator],
	[gnome-battery-bench],
	[https://wiki.gnome.org/Projects/BatteryLife])

AC_CONFIG_HEADERS([config.h])
AC_CONFIG_MACRO_DIR([m4])

AM_INIT_AUTOMAKE([1.12 dist-xz no-dist-gzip foreign tar-ustar no-define serial-tests -Wno-portability])

AM_SILENT_RULES([yes])

PKG_PROG_PKG_CONFIG([0.22])

GLIB_GSETTINGS

base_packages="libevdev glib-2.0 gio-unix-2.0"
x_packages="x11 xi xtst"

AC_PROG_CC
AM_PROG_CC_C_O

PKG_CHECK_MODULES([HELPER], [$base_packages polkit-gobject-1])
PKG_CHECK_MODULES([COMMANDLINE], [$base_packages $x_packages])
PKG_CHECK_MODULES([APPLICATION], [$base_packages $x_packages gtk+-3.0])

GLIB_COMPILE_RESOURCES=`$PKG_CONFIG --variable glib_compile_resources gio-2.0`
AC_SUBST(GLIB_COMPILE_RESOURCES)

GNOME_COMPILE_WARNINGS([error])
case "$WARN_CFLAGS" in
    *-Werror*)
        WARN_CFLAGS="$WARN_CFLAGS -Wno-error=deprecated-declarations"
        ;;
esac

AM_CFLAGS="$AM_CFLAGS $WARN_CFLAGS"
AC_SUBST(AM_CFLAGS)

AC_CONFIG_FILES([
  Makefile
  data/Makefile
  src/Makefile
  tests/Makefile
])

AC_OUTPUT
