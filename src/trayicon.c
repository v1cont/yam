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
#include <gtk/gtk.h>

#include "trayicon.h"

#ifdef USE_TRAY

#include "mainwindow.h"
#include "utils.h"
#include "gtkutils.h"
#include "stock_pixmap.h"
#include "menu.h"
#include "main.h"
#include "inc.h"
#include "compose.h"
#include "prefs_common.h"

#define TRAYICON_IMAGE		STOCK_PIXMAP_TRAY
#define TRAYICON_NEW_IMAGE	STOCK_PIXMAP_TRAY_NEWMAIL

#define TRAYICON_NOTIFY_PERIOD	10000

static TrayIcon trayicon;
static GtkWidget *trayicon_menu;
static gboolean on_notify = FALSE;
static gboolean default_tooltip = FALSE;

static void trayicon_activated (GtkStatusIcon * status_icon, gpointer data);
static void trayicon_popup_menu_cb (GtkStatusIcon * status_icon, guint button, guint activate_time, gpointer data);

static void trayicon_present (GtkWidget * widget, gpointer data);
static void trayicon_inc (GtkWidget * widget, gpointer data);
static void trayicon_inc_all (GtkWidget * widget, gpointer data);
static void trayicon_send (GtkWidget * widget, gpointer data);
static void trayicon_compose (GtkWidget * widget, gpointer data);
static void trayicon_app_exit (GtkWidget * widget, gpointer data);

TrayIcon *
trayicon_create (MainWindow * mainwin)
{
  GtkWidget *menuitem;
  GdkPixbuf *pixbuf = NULL;

  stock_pixbuf_gdk (TRAYICON_IMAGE, &pixbuf);
  trayicon.status_icon = gtk_status_icon_new_from_pixbuf (pixbuf);

  g_signal_connect (G_OBJECT (trayicon.status_icon), "activate", G_CALLBACK (trayicon_activated), mainwin);
  g_signal_connect (G_OBJECT (trayicon.status_icon), "popup-menu", G_CALLBACK (trayicon_popup_menu_cb), mainwin);

  on_notify = FALSE;
  default_tooltip = FALSE;
  trayicon_set_tooltip (NULL);

  if (!trayicon_menu)
    {
      trayicon_menu = gtk_menu_new ();
      gtk_widget_show (trayicon_menu);
      MENUITEM_ADD_WITH_MNEMONIC (trayicon_menu, menuitem, _("_Display YAM"), 0);
      g_signal_connect (G_OBJECT (menuitem), "activate", G_CALLBACK (trayicon_present), mainwin);
      MENUITEM_ADD (trayicon_menu, menuitem, NULL, 0);
      MENUITEM_ADD_WITH_MNEMONIC (trayicon_menu, menuitem, _("Get from _current account"), 0);
      g_signal_connect (G_OBJECT (menuitem), "activate", G_CALLBACK (trayicon_inc), mainwin);
      MENUITEM_ADD_WITH_MNEMONIC (trayicon_menu, menuitem, _("Get from _all accounts"), 0);
      g_signal_connect (G_OBJECT (menuitem), "activate", G_CALLBACK (trayicon_inc_all), mainwin);
      MENUITEM_ADD_WITH_MNEMONIC (trayicon_menu, menuitem, _("_Send queued messages"), 0);
      g_signal_connect (G_OBJECT (menuitem), "activate", G_CALLBACK (trayicon_send), mainwin);

      MENUITEM_ADD (trayicon_menu, menuitem, NULL, 0);
      MENUITEM_ADD_WITH_MNEMONIC (trayicon_menu, menuitem, _("Compose _new message"), 0);
      g_signal_connect (G_OBJECT (menuitem), "activate", G_CALLBACK (trayicon_compose), mainwin);

      MENUITEM_ADD (trayicon_menu, menuitem, NULL, 0);
      MENUITEM_ADD_WITH_MNEMONIC (trayicon_menu, menuitem, _("E_xit"), 0);
      g_signal_connect (G_OBJECT (menuitem), "activate", G_CALLBACK (trayicon_app_exit), mainwin);
    }

  return &trayicon;
}

void
trayicon_show (TrayIcon * tray_icon)
{
  gtk_status_icon_set_visible (tray_icon->status_icon, TRUE);
};

