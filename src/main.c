/*
 * YAM -- a GTK+ based, lightweight, and fast e-mail client
 * Copyright (C) 2020 Victor Ananjevsky <victor@sanana.kiev.ua>
 * Copyright (C) 1999-2016 Hiroyuki Yamamoto
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA 02111-1307, USA.
 */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include "defs.h"

#include <glib.h>
#include <glib/gi18n.h>
#include <gtk/gtk.h>

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <unistd.h>
#include <time.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <signal.h>
#include <locale.h>

#if USE_GPGME
#include <gpgme.h>
#endif

#include "main.h"
#include "mainwindow.h"
#include "folderview.h"
#include "summaryview.h"
#include "prefs_common.h"
#include "prefs_account.h"
#include "prefs_actions.h"
#include "prefs_display_header.h"
#include "account.h"
#include "account_dialog.h"
#include "procmsg.h"
#include "procheader.h"
#include "filter.h"
#include "send_message.h"
#include "inc.h"
#include "manage_window.h"
#include "alertpanel.h"
#include "inputdialog.h"
#include "statusbar.h"
#include "addressbook.h"
#include "addrindex.h"
#include "compose.h"
#include "logwindow.h"
#include "folder.h"
#include "setup.h"
#include "ymain.h"
#include "utils.h"
#include "gtkutils.h"
#include "socket.h"
#include "stock_pixmap.h"
#include "trayicon.h"
#include "notificationwindow.h"
#include "plugin.h"
#include "plugin_manager.h"
#include "foldersel.h"
#include "colorlabel.h"

#if USE_GPGME
#include "rfc2015.h"
#endif
#if USE_SSL
#include "ssl.h"
#include "sslmanager.h"
#endif

gchar *prog_version;

static gint lock_socket = -1;
static gint lock_socket_tag = 0;
static GIOChannel *lock_ch = NULL;
static gchar *instance_id = NULL;

/* enables recursive locking with gdk_thread_enter / gdk_threads_leave */
static GRecMutex yam_mutex;

static GThread *main_thread;

static struct RemoteCmd {
  gboolean receive;
  gboolean receive_all;
  gboolean compose;
  const gchar *compose_mailto;
  GPtrArray *attach_files;
  gboolean send;
  gboolean status;
  gboolean status_full;
  GPtrArray *status_folders;
  GPtrArray *status_full_folders;
  gchar *open_msg;
  gboolean configdir;
  gboolean safe_mode;
  gboolean exit;
  gboolean restart;
  gchar *argv0;
} cmd;

#define STATUSBAR_PUSH(mainwin, str)                        \
  {                                                         \
	gtk_statusbar_push(GTK_STATUSBAR(mainwin->statusbar),   \
                       mainwin->mainwin_cid, str);          \
	gtk_widget_queue_draw(mainwin->statusbar);              \
  }

#define STATUSBAR_POP(mainwin)                              \
  {                                                         \
	gtk_statusbar_pop(GTK_STATUSBAR(mainwin->statusbar),    \
                      mainwin->mainwin_cid);                \
  }

static void parse_cmd_opt (int argc, char *argv[]);

static void app_init (void);
static void parse_gtkrc_files (void);
static void setup_rc_dir (void);
static void check_gpg (void);
static void set_log_handlers (gboolean enable);
static void register_system_events (void);
static void plugin_init (void);

static gchar *get_socket_name (void);
static gint prohibit_duplicate_launch (void);
static gint lock_socket_remove (void);
static gboolean lock_socket_input_cb (GIOChannel * source, GIOCondition condition, gpointer data);

static void remote_command_exec (void);

static void open_compose_new (const gchar * address, GPtrArray * attach_files);
static void open_message (const gchar * path);

static void send_queue (void);

#define MAKE_DIR_IF_NOT_EXIST(dir)              \
  {                                             \
	if (!is_dir_exist(dir)) {					\
      if (is_file_exist(dir)) {                 \
        alertpanel_warning                      \
          (_("File `%s' already exists.\n"      \
             "Can't create folder."),           \
           dir);                                \
        exit(1);                                \
      }                                         \
      if (make_dir(dir) < 0)					\
        exit(1);                                \
	}                                           \
  }

#define CHDIR_EXIT_IF_FAIL(dir, val)            \
  {                                             \
	if (change_dir(dir) < 0)                    \
      exit(val);                                \
  }

static void
load_cb (GObject * obj, GModule * module, gpointer data)
{
  debug_print ("load_cb: %p (%s), %p\n", module, module ? g_module_name (module) : "(null)", data);
}

