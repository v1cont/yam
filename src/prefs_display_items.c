/*
 * YAM -- a GTK+ based, lightweight, and fast e-mail client
 * Copyright (C) 2020 Victor Ananjevsky <victor@sanana.kiev.ua>
 * Copyright (C) 1999-2015 Hiroyuki Yamamoto
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
#include <gdk/gdkkeysyms.h>
#include <gtk/gtk.h>

#include "prefs.h"
#include "prefs_ui.h"
#include "prefs_common.h"
#include "prefs_display_items.h"
#include "manage_window.h"
#include "mainwindow.h"
#include "gtkutils.h"
#include "utils.h"

#define SCROLLED_WINDOW_WIDTH	180
#define SCROLLED_WINDOW_HEIGHT	210

/* callback functions */
static void prefs_display_items_add (GtkWidget * widget, gpointer data);
static void prefs_display_items_remove (GtkWidget * widget, gpointer data);

static void prefs_display_items_up (GtkWidget * widget, gpointer data);
static void prefs_display_items_down (GtkWidget * widget, gpointer data);

static void prefs_display_items_default (GtkWidget * widget, gpointer data);

static void prefs_display_items_ok (GtkWidget * widget, gpointer data);
static void prefs_display_items_cancel (GtkWidget * widget, gpointer data);

static gint prefs_display_items_delete_event (GtkWidget * widget, GdkEventAny * event, gpointer data);
static gboolean prefs_display_items_key_pressed (GtkWidget * widget, GdkEventKey * event, gpointer data);

