/*
 * YAM -- a GTK+ based, lightweight, and fast e-mail client
 * Copyright (C) 2020 Victor Ananjevsky <victor@sanana.kiev.ua>
 * Copyright (C) 1999-2005 Hiroyuki Yamamoto
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

#include "main.h"
#include "prefs.h"
#include "prefs_ui.h"
#include "prefs_customheader.h"
#include "prefs_common.h"
#include "prefs_account.h"
#include "mainwindow.h"
#include "foldersel.h"
#include "manage_window.h"
#include "customheader.h"
#include "folder.h"
#include "utils.h"
#include "gtkutils.h"
#include "alertpanel.h"

static struct CustomHdr {
  GtkWidget *window;

  GtkWidget *ok_btn;
  GtkWidget *cancel_btn;

  GtkWidget *hdr_entry;
  GtkWidget *val_entry;
  GtkWidget *customhdr_list;
} customhdr;

/* widget creating functions */
static void prefs_custom_header_create (void);

static void prefs_custom_header_set_dialog (PrefsAccount * ac);
static void prefs_custom_header_set_list (PrefsAccount * ac);

/* callback functions */
static void prefs_custom_header_add_cb (void);
static void prefs_custom_header_delete_cb (void);
static void prefs_custom_header_up (void);
static void prefs_custom_header_down (void);
static void prefs_custom_header_select (GtkTreeView *list, GtkTreePath *path, GtkTreeViewColumn *col, gpointer data);

static gboolean prefs_custom_header_key_pressed (GtkWidget * widget, GdkEventKey * event, gpointer data);
static void prefs_custom_header_ok (void);
static void prefs_custom_header_cancel (void);
static gint prefs_custom_header_deleted (GtkWidget * widget, GdkEventAny * event, gpointer data);

static PrefsAccount *cur_ac = NULL;

void
prefs_custom_header_open (PrefsAccount * ac)
{
  if (!customhdr.window)
    prefs_custom_header_create ();

  manage_window_set_transient (GTK_WINDOW (customhdr.window));
  gtk_widget_grab_focus (customhdr.ok_btn);

  prefs_custom_header_set_dialog (ac);

  cur_ac = ac;

  gtk_widget_show (customhdr.window);
}