int
main (int argc, char *argv[])
{
  MainWindow *mainwin;
  FolderView *folderview;
  GObject *yam_app;
  PrefsAccount *new_account = NULL;
  gchar *path;

  setlocale (LC_ALL, "");

#ifdef ENABLE_NLS
  bindtextdomain (GETTEXT_PACKAGE, LOCALEDIR);
  bind_textdomain_codeset (GETTEXT_PACKAGE, "UTF-8");
  textdomain (GETTEXT_PACKAGE);
#endif

  app_init ();
  parse_cmd_opt (argc, argv);

  /* check and create (unix domain) socket for remote operation */
  lock_socket = prohibit_duplicate_launch ();
  if (lock_socket < 0)
    return 0;

  if (cmd.status || cmd.status_full)
    {
      puts ("0 YAM not running.");
      lock_socket_remove ();
      return 0;
    }

  g_rec_mutex_init (&yam_mutex);

  gdk_threads_enter ();
  gtk_init (&argc, &argv);

  yam_app = yam_app_create ();

  parse_gtkrc_files ();
  setup_rc_dir ();

  if (is_file_exist ("yam.log"))
    {
      if (rename_force ("yam.log", "yam.log.bak") < 0)
        FILE_OP_ERROR ("yam.log", "rename");
    }
  set_log_file ("yam.log");

  set_ui_update_func (yam_events_flush);
  set_progress_func (main_window_progress_show);
  set_input_query_password_func (input_dialog_query_password);
#if USE_SSL
  ssl_init ();
  ssl_set_verify_func (ssl_manager_verify_cert);
#endif

  CHDIR_EXIT_IF_FAIL (g_get_home_dir (), 1);

  prefs_common_read_config ();
  filter_set_addressbook_func (addressbook_has_address);
  filter_read_config ();
  prefs_actions_read_config ();
  prefs_display_header_read_config ();
  colorlabel_read_config ();

  prefs_common.user_agent_str = g_strdup_printf
    ("%s (GTK+ %d.%d.%d)", prog_version, gtk_major_version, gtk_minor_version, gtk_micro_version);

  check_gpg ();

  sock_set_io_timeout (prefs_common.io_timeout_secs);

  path = g_build_filename (get_rc_dir (), "icons", NULL);
  if (is_dir_exist (path))
    {
      debug_print ("icon theme dir: %s\n", path);
      stock_pixbuf_set_theme_dir (path);
    }
  g_free (path);

  gtk_window_set_default_icon_name ("yam");

  mainwin = main_window_create (prefs_common.sep_folder | prefs_common.sep_msg << 1);
  folderview = mainwin->folderview;

  /* register the callback of socket input */
  if (lock_socket > 0)
    {
      lock_ch = g_io_channel_unix_new (lock_socket);
      lock_socket_tag = g_io_add_watch (lock_ch, G_IO_IN | G_IO_PRI | G_IO_ERR, lock_socket_input_cb, mainwin);
    }

  set_log_handlers (TRUE);

  account_read_config_all ();
  account_set_menu ();
  main_window_reflect_prefs_all ();

  if (folder_read_list () < 0)
    {
      setup_mailbox ();
      folder_write_list ();
    }
  if (!account_get_list ())
    new_account = setup_account ();

  account_set_menu ();
  main_window_reflect_prefs_all ();

  account_set_missing_folder ();
  folder_set_missing_folders ();
  folderview_set (folderview);
  if (new_account && new_account->folder)
    folder_write_list ();

  addressbook_read_file ();

  register_system_events ();

  inc_autocheck_timer_init (mainwin);

  plugin_init ();

  g_signal_emit_by_name (yam_app, "init-done");

  remote_command_exec ();

  gtk_main ();
  gdk_threads_leave ();

  return 0;
}