PrefsDisplayItemsDialog *
prefs_display_items_dialog_create (void)
{
  PrefsDisplayItemsDialog *dialog;

  GtkWidget *window;
  GtkWidget *vbox;

  GtkWidget *label_hbox;
  GtkWidget *label;

  GtkWidget *vbox1;

  GtkWidget *hbox1;
  GtkWidget *list_hbox;
  GtkWidget *scrolledwin;
  GtkWidget *stock_list;
  GtkWidget *shown_list;

  GtkListStore *store;
  GtkTreeViewColumn *col;
  GtkCellRenderer *renderer;

  GtkWidget *btn_vbox;
  GtkWidget *btn_vbox1;
  GtkWidget *add_btn;
  GtkWidget *remove_btn;
  GtkWidget *up_btn;
  GtkWidget *down_btn;

  GtkWidget *btn_hbox;
  GtkWidget *default_btn;
  GtkWidget *confirm_area;
  GtkWidget *ok_btn;
  GtkWidget *cancel_btn;

  dialog = g_new0 (PrefsDisplayItemsDialog, 1);

  window = gtk_window_new (GTK_WINDOW_TOPLEVEL);
  gtk_container_set_border_width (GTK_CONTAINER (window), 8);
  gtk_window_set_position (GTK_WINDOW (window), GTK_WIN_POS_CENTER_ON_PARENT);
  gtk_window_set_modal (GTK_WINDOW (window), TRUE);
  gtk_window_set_title (GTK_WINDOW (window), _("Display items setting"));
  gtk_window_set_resizable (GTK_WINDOW (window), FALSE);
  g_signal_connect (G_OBJECT (window), "delete_event", G_CALLBACK (prefs_display_items_delete_event), dialog);
  g_signal_connect (G_OBJECT (window), "key_press_event", G_CALLBACK (prefs_display_items_key_pressed), dialog);

  vbox = gtk_box_new (GTK_ORIENTATION_VERTICAL, 6);
  gtk_container_add (GTK_CONTAINER (window), vbox);

  label_hbox = gtk_box_new (GTK_ORIENTATION_HORIZONTAL, 0);
  gtk_box_pack_start (GTK_BOX (vbox), label_hbox, FALSE, FALSE, 4);

  label = gtk_label_new (NULL);
  gtk_box_pack_start (GTK_BOX (label_hbox), label, FALSE, FALSE, 4);
  gtk_label_set_justify (GTK_LABEL (label), GTK_JUSTIFY_LEFT);

  vbox1 = gtk_box_new (GTK_ORIENTATION_VERTICAL, VSPACING);
  gtk_box_pack_start (GTK_BOX (vbox), vbox1, TRUE, TRUE, 0);
  gtk_container_set_border_width (GTK_CONTAINER (vbox1), 2);

  hbox1 = gtk_box_new (GTK_ORIENTATION_HORIZONTAL, 8);
  gtk_box_pack_start (GTK_BOX (vbox1), hbox1, FALSE, TRUE, 0);

  list_hbox = gtk_box_new (GTK_ORIENTATION_HORIZONTAL, 8);
  gtk_box_pack_start (GTK_BOX (hbox1), list_hbox, TRUE, TRUE, 0);

  scrolledwin = gtk_scrolled_window_new (NULL, NULL);
  gtk_widget_set_size_request (scrolledwin, SCROLLED_WINDOW_WIDTH, SCROLLED_WINDOW_HEIGHT);
  gtk_box_pack_start (GTK_BOX (list_hbox), scrolledwin, TRUE, TRUE, 0);
  gtk_scrolled_window_set_policy (GTK_SCROLLED_WINDOW (scrolledwin), GTK_POLICY_AUTOMATIC, GTK_POLICY_AUTOMATIC);

  store = gtk_list_store_new (2, G_TYPE_STRING, G_TYPE_POINTER);
  stock_list = gtk_tree_view_new_with_model (GTK_TREE_MODEL (store));
  gtk_tree_view_set_headers_visible (GTK_TREE_VIEW (stock_list), TRUE);
  gtk_tree_selection_set_mode (gtk_tree_view_get_selection (GTK_TREE_VIEW (stock_list)), GTK_SELECTION_BROWSE);
  gtk_container_add (GTK_CONTAINER (scrolledwin), stock_list);
  g_object_unref (store);

  renderer = gtk_cell_renderer_text_new ();
  col = gtk_tree_view_column_new_with_attributes (_("Available items"), renderer, "text", 0, NULL);
  gtk_tree_view_column_set_expand (col, TRUE);
  gtk_tree_view_append_column (GTK_TREE_VIEW (stock_list), col);

  /* add/remove button */
  btn_vbox = gtk_box_new (GTK_ORIENTATION_VERTICAL, 0);
  gtk_box_pack_start (GTK_BOX (hbox1), btn_vbox, FALSE, FALSE, 0);

  btn_vbox1 = gtk_box_new (GTK_ORIENTATION_VERTICAL, 8);
  gtk_box_pack_start (GTK_BOX (btn_vbox), btn_vbox1, TRUE, FALSE, 0);

  add_btn = gtk_button_new_with_label ("  ->  ");
  gtk_widget_show (add_btn);
  gtk_box_pack_start (GTK_BOX (btn_vbox1), add_btn, FALSE, FALSE, 0);

  remove_btn = gtk_button_new_with_label ("  <-  ");
  gtk_box_pack_start (GTK_BOX (btn_vbox1), remove_btn, FALSE, FALSE, 0);

  g_signal_connect (G_OBJECT (add_btn), "clicked", G_CALLBACK (prefs_display_items_add), dialog);
  g_signal_connect (G_OBJECT (remove_btn), "clicked", G_CALLBACK (prefs_display_items_remove), dialog);

  list_hbox = gtk_box_new (GTK_ORIENTATION_HORIZONTAL, 8);
  gtk_box_pack_start (GTK_BOX (hbox1), list_hbox, TRUE, TRUE, 0);

  scrolledwin = gtk_scrolled_window_new (NULL, NULL);
  gtk_widget_set_size_request (scrolledwin, SCROLLED_WINDOW_WIDTH, SCROLLED_WINDOW_HEIGHT);
  gtk_box_pack_start (GTK_BOX (list_hbox), scrolledwin, TRUE, TRUE, 0);
  gtk_scrolled_window_set_policy (GTK_SCROLLED_WINDOW (scrolledwin), GTK_POLICY_AUTOMATIC, GTK_POLICY_AUTOMATIC);

  store = gtk_list_store_new (2, G_TYPE_STRING, G_TYPE_POINTER);
  shown_list = gtk_tree_view_new_with_model (GTK_TREE_MODEL (store));
  gtk_tree_view_set_headers_visible (GTK_TREE_VIEW (shown_list), TRUE);
  gtk_tree_selection_set_mode (gtk_tree_view_get_selection (GTK_TREE_VIEW (shown_list)), GTK_SELECTION_BROWSE);
  gtk_container_add (GTK_CONTAINER (scrolledwin), shown_list);
  g_object_unref (store);

  renderer = gtk_cell_renderer_text_new ();
  col = gtk_tree_view_column_new_with_attributes (_("Displayed items"), renderer, "text", 0, NULL);
  gtk_tree_view_column_set_expand (col, TRUE);
  gtk_tree_view_append_column (GTK_TREE_VIEW (shown_list), col);

  /* up/down button */
  btn_vbox = gtk_box_new (GTK_ORIENTATION_VERTICAL, 0);
  gtk_box_pack_start (GTK_BOX (hbox1), btn_vbox, FALSE, FALSE, 0);

  btn_vbox1 = gtk_box_new (GTK_ORIENTATION_VERTICAL, 8);
  gtk_box_pack_start (GTK_BOX (btn_vbox), btn_vbox1, TRUE, FALSE, 0);

  up_btn = gtk_button_new_with_label (_("Up"));
  gtk_box_pack_start (GTK_BOX (btn_vbox1), up_btn, FALSE, FALSE, 0);

  down_btn = gtk_button_new_with_label (_("Down"));
  gtk_box_pack_start (GTK_BOX (btn_vbox1), down_btn, FALSE, FALSE, 0);

  g_signal_connect (G_OBJECT (up_btn), "clicked", G_CALLBACK (prefs_display_items_up), dialog);
  g_signal_connect (G_OBJECT (down_btn), "clicked", G_CALLBACK (prefs_display_items_down), dialog);

  btn_hbox = gtk_box_new (GTK_ORIENTATION_HORIZONTAL, 8);
  gtk_box_pack_end (GTK_BOX (vbox), btn_hbox, FALSE, FALSE, 0);

  btn_vbox = gtk_box_new (GTK_ORIENTATION_VERTICAL, 0);
  gtk_box_pack_start (GTK_BOX (btn_hbox), btn_vbox, FALSE, FALSE, 0);

  default_btn = gtk_button_new_with_label (_(" Revert to default "));
  gtk_box_pack_start (GTK_BOX (btn_vbox), default_btn, TRUE, FALSE, 0);

  g_signal_connect (G_OBJECT (default_btn), "clicked", G_CALLBACK (prefs_display_items_default), dialog);

  yam_stock_button_set_create (&confirm_area, &ok_btn, "yam-ok", &cancel_btn, "yam-cancel", NULL, NULL);
  gtk_box_pack_end (GTK_BOX (btn_hbox), confirm_area, FALSE, FALSE, 0);
  gtk_widget_grab_default (ok_btn);

  g_signal_connect (G_OBJECT (ok_btn), "clicked", G_CALLBACK (prefs_display_items_ok), dialog);
  g_signal_connect (G_OBJECT (cancel_btn), "clicked", G_CALLBACK (prefs_display_items_cancel), dialog);

  gtk_widget_show_all (vbox);

  dialog->window = window;
  dialog->label = label;
  dialog->stock_list = stock_list;
  dialog->shown_list = shown_list;
  dialog->add_btn = add_btn;
  dialog->remove_btn = remove_btn;
  dialog->up_btn = up_btn;
  dialog->down_btn = down_btn;
  dialog->confirm_area = confirm_area;
  dialog->ok_btn = ok_btn;
  dialog->cancel_btn = cancel_btn;

  manage_window_set_transient (GTK_WINDOW (dialog->window));
  gtk_widget_grab_focus (dialog->ok_btn);

  dialog->finished = FALSE;
  dialog->cancelled = FALSE;

  return dialog;
}