static void
prefs_custom_header_create (void)
{
  GtkWidget *window;
  GtkWidget *vbox;

  GtkWidget *ok_btn;
  GtkWidget *cancel_btn;

  GtkWidget *confirm_area;

  GtkWidget *table;
  GtkWidget *hdr_label;
  GtkWidget *hdr_combo;
  GtkWidget *val_label;
  GtkWidget *val_entry;

  GtkWidget *reg_hbox;
  GtkWidget *btn_hbox;
  GtkWidget *arrow;
  GtkWidget *add_btn;
  GtkWidget *del_btn;

  GtkWidget *ch_hbox;
  GtkWidget *ch_scrolledwin;
  GtkWidget *customhdr_list;

  GtkListStore *store;
  GtkTreeViewColumn *col;
  GtkCellRenderer *renderer;

  GtkWidget *btn_vbox;
  GtkWidget *up_btn;
  GtkWidget *down_btn;

  debug_print ("Creating custom header setting window...\n");

  window = gtk_window_new (GTK_WINDOW_TOPLEVEL);
  gtk_container_set_border_width (GTK_CONTAINER (window), 8);
  gtk_window_set_position (GTK_WINDOW (window), GTK_WIN_POS_CENTER);
  gtk_window_set_modal (GTK_WINDOW (window), TRUE);

  vbox = gtk_box_new (GTK_ORIENTATION_VERTICAL, 6);
  gtk_container_add (GTK_CONTAINER (window), vbox);

  yam_stock_button_set_create (&confirm_area, &ok_btn, "yam-ok", &cancel_btn, "yam-cancel", NULL, NULL);
  gtk_box_pack_end (GTK_BOX (vbox), confirm_area, FALSE, FALSE, 0);
  gtk_widget_grab_default (ok_btn);

  gtk_window_set_title (GTK_WINDOW (window), _("Custom header setting"));
  MANAGE_WINDOW_SIGNALS_CONNECT (window);
  g_signal_connect (G_OBJECT (window), "delete_event", G_CALLBACK (prefs_custom_header_deleted), NULL);
  g_signal_connect (G_OBJECT (window), "key_press_event", G_CALLBACK (prefs_custom_header_key_pressed), NULL);
  g_signal_connect (G_OBJECT (ok_btn), "clicked", G_CALLBACK (prefs_custom_header_ok), NULL);
  g_signal_connect (G_OBJECT (cancel_btn), "clicked", G_CALLBACK (prefs_custom_header_cancel), NULL);

  table = gtk_grid_new ();
  gtk_box_pack_start (GTK_BOX (vbox), table, FALSE, FALSE, 0);
  gtk_grid_set_row_spacing (GTK_GRID (table), 8);
  gtk_grid_set_column_spacing (GTK_GRID (table), 8);

  hdr_label = gtk_label_new (_("Header"));
  gtk_grid_attach (GTK_GRID (table), hdr_label, 0, 0, 1, 1);
  gtk_label_set_xalign (GTK_LABEL (hdr_label), 0.0);

  hdr_combo = gtk_combo_box_text_new_with_entry ();
  gtk_grid_attach (GTK_GRID (table), hdr_combo, 0, 1, 1, 1);
  gtk_widget_set_size_request (hdr_combo, 150, -1);
  gtk_combo_box_text_append_text (GTK_COMBO_BOX_TEXT (hdr_combo), "User-Agent");
  gtk_combo_box_text_append_text (GTK_COMBO_BOX_TEXT (hdr_combo), "X-Face");
  gtk_combo_box_text_append_text (GTK_COMBO_BOX_TEXT (hdr_combo), "X-Operating-System");

  val_label = gtk_label_new (_("Value"));
  gtk_grid_attach (GTK_GRID (table), val_label, 1, 0, 1, 1);
  gtk_label_set_xalign (GTK_LABEL (val_label), 0.0);

  val_entry = gtk_entry_new ();
  gtk_grid_attach (GTK_GRID (table), val_entry, 1, 1, 1, 1);
  gtk_widget_set_size_request (val_entry, 200, -1);

  /* add / delete */

  reg_hbox = gtk_box_new (GTK_ORIENTATION_HORIZONTAL, 4);
  gtk_box_pack_start (GTK_BOX (vbox), reg_hbox, FALSE, FALSE, 0);

  arrow = yam_arrow_new (ARROW_DOWN);
  gtk_box_pack_start (GTK_BOX (reg_hbox), arrow, FALSE, FALSE, 0);
  //gtk_widget_set_size_request (arrow, -1, 16);

  btn_hbox = gtk_box_new (GTK_ORIENTATION_HORIZONTAL, 4);
  gtk_box_pack_start (GTK_BOX (reg_hbox), btn_hbox, FALSE, FALSE, 0);

  add_btn = gtk_button_new_with_label (_("Add"));
  gtk_box_pack_start (GTK_BOX (btn_hbox), add_btn, FALSE, TRUE, 0);
  g_signal_connect (G_OBJECT (add_btn), "clicked", G_CALLBACK (prefs_custom_header_add_cb), NULL);

  del_btn = gtk_button_new_with_label (_(" Delete "));
  gtk_box_pack_start (GTK_BOX (btn_hbox), del_btn, FALSE, TRUE, 0);
  g_signal_connect (G_OBJECT (del_btn), "clicked", G_CALLBACK (prefs_custom_header_delete_cb), NULL);

  ch_hbox = gtk_box_new (GTK_ORIENTATION_HORIZONTAL, 8);
  gtk_box_pack_start (GTK_BOX (vbox), ch_hbox, TRUE, TRUE, 0);

  ch_scrolledwin = gtk_scrolled_window_new (NULL, NULL);
  gtk_widget_set_size_request (ch_scrolledwin, -1, 200);
  gtk_box_pack_start (GTK_BOX (ch_hbox), ch_scrolledwin, TRUE, TRUE, 0);
  gtk_scrolled_window_set_policy (GTK_SCROLLED_WINDOW (ch_scrolledwin), GTK_POLICY_AUTOMATIC, GTK_POLICY_AUTOMATIC);

  store = gtk_list_store_new (2, G_TYPE_STRING, G_TYPE_POINTER);
  customhdr_list = gtk_tree_view_new_with_model (GTK_TREE_MODEL (store));
  gtk_tree_view_set_headers_visible (GTK_TREE_VIEW (customhdr_list), TRUE);
  gtk_tree_view_set_activate_on_single_click (GTK_TREE_VIEW (customhdr_list), TRUE);
  gtk_tree_selection_set_mode (gtk_tree_view_get_selection (GTK_TREE_VIEW (customhdr_list)), GTK_SELECTION_BROWSE);
  gtk_container_add (GTK_CONTAINER (ch_scrolledwin), customhdr_list);
  g_object_unref (store);

  renderer = gtk_cell_renderer_text_new ();
  col = gtk_tree_view_column_new_with_attributes (_("Custom headers"), renderer, "text", 0, NULL);
  gtk_tree_view_column_set_expand (col, TRUE);
  gtk_tree_view_append_column (GTK_TREE_VIEW (customhdr_list), col);

  g_signal_connect (G_OBJECT (customhdr_list), "row-activated", G_CALLBACK (prefs_custom_header_select), NULL);

  btn_vbox = gtk_box_new (GTK_ORIENTATION_VERTICAL, 8);
  gtk_box_pack_start (GTK_BOX (ch_hbox), btn_vbox, FALSE, FALSE, 0);

  up_btn = gtk_button_new_with_label (_("Up"));
  gtk_box_pack_start (GTK_BOX (btn_vbox), up_btn, FALSE, FALSE, 0);
  g_signal_connect (G_OBJECT (up_btn), "clicked", G_CALLBACK (prefs_custom_header_up), NULL);

  down_btn = gtk_button_new_with_label (_("Down"));
  gtk_box_pack_start (GTK_BOX (btn_vbox), down_btn, FALSE, FALSE, 0);
  g_signal_connect (G_OBJECT (down_btn), "clicked", G_CALLBACK (prefs_custom_header_down), NULL);

  gtk_widget_show_all (window);

  customhdr.window = window;
  customhdr.ok_btn = ok_btn;
  customhdr.cancel_btn = cancel_btn;

  customhdr.hdr_entry = gtk_bin_get_child (GTK_BIN (hdr_combo));
  customhdr.val_entry = val_entry;

  customhdr.customhdr_list = customhdr_list;
}

