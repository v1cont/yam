/* GTK - The GIMP Toolkit
 * Copyright (C) 1995-1997 Peter Mattis, Spencer Kimball and Josh MacDonald
 *
 * GtkItemFactory: Flexible item factory with automatic rc handling
 * Copyright (C) 1998 Tim Janik
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.	 See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, write to the
 * Free Software Foundation, Inc., 59 Temple Place - Suite 330,
 * Boston, MA 02111-1307, USA.
 */

/*
 * Modified by the GTK+ Team and others 1997-2000.  See the AUTHORS
 * file for a list of people on the GTK+ Team.  See the ChangeLog
 * files for a list of changes.  These files are distributed with
 * GTK+ at ftp://ftp.gtk.org/pub/gtk/.
 */

#include <config.h>

#include <string.h>
#include <fcntl.h>
#include <unistd.h>
#include <stdio.h>

#include <libintl.h>

#include "itemfactory.h"

/* --- defines --- */
#define ITEM_FACTORY_STRING ((gchar*) item_factory_string)
#define ITEM_BLOCK_SIZE (128)

/* --- structures --- */
typedef struct _GtkIFCBData GtkIFCBData;
typedef struct _GtkIFDumpData GtkIFDumpData;
struct _GtkIFCBData {
  GtkItemFactoryCallback func;
  guint callback_type;
  gpointer func_data;
  guint callback_action;
};

/* --- prototypes --- */
static void gtk_item_factory_destroy (GtkWidget *object);
static void gtk_item_factory_finalize (GObject *object);

/* --- static variables --- */
static const gchar item_factory_string[] = "Gtk-<ItemFactory>";
static GQuark quark_popup_data = 0;
static GQuark quark_if_menu_pos = 0;
static GQuark quark_item_factory = 0;
static GQuark quark_item_path = 0;
static GQuark quark_action = 0;
static GQuark quark_accel_group = 0;
static GQuark quark_type_item = 0;
static GQuark quark_type_title = 0;
static GQuark quark_type_radio_item = 0;
static GQuark quark_type_check_item = 0;
static GQuark quark_type_toggle_item = 0;
static GQuark quark_type_separator_item = 0;
static GQuark quark_type_branch = 0;

G_DEFINE_TYPE (GtkItemFactory, gtk_item_factory, GTK_TYPE_WIDGET)

/* --- functions --- */
static void
gtk_item_factory_class_init (GtkItemFactoryClass * class)
{
  GObjectClass *gobject_class = G_OBJECT_CLASS (class);
  GtkWidgetClass *object_class = GTK_WIDGET_CLASS (class);

  gobject_class->finalize = gtk_item_factory_finalize;

  object_class->destroy = gtk_item_factory_destroy;

  class->item_ht = g_hash_table_new (g_str_hash, g_str_equal);

  quark_popup_data = g_quark_from_static_string ("GtkItemFactory-popup-data");
  quark_if_menu_pos = g_quark_from_static_string ("GtkItemFactory-menu-position");
  quark_item_factory = g_quark_from_static_string ("GtkItemFactory");
  quark_item_path = g_quark_from_static_string ("GtkItemFactory-path");
  quark_action = g_quark_from_static_string ("GtkItemFactory-action");
  quark_accel_group = g_quark_from_static_string ("GtkAccelGroup");
  quark_type_item = g_quark_from_static_string ("<Item>");
  quark_type_title = g_quark_from_static_string ("<Title>");
  quark_type_radio_item = g_quark_from_static_string ("<RadioItem>");
  quark_type_check_item = g_quark_from_static_string ("<CheckItem>");
  quark_type_toggle_item = g_quark_from_static_string ("<ToggleItem>");
  quark_type_separator_item = g_quark_from_static_string ("<Separator>");
  quark_type_branch = g_quark_from_static_string ("<Branch>");
}

static void
gtk_item_factory_init (GtkItemFactory * ifactory)
{
  ifactory->path = NULL;
  ifactory->accel_group = NULL;
  ifactory->widget = NULL;
  ifactory->items = NULL;
  ifactory->translate_func = NULL;
  ifactory->translate_data = NULL;
  ifactory->translate_notify = NULL;
}