void
prefs_display_items_dialog_destroy (PrefsDisplayItemsDialog * dialog)
{
  if (!dialog)
    return;

  if (dialog->available_items)
    g_list_free (dialog->available_items);
  if (dialog->visible_items)
    g_list_free (dialog->visible_items);
  gtk_widget_destroy (dialog->window);
  g_free (dialog);
}

static void
prefs_display_items_update_available (PrefsDisplayItemsDialog * dialog)
{
  GtkListStore *store;
  GtkTreeIter iter;
  GList *cur;

  g_return_if_fail (dialog->available_items != NULL);

  store = GTK_LIST_STORE (gtk_tree_view_get_model (GTK_TREE_VIEW (dialog->stock_list)));

  gtk_list_store_clear (store);

  for (cur = dialog->available_items; cur != NULL; cur = cur->next)
    {
      PrefsDisplayItem *item = cur->data;

      if (item->allow_multiple || item->in_use == FALSE)
        {
          gtk_list_store_append (store, &iter);
          gtk_list_store_set (store, &iter, 0, gettext (item->label), 1, item, -1);
        }
    }
}

static PrefsDisplayItem *
prefs_display_items_get_item_from_id (PrefsDisplayItemsDialog * dialog, gint id)
{
  gint i;

  for (i = 0; dialog->all_items[i].id != -1; i++)
    {
      if (id == dialog->all_items[i].id)
        return (PrefsDisplayItem *) & dialog->all_items[i];
    }

  return NULL;
}

