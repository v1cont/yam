/*
 * YAM -- a GTK+ based, lightweight, and fast e-mail client
 * Copyright (C) 2020 Victor Ananjevsky <victor@sanana.kiev.ua>
 * Copyright (C) 1999-2014 Hiroyuki Yamamoto
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

#include <glib.h>
#include <glib/gi18n.h>
#include <gdk/gdkkeysyms.h>
#include <gtk/gtk.h>

#include "logwindow.h"
#include "prefs_common.h"
#include "utils.h"
#include "gtkutils.h"
#include "codeconv.h"

#define LOG_MSG_COLOR  "#008000"
#define LOG_WARN_COLOR  "#808000"
#define LOG_ERR_COLOR  "#800000"

#define TRIM_LINES	25

static LogWindow *logwindow;

static GThread *main_thread;

static void log_window_print_func (const gchar * str);
static void log_window_message_func (const gchar * str);
static void log_window_warning_func (const gchar * str);
static void log_window_error_func (const gchar * str);

static gboolean key_pressed (GtkWidget * widget, GdkEventKey * event, LogWindow * logwin);

LogWindow *
log_window_create (void)
{
  LogWindow *logwin;
  GtkWidget *window;
  GtkWidget *scrolledwin;
  GtkWidget *text;
  GtkTextBuffer *buffer;
  GtkTextIter iter;

  debug_print ("Creating log window...\n");
  logwin = g_new0 (LogWindow, 1);

  window = gtk_window_new (GTK_WINDOW_TOPLEVEL);
  gtk_window_set_title (GTK_WINDOW (window), _("Protocol log"));
  gtk_window_set_default_size (GTK_WINDOW (window), 520, 400);
  g_signal_connect (G_OBJECT (window), "delete_event", G_CALLBACK (gtk_widget_hide_on_delete), NULL);
  g_signal_connect (G_OBJECT (window), "key_press_event", G_CALLBACK (key_pressed), logwin);
  gtk_widget_realize (window);

  scrolledwin = gtk_scrolled_window_new (NULL, NULL);
  gtk_scrolled_window_set_policy (GTK_SCROLLED_WINDOW (scrolledwin), GTK_POLICY_NEVER, GTK_POLICY_ALWAYS);
  gtk_scrolled_window_set_shadow_type (GTK_SCROLLED_WINDOW (scrolledwin), GTK_SHADOW_IN);
  gtk_container_add (GTK_CONTAINER (window), scrolledwin);
  gtk_widget_show (scrolledwin);

  text = gtk_text_view_new ();
  gtk_text_view_set_editable (GTK_TEXT_VIEW (text), FALSE);
  gtk_text_view_set_wrap_mode (GTK_TEXT_VIEW (text), GTK_WRAP_WORD);
  buffer = gtk_text_view_get_buffer (GTK_TEXT_VIEW (text));
  gtk_text_buffer_get_start_iter (buffer, &iter);
  gtk_text_buffer_create_mark (buffer, "end", &iter, FALSE);
  gtk_container_add (GTK_CONTAINER (scrolledwin), text);
  gtk_widget_show (text);

  logwin->window = window;
  logwin->scrolledwin = scrolledwin;
  logwin->text = text;
  logwin->lines = 1;

  logwin->aqueue = g_async_queue_new ();

  main_thread = g_thread_self ();
  debug_print ("main_thread = %p\n", main_thread);

  logwindow = logwin;

  return logwin;
}

void
log_window_init (LogWindow * logwin)
{
  GtkTextBuffer *buffer;

  gdk_rgba_parse (&logwin->msg_color, LOG_MSG_COLOR);
  gdk_rgba_parse (&logwin->warn_color, LOG_WARN_COLOR);
  gdk_rgba_parse (&logwin->error_color, LOG_ERR_COLOR);

  buffer = gtk_text_view_get_buffer (GTK_TEXT_VIEW (logwin->text));
  gtk_text_buffer_create_tag (buffer, "message", "foreground-rgba", &logwindow->msg_color, NULL);
  gtk_text_buffer_create_tag (buffer, "warn", "foreground-rgba", &logwindow->warn_color, NULL);
  gtk_text_buffer_create_tag (buffer, "error", "foreground-rgba", &logwindow->error_color, NULL);

  set_log_ui_func_full (log_window_print_func, log_window_message_func,
                        log_window_warning_func, log_window_error_func, log_window_flush);
}

void
log_window_show (LogWindow * logwin)
{
  GtkTextView *text = GTK_TEXT_VIEW (logwin->text);
  GtkTextBuffer *buffer;
  GtkTextMark *mark;

  buffer = gtk_text_view_get_buffer (text);
  mark = gtk_text_buffer_get_mark (buffer, "end");
  gtk_text_view_scroll_mark_onscreen (text, mark);

  gtk_window_present (GTK_WINDOW (logwin->window));
}

static void
log_window_append_real (const gchar * str, LogType type)
{
  GtkTextView *text;
  GtkTextBuffer *buffer;
  GtkTextIter iter;
  gchar *head = NULL;
  const gchar *tag;
  gint line_limit = prefs_common.logwin_line_limit;

  g_return_if_fail (logwindow != NULL);

  if (g_thread_self () != main_thread)
    {
      g_fprintf (stderr, "log_window_append_real called from non-main thread (%p)\n", g_thread_self ());
      return;
    }

  gdk_threads_enter ();

  text = GTK_TEXT_VIEW (logwindow->text);
  buffer = gtk_text_view_get_buffer (text);

  if (line_limit > 0 && logwindow->lines >= line_limit)
    {
      GtkTextIter start, end;

      gtk_text_buffer_get_start_iter (buffer, &start);
      end = start;
      gtk_text_iter_forward_lines (&end, TRIM_LINES);
      gtk_text_buffer_delete (buffer, &start, &end);
      logwindow->lines = gtk_text_buffer_get_line_count (buffer);
    }

  switch (type)
    {
    case LOG_MSG:
      tag = "message";
      head = "* ";
      break;
    case LOG_WARN:
      tag = "warn";
      head = "** ";
      break;
    case LOG_ERROR:
      tag = "error";
      head = "*** ";
      break;
    default:
      tag = NULL;
      break;
    }

  gtk_text_buffer_get_end_iter (buffer, &iter);

  if (head)
    gtk_text_buffer_insert_with_tags_by_name (buffer, &iter, head, -1, tag, NULL);

  if (!g_utf8_validate (str, -1, NULL))
    {
      gchar *str_;

      str_ = conv_utf8todisp (str, NULL);
      if (str_)
        {
          gtk_text_buffer_insert_with_tags_by_name (buffer, &iter, str_, -1, tag, NULL);
          g_free (str_);
        }
    }
  else
    gtk_text_buffer_insert_with_tags_by_name (buffer, &iter, str, -1, tag, NULL);

  if (gtk_widget_get_visible (GTK_WIDGET (text)))
    {
      GtkTextMark *mark;
      mark = gtk_text_buffer_get_mark (buffer, "end");
      gtk_text_view_scroll_mark_onscreen (text, mark);
    }

  logwindow->lines++;

  gdk_threads_leave ();
}

void
log_window_append (const gchar * str, LogType type)
{
  if (g_thread_self () != main_thread)
    {
      log_window_append_queue (str, type);
      return;
    }

  log_window_flush ();
  log_window_append_real (str, type);
}

typedef struct _LogData {
  gchar *str;
  LogType type;
} LogData;

void
log_window_append_queue (const gchar * str, LogType type)
{
  LogData *logdata;

  logdata = g_new (LogData, 1);
  logdata->str = g_strdup (str);
  logdata->type = type;

  g_async_queue_push (logwindow->aqueue, logdata);
}

void
log_window_flush (void)
{
  LogData *logdata;

  if (g_thread_self () != main_thread)
    {
      g_fprintf (stderr, "log_window_flush called from non-main thread (%p)\n", g_thread_self ());
      return;
    }

  while ((logdata = g_async_queue_try_pop (logwindow->aqueue)))
    {
      log_window_append_real (logdata->str, logdata->type);
      g_free (logdata->str);
      g_free (logdata);
    }
}

static void
log_window_print_func (const gchar * str)
{
  log_window_append (str, LOG_NORMAL);
}

static void
log_window_message_func (const gchar * str)
{
  log_window_append (str, LOG_MSG);
}

static void
log_window_warning_func (const gchar * str)
{
  log_window_append (str, LOG_WARN);
}

static void
log_window_error_func (const gchar * str)
{
  log_window_append (str, LOG_ERROR);
}

static gboolean
key_pressed (GtkWidget * widget, GdkEventKey * event, LogWindow * logwin)
{
  if (event && event->keyval == GDK_KEY_Escape)
    gtk_widget_hide (logwin->window);
  return FALSE;
}
