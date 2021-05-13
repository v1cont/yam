/*
 * YAM -- a GTK+ based, lightweight, and fast e-mail client
 * Copyright (C) 2020 Victor Ananjevsky <victor@sanana.kiev.ua>
 * Copyright (C) 1999-2007 Hiroyuki Yamamoto
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
#include "prefs_summary_column.h"
#include "manage_window.h"
#include "summaryview.h"
#include "mainwindow.h"
#include "inc.h"
#include "gtkutils.h"
#include "utils.h"

static struct _SummaryColumnDialog {
  GtkWidget *window;

  GtkWidget *stock_list;
  GtkWidget *shown_list;

  GtkWidget *add_btn;
  GtkWidget *remove_btn;
  GtkWidget *up_btn;
  GtkWidget *down_btn;

  GtkWidget *default_btn;

  GtkWidget *confirm_area;
  GtkWidget *ok_btn;
  GtkWidget *cancel_btn;

  gboolean sent_folder;

  gboolean finished;
} summary_col;

static const gchar *const col_name[N_SUMMARY_VISIBLE_COLS] = {
  N_("Mark"),                   /* S_COL_MARK    */
  N_("Unread"),                 /* S_COL_UNREAD  */
  N_("Attachment"),             /* S_COL_MIME    */
  N_("Subject"),                /* S_COL_SUBJECT */
  N_("From"),                   /* S_COL_FROM    */
  N_("Date"),                   /* S_COL_DATE    */
  N_("Size"),                   /* S_COL_SIZE    */
  N_("Number"),                 /* S_COL_NUMBER  */
  N_("To")                      /* S_COL_TO      */
};

static SummaryColumnState default_state[N_SUMMARY_VISIBLE_COLS] = {
  {S_COL_MARK, TRUE},
  {S_COL_UNREAD, TRUE},
  {S_COL_MIME, TRUE},
  {S_COL_SUBJECT, TRUE},
  {S_COL_FROM, TRUE},
  {S_COL_DATE, TRUE},
  {S_COL_SIZE, TRUE},
  {S_COL_NUMBER, FALSE},
  {S_COL_TO, FALSE}
};

static SummaryColumnState default_sent_state[N_SUMMARY_VISIBLE_COLS] = {
  {S_COL_MARK, TRUE},
  {S_COL_UNREAD, TRUE},
  {S_COL_MIME, TRUE},
  {S_COL_SUBJECT, TRUE},
  {S_COL_TO, TRUE},
  {S_COL_DATE, TRUE},
  {S_COL_SIZE, TRUE},
  {S_COL_NUMBER, FALSE},
  {S_COL_FROM, FALSE}
};

static void prefs_summary_column_create (void);

static void prefs_summary_column_set_dialog (SummaryColumnState * state);
static void prefs_summary_column_set_view (void);

/* callback functions */
static void prefs_summary_column_add (void);
static void prefs_summary_column_remove (void);

static void prefs_summary_column_up (void);
static void prefs_summary_column_down (void);

static void prefs_summary_column_set_to_default (void);

static void prefs_summary_column_ok (void);
static void prefs_summary_column_cancel (void);

static gint prefs_summary_column_delete_event (GtkWidget * widget, GdkEventAny * event, gpointer data);
static gboolean prefs_summary_column_key_pressed (GtkWidget * widget, GdkEventKey * event, gpointer data);

void
prefs_summary_column_open (gboolean sent_folder)
{
  inc_lock ();

  prefs_summary_column_create ();
  summary_col.sent_folder = sent_folder;

  manage_window_set_transient (GTK_WINDOW (summary_col.window));
  gtk_widget_grab_focus (summary_col.ok_btn);

  prefs_summary_column_set_dialog (NULL);

  gtk_widget_show (summary_col.window);

  summary_col.finished = FALSE;
  while (summary_col.finished == FALSE)
    gtk_main_iteration ();

  gtk_widget_destroy (summary_col.window);
  summary_col.window = NULL;
  main_window_popup (main_window_get ());

  inc_unlock ();
}