static void gtk_item_factory_add_item (GtkItemFactory * ifactory, const gchar * path, const gchar * accelerator,
                                       GtkItemFactoryCallback callback, guint callback_action,
                                       gpointer callback_data, guint callback_type, gchar * item_type, GtkWidget * widget);

static gchar *
translate_func (const gchar *str, gpointer data)
{
  return gettext (str);
}

/**
 * gtk_item_factory_new:
 * @container_type: the kind of menu to create; can be
 *    #GTK_TYPE_MENU_BAR, #GTK_TYPE_MENU or #GTK_TYPE_OPTION_MENU
 * @path: the factory path of the new item factory, a string of the form
 *    <literal>"&lt;name&gt;"</literal>
 * @accel_group: (allow-none): a #GtkAccelGroup to which the accelerators for the
 *    menu items will be added, or %NULL to create a new one
 * @returns: a new #GtkItemFactory
 *
 * Creates a new #GtkItemFactory.
 *
 * Beware that the returned object does not have a floating reference.
 *
 * Deprecated: 2.4: Use #GtkUIManager instead.
 */
GtkItemFactory *
gtk_item_factory_new (GType container_type, const gchar * path, GtkAccelGroup * accel_group)
{
  GtkItemFactory *ifactory;
  guint len;

  g_return_val_if_fail (path != NULL, NULL);

  ifactory = g_object_new (GTK_TYPE_ITEM_FACTORY, NULL);

  len = strlen (path);

  if (path[0] != '<' && path[len - 1] != '>')
    {
      g_warning ("GtkItemFactory: invalid factory path `%s'", path);
      return NULL;
    }

  if (accel_group)
    {
      ifactory->accel_group = accel_group;
      g_object_ref (ifactory->accel_group);
    }
  else
    ifactory->accel_group = gtk_accel_group_new ();

  ifactory->path = g_strdup (path);
  ifactory->widget = g_object_connect (g_object_new (container_type, NULL),
                                       "signal::destroy", gtk_widget_destroyed, &ifactory->widget, NULL);
  g_object_ref_sink (ifactory);

  ifactory->translate_func = translate_func;

  gtk_item_factory_add_item (ifactory, "", NULL, NULL, 0, NULL, 0, ITEM_FACTORY_STRING, ifactory->widget);

  return ifactory;
}

static void
gtk_item_factory_callback_marshal (GtkWidget * widget, gpointer func_data)
{
  GtkIFCBData *data;

  data = func_data;

  if (data->callback_type == 1)
    {
      GtkItemFactoryCallback1 func1 = (GtkItemFactoryCallback1) data->func;
      func1 (data->func_data, data->callback_action, widget);
    }
  else if (data->callback_type == 2)
    {
      GtkItemFactoryCallback2 func2 = (GtkItemFactoryCallback2) data->func;
      func2 (widget, data->func_data, data->callback_action);
    }
}

static void
gtk_item_factory_item_remove_widget (GtkWidget * widget, GtkItemFactoryItem * item)
{
  item->widgets = g_slist_remove (item->widgets, widget);
  g_object_set_qdata (G_OBJECT (widget), quark_item_factory, NULL);
  g_object_set_qdata (G_OBJECT (widget), quark_item_path, NULL);
}

static void
ifactory_cb_data_free (gpointer mem)
{
  g_slice_free (GtkIFCBData, mem);
}

static void
gtk_item_factory_add_foreign (GtkWidget *accel_widget, const gchar *full_path,
                              GtkAccelGroup *accel_group, guint keyval,  GdkModifierType modifiers)
{
  GtkItemFactoryClass *class;
  GtkItemFactoryItem *item;

  g_return_if_fail (GTK_IS_WIDGET (accel_widget));
  g_return_if_fail (full_path != NULL);

  class = g_type_class_peek (GTK_TYPE_ITEM_FACTORY);

  keyval = keyval != GDK_KEY_VoidSymbol ? keyval : 0;

  item = g_hash_table_lookup (class->item_ht, full_path);
  if (!item)
    {
      item = g_slice_new (GtkItemFactoryItem);

      item->path = g_strdup (full_path);
      item->widgets = NULL;

      g_hash_table_insert (class->item_ht, item->path, item);
    }

  item->widgets = g_slist_prepend (item->widgets, accel_widget);
  g_signal_connect (accel_widget, "destroy", G_CALLBACK (gtk_item_factory_item_remove_widget), item);

  /* set the item path for the widget
   */
  g_object_set_qdata (G_OBJECT (accel_widget), quark_item_path, item->path);
  gtk_widget_set_name (accel_widget, item->path);
  if (accel_group)
    {
      g_object_ref (accel_group);
      g_object_set_qdata_full (G_OBJECT (accel_widget), quark_accel_group, accel_group, g_object_unref);
    }
  else
    g_object_set_qdata (G_OBJECT (accel_widget), quark_accel_group, NULL);

  /* install defined accelerators
   */
  if (g_signal_lookup ("activate", G_TYPE_FROM_INSTANCE (accel_widget)))
    {
      if (accel_group)
        {
          gtk_accel_map_add_entry (full_path, keyval, modifiers);
          gtk_widget_set_accel_path (accel_widget, full_path, accel_group);
        }
    }
}

