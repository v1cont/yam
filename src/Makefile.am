
SUBDIRS = icons

AM_CPPFLAGS = \
	-DLOCALEDIR=\""$(localedir)"\" \
	-DPLUGINDIR=\""$(libdir)/yam/plugins"\" \
	-DSYSCONFDIR=\""$(sysconfdir)"\" \
	-DG_LOG_DOMAIN=\"YAM\"

AM_YFLAGS = -d

bin_PROGRAMS = yam

yam_SOURCES = \
	main.c main.h \
	itemfactory.c itemfactory.h \
	mainwindow.c mainwindow.h \
	folderview.c folderview.h \
	summaryview.c summaryview.h \
	messageview.c messageview.h \
	headerview.c headerview.h \
	textview.c textview.h \
	imageview.c imageview.h \
	mimeview.c mimeview.h \
	quick_search.c quick_search.h \
	query_search.c query_search.h \
	message_search.c message_search.h \
	colorlabel.c colorlabel.h \
	action.c action.h \
	compose.c compose.h \
	menu.c menu.h \
	stock_pixmap.c stock_pixmap.h \
	prefs_ui.c prefs_ui.h \
	prefs_common_dialog.c prefs_common_dialog.h \
	prefs_filter.c prefs_filter.h \
	prefs_filter_edit.c prefs_filter_edit.h \
	prefs_account_dialog.c prefs_account_dialog.h \
	prefs_folder_item.c prefs_folder_item.h \
	prefs_display_items.c prefs_display_items.h \
	prefs_display_header.c prefs_display_header.h \
	prefs_customheader.c prefs_customheader.h \
	prefs_summary_column.c prefs_summary_column.h \
	prefs_template.c prefs_template.h \
	prefs_actions.c prefs_actions.h \
	prefs_search_folder.c prefs_search_folder.h \
	prefs_toolbar.c prefs_toolbar.h \
	account_dialog.c account_dialog.h \
	template.c template.h \
	addressbook.c addressbook.h \
	addr_compl.c addr_compl.h \
	addressitem.h \
	addritem.c addritem.h \
	addrcache.c addrcache.h \
	addrbook.c addrbook.h \
	addrindex.c addrindex.h \
	mgutils.c mgutils.h \
	vcard.c vcard.h \
	ldif.c ldif.h \
	importldif.c importldif.h \
	importcsv.c importcsv.h \
	exportcsv.c exportcsv.h \
	ldap.c ldap.h \
	editbook.c editbook.h \
	editgroup.c editgroup.h \
	editaddress.c editaddress.h \
	editvcard.c editvcard.h \
	editldap.c editldap.h \
	editldap_basedn.c editldap_basedn.h \
	addressadd.c addressadd.h \
	filesel.c filesel.h \
	foldersel.c foldersel.h \
	statusbar.c statusbar.h \
	logwindow.c logwindow.h \
	sourcewindow.c sourcewindow.h \
	manage_window.c manage_window.h \
	undo.c undo.h \
	alertpanel.c alertpanel.h \
	inputdialog.c inputdialog.h \
	progressdialog.c progressdialog.h \
	subscribedialog.c subscribedialog.h \
	about.c about.h \
	setup.c setup.h \
	gtkutils.c gtkutils.h \
	send_message.c send_message.h \
	inc.c inc.h \
	import.c import.h \
	export.c export.h \
	rfc2015.c rfc2015.h \
	passphrase.c passphrase.h \
	select-keys.c select-keys.h \
	sigstatus.c sigstatus.h \
	trayicon.c trayicon.h \
	printing.c printing.h \
	sslmanager.c sslmanager.h \
	plugin_manager.c plugin_manager.h \
	quote_fmt_lex.l quote_fmt_lex.h \
	quote_fmt_parse.y quote_fmt.h \
	yam-marshal.c yam-marshal.h \
	notificationwindow.c notificationwindow.h \
	resources.c

BUILT_SOURCES = \
	quote_fmt_lex.c \
	quote_fmt_parse.c \
	quote_fmt_parse.h \
	yam-marshal.c \
	yam-marshal.h \
	plugin-marshal.c \
	plugin-marshal.h \
	resources.c

lib_LTLIBRARIES = libyam-plugin.la

libyam_plugin_la_SOURCES = \
	plugin-marshal.c plugin-marshal.h \
	plugin.c

libyam_plugin_includedir=$(includedir)/yam
libyam_plugin_include_HEADERS = \
	plugin.h \
	plugin-types.h

libyam_plugin_la_CFLAGS = \
	-I$(top_srcdir)/lib \
	$(GLIB_CFLAGS) \
	$(GTK_CFLAGS)

libyam_plugin_la_LDFLAGS = \
	-version-info $(LT_CURRENT):$(LT_REVISION):$(LT_AGE) \
	-export-dynamic $(no_undefined) $(export_symbols)

libyam_plugin_la_LIBADD = \
	$(GTK_LIBS) \
	$(GLIB_LIBS) \
	../lib/libyam.la

yam_CFLAGS = \
	-I$(top_srcdir)/lib \
	$(GLIB_CFLAGS) \
	$(GTK_CFLAGS) \
	$(GPGME_CFLAGS) \
	$(SPELL_CFLAGS)

yam_LDADD = \
	$(INTLLIBS) \
	$(GTK_LIBS) \
	$(GLIB_LIBS) \
	$(SPELL_LIBS) \
	$(GPGME_LIBS) \
	$(LDAP_LIBS) \
	$(LIBICONV) \
	$(YAM_LIBS) \
	./libyam-plugin.la \
	../lib/libyam.la

resources.c: yam.gresource.xml $(shell $(GLIB_COMPILE_RESOURCES) --sourcedir=. --generate-dependencies ./yam.gresource.xml)
	$(AM_V_GEN) $(GLIB_COMPILE_RESOURCES) ./yam.gresource.xml --target=$@ --sourcedir=. --generate-source

yam-marshal.h: yam-marshal.list
	$(AM_V_GEN) $(GLIB_GENMARSHAL) $< --header --prefix=yam_marshal > $@

yam-marshal.c: yam-marshal.list
	$(AM_V_GEN) $(GLIB_GENMARSHAL) $< --body --prefix=yam_marshal > $@

plugin-marshal.h: plugin-marshal.list
	$(AM_V_GEN) $(GLIB_GENMARSHAL) $< --header --prefix=yam_plugin_marshal > $@

plugin-marshal.c: plugin-marshal.list
	$(AM_V_GEN) $(GLIB_GENMARSHAL) $< --body --prefix=yam_plugin_marshal > $@

EXTRA_DIST = yam-marshal.list plugin-marshal.list libyam-plugin.def yam.gresource.xml

DISTCLEANFILES = $(BUILT_SOURCES)