static void
prefs_summary_column_create (void)
{
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

  debug_print ("Creating summary column setting window...\n");

  window = gtk_window_new (GTK_WINDOW_TOPLEVEL);
  gtk_container_set_border_width (GTK_CONTAINER (window), 8);
  gtk_window_set_position (GTK_WINDOW (window), GTK_WIN_POS_CENTER_ON_PARENT);
  gtk_window_set_modal (GTK_WINDOW (window), TRUE);
  gtk_window_set_resizable (GTK_WINDOW (window), FALSE);
  gtk_window_set_title (GTK_WINDOW (window), _("Summary display item setting"));
  g_signal_connect (G_OBJECT (window), "delete_event", G_CALLBACK (prefs_summary_column_delete_event), NULL);
  g_signal_connect (G_OBJECT (window), "key_press_event", G_CALLBACK (prefs_summary_column_key_pressed), NULL);

  vbox = gtk_box_new (GTK_ORIENTATION_VERTICAL, 6);
  gtk_container_add (GTK_CONTAINER (window), vbox);

  label_hbox = gtk_box_new (GTK_ORIENTATION_HORIZONTAL, 0);
  gtk_box_pack_start (GTK_BOX (vbox), label_hbox, FALSE, FALSE, 4);

  label = gtk_label_new (_("Select items to be displayed on the summary view. You can modify\n"
                           "the order by using the Up / Down button."));
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
  gtk_widget_set_size_request (scrolledwin, 180, 210);
  gtk_box_pack_start (GTK_BOX (list_hbox), scrolledwin, TRUE, TRUE, 0);
  gtk_scrolled_window_set_policy (GTK_SCROLLED_WINDOW (scrolledwin), GTK_POLICY_AUTOMATIC, GTK_POLICY_AUTOMATIC);

  store = gtk_list_store_new (2, G_TYPE_STRING, G_TYPE_INT);
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
  gtk_box_pack_start (GTK_BOX (btn_vbox1), add_btn, FALSE, FALSE, 0);

  remove_btn = gtk_button_new_with_label ("  <-  ");
  gtk_box_pack_start (GTK_BOX (btn_vbox1), remove_btn, FALSE, FALSE, 0);

  g_signal_connect (G_OBJECT (add_btn), "clicked", G_CALLBACK (prefs_summary_column_add), NULL);
  g_signal_connect (G_OBJECT (remove_btn), "clicked", G_CALLBACK (prefs_summary_column_remove), NULL);

  list_hbox = gtk_box_new (GTK_ORIENTATION_HORIZONTAL, 8);
  gtk_box_pack_start (GTK_BOX (hbox1), list_hbox, TRUE, TRUE, 0);

  scrolledwin = gtk_scrolled_window_new (NULL, NULL);
  gtk_widget_set_size_request (scrolledwin, 180, 210);
  gtk_box_pack_start (GTK_BOX (list_hbox), scrolledwin, TRUE, TRUE, 0);
  gtk_scrolled_window_set_policy (GTK_SCROLLED_WINDOW (scrolledwin), GTK_POLICY_AUTOMATIC, GTK_POLICY_AUTOMATIC);

  store = gtk_list_store_new (2, G_TYPE_STRING, G_TYPE_INT);
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

  g_signal_connect (G_OBJECT (up_btn), "clicked", G_CALLBACK (prefs_summary_column_up), NULL);
  g_signal_connect (G_OBJECT (down_btn), "clicked", G_CALLBACK (prefs_summary_column_down), NULL);

  btn_hbox = gtk_box_new (GTK_ORIENTATION_HORIZONTAL, 8);
  gtk_box_pack_end (GTK_BOX (vbox), btn_hbox, FALSE, FALSE, 0);

  btn_vbox = gtk_box_new (GTK_ORIENTATION_VERTICAL, 0);
  gtk_box_pack_start (GTK_BOX (btn_hbox), btn_vbox, FALSE, FALSE, 0);

  default_btn = gtk_button_new_with_label (_(" Revert to default "));
  gtk_box_pack_start (GTK_BOX (btn_vbox), default_btn, TRUE, FALSE, 0);
  g_signal_connect (G_OBJECT (default_btn), "clicked", G_CALLBACK (prefs_summary_column_set_to_default), NULL);

  yam_stock_button_set_create (&confirm_area, &ok_btn, "yam-ok", &cancel_btn, "yam-cancel", NULL, NULL);
  gtk_box_pack_end (GTK_BOX (btn_hbox), confirm_area, FALSE, FALSE, 0);
  gtk_widget_grab_default (ok_btn);

  g_signal_connect (G_OBJECT (ok_btn), "clicked", G_CALLBACK (prefs_summary_column_ok), NULL);
  g_signal_connect (G_OBJECT (cancel_btn), "clicked", G_CALLBACK (prefs_summary_column_cancel), NULL);

  gtk_widget_show_all (vbox);

  summary_col.window = window;
  summary_col.stock_list = stock_list;
  summary_col.shown_list = shown_list;
  summary_col.add_btn = add_btn;
  summary_col.remove_btn = remove_btn;
  summary_col.up_btn = up_btn;
  summary_col.down_btn = down_btn;
  summary_col.confirm_area = confirm_area;
  summary_col.ok_btn = ok_btn;
  summary_col.cancel_btn = cancel_btn;
}