static void
gtk_item_factory_add_item (GtkItemFactory * ifactory, const gchar * path, const gchar * accelerator,
                           GtkItemFactoryCallback callback, guint callback_action,
                           gpointer callback_data, guint callback_type, gchar * item_type, GtkWidget * widget)
{
  GtkItemFactoryClass *class;
  GtkItemFactoryItem *item;
  gchar *fpath;
  guint keyval;
  GdkModifierType mods;

  g_return_if_fail (widget != NULL);
  g_return_if_fail (item_type != NULL);

  class = GTK_ITEM_FACTORY_GET_CLASS (ifactory);

  /* set accelerator group on menu widgets
   */
  if (GTK_IS_MENU (widget))
    gtk_menu_set_accel_group ((GtkMenu *) widget, ifactory->accel_group);

  /* connect callback if necessary
   */
  if (callback)
    {
      GtkIFCBData *data;

      data = g_slice_new (GtkIFCBData);
      data->func = callback;
      data->callback_type = callback_type;
      data->func_data = callback_data;
      data->callback_action = callback_action;

      g_object_weak_ref (G_OBJECT (widget), (GWeakNotify) ifactory_cb_data_free, data);
      g_signal_connect (widget, "activate", G_CALLBACK (gtk_item_factory_callback_marshal), data);
    }

  /* link the widget into its item-entry
   * and keep back pointer on both the item factory and the widget
   */
  g_object_set_qdata (G_OBJECT (widget), quark_action, GUINT_TO_POINTER (callback_action));
  g_object_set_qdata (G_OBJECT (widget), quark_item_factory, ifactory);
  if (accelerator)
    gtk_accelerator_parse (accelerator, &keyval, &mods);
  else
    {
      keyval = 0;
      mods = 0;
    }
  fpath = g_strconcat (ifactory->path, path, NULL);
  gtk_item_factory_add_foreign (widget, fpath, ifactory->accel_group, keyval, mods);
  item = g_hash_table_lookup (class->item_ht, fpath);
  g_free (fpath);

  g_return_if_fail (item != NULL);

  if (!g_slist_find (ifactory->items, item))
    ifactory->items = g_slist_prepend (ifactory->items, item);
}

/**
 * gtk_item_factory_from_path:
 * @path: a string starting with a factory path of the form
 *   <literal>"&lt;name&gt;"</literal>
 * @returns: (allow-none): the #GtkItemFactory created for the given factory path, or %NULL
 *
 * Finds an item factory which has been constructed using the
 * <literal>"&lt;name&gt;"</literal> prefix of @path as the @path argument
 * for gtk_item_factory_new().
 *
 * Deprecated: 2.4: Use #GtkUIManager instead.
 */
GtkItemFactory *
gtk_item_factory_from_path (const gchar * path)
{
  GtkItemFactoryClass *class;
  GtkItemFactoryItem *item;
  gchar *fname;
  guint i;

  g_return_val_if_fail (path != NULL, NULL);
  g_return_val_if_fail (path[0] == '<', NULL);

  class = g_type_class_peek (GTK_TYPE_ITEM_FACTORY);

  i = 0;
  while (path[i] && path[i] != '>')
    i++;
  if (path[i] != '>')
    {
      g_warning ("gtk_item_factory_from_path(): invalid factory path \"%s\"", path);
      return NULL;
    }
  fname = g_new (gchar, i + 2);
  memmove (fname, path, i + 1);
  fname[i + 1] = 0;

  item = g_hash_table_lookup (class->item_ht, fname);

  g_free (fname);

  if (item && item->widgets)
    return gtk_item_factory_from_widget (item->widgets->data);

  return NULL;
}

