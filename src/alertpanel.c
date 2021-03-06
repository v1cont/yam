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

#include "defs.h"

#include <glib.h>
#include <glib/gi18n.h>
#include <gtk/gtk.h>
#include <gdk/gdkkeysyms.h>

#include "alertpanel.h"
#include "mainwindow.h"
#include "manage_window.h"
#include "utils.h"
#include "gtkutils.h"
#include "inc.h"
#include "prefs_common.h"

#define ALERT_PANEL_WIDTH	380
#define TITLE_HEIGHT		72
#define MESSAGE_HEIGHT		62

static gboolean alertpanel_is_open = FALSE;
static AlertValue value;

static GtkWidget *dialog;

static void alertpanel_show (void);
static void alertpanel_create (const gchar *title, const gchar *message, AlertType type, AlertValue default_value, gboolean can_disable,
                               const gchar *button1_label, const gchar *button2_label, const gchar *button3_label);

static void alertpanel_button_toggled (GtkToggleButton * button, gpointer data);
static void alertpanel_button_clicked (GtkWidget * widget, gpointer data);
static gint alertpanel_deleted (GtkWidget * widget, GdkEventAny * event, gpointer data);
static gboolean alertpanel_close (GtkWidget * widget, GdkEventAny * event, gpointer data);
static gint alertpanel_focus_out (GtkWidget * widget, GdkEventFocus * event, gpointer data);

AlertValue
alertpanel_full (const gchar *title, const gchar *message, AlertType type, AlertValue default_value, gboolean can_disable,
                 const gchar *button1_label, const gchar *button2_label, const gchar *button3_label)
{
  if (alertpanel_is_open)
    return -1;
  else
    alertpanel_is_open = TRUE;

  alertpanel_create (title, message, type, default_value, can_disable, button1_label, button2_label, button3_label);
  alertpanel_show ();

  debug_print ("return value = %d\n", value);
  return value;
}

AlertValue
alertpanel (const gchar *title, const gchar *message,
            const gchar *button1_label, const gchar *button2_label, const gchar *button3_label)
{
  return alertpanel_full (title, message, ALERT_QUESTION, G_ALERTDEFAULT,
                          FALSE, button1_label, button2_label, button3_label);
}

void
alertpanel_message (const gchar * title, const gchar * message, AlertType type)
{
  if (alertpanel_is_open)
    return;
  else
    alertpanel_is_open = TRUE;

  alertpanel_create (title, message, type, G_ALERTDEFAULT, FALSE, NULL, NULL, NULL);
  alertpanel_show ();
}

AlertValue
alertpanel_message_with_disable (const gchar * title, const gchar * message, AlertType type)
{
  if (alertpanel_is_open)
    return 0;
  else
    alertpanel_is_open = TRUE;

  alertpanel_create (title, message, type, G_ALERTDEFAULT, TRUE, NULL, NULL, NULL);
  alertpanel_show ();

  return value;
}

void
alertpanel_notice (const gchar * format, ...)
{
  va_list args;
  gchar buf[256];

  va_start (args, format);
  g_vsnprintf (buf, sizeof (buf), format, args);
  va_end (args);
  strretchomp (buf);

  alertpanel_message (_("Notice"), buf, ALERT_NOTICE);
}

void
alertpanel_warning (const gchar * format, ...)
{
  va_list args;
  gchar buf[256];

  va_start (args, format);
  g_vsnprintf (buf, sizeof (buf), format, args);
  va_end (args);
  strretchomp (buf);

  alertpanel_message (_("Warning"), buf, ALERT_WARNING);
}

void
alertpanel_error (const gchar * format, ...)
{
  va_list args;
  gchar buf[256];

  va_start (args, format);
  g_vsnprintf (buf, sizeof (buf), format, args);
  va_end (args);
  strretchomp (buf);

  alertpanel_message (_("Error"), buf, ALERT_ERROR);
}