static void
parse_cmd_opt (int argc, char *argv[])
{
  gint i;

  for (i = 1; i < argc; i++)
    {
      if (!strncmp (argv[i], "--debug", 7))
        set_debug_mode (TRUE);
      else if (!strncmp (argv[i], "--receive-all", 13))
        cmd.receive_all = TRUE;
      else if (!strncmp (argv[i], "--receive", 9))
        cmd.receive = TRUE;
      else if (!strncmp (argv[i], "--compose", 9))
        {
          const gchar *p = argv[i + 1];

          cmd.compose = TRUE;
          cmd.compose_mailto = NULL;
          if (p && *p != '\0' && *p != '-')
            {
              if (!strncmp (p, "mailto:", 7))
                cmd.compose_mailto = p + 7;
              else
                cmd.compose_mailto = p;
              i++;
            }
        }
      else if (!strncmp (argv[i], "--attach", 8))
        {
          const gchar *p = argv[i + 1];
          gchar *file;

          while (p && *p != '\0' && *p != '-')
            {
              if (!cmd.attach_files)
                cmd.attach_files = g_ptr_array_new ();
              if (!g_path_is_absolute (p))
                file = g_strconcat (get_startup_dir (), G_DIR_SEPARATOR_S, p, NULL);
              else
                file = g_strdup (p);
              g_ptr_array_add (cmd.attach_files, file);
              i++;
              p = argv[i + 1];
            }
        }
      else if (!strncmp (argv[i], "--send", 6))
        cmd.send = TRUE;
      else if (!strncmp (argv[i], "--version", 9))
        {
          puts ("YAM version " VERSION);
          exit (0);
        }
      else if (!strncmp (argv[i], "--status-full", 13))
        {
          const gchar *p = argv[i + 1];

          cmd.status_full = TRUE;
          while (p && *p != '\0' && *p != '-')
            {
              if (!cmd.status_full_folders)
                cmd.status_full_folders = g_ptr_array_new ();
              g_ptr_array_add (cmd.status_full_folders, g_strdup (p));
              i++;
              p = argv[i + 1];
            }
        }
      else if (!strncmp (argv[i], "--status", 8))
        {
          const gchar *p = argv[i + 1];

          cmd.status = TRUE;
          while (p && *p != '\0' && *p != '-')
            {
              if (!cmd.status_folders)
                cmd.status_folders = g_ptr_array_new ();
              g_ptr_array_add (cmd.status_folders, g_strdup (p));
              i++;
              p = argv[i + 1];
            }
        }
      else if (!strncmp (argv[i], "--open", 6))
        {
          const gchar *p = argv[i + 1];

          if (p && *p != '\0' && *p != '-')
            {
              if (cmd.open_msg)
                g_free (cmd.open_msg);
              cmd.open_msg = g_locale_to_utf8 (p, -1, NULL, NULL, NULL);
              i++;
            }
        }
      else if (!strncmp (argv[i], "--configdir", 11))
        {
          const gchar *p = argv[i + 1];

          if (p && *p != '\0' && *p != '-')
            {
              /* this must only be done at startup */
              set_rc_dir (p);
              cmd.configdir = TRUE;
              i++;
            }
        }
      else if (!strncmp (argv[i], "--instance-id", 13))
        {
          if (argv[i + 1])
            {
              instance_id = g_locale_to_utf8 (argv[i + 1], -1, NULL, NULL, NULL);
              i++;
            }
        }
      else if (!strncmp (argv[i], "--safe-mode", 11))
        cmd.safe_mode = TRUE;
      else if (!strncmp (argv[i], "--exit", 6))
        cmd.exit = TRUE;
      else if (!strncmp (argv[i], "--help", 6))
        {
          g_print (_("Usage: %s [OPTIONS ...] [URL]\n"), g_path_get_basename (argv[0]));

          g_print ("%s\n", _("  --compose [mailto URL] open composition window"));
          g_print ("%s\n", _("  --attach file1 [file2]...\n"
                             "                         open composition window with specified files\n"
                             "                         attached"));
          g_print ("%s\n", _("  --receive              receive new messages"));
          g_print ("%s\n", _("  --receive-all          receive new messages of all accounts"));
          g_print ("%s\n", _("  --send                 send all queued messages"));
          g_print ("%s\n", _("  --status [folder]...   show the total number of messages"));
          g_print ("%s\n", _("  --status-full [folder]...\n"
                             "                         show the status of each folder"));
          g_print ("%s\n", _("  --open folderid/msgnum open existing message in a new window"));
          g_print ("%s\n", _("  --open <file URL>      open an rfc822 message file in a new window"));
          g_print ("%s\n", _("  --configdir dirname    specify directory which stores configuration files"));
          g_print ("%s\n", _("  --exit                 exit YAM"));
          g_print ("%s\n", _("  --debug                debug mode"));
          g_print ("%s\n", _("  --safe-mode            safe mode"));
          g_print ("%s\n", _("  --help                 display this help and exit"));
          g_print ("%s\n", _("  --version              output version information and exit"));

          exit (1);
        }
      else
        {
          /* file or URL */
          const gchar *p = argv[i];

          if (p && *p != '\0')
            {
              if (!strncmp (p, "mailto:", 7))
                {
                  cmd.compose = TRUE;
                  cmd.compose_mailto = p + 7;
                }
              else
                {
                  if (cmd.open_msg)
                    g_free (cmd.open_msg);
                  cmd.open_msg = g_locale_to_utf8 (p, -1, NULL, NULL, NULL);
                }
            }
        }
    }

  if (cmd.attach_files && cmd.compose == FALSE)
    {
      cmd.compose = TRUE;
      cmd.compose_mailto = NULL;
    }

  cmd.argv0 = g_locale_to_utf8 (argv[0], -1, NULL, NULL, NULL);
  if (!cmd.argv0)
    cmd.argv0 = g_strdup (argv[0]);
}

static gint
get_queued_message_num (void)
{
  FolderItem *queue;

  queue = folder_get_default_queue ();
  if (!queue)
    return -1;

  folder_item_scan (queue);
  return queue->total;
}

static void
thread_enter_func (void)
{
  g_rec_mutex_lock (&yam_mutex);
}

static void
thread_leave_func (void)
{
  g_rec_mutex_unlock (&yam_mutex);
}

static void
event_loop_iteration_func (void)
{
  if (g_thread_self () != main_thread)
    {
      g_fprintf (stderr, "event_loop_iteration_func called from non-main thread (%p)\n", g_thread_self ());
      g_usleep (10000);
      return;
    }
  gtk_main_iteration ();
}

static void
app_init (void)
{
  gdk_threads_set_lock_functions (thread_enter_func, thread_leave_func);
  gdk_threads_init ();
  main_thread = g_thread_self ();

  yam_init ();

  set_event_loop_func (event_loop_iteration_func);
  prog_version = VERSION;
}

static void
parse_gtkrc_files (void)
{
  gchar *userrc;

  userrc = g_strconcat (get_rc_dir (), G_DIR_SEPARATOR_S, MENU_RC, NULL);
  gtk_accel_map_load (userrc);
  g_free (userrc);
}

static void
setup_rc_dir (void)
{
  yam_setup_rc_dir ();
}