static void
gtk_item_factory_destroy (GtkWidget * object)
{
  GtkItemFactory *ifactory = (GtkItemFactory *) object;
  GSList *slist;

  if (ifactory->widget)
    {
      GObject *dobj;

      dobj = G_OBJECT (ifactory->widget);

      g_object_ref_sink (dobj);
      gtk_widget_destroy (GTK_WIDGET (dobj));
      g_object_unref (dobj);

      ifactory->widget = NULL;
    }

  for (slist = ifactory->items; slist; slist = slist->next)
    {
      GtkItemFactoryItem *item = slist->data;
      GSList *link;

      for (link = item->widgets; link; link = link->next)
        if (g_object_get_qdata (link->data, quark_item_factory) == ifactory)
          g_object_set_qdata (link->data, quark_item_factory, NULL);
    }
  g_slist_free (ifactory->items);
  ifactory->items = NULL;

  GTK_WIDGET_CLASS (gtk_item_factory_parent_class)->destroy (object);
}

static void
gtk_item_factory_finalize (GObject *object)
{
  GtkItemFactory *ifactory = GTK_ITEM_FACTORY (object);

  if (ifactory->accel_group)
    g_object_unref (ifactory->accel_group);

  g_free (ifactory->path);
  g_assert (ifactory->widget == NULL);

  if (ifactory->translate_notify)
    ifactory->translate_notify (ifactory->translate_data);

  G_OBJECT_CLASS (gtk_item_factory_parent_class)->finalize (object);
}

/**
 * gtk_item_factory_from_widget:
 * @widget: a widget
 * @returns: (allow-none): the item factory from which @widget was created, or %NULL
 *
 * Obtains the item factory from which a widget was created.
 *
 * Deprecated: 2.4: Use #GtkUIManager instead.
 */
GtkItemFactory *
gtk_item_factory_from_widget (GtkWidget * widget)
{
  GtkItemFactory *ifactory;

  g_return_val_if_fail (GTK_IS_WIDGET (widget), NULL);

  ifactory = g_object_get_qdata (G_OBJECT (widget), quark_item_factory);

  if (ifactory == NULL && GTK_IS_MENU_ITEM (widget) &&
      gtk_menu_item_get_submenu (GTK_MENU_ITEM (widget)) != NULL)
    {
      GtkWidget *menu = gtk_menu_item_get_submenu (GTK_MENU_ITEM (widget));
      ifactory = g_object_get_qdata (G_OBJECT (menu), quark_item_factory);
    }

  return ifactory;
}

/**
 * gtk_item_factory_create_items:
 * @ifactory: a #GtkItemFactory
 * @n_entries: the length of @entries
 * @entries: an array of #GtkItemFactoryEntry<!-- -->s whose @callback members
 *    must by of type #GtkItemFactoryCallback1
 * @callback_data: data passed to the callback functions of all entries
 *
 * Creates the menu items from the @entries.
 *
 * Deprecated: 2.4: Use #GtkUIManager instead.
 */
void
gtk_item_factory_create_items (GtkItemFactory * ifactory,
                               guint n_entries, GtkItemFactoryEntry * entries, gpointer callback_data)
{
  guint i;

  g_return_if_fail (GTK_IS_ITEM_FACTORY (ifactory));

  if (n_entries == 0)
    return;

  g_return_if_fail (entries != NULL);

  for (i = 0; i < n_entries; i++)
    gtk_item_factory_create_item (ifactory, entries + i, callback_data, 1);
}

/**
 * gtk_item_factory_get_widget:
 * @ifactory: a #GtkItemFactory
 * @path: the path to the widget
 * @returns: (allow-none): the widget for the given path, or %NULL if @path doesn't lead
 *   to a widget
 *
 * Obtains the widget which corresponds to @path.
 *
 * If the widget corresponding to @path is a menu item which opens a
 * submenu, then the submenu is returned. If you are interested in the menu
 * item, use gtk_item_factory_get_item() instead.
 *
 * Deprecated: 2.4: Use #GtkUIManager instead.
 */