static void
alertpanel_show (void)
{
  gint x, y, w, h;
  guint sx, sy;

  value = G_ALERTWAIT;

  inc_lock ();

  yam_screen_get_size (gtk_widget_get_window (dialog), &sx, &sy);
  gdk_window_get_origin (gtk_widget_get_window (dialog), &x, &y);
  w = gtk_widget_get_allocated_width (dialog);
  h = gtk_widget_get_allocated_height (dialog);
  if (x < 0 || y < 0 || x + w > sx || y + h > sy)
    {
      debug_print ("sx, sy,  x, y,  w, h = %d, %d,  %d, %d,  %d, %d\n", sx, sy, x, y, w, h);
      debug_print ("alert dialog position out of range\n");
      gtk_window_set_position (GTK_WINDOW (dialog), GTK_WIN_POS_CENTER_ALWAYS);
    }

  while ((value & G_ALERT_VALUE_MASK) == G_ALERTWAIT)
    gtk_main_iteration ();

  gtk_widget_destroy (dialog);
  GTK_EVENTS_FLUSH ();

  alertpanel_is_open = FALSE;
  inc_unlock ();
}

static void
alertpanel_create (const gchar *title, const gchar *message, AlertType type, AlertValue default_value, gboolean can_disable,
                   const gchar *button1_label, const gchar *button2_label, const gchar *button3_label)
{
  GtkWidget *image;
  GtkWidget *label;
  GtkWidget *hbox;
  GtkWidget *vbox;
  GtkWidget *disable_chkbtn;
  GtkWidget *confirm_area;
  GtkWidget *button1;
  GtkWidget *button2;
  GtkWidget *button3;
  const gchar *label2;
  const gchar *label3;
  gint spacing;

  debug_print ("Creating alert panel dialog...\n");

  dialog = gtk_dialog_new ();
  gtk_window_set_title (GTK_WINDOW (dialog), title);
  gtk_window_set_position (GTK_WINDOW (dialog), GTK_WIN_POS_CENTER_ON_PARENT);
  gtk_window_set_modal (GTK_WINDOW (dialog), TRUE);
  gtk_window_set_resizable (GTK_WINDOW (dialog), FALSE);
  manage_window_set_transient (GTK_WINDOW (dialog));
  gtk_widget_realize (dialog);
  g_signal_connect (G_OBJECT (dialog), "delete_event", G_CALLBACK (alertpanel_deleted), (gpointer) G_ALERTCANCEL);
  g_signal_connect (G_OBJECT (dialog), "key_press_event", G_CALLBACK (alertpanel_close), (gpointer) G_ALERTCANCEL);
  g_signal_connect (G_OBJECT (dialog), "focus_out_event", G_CALLBACK (alertpanel_focus_out), NULL);

  /* for title icon, label and message */
  spacing = 10;
  hbox = gtk_box_new (GTK_ORIENTATION_HORIZONTAL, spacing);
  gtk_container_set_border_width (GTK_CONTAINER (hbox), spacing);
  gtk_box_pack_start (GTK_BOX (gtk_dialog_get_content_area (GTK_DIALOG (dialog))), hbox, FALSE, FALSE, 0);

  /* title icon */
  switch (type)
    {
    case ALERT_QUESTION:
      image = gtk_image_new_from_icon_name ("dialog-question", GTK_ICON_SIZE_DIALOG);
      break;
    case ALERT_WARNING:
      image = gtk_image_new_from_icon_name ("dialog-warning", GTK_ICON_SIZE_DIALOG);
      break;
    case ALERT_ERROR:
      image = gtk_image_new_from_icon_name ("dialog-error", GTK_ICON_SIZE_DIALOG);
      break;
    case ALERT_NOTICE:
    default:
      image = gtk_image_new_from_icon_name ("dialog-information", GTK_ICON_SIZE_DIALOG);
      break;
    }
  gtk_widget_set_valign (image, 0.0);
  gtk_box_pack_start (GTK_BOX (hbox), image, FALSE, FALSE, 0);

  /* for title and message */
  vbox = gtk_box_new (GTK_ORIENTATION_VERTICAL, spacing);
  gtk_box_pack_start (GTK_BOX (hbox), vbox, TRUE, TRUE, 0);

  label = yam_label_title ((gchar *) title);
  gtk_box_pack_start (GTK_BOX (vbox), label, TRUE, TRUE, 0);
  gtk_label_set_xalign (GTK_LABEL (label), 0.0);
  gtk_label_set_yalign (GTK_LABEL (label), 0.0);
  gtk_label_set_line_wrap (GTK_LABEL (label), TRUE);

  /* message label */
  label = gtk_label_new (message);
  gtk_box_pack_start (GTK_BOX (vbox), label, TRUE, TRUE, 0);
  gtk_label_set_xalign (GTK_LABEL (label), 0.0);
  gtk_label_set_yalign (GTK_LABEL (label), 0.0);
  gtk_label_set_line_wrap (GTK_LABEL (label), TRUE);
  gtk_label_set_selectable (GTK_LABEL (label), TRUE);
  gtk_widget_set_can_focus (label, FALSE);

  if (can_disable)
    {
      hbox = gtk_box_new (GTK_ORIENTATION_HORIZONTAL, 0);
      gtk_box_pack_start (GTK_BOX (gtk_dialog_get_content_area (GTK_DIALOG (dialog))), hbox, FALSE, FALSE, 0);

      disable_chkbtn = gtk_check_button_new_with_label (_("Show this message next time"));
      gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (disable_chkbtn), TRUE);
      gtk_box_pack_start (GTK_BOX (hbox), disable_chkbtn, FALSE, FALSE, spacing);
      g_signal_connect (G_OBJECT (disable_chkbtn), "toggled", G_CALLBACK (alertpanel_button_toggled), GUINT_TO_POINTER (G_ALERTDISABLE));
    }

  /* for button(s) */
  if (!button1_label)
    button1_label = "yam-close";
  label2 = button2_label;
  label3 = button3_label;
  if (label2 && *label2 == '+')
    label2++;
  if (label3 && *label3 == '+')
    label3++;

  yam_stock_button_set_create (&confirm_area, &button1, button1_label,
                               button2_label ? &button2 : NULL, label2, button3_label ? &button3 : NULL, label3);

  gtk_box_pack_end (GTK_BOX (gtk_dialog_get_action_area (GTK_DIALOG (dialog))), confirm_area, FALSE, FALSE, 0);
  gtk_container_set_border_width (GTK_CONTAINER (confirm_area), 5);
  gtk_widget_grab_default (button1);
  gtk_widget_grab_focus (button1);
  if (button2_label && (default_value == G_ALERTALTERNATE || *button2_label == '+'))
    {
      gtk_widget_grab_default (button2);
      gtk_widget_grab_focus (button2);
    }
  if (button3_label && (default_value == G_ALERTOTHER || *button3_label == '+'))
    {
      gtk_widget_grab_default (button3);
      gtk_widget_grab_focus (button3);
    }

  g_signal_connect (G_OBJECT (button1), "clicked", G_CALLBACK (alertpanel_button_clicked), GUINT_TO_POINTER (G_ALERTDEFAULT));
  if (button2_label)
    g_signal_connect (G_OBJECT (button2), "clicked", G_CALLBACK (alertpanel_button_clicked), GUINT_TO_POINTER (G_ALERTALTERNATE));
  if (button3_label)
    g_signal_connect (G_OBJECT (button3), "clicked", G_CALLBACK (alertpanel_button_clicked), GUINT_TO_POINTER (G_ALERTOTHER));

  gtk_widget_show_all (dialog);
}

static void
alertpanel_button_toggled (GtkToggleButton * button, gpointer data)
{
  if (gtk_toggle_button_get_active (button))
    value &= ~GPOINTER_TO_UINT (data);
  else
    value |= GPOINTER_TO_UINT (data);
}

static void
alertpanel_button_clicked (GtkWidget * widget, gpointer data)
{
  value = (value & ~G_ALERT_VALUE_MASK) | (AlertValue) data;
}

static gint
alertpanel_deleted (GtkWidget * widget, GdkEventAny * event, gpointer data)
{
  value = (value & ~G_ALERT_VALUE_MASK) | (AlertValue) data;
  return TRUE;
}

static gboolean
alertpanel_close (GtkWidget * widget, GdkEventAny * event, gpointer data)
{
  if (event->type == GDK_KEY_PRESS)
    if (((GdkEventKey *) event)->keyval != GDK_KEY_Escape)
      return FALSE;

  value = (value & ~G_ALERT_VALUE_MASK) | (AlertValue) data;
  return FALSE;
}

static gint
alertpanel_focus_out (GtkWidget * widget, GdkEventFocus * event, gpointer data)
{
  return FALSE;
}