static void
prefs_custom_header_set_dialog (PrefsAccount * ac)
{
  GtkListStore *store;
  GtkTreeIter iter;
  GSList *cur;
  gchar *ch_str;

  store = GTK_LIST_STORE (gtk_tree_view_get_model (GTK_TREE_VIEW (customhdr.customhdr_list)));

  gtk_widget_freeze_child_notify (customhdr.customhdr_list);

  gtk_list_store_clear (store);

  for (cur = ac->customhdr_list; cur != NULL; cur = cur->next)
    {
      CustomHeader *ch = (CustomHeader *) cur->data;

      ch_str = g_strdup_printf ("%s: %s", ch->name, ch->value ? ch->value : "");
      gtk_list_store_append (store, &iter);
      gtk_list_store_set (store, &iter, 0, ch_str, 1, ch, -1);
      g_free (ch_str);
    }

  gtk_widget_thaw_child_notify (customhdr.customhdr_list);
}

static void
prefs_custom_header_set_list (PrefsAccount * ac)
{
  GtkTreeModel *model;
  GtkTreeIter iter;
  CustomHeader *ch;

  g_slist_free (ac->customhdr_list);
  ac->customhdr_list = NULL;

  model = gtk_tree_view_get_model (GTK_TREE_VIEW (customhdr.customhdr_list));
  if (gtk_tree_model_get_iter_first (model, &iter))
    {
      do
        {
          gtk_tree_model_get (model, &iter, 1, &ch, -1);
          ac->customhdr_list = g_slist_append (ac->customhdr_list, ch);
        }
      while (gtk_tree_model_iter_next (model, &iter));
    }
}

static void
prefs_custom_header_add_cb (void)
{
  GtkListStore *store;
  GtkTreeIter iter;
  CustomHeader *ch;
  const gchar *entry_text;
  gchar *ch_str;

  entry_text = gtk_entry_get_text (GTK_ENTRY (customhdr.hdr_entry));
  if (entry_text[0] == '\0')
    {
      alertpanel_error (_("Header name is not set."));
      return;
    }

  ch = g_new0 (CustomHeader, 1);
  ch->account_id = cur_ac->account_id;
  ch->name = g_strdup (entry_text);

  unfold_line (ch->name);
  g_strstrip (ch->name);
  gtk_entry_set_text (GTK_ENTRY (customhdr.hdr_entry), ch->name);

  entry_text = gtk_entry_get_text (GTK_ENTRY (customhdr.val_entry));
  if (entry_text[0] != '\0')
    {
      ch->value = g_strdup (entry_text);
      unfold_line (ch->value);
      g_strstrip (ch->value);
      gtk_entry_set_text (GTK_ENTRY (customhdr.val_entry), ch->value);
    }

  ch_str = g_strdup_printf ("%s: %s", ch->name, ch->value ? ch->value : "");

  store = GTK_LIST_STORE (gtk_tree_view_get_model (GTK_TREE_VIEW (customhdr.customhdr_list)));
  gtk_list_store_append (store, &iter);
  gtk_list_store_set (store, &iter, 0, ch_str, 1, ch, -1);

  g_free (ch_str);

  prefs_custom_header_set_list (cur_ac);
}