static void
app_restart (void)
{
  gchar *cmdline;
  GError *error = NULL;
  if (cmd.configdir)
    cmdline = g_strdup_printf ("\"%s\"%s --configdir \"%s\"", cmd.argv0, get_debug_mode () ? " --debug" : "", get_rc_dir ());
  else
    cmdline = g_strdup_printf ("\"%s\"%s", cmd.argv0, get_debug_mode () ? " --debug" : "");

  if (!g_spawn_command_line_async (cmdline, &error))
    {
      alertpanel_error ("restart failed\n'%s'\n%s", cmdline, error->message);
      g_error_free (error);
    }
  g_free (cmdline);
}

void
app_will_restart (gboolean force)
{
  cmd.restart = TRUE;
  app_will_exit (force);
  /* canceled */
  cmd.restart = FALSE;
}

void
app_will_exit (gboolean force)
{
  MainWindow *mainwin;
  gchar *filename;
  static gboolean on_exit = FALSE;
  GList *cur;

  if (on_exit)
    return;
  on_exit = TRUE;

  mainwin = main_window_get ();

  if (!force && compose_get_compose_list ())
    {
      if (alertpanel (_("Notice"), _("Composing message exists. Really quit?"),
                      "yam-ok", "yam-cancel", NULL) != G_ALERTDEFAULT)
        {
          on_exit = FALSE;
          return;
        }
      manage_window_focus_in (mainwin->window, NULL, NULL);
    }

  if (!force && prefs_common.warn_queued_on_exit && get_queued_message_num () > 0)
    {
      if (alertpanel (_("Queued messages"), _("Some unsent messages are queued. Exit now?"),
                      "yam-ok", "yam-cancel", NULL) != G_ALERTDEFAULT)
        {
          on_exit = FALSE;
          return;
        }
      manage_window_focus_in (mainwin->window, NULL, NULL);
    }

  if (force)
    g_signal_emit_by_name (yam_app_get (), "app-force-exit");
  g_signal_emit_by_name (yam_app_get (), "app-exit");

  inc_autocheck_timer_remove ();

  if (prefs_common.clean_on_exit)
    main_window_empty_trash (mainwin, !force && prefs_common.ask_on_clean);

  for (cur = account_get_list (); cur != NULL; cur = cur->next)
    {
      PrefsAccount *ac = (PrefsAccount *) cur->data;
      if (ac->protocol == A_IMAP4 && ac->imap_clear_cache_on_exit && ac->folder)
        procmsg_remove_all_cached_messages (FOLDER (ac->folder));
    }

  yam_plugin_unload_all ();

  trayicon_destroy (mainwin->tray_icon);

  /* save all state before exiting */
  summary_write_cache (mainwin->summaryview);
  main_window_get_size (mainwin);
  main_window_get_position (mainwin);
  yam_save_all_state ();
  addressbook_export_to_file ();

  filename = g_strconcat (get_rc_dir (), G_DIR_SEPARATOR_S, MENU_RC, NULL);
  gtk_accel_map_save (filename);
  g_free (filename);

  /* remove temporary files, close log file, socket cleanup */
#if USE_SSL
  ssl_done ();
#endif
  yam_cleanup ();
  lock_socket_remove ();

  if (gtk_main_level () > 0)
    gtk_main_quit ();

  if (cmd.restart)
    app_restart ();

  exit (0);
}

static void
check_gpg (void)
{
#if USE_GPGME
  const gchar *version;
  gpgme_error_t err = 0;

  version = gpgme_check_version ("1.0.0");
  if (version)
    {
      debug_print ("GPGME Version: %s\n", version);
      err = gpgme_engine_check_version (GPGME_PROTOCOL_OpenPGP);
      if (err)
        debug_print ("gpgme_engine_check_version: %s\n", gpgme_strerror (err));
    }

  if (version && !err)
    {
      /* Also does some gpgme init */
      gpgme_engine_info_t engineInfo;

      gpgme_set_locale (NULL, LC_CTYPE, setlocale (LC_CTYPE, NULL));
      gpgme_set_locale (NULL, LC_MESSAGES, setlocale (LC_MESSAGES, NULL));

      if (!gpgme_get_engine_info (&engineInfo))
        {
          while (engineInfo)
            {
              debug_print ("GPGME Protocol: %s\n      Version: %s\n",
                           gpgme_get_protocol_name
                           (engineInfo->protocol), engineInfo->version ? engineInfo->version : "(unknown)");
              engineInfo = engineInfo->next;
            }
        }

      procmsg_set_decrypt_message_func (rfc2015_open_message_decrypted);
      procmsg_set_auto_decrypt_message (TRUE);
    }
  else
    {
      rfc2015_disable_all ();

      if (prefs_common.gpg_warning)
        {
          AlertValue val;

          val = alertpanel_message_with_disable
            (_("Warning"),
             _("GnuPG is not installed properly, or its version is too old.\n"
               "OpenPGP support disabled."), ALERT_WARNING);
          if (val & G_ALERTDISABLE)
            prefs_common.gpg_warning = FALSE;
        }
    }
  /* FIXME: This function went away.  We can either block until gpgme
   * operations finish (currently implemented) or register callbacks
   * with the gtk main loop via the gpgme io callback interface instead.
   *
   * gpgme_register_idle(idle_function_for_gpgme);
   */
#endif
}