void
prefs_display_items_dialog_set_available (PrefsDisplayItemsDialog * dialog,
                                          PrefsDisplayItem * all_items, const gint * ids)
{
  gint i;
  GList *list = NULL;

  dialog->all_items = all_items;
  for (i = 0; ids[i] != -1; i++)
    {
      PrefsDisplayItem *item = prefs_display_items_get_item_from_id (dialog, ids[i]);
      if (item)
        list = g_list_append (list, item);
    }
  dialog->available_items = list;
  prefs_display_items_update_available (dialog);
}

void
prefs_display_items_dialog_set_default_visible (PrefsDisplayItemsDialog * dialog, const gint * ids)
{
  dialog->default_visible_ids = ids;
}

void
prefs_display_items_dialog_set_visible (PrefsDisplayItemsDialog * dialog, const gint * ids)
{
  GtkListStore *store;
  GtkTreeIter iter;
  GList *cur;
  PrefsDisplayItem *item;
  gint i;

  g_return_if_fail (dialog->available_items != NULL);

  if (!ids)
    ids = dialog->default_visible_ids;
  g_return_if_fail (ids != NULL);

  store = GTK_LIST_STORE (gtk_tree_view_get_model (GTK_TREE_VIEW (dialog->shown_list)));

  gtk_list_store_clear (store);

  if (dialog->visible_items)
    {
      g_list_free (dialog->visible_items);
      dialog->visible_items = NULL;
    }

  for (cur = dialog->available_items; cur != NULL; cur = cur->next)
    {
      item = cur->data;
      item->in_use = FALSE;
    }

  for (i = 0; ids[i] != -1; i++)
    {
      gint id = ids[i];

      item = prefs_display_items_get_item_from_id (dialog, id);

      g_return_if_fail (item != NULL);
      g_return_if_fail (item->allow_multiple || item->in_use == FALSE);

      item->in_use = TRUE;

      gtk_list_store_append (store, &iter);
      gtk_list_store_set (store, &iter, 0, gettext (item->label), 1, item, -1);
    }

  prefs_display_items_update_available (dialog);
}

static void
prefs_display_items_add (GtkWidget * widget, gpointer data)
{
  PrefsDisplayItemsDialog *dialog = (PrefsDisplayItemsDialog *) data;
  GtkTreeSelection *sel;
  GtkTreeModel *model;
  GtkTreeIter iter, pos;
  PrefsDisplayItem *item;
  gchar *name;

  sel = gtk_tree_view_get_selection (GTK_TREE_VIEW (dialog->stock_list));

  if (!gtk_tree_selection_get_selected (sel, &model, &iter))
    return;

  gtk_tree_model_get (model, &iter, 0, &name, 1, &item, -1);
  if (!item->allow_multiple)
    gtk_list_store_remove (GTK_LIST_STORE (model), &iter);

  sel = gtk_tree_view_get_selection (GTK_TREE_VIEW (dialog->shown_list));
  model = gtk_tree_view_get_model (GTK_TREE_VIEW (dialog->shown_list));

  if (gtk_tree_selection_get_selected (sel, NULL, &pos))
    gtk_list_store_insert_after (GTK_LIST_STORE (model), &iter, &pos);
  else
    gtk_list_store_append (GTK_LIST_STORE (model), &iter);

  item->in_use = TRUE;
  gtk_list_store_set (GTK_LIST_STORE (model), &iter, 0, name, 1, item, -1);
}

