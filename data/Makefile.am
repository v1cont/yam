
SUBDIRS = icons

desktopdir = $(datadir)/applications
desktop_in_files = yam.desktop.in
desktop_DATA = $(desktop_in_files:.desktop.in=.desktop)
@INTLTOOL_DESKTOP_RULE@

pkgconfigdir = $(libdir)/pkgconfig
pkgconfig_DATA = yam.pc

EXTRA_DIST = $(desktop_in_files) PLUGIN.txt mime.types
 