static void
prefs_custom_header_delete_cb (void)
{
  GtkTreeSelection *sel;
  GtkTreeModel *model;
  GtkTreeIter iter;
  CustomHeader *ch;

  sel = gtk_tree_view_get_selection (GTK_TREE_VIEW (customhdr.customhdr_list));
  if (!gtk_tree_selection_get_selected (sel, &model, &iter))
    return;

  if (alertpanel (_("Delete header"), _("Do you really want to delete this header?"),
                  "yam-yes", "yam-no", NULL) != G_ALERTDEFAULT)
    return;

  gtk_tree_model_get (model, &iter, 1, &ch, -1);
  custom_header_free (ch);
  gtk_list_store_remove (GTK_LIST_STORE (model), &iter);
  cur_ac->customhdr_list = g_slist_remove (cur_ac->customhdr_list, ch);
}

static void
prefs_custom_header_up (void)
{
  GtkTreeSelection *sel;
  GtkTreeModel *model;
  GtkTreeIter iter, pos;

  sel = gtk_tree_view_get_selection (GTK_TREE_VIEW (customhdr.customhdr_list));

  if (!gtk_tree_selection_get_selected (sel, &model, &iter))
    return;

  pos = iter;
  if (gtk_tree_model_iter_previous (model, &pos))
    gtk_list_store_move_before (GTK_LIST_STORE (model), &iter, &pos);

  prefs_custom_header_set_list (cur_ac);
}

static void
prefs_custom_header_down (void)
{
  GtkTreeSelection *sel;
  GtkTreeModel *model;
  GtkTreeIter iter, pos;

  sel = gtk_tree_view_get_selection (GTK_TREE_VIEW (customhdr.customhdr_list));

  if (!gtk_tree_selection_get_selected (sel, &model, &iter))
    return;

  pos = iter;
  if (gtk_tree_model_iter_next (model, &pos))
    gtk_list_store_move_after (GTK_LIST_STORE (model), &iter, &pos);

  prefs_custom_header_set_list (cur_ac);
}

#define ENTRY_SET_TEXT(entry, str)                      \
  gtk_entry_set_text(GTK_ENTRY(entry), str ? str : "")

static void
prefs_custom_header_select (GtkTreeView *list, GtkTreePath *path, GtkTreeViewColumn *col, gpointer data)
{
  GtkTreeModel *model;
  GtkTreeIter iter;
  CustomHeader *ch;
  CustomHeader default_ch = { 0, "", NULL };

  model = gtk_tree_view_get_model (list);
  gtk_tree_model_get_iter (model, &iter, path);
  gtk_tree_model_get (model, &iter, 1, &ch, -1);

  if (!ch)
    ch = &default_ch;

  ENTRY_SET_TEXT (customhdr.hdr_entry, ch->name);
  ENTRY_SET_TEXT (customhdr.val_entry, ch->value);
}

#undef ENTRY_SET_TEXT

static gboolean
prefs_custom_header_key_pressed (GtkWidget * widget, GdkEventKey * event, gpointer data)
{
  if (event && event->keyval == GDK_KEY_Escape)
    prefs_custom_header_cancel ();
  return FALSE;
}

static void
prefs_custom_header_ok (void)
{
  custom_header_write_config (cur_ac);
  gtk_widget_hide (customhdr.window);
}

static void
prefs_custom_header_cancel (void)
{
  custom_header_read_config (cur_ac);
  gtk_widget_hide (customhdr.window);
}

static gint
prefs_custom_header_deleted (GtkWidget * widget, GdkEventAny * event, gpointer data)
{
  prefs_custom_header_cancel ();
  return TRUE;
}
