AC_PREREQ(2.63)
AC_INIT([gnome-battery-bench],[3.25.1],
	[https://bugzilla.gnome.org/enter_bug.cgi?product=gnome-hwtest&component=simulator],
	[gnome-battery-bench],
	[https://wiki.gnome.org/Projects/BatteryLife])

AC_CONFIG_HEADERS([config.h])
AC_CONFIG_MACRO_DIR([m4])

AM_INIT_AUTOMAKE([1.12 dist-xz no-dist-gzip foreign tar-ustar no-define serial-tests -Wno-portability])

AM_SILENT_RULES([yes])

PKG_PROG_PKG_CONFIG([0.22])

GLIB_GSETTINGS

base_packages="libevdev glib-2.0 >= 2.44.0 gio-unix-2.0"
x_packages="x11 xi xtst"
app_packages="json-glib-1.0 gudev-1.0 gdk-3.0"

AC_PROG_CC
AM_PROG_CC_C_O

PKG_CHECK_MODULES([HELPER], [$base_packages polkit-gobject-1])
PKG_CHECK_MODULES([COMMANDLINE], [$base_packages $x_packages $app_packages])
PKG_CHECK_MODULES([APPLICATION], [$base_packages $x_packages $app_packages gtk+-3.0])

AC_PATH_PROG(ASCIIDOC, asciidoc, "")
AC_PATH_PROG(XMLTO, xmlto, "")
AM_CONDITIONAL(BUILD_DOCS, [ test $ASCIIDOC != x  -a $XMLTO != x ])

GLIB_COMPILE_RESOURCES=`$PKG_CONFIG --variable glib_compile_resources gio-2.0`
AC_SUBST(GLIB_COMPILE_RESOURCES)

dnl needed to locate gnome-session-check-accelerated
AC_ARG_WITH([gnome-session-libexecdir],
	    AS_HELP_STRING([--with-gnome-session-libexecdir=DIR], [Directory for gnome-session s libexecdir]),
	    [],
	    [with_gnome_session_libexecdir=${libexecdir}])
if test x$with_gnome_session_libexecdir == xno; then
	with_gnome_session_libexecdir=${libexecdir}
fi
AS_AC_EXPAND(GNOMESESSIONDIR, $with_gnome_session_libexecdir)
AC_SUBST([gnome_session_libexecdir], [$GNOMESESSIONDIR])


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
  docs/Makefile
  src/Makefile
  tests/Makefile
])

AC_OUTPUT