static void
default_log_func (const gchar * log_domain, GLogLevelFlags log_level, const gchar * message, gpointer user_data)
{
  gchar *prefix = "";
  gchar *file_prefix = "";
  LogType level = LOG_NORMAL;
  gchar *str;
  const gchar *message_;

  switch (log_level)
    {
    case G_LOG_LEVEL_ERROR:
      prefix = "ERROR";
      file_prefix = "*** ";
      level = LOG_ERROR;
      break;
    case G_LOG_LEVEL_CRITICAL:
      prefix = "CRITICAL";
      file_prefix = "** ";
      level = LOG_WARN;
      break;
    case G_LOG_LEVEL_WARNING:
      prefix = "WARNING";
      file_prefix = "** ";
      level = LOG_WARN;
      break;
    case G_LOG_LEVEL_MESSAGE:
      prefix = "Message";
      file_prefix = "* ";
      level = LOG_MSG;
      break;
    case G_LOG_LEVEL_INFO:
      prefix = "INFO";
      file_prefix = "* ";
      level = LOG_MSG;
      break;
    case G_LOG_LEVEL_DEBUG:
      prefix = "DEBUG";
      break;
    default:
      prefix = "LOG";
      break;
    }

  if (!message)
    message_ = "(NULL) message";
  else
    message_ = message;
  if (log_domain)
    str = g_strconcat (log_domain, "-", prefix, ": ", message_, "\n", NULL);
  else
    str = g_strconcat (prefix, ": ", message_, "\n", NULL);
  log_window_append (str, level);
  log_write (str, file_prefix);
  g_free (str);

  g_log_default_handler (log_domain, log_level, message, user_data);
}

static void
set_log_handlers (gboolean enable)
{
  if (enable)
    g_log_set_default_handler (default_log_func, NULL);
  else
    g_log_set_default_handler (g_log_default_handler, NULL);
}

static void
sig_handler (gint signum)
{
  debug_print ("signal %d received\n", signum);

  switch (signum)
    {
    case SIGHUP:
    case SIGINT:
    case SIGTERM:
    case SIGQUIT:
      app_will_exit (TRUE);
      break;
    default:
      break;
    }
}

static void
register_system_events (void)
{
  struct sigaction sa;

  memset (&sa, 0, sizeof (sa));
  sa.sa_handler = sig_handler;
  sa.sa_flags = SA_RESTART;

  sigemptyset (&sa.sa_mask);
  sigaddset (&sa.sa_mask, SIGHUP);
  sigaddset (&sa.sa_mask, SIGINT);
  sigaddset (&sa.sa_mask, SIGTERM);
  sigaddset (&sa.sa_mask, SIGQUIT);
  sigaddset (&sa.sa_mask, SIGPIPE);

  sigaction (SIGHUP, &sa, NULL);
  sigaction (SIGINT, &sa, NULL);
  sigaction (SIGTERM, &sa, NULL);
  sigaction (SIGQUIT, &sa, NULL);
  sigaction (SIGPIPE, &sa, NULL);
}

#define ADD_SYM(sym)	yam_plugin_add_symbol(#sym, sym)