void
trayicon_hide (TrayIcon * tray_icon)
{
  gtk_status_icon_set_visible (tray_icon->status_icon, FALSE);
}

void
trayicon_destroy (TrayIcon * tray_icon)
{
  g_object_unref (tray_icon->status_icon);
  tray_icon->status_icon = NULL;
}

void
trayicon_set_tooltip (const gchar * text)
{
  if (text)
    {
      default_tooltip = FALSE;
      gtk_status_icon_set_tooltip_text (trayicon.status_icon, text);
    }
  else if (!default_tooltip)
    {
      default_tooltip = TRUE;
      gtk_status_icon_set_tooltip_text (trayicon.status_icon, "YAM");
    }
}

static guint notify_tag = 0;

gboolean
notify_timeout_cb (gpointer data)
{
  gdk_threads_enter ();
  notify_tag = 0;
  gdk_threads_leave ();

  return FALSE;
}

void
trayicon_set_notify (gboolean enabled)
{
  if (enabled && !on_notify)
    {
      trayicon_set_stock_icon (TRAYICON_NEW_IMAGE);
      on_notify = TRUE;
    }
  else if (!enabled && on_notify)
    {
      trayicon_set_stock_icon (TRAYICON_IMAGE);
      on_notify = FALSE;
    }

  if (enabled && notify_tag == 0)
    notify_tag = g_timeout_add (TRAYICON_NOTIFY_PERIOD, notify_timeout_cb, NULL);
  else if (!enabled && notify_tag > 0)
    {
      g_source_remove (notify_tag);
      notify_timeout_cb (NULL);
    }
}

void
trayicon_set_stock_icon (StockPixmap icon)
{
  GdkPixbuf *pixbuf = NULL;

  stock_pixbuf_gdk (icon, &pixbuf);
  gtk_status_icon_set_from_pixbuf (trayicon.status_icon, pixbuf);
}

static void
trayicon_activated (GtkStatusIcon * status_icon, gpointer data)
{
  MainWindow *mainwin = (MainWindow *) data;

  if (prefs_common.toggle_window_on_trayicon_click && gtk_window_is_active (GTK_WINDOW (mainwin->window)))
    gtk_widget_hide (mainwin->window);
  else
    {
      if (!gtk_widget_get_visible (mainwin->window))
        gtk_widget_show (mainwin->window);
      main_window_popup (mainwin);
    }
}

static void
trayicon_popup_menu_cb (GtkStatusIcon * status_icon, guint button, guint activate_time, gpointer data)
{
  gtk_menu_popup_at_pointer (GTK_MENU (trayicon_menu), NULL);
}

static void
trayicon_present (GtkWidget * widget, gpointer data)
{
  MainWindow *mainwin = (MainWindow *) data;

  main_window_popup (mainwin);
}

static void
trayicon_inc (GtkWidget * widget, gpointer data)
{
  if (!inc_is_active () && !yam_window_modal_exist ())
    inc_mail ((MainWindow *) data);
}

static void
trayicon_inc_all (GtkWidget * widget, gpointer data)
{
  if (!inc_is_active () && !yam_window_modal_exist ())
    inc_all_account_mail ((MainWindow *) data, FALSE);
}

static void
trayicon_send (GtkWidget * widget, gpointer data)
{
  if (!yam_window_modal_exist ())
    main_window_send_queue ((MainWindow *) data);
}

static void
trayicon_compose (GtkWidget * widget, gpointer data)
{
  if (!yam_window_modal_exist ())
    compose_new (NULL, NULL, NULL, NULL);
}

static void
trayicon_app_exit (GtkWidget * widget, gpointer data)
{
  MainWindow *mainwin = (MainWindow *) data;

  if (mainwin->lock_count == 0 && !yam_window_modal_exist ())
    app_will_exit (FALSE);
}

#else /* USE_TRAY */

TrayIcon *
trayicon_create (MainWindow * mainwin)
{
  return NULL;
}

void trayicon_show (TrayIcon * tray_icon)
{
}

void
trayicon_hide (TrayIcon * tray_icon)
{
}

void
trayicon_destroy (TrayIcon * tray_icon)
{
}

void
trayicon_set_tooltip (const gchar * text)
{
}

void
trayicon_set_notify (gboolean enabled)
{
}

void
trayicon_set_stock_icon (StockPixmap icon)
{
}

#endif /* USE_TRAY */