GtkWidget *
gtk_item_factory_get_widget (GtkItemFactory * ifactory, const gchar * path)
{
  GtkItemFactoryClass *class;
  GtkItemFactoryItem *item;

  g_return_val_if_fail (GTK_IS_ITEM_FACTORY (ifactory), NULL);
  g_return_val_if_fail (path != NULL, NULL);

  class = GTK_ITEM_FACTORY_GET_CLASS (ifactory);

  if (path[0] == '<')
    item = g_hash_table_lookup (class->item_ht, (gpointer) path);
  else
    {
      gchar *fpath;

      fpath = g_strconcat (ifactory->path, path, NULL);
      item = g_hash_table_lookup (class->item_ht, fpath);
      g_free (fpath);
    }

  if (item)
    {
      GSList *slist;

      for (slist = item->widgets; slist; slist = slist->next)
        {
          if (gtk_item_factory_from_widget (slist->data) == ifactory)
            return slist->data;
        }
    }

  return NULL;
}

/**
 * gtk_item_factory_get_widget_by_action:
 * @ifactory: a #GtkItemFactory
 * @action: an action as specified in the @callback_action field
 *   of #GtkItemFactoryEntry
 * @returns: (allow-none): the widget which corresponds to the given action, or %NULL
 *   if no widget was found
 *
 * Obtains the widget which was constructed from the #GtkItemFactoryEntry
 * with the given @action.
 *
 * If there are multiple items with the same action, the result is
 * undefined.
 *
 * Deprecated: 2.4: Use #GtkUIManager instead.
 */
GtkWidget *
gtk_item_factory_get_widget_by_action (GtkItemFactory * ifactory, guint action)
{
  GSList *slist;

  g_return_val_if_fail (GTK_IS_ITEM_FACTORY (ifactory), NULL);

  for (slist = ifactory->items; slist; slist = slist->next)
    {
      GtkItemFactoryItem *item = slist->data;
      GSList *link;

      for (link = item->widgets; link; link = link->next)
        if (g_object_get_qdata (link->data, quark_item_factory) == ifactory &&
            g_object_get_qdata (link->data, quark_action) == GUINT_TO_POINTER (action))
          return link->data;
    }

  return NULL;
}

/**
 * gtk_item_factory_get_item:
 * @ifactory: a #GtkItemFactory
 * @path: the path to the menu item
 * @returns: (allow-none): the menu item for the given path, or %NULL if @path doesn't
 *   lead to a menu item
 *
 * Obtains the menu item which corresponds to @path.
 *
 * If the widget corresponding to @path is a menu item which opens a
 * submenu, then the item is returned. If you are interested in the submenu,
 * use gtk_item_factory_get_widget() instead.
 *
 * Deprecated: 2.4: Use #GtkUIManager instead.
 */
GtkWidget *
gtk_item_factory_get_item (GtkItemFactory * ifactory, const gchar * path)
{
  GtkWidget *widget;

  g_return_val_if_fail (GTK_IS_ITEM_FACTORY (ifactory), NULL);
  g_return_val_if_fail (path != NULL, NULL);

  widget = gtk_item_factory_get_widget (ifactory, path);

  if (GTK_IS_MENU (widget))
    widget = gtk_menu_get_attach_widget (GTK_MENU (widget));

  return GTK_IS_WIDGET (widget) ? widget : NULL;
}

static char *
item_factory_find_separator_r (char *path)
{
  gchar *result = NULL;
  gboolean escaped = FALSE;

  while (*path)
    {
      if (escaped)
        escaped = FALSE;
      else
        {
          if (*path == '\\')
            escaped = TRUE;
          else if (*path == '/')
            result = path;
        }

      path++;
    }

  return result;
}

static char *
item_factory_unescape_label (const char *label)
{
  char *new = g_malloc (strlen (label) + 1);
  char *p = new;
  gboolean escaped = FALSE;

  while (*label)
    {
      if (escaped)
        {
          *p++ = *label;
          escaped = FALSE;
        }
      else
        {
          if (*label == '\\')
            escaped = TRUE;
          else
            *p++ = *label;
        }

      label++;
    }

  *p = '\0';

  return new;
}

