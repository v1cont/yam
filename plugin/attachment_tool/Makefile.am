moduledir = $(libdir)/yam/plugins

module_LTLIBRARIES = attachment_tool.la

attachment_tool_la_SOURCES = attachment_tool.c
attachment_tool_la_LDFLAGS = -module -no-undefined -avoid-version

AM_CPPFLAGS = \
	-I$(top_srcdir) \
	-I$(top_srcdir)/lib \
	-I$(top_srcdir)/src \
	$(GTK_CFLAGS)

attachment_tool_la_LIBADD = \
	$(GTK_LIBS) \
	$(top_srcdir)/src/libyam-plugin.la \
	$(top_srcdir)/lib/libyam.la