static void
prefs_display_items_remove (GtkWidget * widget, gpointer data)
{
  PrefsDisplayItemsDialog *dialog = (PrefsDisplayItemsDialog *) data;
  GtkTreeSelection *sel;
  GtkTreeModel *model;
  GtkTreeIter iter;
  PrefsDisplayItem *item;

  sel = gtk_tree_view_get_selection (GTK_TREE_VIEW (dialog->shown_list));

  if (!gtk_tree_selection_get_selected (sel, &model, &iter))
    return;

  gtk_tree_model_get (model, &iter, 1, &item, -1);
  gtk_list_store_remove (GTK_LIST_STORE (model), &iter);

  if (!item->allow_multiple)
    {
      item->in_use = FALSE;
      prefs_display_items_update_available (dialog);
    }
}

static void
prefs_display_items_up (GtkWidget * widget, gpointer data)
{
  PrefsDisplayItemsDialog *dialog = (PrefsDisplayItemsDialog *) data;
  GtkTreeSelection *sel;
  GtkTreeModel *model;
  GtkTreeIter iter, pos;

  sel = gtk_tree_view_get_selection (GTK_TREE_VIEW (dialog->shown_list));

  if (!gtk_tree_selection_get_selected (sel, &model, &iter))
    return;

  pos = iter;
  if (gtk_tree_model_iter_previous (model, &pos))
    gtk_list_store_move_before (GTK_LIST_STORE (model), &iter, &pos);
}

static void
prefs_display_items_down (GtkWidget * widget, gpointer data)
{
  PrefsDisplayItemsDialog *dialog = (PrefsDisplayItemsDialog *) data;
  GtkTreeSelection *sel;
  GtkTreeModel *model;
  GtkTreeIter iter, pos;

  sel = gtk_tree_view_get_selection (GTK_TREE_VIEW (dialog->shown_list));

  if (!gtk_tree_selection_get_selected (sel, &model, &iter))
    return;

  pos = iter;
  if (gtk_tree_model_iter_next (model, &pos))
    gtk_list_store_move_after (GTK_LIST_STORE (model), &iter, &pos);
}

static void
prefs_display_items_default (GtkWidget * widget, gpointer data)
{
  PrefsDisplayItemsDialog *dialog = (PrefsDisplayItemsDialog *) data;

  prefs_display_items_dialog_set_visible (dialog, NULL);
}

static void
prefs_display_items_ok (GtkWidget * widget, gpointer data)
{
  PrefsDisplayItemsDialog *dialog = (PrefsDisplayItemsDialog *) data;
  GtkTreeModel *model;
  GtkTreeIter iter;
  GList *list = NULL;
  PrefsDisplayItem *item;

  model = gtk_tree_view_get_model (GTK_TREE_VIEW (dialog->shown_list));
  if (gtk_tree_model_get_iter_first (model, &iter))
    {
      do
        {
          gtk_tree_model_get (model, &iter, 1, &item, -1);
          if (item)
            list = g_list_append (list, item);
        }
      while (gtk_tree_model_iter_next (model, &iter));
    }

  dialog->visible_items = list;
  dialog->finished = TRUE;
}

static void
prefs_display_items_cancel (GtkWidget * widget, gpointer data)
{
  PrefsDisplayItemsDialog *dialog = (PrefsDisplayItemsDialog *) data;

  dialog->finished = TRUE;
  dialog->cancelled = TRUE;
}

static gint
prefs_display_items_delete_event (GtkWidget * widget, GdkEventAny * event, gpointer data)
{
  PrefsDisplayItemsDialog *dialog = (PrefsDisplayItemsDialog *) data;

  dialog->finished = TRUE;
  dialog->cancelled = TRUE;
  return TRUE;
}

static gboolean
prefs_display_items_key_pressed (GtkWidget * widget, GdkEventKey * event, gpointer data)
{
  PrefsDisplayItemsDialog *dialog = (PrefsDisplayItemsDialog *) data;

  if (event && event->keyval == GDK_KEY_Escape)
    {
      dialog->finished = TRUE;
      dialog->cancelled = TRUE;
    }
  return FALSE;
}