static gboolean
gtk_item_factory_parse_path (GtkItemFactory * ifactory, gchar * str, gchar ** path, gchar ** parent_path, gchar ** item)
{
  gchar *translation;
  gchar *p, *q;

  *path = g_strdup (str);

  p = q = *path;
  while (*p)
    {
      if (*p == '_')
        {
          if (p[1] == '_')
            {
              p++;
              *q++ = '_';
            }
        }
      else
        {
          *q++ = *p;
        }
      p++;
    }
  *q = 0;

  *parent_path = g_strdup (*path);
  p = item_factory_find_separator_r (*parent_path);
  if (!p)
    {
      g_warning ("GtkItemFactory: invalid entry path `%s'", str);
      return FALSE;
    }
  *p = 0;

  if (ifactory->translate_func)
    translation = ifactory->translate_func (str, ifactory->translate_data);
  else
    translation = str;

  p = item_factory_find_separator_r (translation);
  if (p)
    p++;
  else
    p = translation;

  *item = item_factory_unescape_label (p);

  return TRUE;
}

/**
 * gtk_item_factory_create_item:
 * @ifactory: a #GtkItemFactory
 * @entry: the #GtkItemFactoryEntry to create an item for
 * @callback_data: data passed to the callback function of @entry
 * @callback_type: 1 if the callback function of @entry is of type
 *    #GtkItemFactoryCallback1, 2 if it is of type #GtkItemFactoryCallback2
 *
 * Creates an item for @entry.
 *
 * Deprecated: 2.4: Use #GtkUIManager instead.
 */
void
gtk_item_factory_create_item (GtkItemFactory *ifactory,
                              GtkItemFactoryEntry *entry, gpointer callback_data, guint callback_type)
{
  GtkWidget *parent;
  GtkWidget *widget;
  GSList *radio_group;
  gchar *name;
  gchar *parent_path;
  gchar *path;
  gchar *accelerator;
  guint type_id;
  GType type;
  gchar *item_type_path;

  g_return_if_fail (GTK_IS_ITEM_FACTORY (ifactory));
  g_return_if_fail (entry != NULL);
  g_return_if_fail (entry->path != NULL);
  g_return_if_fail (entry->path[0] == '/');
  g_return_if_fail (callback_type >= 1 && callback_type <= 2);

  if (!entry->item_type || entry->item_type[0] == 0)
    {
      item_type_path = "<Item>";
      type_id = quark_type_item;
    }
  else
    {
      item_type_path = entry->item_type;
      type_id = g_quark_try_string (item_type_path);
    }

  radio_group = NULL;
  if (type_id == quark_type_item)
    type = GTK_TYPE_MENU_ITEM;
  else if (type_id == quark_type_title)
    type = GTK_TYPE_MENU_ITEM;
  else if (type_id == quark_type_radio_item)
    type = GTK_TYPE_RADIO_MENU_ITEM;
  else if (type_id == quark_type_check_item)
    type = GTK_TYPE_CHECK_MENU_ITEM;
  else if (type_id == quark_type_toggle_item)
    type = GTK_TYPE_CHECK_MENU_ITEM;
  else if (type_id == quark_type_separator_item)
    type = GTK_TYPE_SEPARATOR_MENU_ITEM;
  else if (type_id == quark_type_branch)
    type = GTK_TYPE_MENU_ITEM;
  else
    {
      GtkWidget *radio_link;

      radio_link = gtk_item_factory_get_widget (ifactory, item_type_path);
      if (radio_link && GTK_IS_RADIO_MENU_ITEM (radio_link))
        {
          type = GTK_TYPE_RADIO_MENU_ITEM;
          radio_group = gtk_radio_menu_item_get_group (GTK_RADIO_MENU_ITEM (radio_link));
        }
      else
        {
          g_warning ("GtkItemFactory: entry path `%s' has invalid type `%s'", entry->path, item_type_path);
          return;
        }
    }

  if (!gtk_item_factory_parse_path (ifactory, entry->path, &path, &parent_path, &name))
    return;

  parent = gtk_item_factory_get_widget (ifactory, parent_path);
  if (!parent)
    {
      GtkItemFactoryEntry pentry;
      gchar *ppath, *p;

      ppath = g_strdup (entry->path);
      p = item_factory_find_separator_r (ppath);
      g_return_if_fail (p != NULL);
      *p = 0;
      pentry.path = ppath;
      pentry.accelerator = NULL;
      pentry.callback = NULL;
      pentry.callback_action = 0;
      pentry.item_type = "<Branch>";

      gtk_item_factory_create_item (ifactory, &pentry, NULL, 1);
      g_free (ppath);

      parent = gtk_item_factory_get_widget (ifactory, parent_path);
      g_return_if_fail (parent != NULL);
    }

  g_free (parent_path);

  g_return_if_fail (GTK_IS_CONTAINER (parent));

  accelerator = entry->accelerator;

  widget = g_object_new (type, "visible", TRUE,
                         "sensitive", (type_id != quark_type_separator_item &&
                                       type_id != quark_type_title), "parent", parent, NULL);

  if (GTK_IS_RADIO_MENU_ITEM (widget))
    gtk_radio_menu_item_set_group (GTK_RADIO_MENU_ITEM (widget), radio_group);

  /* install underline accelerators for this item
   */
  if (type_id != quark_type_separator_item && *name)
    {
      GtkWidget *label;

      label = g_object_new (GTK_TYPE_ACCEL_LABEL,
                            "visible", TRUE, "parent", widget, "accel-widget", widget, "xalign", 0.0, NULL);
      gtk_label_set_text_with_mnemonic (GTK_LABEL (label), name);
    }

  g_free (name);

  if (type_id == quark_type_branch)
    {
      gchar *p;

      if (entry->callback)
        g_warning ("gtk_item_factory_create_item(): Can't specify a callback on a branch: \"%s\"", entry->path);

      parent = widget;
      widget = g_object_new (GTK_TYPE_MENU, NULL);
      p = g_strconcat (ifactory->path, path, NULL);
      gtk_menu_set_accel_path (GTK_MENU (widget), p);
      g_free (p);

      gtk_menu_item_set_submenu (GTK_MENU_ITEM (parent), widget);
    }

  gtk_item_factory_add_item (ifactory, path, accelerator,
                             (type_id == quark_type_branch) ? (GtkItemFactoryCallback) NULL : entry->callback,
                             entry->callback_action, callback_data, callback_type, item_type_path, widget);
  if (accelerator != entry->accelerator)
    g_free (accelerator);
  g_free (path);
}