static void
plugin_init (void)
{
  MainWindow *mainwin;
  gchar *path;

  mainwin = main_window_get ();

  STATUSBAR_PUSH (mainwin, _("Loading plug-ins..."));

  if (yam_plugin_init_lib () != 0)
    {
      STATUSBAR_POP (mainwin);
      return;
    }

  if (cmd.safe_mode)
    {
      debug_print ("plugin_init: safe mode enabled, skipping plug-in loading.\n");
      STATUSBAR_POP (mainwin);
      return;
    }

  ADD_SYM (prog_version);
  ADD_SYM (app_will_exit);

  ADD_SYM (main_window_lock);
  ADD_SYM (main_window_unlock);
  ADD_SYM (main_window_get);
  ADD_SYM (main_window_popup);

  yam_plugin_add_symbol ("main_window_menu_factory", mainwin->menu_factory);
  yam_plugin_add_symbol ("main_window_toolbar", mainwin->toolbar);
  yam_plugin_add_symbol ("main_window_statusbar", mainwin->statusbar);

  ADD_SYM (folderview_get);
  ADD_SYM (folderview_add_sub_widget);
  ADD_SYM (folderview_select);
  ADD_SYM (folderview_unselect);
  ADD_SYM (folderview_select_next_unread);
  ADD_SYM (folderview_get_selected_item);
  ADD_SYM (folderview_check_new);
  ADD_SYM (folderview_check_new_item);
  ADD_SYM (folderview_check_new_all);
  ADD_SYM (folderview_update_item);
  ADD_SYM (folderview_update_item_foreach);
  ADD_SYM (folderview_update_all_updated);
  ADD_SYM (folderview_check_new_selected);

  yam_plugin_add_symbol ("folderview_mail_popup_factory", mainwin->folderview->mail_factory);
  yam_plugin_add_symbol ("folderview_imap_popup_factory", mainwin->folderview->imap_factory);
  yam_plugin_add_symbol ("folderview_news_popup_factory", mainwin->folderview->news_factory);

  yam_plugin_add_symbol ("summaryview", mainwin->summaryview);
  yam_plugin_add_symbol ("summaryview_popup_factory", mainwin->summaryview->popupfactory);

  ADD_SYM (summary_select_by_msgnum);
  ADD_SYM (summary_select_by_msginfo);
  ADD_SYM (summary_lock);
  ADD_SYM (summary_unlock);
  ADD_SYM (summary_is_locked);
  ADD_SYM (summary_is_read_locked);
  ADD_SYM (summary_write_lock);
  ADD_SYM (summary_write_unlock);
  ADD_SYM (summary_is_write_locked);
  ADD_SYM (summary_get_current_folder);
  ADD_SYM (summary_get_selection_type);
  ADD_SYM (summary_get_selected_msg_list);
  ADD_SYM (summary_get_msg_list);
  ADD_SYM (summary_show_queued_msgs);
  ADD_SYM (summary_redisplay_msg);
  ADD_SYM (summary_open_msg);
  ADD_SYM (summary_view_source);
  ADD_SYM (summary_reedit);
  ADD_SYM (summary_update_selected_rows);
  ADD_SYM (summary_update_by_msgnum);

  ADD_SYM (messageview_create_with_new_window);
  ADD_SYM (messageview_show);

  ADD_SYM (compose_new);
  ADD_SYM (compose_reply);
  ADD_SYM (compose_forward);
  ADD_SYM (compose_redirect);
  ADD_SYM (compose_reedit);
  ADD_SYM (compose_entry_set);
  ADD_SYM (compose_entry_append);
  ADD_SYM (compose_entry_get_text);
  ADD_SYM (compose_lock);
  ADD_SYM (compose_unlock);
  ADD_SYM (compose_get_toolbar);
  ADD_SYM (compose_get_misc_hbox);
  ADD_SYM (compose_get_textview);
  ADD_SYM (compose_attach_append);
  ADD_SYM (compose_attach_remove_all);
  ADD_SYM (compose_get_attach_list);
  ADD_SYM (compose_send);

  ADD_SYM (foldersel_folder_sel);
  ADD_SYM (foldersel_folder_sel_full);

  ADD_SYM (input_dialog);
  ADD_SYM (input_dialog_with_invisible);

  ADD_SYM (manage_window_set_transient);
  ADD_SYM (manage_window_signals_connect);
  ADD_SYM (manage_window_get_focus_window);

  ADD_SYM (inc_mail);
  ADD_SYM (inc_is_active);
  ADD_SYM (inc_lock);
  ADD_SYM (inc_unlock);

  ADD_SYM (alertpanel_full);
  ADD_SYM (alertpanel);
  ADD_SYM (alertpanel_message);
  ADD_SYM (alertpanel_message_with_disable);

  ADD_SYM (send_message);
  ADD_SYM (send_message_queue_all);
  ADD_SYM (send_message_set_reply_flag);
  ADD_SYM (send_message_set_forward_flags);

  ADD_SYM (notification_window_open);
  ADD_SYM (notification_window_set_message);
  ADD_SYM (notification_window_close);

  yam_plugin_signal_connect ("plugin-load", G_CALLBACK (load_cb), NULL);

  /* loading plug-ins from user plug-in directory */
  path = g_strconcat (get_rc_dir (), G_DIR_SEPARATOR_S, PLUGIN_DIR, NULL);
  yam_plugin_load_all (path);
  g_free (path);

  /* loading plug-ins from system plug-in directory */
  yam_plugin_load_all (PLUGINDIR);

  STATUSBAR_POP (mainwin);
}

static gchar *
get_socket_name (void)
{
  static gchar *filename = NULL;

  if (filename == NULL)
    {
      filename = g_strdup_printf ("%s%c%s-%d",
                                  g_get_tmp_dir (), G_DIR_SEPARATOR, instance_id ? instance_id : "yam",
                                  getuid ());
    }

  return filename;
}

static gint
prohibit_duplicate_launch (void)
{
  gint sock;
  gchar *path;

  path = get_socket_name ();
  debug_print ("prohibit_duplicate_launch: checking socket: %s\n", path);
  sock = fd_connect_unix (path);
  if (sock < 0)
    {
      debug_print ("prohibit_duplicate_launch: creating socket: %s\n", path);
      g_unlink (path);
      return fd_open_unix (path);
    }

  /* remote command mode */

  debug_print ("another YAM is already running.\n");

  if (cmd.receive_all)
    fd_write_all (sock, "receive_all\n", 12);
  else if (cmd.receive)
    fd_write_all (sock, "receive\n", 8);
  else if (cmd.compose && cmd.attach_files)
    {
      gchar *str, *compose_str;
      gint i;

      if (cmd.compose_mailto)
        compose_str = g_strdup_printf ("compose_attach %s\n", cmd.compose_mailto);
      else
        compose_str = g_strdup ("compose_attach\n");

      fd_write_all (sock, compose_str, strlen (compose_str));
      g_free (compose_str);

      for (i = 0; i < cmd.attach_files->len; i++)
        {
          str = g_ptr_array_index (cmd.attach_files, i);
          fd_write_all (sock, str, strlen (str));
          fd_write_all (sock, "\n", 1);
        }

      fd_write_all (sock, ".\n", 2);
    }
  else if (cmd.compose)
    {
      gchar *compose_str;

      if (cmd.compose_mailto)
        compose_str = g_strdup_printf ("compose %s\n", cmd.compose_mailto);
      else
        compose_str = g_strdup ("compose\n");

      fd_write_all (sock, compose_str, strlen (compose_str));
      g_free (compose_str);
    }
  else if (cmd.send)
    fd_write_all (sock, "send\n", 5);
  else if (cmd.status || cmd.status_full)
    {
      gchar buf[BUFFSIZE];
      gint i;
      const gchar *command;
      GPtrArray *folders;
      gchar *folder;

      command = cmd.status_full ? "status-full\n" : "status\n";
      folders = cmd.status_full ? cmd.status_full_folders : cmd.status_folders;

      fd_write_all (sock, command, strlen (command));
      for (i = 0; folders && i < folders->len; ++i)
        {
          folder = g_ptr_array_index (folders, i);
          fd_write_all (sock, folder, strlen (folder));
          fd_write_all (sock, "\n", 1);
        }
      fd_write_all (sock, ".\n", 2);
      for (;;)
        {
          if (fd_gets (sock, buf, sizeof (buf)) <= 0)
            break;
          if (!strncmp (buf, ".\n", 2))
            break;
          fputs (buf, stdout);
        }
    }
  else if (cmd.open_msg)
    {
      gchar *str;

      str = g_strdup_printf ("open %s\n", cmd.open_msg);
      fd_write_all (sock, str, strlen (str));
      g_free (str);
    }
  else if (cmd.exit)
    fd_write_all (sock, "exit\n", 5);
  else
    fd_write_all (sock, "popup\n", 6);

  fd_close (sock);
  return -1;
}

