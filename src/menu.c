/*
 * YAM -- a GTK+ based, lightweight, and fast e-mail client
 * Copyright (C) 2020 Victor Ananjevsky <victor@sanana.kiev.ua>
 * Copyright (C) 1999-2012 Hiroyuki Yamamoto
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

#include "itemfactory.h"
#include "menu.h"
#include "utils.h"

GtkWidget *
menubar_create (GtkWidget * window, GtkItemFactoryEntry * entries, guint n_entries, const gchar * path, gpointer data)
{
  GtkItemFactory *factory;

  factory = gtk_item_factory_new (GTK_TYPE_MENU_BAR, path, NULL);
  gtk_item_factory_create_items (factory, n_entries, entries, data);
  gtk_window_add_accel_group (GTK_WINDOW (window), factory->accel_group);

  return gtk_item_factory_get_widget (factory, path);
}

GtkWidget *
menu_create_items (GtkItemFactoryEntry * entries,
                   guint n_entries, const gchar * path, GtkItemFactory ** factory, gpointer data)
{
  *factory = gtk_item_factory_new (GTK_TYPE_MENU, path, NULL);
  gtk_item_factory_create_items (*factory, n_entries, entries, data);

  return gtk_item_factory_get_widget (*factory, path);
}

void
menu_set_sensitive (GtkItemFactory * ifactory, const gchar * path, gboolean sensitive)
{
  GtkWidget *widget;

  g_return_if_fail (ifactory != NULL);

  widget = gtk_item_factory_get_item (ifactory, path);
  gtk_widget_set_sensitive (widget, sensitive);
}

void
menu_set_sensitive_all (GtkMenuShell * menu_shell, gboolean sensitive)
{
  GList *cur;

  for (cur = gtk_container_get_children (GTK_CONTAINER (menu_shell)); cur != NULL; cur = cur->next)
    gtk_widget_set_sensitive (GTK_WIDGET (cur->data), sensitive);
}

void
menu_set_active (GtkItemFactory * ifactory, const gchar * path, gboolean is_active)
{
  GtkWidget *widget;

  g_return_if_fail (ifactory != NULL);

  widget = gtk_item_factory_get_item (ifactory, path);
  gtk_check_menu_item_set_active (GTK_CHECK_MENU_ITEM (widget), is_active);
}