/**
 * gtk_item_factory_create_menu_entries:
 * @n_entries: the length of @entries
 * @entries: an array of #GtkMenuEntry<!-- -->s
 *
 * Creates the menu items from the @entries.
 *
 * Deprecated: 2.4: Use #GtkUIManager instead.
 */
void
gtk_item_factory_create_menu_entries (guint n_entries, GtkMenuEntry * entries)
{
  static GPatternSpec *pspec_separator = NULL;
  static GPatternSpec *pspec_check = NULL;
  guint i;

  if (!n_entries)
    return;
  g_return_if_fail (entries != NULL);

  if (!pspec_separator)
    {
      pspec_separator = g_pattern_spec_new ("*<separator>*");
      pspec_check = g_pattern_spec_new ("*<check>*");
    }

  for (i = 0; i < n_entries; i++)
    {
      GtkItemFactory *ifactory;
      GtkItemFactoryEntry entry;
      gchar *path;
      gchar *cpath;

      path = entries[i].path;
      ifactory = gtk_item_factory_from_path (path);
      if (!ifactory)
        {
          g_warning ("gtk_item_factory_create_menu_entries(): "
                     "entry[%u] refers to unknown item factory: \"%s\"", i, entries[i].path);
          continue;
        }

      while (*path != '>')
        path++;
      path++;
      cpath = NULL;

      entry.path = path;
      entry.accelerator = entries[i].accelerator;
      entry.callback = entries[i].callback;
      entry.callback_action = 0;
      if (g_pattern_match_string (pspec_separator, path))
        entry.item_type = "<Separator>";
      else if (!g_pattern_match_string (pspec_check, path))
        entry.item_type = NULL;
      else
        {
          gboolean in_brace = FALSE;
          gchar *c;

          cpath = g_new (gchar, strlen (path));
          c = cpath;
          while (*path != 0)
            {
              if (*path == '<')
                in_brace = TRUE;
              else if (*path == '>')
                in_brace = FALSE;
              else if (!in_brace)
                *(c++) = *path;
              path++;
            }
          *c = 0;
          entry.item_type = "<ToggleItem>";
          entry.path = cpath;
        }

      gtk_item_factory_create_item (ifactory, &entry, entries[i].callback_data, 2);
      entries[i].widget = gtk_item_factory_get_widget (ifactory, entries[i].path);
      g_free (cpath);
    }
}

