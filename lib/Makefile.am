
AM_CPPFLAGS = \
	-DG_LOG_DOMAIN=\"LibYAM\" \
	-DSYSCONFDIR=\""$(sysconfdir)"\" \
	-DLOCALEDIR=\""$(localedir)"\"

lib_LTLIBRARIES = libyam.la

libyam_la_SOURCES = \
	account.c \
	base64.c \
	codeconv.c \
	customheader.c \
	displayheader.c \
	filter.c \
	folder.c \
	html.c \
	imap.c \
	mbox.c \
	md5.c \
	md5_hmac.c \
	mh.c \
	news.c \
	nntp.c \
	pop.c \
	prefs.c \
	prefs_account.c \
	prefs_common.c \
	procheader.c \
	procmime.c \
	procmsg.c \
	quoted-printable.c \
	recv.c \
	session.c \
	smtp.c \
	socket.c \
	socks.c \
	ssl.c \
	ssl_hostname_validation.c \
	stringtable.c \
	unmime.c \
	utils.c \
	uuencode.c \
	virtual.c \
	xml.c \
	ymain.c \
	yam-marshal.c

libyam_la_CFLAGS = -I$(top_srcdir) $(GLIB_CFLAGS) $(LIBYAM_CFLAGS)

libyam_includedir=$(includedir)/yam/lib
libyam_include_HEADERS = \
	defs.h \
	enums.h \
	account.h \
	base64.h \
	codeconv.h \
	customheader.h \
	displayheader.h \
	filter.h \
	folder.h \
	html.h \
	imap.h \
	mbox.h \
	md5.h \
	md5_hmac.h \
	mh.h \
	news.h \
	nntp.h \
	pop.h \
	prefs.h \
	prefs_account.h \
	prefs_common.h \
	procheader.h \
	procmime.h \
	procmsg.h \
	quoted-printable.h \
	recv.h \
	session.h \
	smtp.h \
	socket.h \
	socks.h \
	ssl.h \
	ssl_hostname_validation.h \
	stringtable.h \
	unmime.h \
	utils.h \
	uuencode.h \
	virtual.h \
	xml.h \
	ymain.h \
	yam-marshal.h

BUILT_SOURCES = \
	yam-marshal.c \
	yam-marshal.h

libyam_la_LDFLAGS = \
	$(GLIB_LIBS) \
	-version-info $(LT_CURRENT):$(LT_REVISION):$(LT_AGE) \
	-export-dynamic $(no_undefined) $(export_symbols)

libyam_la_LIBADD = $(GLIB_LIBS) $(LIBICONV) $(LIBYAM_LIBS)

yam-marshal.h: yam-marshal.list
	$(AM_V_GEN) $(GLIB_GENMARSHAL) $< --header --prefix=yam_marshal > $@

yam-marshal.c: yam-marshal.list
	$(AM_V_GEN) $(GLIB_GENMARSHAL) $< --body --prefix=yam_marshal > $@

EXTRA_DIST = yam-marshal.list

DISTCLEANFILES = $(BUILT_SOURCES)
