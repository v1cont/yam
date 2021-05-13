/*
 * LibYAM -- E-Mail client library
 * Copyright (C) 2020 Victor Ananjevsky <victor@sanana.kiev.ua>
 * Copyright (C) 1999-2010 Hiroyuki Yamamoto
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301  USA
 */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include "defs.h"

#include <glib.h>
#include <glib/gi18n.h>

#ifdef G_OS_UNIX
#include <signal.h>
#endif

#if HAVE_LOCALE_H
#include <locale.h>
#endif

#include "ymain.h"
#include "yam-marshal.h"
#include "prefs_common.h"
#include "account.h"
#include "filter.h"
#include "folder.h"
#include "socket.h"
#include "codeconv.h"
#include "utils.h"

#if USE_SSL
#include "ssl.h"
#endif

#ifndef PACKAGE
#define PACKAGE	GETTEXT_PACKAGE
#endif

G_DEFINE_TYPE (YamApp, yam_app, G_TYPE_OBJECT);

enum {
  INIT_DONE,
  APP_EXIT,
  APP_FORCE_EXIT,
  ADD_MSG,
  REMOVE_MSG,
  REMOVE_ALL_MSG,
  REMOVE_FOLDER,
  MOVE_FOLDER,
  FOLDERLIST_UPDATED,
  ACCOUNT_UPDATED,
  LAST_SIGNAL
};

static guint app_signals[LAST_SIGNAL] = { 0 };

static GObject *app = NULL;


static void
yam_app_init (YamApp * self)
{
}

static void
yam_app_class_init (YamAppClass * klass)
{
  GObjectClass *gobject_class = G_OBJECT_CLASS (klass);

  app_signals[INIT_DONE] =
    g_signal_new ("init-done",
                  G_TYPE_FROM_CLASS (gobject_class),
                  G_SIGNAL_RUN_FIRST, 0, NULL, NULL, yam_marshal_VOID__VOID, G_TYPE_NONE, 0);
  app_signals[APP_EXIT] =
    g_signal_new ("app-exit",
                  G_TYPE_FROM_CLASS (gobject_class),
                  G_SIGNAL_RUN_FIRST, 0, NULL, NULL, yam_marshal_VOID__VOID, G_TYPE_NONE, 0);
  app_signals[APP_FORCE_EXIT] =
    g_signal_new ("app-force-exit",
                  G_TYPE_FROM_CLASS (gobject_class),
                  G_SIGNAL_RUN_FIRST, 0, NULL, NULL, yam_marshal_VOID__VOID, G_TYPE_NONE, 0);
  app_signals[ADD_MSG] =
    g_signal_new ("add-msg",
                  G_TYPE_FROM_CLASS (gobject_class),
                  G_SIGNAL_RUN_FIRST,
                  0,
                  NULL, NULL,
                  yam_marshal_VOID__POINTER_STRING_UINT, G_TYPE_NONE, 3, G_TYPE_POINTER, G_TYPE_STRING, G_TYPE_UINT);
  app_signals[REMOVE_MSG] =
    g_signal_new ("remove-msg",
                  G_TYPE_FROM_CLASS (gobject_class),
                  G_SIGNAL_RUN_FIRST,
                  0,
                  NULL, NULL,
                  yam_marshal_VOID__POINTER_STRING_UINT, G_TYPE_NONE, 3, G_TYPE_POINTER, G_TYPE_STRING, G_TYPE_UINT);
  app_signals[REMOVE_ALL_MSG] =
    g_signal_new ("remove-all-msg",
                  G_TYPE_FROM_CLASS (gobject_class),
                  G_SIGNAL_RUN_FIRST, 0, NULL, NULL, yam_marshal_VOID__POINTER, G_TYPE_NONE, 1, G_TYPE_POINTER);
  app_signals[REMOVE_FOLDER] =
    g_signal_new ("remove-folder",
                  G_TYPE_FROM_CLASS (gobject_class),
                  G_SIGNAL_RUN_FIRST, 0, NULL, NULL, yam_marshal_VOID__POINTER, G_TYPE_NONE, 1, G_TYPE_POINTER);
  app_signals[MOVE_FOLDER] =
    g_signal_new ("move-folder",
                  G_TYPE_FROM_CLASS (gobject_class),
                  G_SIGNAL_RUN_FIRST,
                  0,
                  NULL, NULL,
                  yam_marshal_VOID__POINTER_STRING_STRING,
                  G_TYPE_NONE, 3, G_TYPE_POINTER, G_TYPE_STRING, G_TYPE_STRING);
  app_signals[FOLDERLIST_UPDATED] =
    g_signal_new ("folderlist-updated",
                  G_TYPE_FROM_CLASS (gobject_class),
                  G_SIGNAL_RUN_FIRST, 0, NULL, NULL, yam_marshal_VOID__VOID, G_TYPE_NONE, 0);
  app_signals[ACCOUNT_UPDATED] =
    g_signal_new ("account-updated",
                  G_TYPE_FROM_CLASS (gobject_class),
                  G_SIGNAL_RUN_FIRST, 0, NULL, NULL, yam_marshal_VOID__VOID, G_TYPE_NONE, 0);
}