/**
 * gtk_item_factories_path_delete:
 * @ifactory_path: a factory path to prepend to @path. May be %NULL if @path
 *   starts with a factory path
 * @path: a path
 *
 * Deletes all widgets constructed from the specified path.
 *
 * Deprecated: 2.4: Use #GtkUIManager instead.
 */
void
gtk_item_factories_path_delete (const gchar * ifactory_path, const gchar * path)
{
  GtkItemFactoryClass *class;
  GtkItemFactoryItem *item;

  g_return_if_fail (path != NULL);

  class = g_type_class_peek (GTK_TYPE_ITEM_FACTORY);

  if (path[0] == '<')
    item = g_hash_table_lookup (class->item_ht, (gpointer) path);
  else
    {
      gchar *fpath;

      g_return_if_fail (ifactory_path != NULL);

      fpath = g_strconcat (ifactory_path, path, NULL);
      item = g_hash_table_lookup (class->item_ht, fpath);
      g_free (fpath);
    }

  if (item)
    {
      GSList *widget_list;
      GSList *slist;

      widget_list = NULL;
      for (slist = item->widgets; slist; slist = slist->next)
        {
          GtkWidget *widget;

          widget = slist->data;
          widget_list = g_slist_prepend (widget_list, widget);
          g_object_ref (widget);
        }

      for (slist = widget_list; slist; slist = slist->next)
        {
          GtkWidget *widget;

          widget = slist->data;
          gtk_widget_destroy (widget);
          g_object_unref (widget);
        }
      g_slist_free (widget_list);
    }
}

/**
 * gtk_item_factory_delete_item:
 * @ifactory: a #GtkItemFactory
 * @path: a path
 *
 * Deletes the menu item which was created for @path by the given
 * item factory.
 *
 * Deprecated: 2.4: Use #GtkUIManager instead.
 */
void
gtk_item_factory_delete_item (GtkItemFactory * ifactory, const gchar * path)
{
  GtkWidget *widget;

  g_return_if_fail (GTK_IS_ITEM_FACTORY (ifactory));
  g_return_if_fail (path != NULL);

  widget = gtk_item_factory_get_widget (ifactory, path);

  if (widget)
    {
      if (GTK_IS_MENU (widget))
        widget = gtk_menu_get_attach_widget (GTK_MENU (widget));

      gtk_widget_destroy (widget);
    }
}

/**
 * gtk_item_factory_delete_entry:
 * @ifactory: a #GtkItemFactory
 * @entry: a #GtkItemFactoryEntry
 *
 * Deletes the menu item which was created from @entry by the given
 * item factory.
 *
 * Deprecated: 2.4: Use #GtkUIManager instead.
 */
void
gtk_item_factory_delete_entry (GtkItemFactory * ifactory, GtkItemFactoryEntry * entry)
{
  gchar *path;
  gchar *parent_path;
  gchar *name;

  g_return_if_fail (GTK_IS_ITEM_FACTORY (ifactory));
  g_return_if_fail (entry != NULL);
  g_return_if_fail (entry->path != NULL);
  g_return_if_fail (entry->path[0] == '/');

  if (!gtk_item_factory_parse_path (ifactory, entry->path, &path, &parent_path, &name))
    return;

  gtk_item_factory_delete_item (ifactory, path);

  g_free (path);
  g_free (parent_path);
  g_free (name);
}

/**
 * gtk_item_factory_delete_entries:
 * @ifactory: a #GtkItemFactory
 * @n_entries: the length of @entries
 * @entries: an array of #GtkItemFactoryEntry<!-- -->s
 *
 * Deletes the menu items which were created from the @entries by the given
 * item factory.
 *
 * Deprecated: 2.4: Use #GtkUIManager instead.
 */
void
gtk_item_factory_delete_entries (GtkItemFactory * ifactory, guint n_entries, GtkItemFactoryEntry * entries)
{
  guint i;

  g_return_if_fail (GTK_IS_ITEM_FACTORY (ifactory));
  if (n_entries > 0)
    g_return_if_fail (entries != NULL);

  for (i = 0; i < n_entries; i++)
    gtk_item_factory_delete_entry (ifactory, entries + i);
}
