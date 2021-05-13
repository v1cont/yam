moduledir = $(libdir)/yam/plugins

module_LTLIBRARIES = test.la

test_la_SOURCES = test.c test.h
test_la_LDFLAGS = -module -no-undefined -avoid-version

AM_CPPFLAGS = \
	-I$(top_srcdir) \
	-I$(top_srcdir)/lib \
	-I$(top_srcdir)/src \
	$(GTK_CFLAGS)

test_la_LIBADD = \
	$(GTK_LIBS) \
	$(top_builddir)/src/libyam-plugin.la \
	$(top_builddir)/lib/libyam.la

install:
install-plugin: install-am