SummaryColumnState *
prefs_summary_column_get_config (gboolean sent_folder)
{
  static SummaryColumnState state[N_SUMMARY_VISIBLE_COLS];
  SummaryColumnType type;
  gboolean *col_visible;
  gint *col_pos;
  SummaryColumnState *def_state;
  gint pos;

  debug_print ("prefs_summary_column_get_config(): getting %s folder setting\n", sent_folder ? "sent" : "normal");

  if (sent_folder)
    {
      col_visible = prefs_common.summary_sent_col_visible;
      col_pos = prefs_common.summary_sent_col_pos;
      def_state = default_sent_state;
    }
  else
    {
      col_visible = prefs_common.summary_col_visible;
      col_pos = prefs_common.summary_col_pos;
      def_state = default_state;
    }

  for (pos = 0; pos < N_SUMMARY_VISIBLE_COLS; pos++)
    state[pos].type = -1;

  for (type = 0; type < N_SUMMARY_VISIBLE_COLS; type++)
    {
      pos = col_pos[type];
      if (pos < 0 || pos >= N_SUMMARY_VISIBLE_COLS || state[pos].type != -1)
        {
          g_warning ("Wrong column position\n");
          prefs_summary_column_set_config (def_state, sent_folder);
          return def_state;
        }

      state[pos].type = type;
      state[pos].visible = col_visible[type];
    }

  return state;
}

void
prefs_summary_column_set_config (SummaryColumnState * state, gboolean sent_folder)
{
  SummaryColumnType type;
  gboolean *col_visible;
  gint *col_pos;
  gint pos;

  if (sent_folder)
    {
      col_visible = prefs_common.summary_sent_col_visible;
      col_pos = prefs_common.summary_sent_col_pos;
    }
  else
    {
      col_visible = prefs_common.summary_col_visible;
      col_pos = prefs_common.summary_col_pos;
    }

  for (pos = 0; pos < N_SUMMARY_VISIBLE_COLS; pos++)
    {
      type = state[pos].type;
      col_visible[type] = state[pos].visible;
      col_pos[type] = pos;
    }
}

static void
prefs_summary_column_set_dialog (SummaryColumnState * state)
{
  GtkListStore *stock;
  GtkListStore *shown;
  gint pos;
  SummaryColumnType type;
  gchar *name;

  stock = GTK_LIST_STORE (gtk_tree_view_get_model (GTK_TREE_VIEW (summary_col.stock_list)));
  shown = GTK_LIST_STORE (gtk_tree_view_get_model (GTK_TREE_VIEW (summary_col.shown_list)));

  gtk_list_store_clear (stock);
  gtk_list_store_clear (shown);

  if (!state)
    state = prefs_summary_column_get_config (summary_col.sent_folder);

  for (pos = 0; pos < N_SUMMARY_VISIBLE_COLS; pos++)
    {
      GtkTreeIter iter;

      type = state[pos].type;
      name = gettext (col_name[type]);

      if (state[pos].visible)
        {
          gtk_list_store_append (shown, &iter);
          gtk_list_store_set (shown, &iter, 0, name, 1, type, -1);
        }
      else
        {
          gtk_list_store_append (stock, &iter);
          gtk_list_store_set (stock, &iter, 0, name, 1, type, -1);
        }
    }
}

static void
prefs_summary_column_set_view (void)
{
  GtkTreeModel *model;
  GtkTreeIter iter;
  SummaryColumnState state[N_SUMMARY_VISIBLE_COLS];
  SummaryColumnType type;
  gint pos = 0;

  model = gtk_tree_view_get_model (GTK_TREE_VIEW (summary_col.stock_list));
  if (gtk_tree_model_get_iter_first (model, &iter))
    {
      do
        {
          gtk_tree_model_get (model, &iter, 1, &type, -1);
          state[pos].type = type;
          state[pos].visible = FALSE;
          pos++;
        }
      while (gtk_tree_model_iter_next (model, &iter));
    }

  model = gtk_tree_view_get_model (GTK_TREE_VIEW (summary_col.shown_list));
  if (gtk_tree_model_get_iter_first (model, &iter))
    {
      do
        {
          gtk_tree_model_get (model, &iter, 1, &type, -1);
          state[pos].type = type;
          state[pos].visible = TRUE;
          pos++;
        }
      while (gtk_tree_model_iter_next (model, &iter));
    }

  prefs_summary_column_set_config (state, summary_col.sent_folder);
  main_window_set_summary_column ();
}