GObject *
yam_app_create (void)
{
  if (!app)
    app = g_object_new (YAM_TYPE_APP, NULL);
  return app;
}

GObject *
yam_app_get (void)
{
  return app;
}

void
yam_init (void)
{
  setlocale (LC_ALL, "");

  set_startup_dir ();

#ifdef ENABLE_NLS
  yam_init_gettext (PACKAGE, LOCALEDIR);
  textdomain (PACKAGE);
#endif

  /* ignore SIGPIPE signal for preventing sudden death of program */
  signal (SIGPIPE, SIG_IGN);
}

#define MAKE_DIR_IF_NOT_EXIST(dir)					\
{									\
	if (!is_dir_exist(dir)) {					\
		if (is_file_exist(dir)) {				\
			g_warning("File '%s' already exists. "		\
				  "Can't create folder.", dir);		\
			return -1;					\
		}							\
		if (make_dir(dir) < 0)					\
			return -1;					\
	}								\
}

void
yam_init_gettext (const gchar * package, const gchar * dirname)
{
#ifdef ENABLE_NLS
  if (g_path_is_absolute (dirname))
    bindtextdomain (package, dirname);
  else
    {
      gchar *locale_dir;

      locale_dir = g_strconcat (get_startup_dir (), G_DIR_SEPARATOR_S, dirname, NULL);
      bindtextdomain (package, locale_dir);
      g_free (locale_dir);
    }

  bind_textdomain_codeset (package, CS_UTF_8);
#endif /* ENABLE_NLS */
}

gint
yam_setup_rc_dir (void)
{
  if (!is_dir_exist (get_rc_dir ()))
    {
      if (make_dir_hier (get_rc_dir ()) < 0)
        return -1;
    }

  MAKE_DIR_IF_NOT_EXIST (get_mail_base_dir ());

  CHDIR_RETURN_VAL_IF_FAIL (get_rc_dir (), -1);

  MAKE_DIR_IF_NOT_EXIST (get_imap_cache_dir ());
  MAKE_DIR_IF_NOT_EXIST (get_news_cache_dir ());
  MAKE_DIR_IF_NOT_EXIST (get_mime_tmp_dir ());
  MAKE_DIR_IF_NOT_EXIST (get_tmp_dir ());
  MAKE_DIR_IF_NOT_EXIST (UIDL_DIR);
  MAKE_DIR_IF_NOT_EXIST (PLUGIN_DIR);

  /* remove temporary files */
  remove_all_files (get_tmp_dir ());
  remove_all_files (get_mime_tmp_dir ());

  return 0;
}

void
yam_save_all_state (void)
{
  folder_write_list ();
  prefs_common_write_config ();
  filter_write_config ();
  account_write_config_all ();
}

void
yam_cleanup (void)
{
  /* remove temporary files */
  remove_all_files (get_tmp_dir ());
  remove_all_files (get_mime_tmp_dir ());
  g_log_set_default_handler (g_log_default_handler, NULL);
  close_log_file ();

  if (app)
    {
      g_object_unref (app);
      app = NULL;
    }
}