static gint
lock_socket_remove (void)
{
  gchar *filename;

  if (lock_socket < 0)
    return -1;

  if (lock_socket_tag > 0)
    g_source_remove (lock_socket_tag);
  if (lock_ch)
    {
      g_io_channel_shutdown (lock_ch, FALSE, NULL);
      g_io_channel_unref (lock_ch);
      lock_ch = NULL;
    }

  filename = get_socket_name ();
  debug_print ("lock_socket_remove: removing socket: %s\n", filename);
  g_unlink (filename);

  return 0;
}

static GPtrArray *
get_folder_item_list (gint sock)
{
  gchar buf[BUFFSIZE];
  FolderItem *item;
  GPtrArray *folders = NULL;

  for (;;)
    {
      if (fd_gets (sock, buf, sizeof (buf)) <= 0)
        break;
      if (!strncmp (buf, ".\n", 2))
        break;
      strretchomp (buf);
      if (!folders)
        folders = g_ptr_array_new ();
      item = folder_find_item_from_identifier (buf);
      if (item)
        g_ptr_array_add (folders, item);
      else
        g_warning ("no such folder: %s\n", buf);
    }

  return folders;
}

static gboolean
lock_socket_input_cb (GIOChannel * source, GIOCondition condition, gpointer data)
{
  MainWindow *mainwin = (MainWindow *) data;
  gint fd, sock;
  gchar buf[BUFFSIZE];

  gdk_threads_enter ();

  fd = g_io_channel_unix_get_fd (source);
  sock = fd_accept (fd);
  if (fd_gets (sock, buf, sizeof (buf)) <= 0)
    {
      fd_close (sock);
      gdk_threads_leave ();
      return TRUE;
    }

  if (!strncmp (buf, "popup", 5))
    main_window_popup (mainwin);
  else if (!strncmp (buf, "receive_all", 11))
    {
      main_window_popup (mainwin);
      if (!yam_window_modal_exist ())
        inc_all_account_mail (mainwin, FALSE);
    }
  else if (!strncmp (buf, "receive", 7))
    {
      main_window_popup (mainwin);
      if (!yam_window_modal_exist ())
        inc_mail (mainwin);
    }
  else if (!strncmp (buf, "compose_attach", 14))
    {
      GPtrArray *files;
      gchar *mailto;

      mailto = g_strdup (buf + strlen ("compose_attach") + 1);
      files = g_ptr_array_new ();
      while (fd_gets (sock, buf, sizeof (buf)) > 0)
        {
          if (buf[0] == '.' && buf[1] == '\n')
            break;
          strretchomp (buf);
          g_ptr_array_add (files, g_strdup (buf));
        }
      open_compose_new (mailto, files);
      ptr_array_free_strings (files);
      g_ptr_array_free (files, TRUE);
      g_free (mailto);
    }
  else if (!strncmp (buf, "compose", 7))
    open_compose_new (buf + strlen ("compose") + 1, NULL);
  else if (!strncmp (buf, "send", 4))
    send_queue ();
  else if (!strncmp (buf, "status-full", 11) || !strncmp (buf, "status", 6))
    {
      gchar *status;
      GPtrArray *folders;

      folders = get_folder_item_list (sock);
      status = folder_get_status (folders, !strncmp (buf, "status-full", 11));
      fd_write_all (sock, status, strlen (status));
      fd_write_all (sock, ".\n", 2);
      g_free (status);
      if (folders)
        g_ptr_array_free (folders, TRUE);
    }
  else if (!strncmp (buf, "open", 4))
    {
      strretchomp (buf);
      if (strlen (buf) < 6 || buf[4] != ' ')
        {
          fd_close (sock);
          gdk_threads_leave ();
          return TRUE;
        }
      open_message (buf + 5);
    }
  else if (!strncmp (buf, "exit", 4))
    {
      fd_close (sock);
      app_will_exit (TRUE);
    }

  fd_close (sock);

  gdk_threads_leave ();

  return TRUE;
}