static void
prefs_summary_column_add (void)
{
  GtkTreeSelection *sel;
  GtkTreeModel *model;
  GtkTreeIter iter, pos;
  SummaryColumnType type;
  gchar *name;

  sel = gtk_tree_view_get_selection (GTK_TREE_VIEW (summary_col.stock_list));

  if (!gtk_tree_selection_get_selected (sel, &model, &iter))
    return;

  gtk_tree_model_get (model, &iter, 0, &name, 1, &type, -1);
  gtk_list_store_remove (GTK_LIST_STORE (model), &iter);

  sel = gtk_tree_view_get_selection (GTK_TREE_VIEW (summary_col.shown_list));
  model = gtk_tree_view_get_model (GTK_TREE_VIEW (summary_col.shown_list));

  if (gtk_tree_selection_get_selected (sel, NULL, &pos))
    gtk_list_store_insert_after (GTK_LIST_STORE (model), &iter, &pos);
  else
    gtk_list_store_append (GTK_LIST_STORE (model), &iter);

  gtk_list_store_set (GTK_LIST_STORE (model), &iter, 0, name, 1, type, -1);
}

static void
prefs_summary_column_remove (void)
{
  GtkTreeSelection *sel;
  GtkTreeModel *model;
  GtkTreeIter iter, pos;
  SummaryColumnType type;
  gchar *name;

  sel = gtk_tree_view_get_selection (GTK_TREE_VIEW (summary_col.shown_list));

  if (!gtk_tree_selection_get_selected (sel, &model, &iter))
    return;

  gtk_tree_model_get (model, &iter, 0, &name, 1, &type, -1);
  gtk_list_store_remove (GTK_LIST_STORE (model), &iter);

  sel = gtk_tree_view_get_selection (GTK_TREE_VIEW (summary_col.stock_list));
  model = gtk_tree_view_get_model (GTK_TREE_VIEW (summary_col.stock_list));

  if (gtk_tree_selection_get_selected (sel, NULL, &pos))
    gtk_list_store_insert_after (GTK_LIST_STORE (model), &iter, &pos);
  else
    gtk_list_store_append (GTK_LIST_STORE (model), &iter);

  gtk_list_store_set (GTK_LIST_STORE (model), &iter, 0, name, 1, type, -1);
}

static void
prefs_summary_column_up (void)
{
  GtkTreeSelection *sel;
  GtkTreeModel *model;
  GtkTreeIter iter, pos;

  sel = gtk_tree_view_get_selection (GTK_TREE_VIEW (summary_col.shown_list));

  if (!gtk_tree_selection_get_selected (sel, &model, &iter))
    return;

  pos = iter;
  if (gtk_tree_model_iter_previous (model, &pos))
    gtk_list_store_move_before (GTK_LIST_STORE (model), &iter, &pos);
}

static void
prefs_summary_column_down (void)
{
  GtkTreeSelection *sel;
  GtkTreeModel *model;
  GtkTreeIter iter, pos;

  sel = gtk_tree_view_get_selection (GTK_TREE_VIEW (summary_col.shown_list));

  if (!gtk_tree_selection_get_selected (sel, &model, &iter))
    return;

  pos = iter;
  if (gtk_tree_model_iter_next (model, &pos))
    gtk_list_store_move_after (GTK_LIST_STORE (model), &iter, &pos);
}

static void
prefs_summary_column_set_to_default (void)
{
  if (summary_col.sent_folder)
    prefs_summary_column_set_dialog (default_sent_state);
  else
    prefs_summary_column_set_dialog (default_state);
}

static void
prefs_summary_column_ok (void)
{
  if (!summary_col.finished)
    {
      summary_col.finished = TRUE;
      prefs_summary_column_set_view ();
    }
}

static void
prefs_summary_column_cancel (void)
{
  summary_col.finished = TRUE;
}

static gint
prefs_summary_column_delete_event (GtkWidget * widget, GdkEventAny * event, gpointer data)
{
  summary_col.finished = TRUE;
  return TRUE;
}

static gboolean
prefs_summary_column_key_pressed (GtkWidget * widget, GdkEventKey * event, gpointer data)
{
  if (event && event->keyval == GDK_KEY_Escape)
    summary_col.finished = TRUE;
  return FALSE;
}