static void
remote_command_exec (void)
{
  MainWindow *mainwin;

  mainwin = main_window_get ();

  if (prefs_common.open_inbox_on_startup)
    {
      FolderItem *item;
      PrefsAccount *ac;

      ac = account_get_default ();
      if (!ac)
        ac = cur_account;
      item = ac && ac->inbox ? folder_find_item_from_identifier (ac->inbox) : folder_get_default_inbox ();
      folderview_select (mainwin->folderview, item);
    }

  if (!yam_window_modal_exist ())
    {
      if (cmd.compose)
        open_compose_new (cmd.compose_mailto, cmd.attach_files);

      if (cmd.open_msg)
        open_message (cmd.open_msg);

      if (cmd.receive_all)
        inc_all_account_mail (mainwin, FALSE);
      else if (prefs_common.chk_on_startup)
        inc_all_account_mail (mainwin, TRUE);
      else if (cmd.receive)
        inc_mail (mainwin);

      if (cmd.send)
        send_queue ();
    }

  if (cmd.attach_files)
    {
      ptr_array_free_strings (cmd.attach_files);
      g_ptr_array_free (cmd.attach_files, TRUE);
      cmd.attach_files = NULL;
    }
  if (cmd.status_folders)
    {
      g_ptr_array_free (cmd.status_folders, TRUE);
      cmd.status_folders = NULL;
    }
  if (cmd.status_full_folders)
    {
      g_ptr_array_free (cmd.status_full_folders, TRUE);
      cmd.status_full_folders = NULL;
    }
  if (cmd.open_msg)
    {
      g_free (cmd.open_msg);
      cmd.open_msg = NULL;
    }
  if (cmd.exit)
    app_will_exit (TRUE);
}

static void
open_compose_new (const gchar * address, GPtrArray * attach_files)
{
  gchar *utf8addr = NULL;

  if (yam_window_modal_exist ())
    return;

  if (address)
    {
      utf8addr = g_locale_to_utf8 (address, -1, NULL, NULL, NULL);
      if (utf8addr)
        g_strstrip (utf8addr);
      debug_print ("open compose: %s\n", utf8addr ? utf8addr : "");
    }

  compose_new (NULL, NULL, utf8addr, attach_files);

  g_free (utf8addr);
}

static void
open_message_file (const gchar * file)
{
  MsgInfo *msginfo;
  MsgFlags flags = { 0 };
  MessageView *msgview;

  g_return_if_fail (file != NULL);

  debug_print ("open message file: %s\n", file);

  if (!is_file_exist (file) || get_file_size (file) <= 0)
    {
      debug_print ("file not found: %s\n", file);
      return;
    }

  msginfo = procheader_parse_file (file, flags, FALSE);
  if (msginfo)
    {
      msginfo->file_path = g_strdup (file);
      msgview = messageview_create_with_new_window ();
      messageview_show (msgview, msginfo, FALSE);
      procmsg_msginfo_free (msginfo);
    }
  else
    debug_print ("cannot open message: %s\n", file);
}

static void
open_message (const gchar * path)
{
  gchar *fid;
  gchar *msg;
  gint num;
  FolderItem *item;
  MsgInfo *msginfo;
  MessageView *msgview;
  gchar *file;

  g_return_if_fail (path != NULL);

  if (yam_window_modal_exist ())
    return;

  debug_print ("open message: %s\n", path);

  if (!strncmp (path, "file:", 5))
    {
      file = g_filename_from_uri (path, NULL, NULL);
      open_message_file (file);
      g_free (file);
      return;
    }
  else if (g_path_is_absolute (path))
    {
      open_message_file (path);
      return;
    }

  /* relative path, or folder identifier */

  fid = g_path_get_dirname (path);
  msg = g_path_get_basename (path);
  num = to_number (msg);
  item = folder_find_item_from_identifier (fid);

  if (num > 0 && item)
    {
      debug_print ("open folder id: %s (msg %d)\n", fid, num);
      msginfo = folder_item_get_msginfo (item, num);
      if (msginfo)
        {
          msgview = messageview_create_with_new_window ();
          messageview_show (msgview, msginfo, FALSE);
          procmsg_msginfo_free (msginfo);
          g_free (msg);
          g_free (fid);
          return;
        }
      else
        debug_print ("message %d not found\n", num);
    }

  g_free (msg);
  g_free (fid);

  /* relative path */

  file = g_strconcat (get_startup_dir (), G_DIR_SEPARATOR_S, path, NULL);
  open_message_file (file);
  g_free (file);
}

static void
send_queue (void)
{
  GList *list;

  if (yam_window_modal_exist ())
    return;
  if (!main_window_toggle_online_if_offline (main_window_get ()))
    return;

  for (list = folder_get_list (); list != NULL; list = list->next)
    {
      Folder *folder = list->data;

      if (folder->queue)
        {
          gint ret;

          ret = send_message_queue_all (folder->queue, prefs_common.savemsg, prefs_common.filter_sent);
          statusbar_pop_all ();
          if (ret > 0)
            folder_item_scan (folder->queue);
        }
    }

  folderview_update_all_updated (TRUE);
  main_window_set_menu_sensitive (main_window_get ());
  main_window_set_toolbar_sensitive (main_window_get ());
}
