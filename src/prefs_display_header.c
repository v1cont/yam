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
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>

#include "prefs.h"
#include "prefs_ui.h"
#include "prefs_display_header.h"
#include "prefs_common.h"
#include "manage_window.h"
#include "alertpanel.h"
#include "displayheader.h"
#include "utils.h"
#include "gtkutils.h"

static struct DisplayHeader {
  GtkWidget *window;

  GtkWidget *confirm_area;
  GtkWidget *ok_btn;
  GtkWidget *cancel_btn;

  GtkWidget *hdr_combo;
  GtkWidget *hdr_entry;
  GtkWidget *key_check;
  GtkWidget *headers_list;
  GtkWidget *hidden_headers_list;

  GtkWidget *other_headers;
} dispheader;

/* widget creating functions */
static void prefs_display_header_create (void);

static void prefs_display_header_set_dialog (void);
static void prefs_display_header_set_list (void);
static void prefs_display_header_list_set_row (gboolean hidden);

/* callback functions */
static void prefs_display_header_register_cb (GtkButton * btn, gpointer data);
static void prefs_display_header_delete_cb (GtkButton * btn, gpointer data);
static void prefs_display_header_up (void);
static void prefs_display_header_down (void);

static gboolean prefs_display_header_key_pressed (GtkWidget * widget, GdkEventKey * event, gpointer data);
static void prefs_display_header_ok (void);
static void prefs_display_header_cancel (void);
static gint prefs_display_header_deleted (GtkWidget * widget, GdkEventAny * event, gpointer data);

static gchar *defaults[] = {
  "From",
  "To",
  "Cc",
  "Reply-To",
  "Newsgroups",
  "Followup-To",
  "Subject",
  "Date",
  "Sender",
  "Organization",
  "X-Mailer",
  "X-Newsreader",
  "User-Agent",
  "-Received",
  "-Message-Id",
  "-In-Reply-To",
  "-References",
  "-Mime-Version",
  "-Content-Type",
  "-Content-Transfer-Encoding",
  "-X-UIDL",
  "-Precedence",
  "-Status",
  "-Priority",
  "-X-Face"
};

static void
prefs_display_header_set_default (void)
{
  gint i;
  DisplayHeaderProp *dp;

  for (i = 0; i < sizeof (defaults) / sizeof (defaults[0]); i++)
    {
      dp = display_header_prop_read_str (defaults[i]);
      prefs_common.disphdr_list = g_slist_append (prefs_common.disphdr_list, dp);
    }
}

void
prefs_display_header_open (void)
{
  if (!dispheader.window)
    prefs_display_header_create ();

  manage_window_set_transient (GTK_WINDOW (dispheader.window));
  gtk_widget_grab_focus (dispheader.ok_btn);

  prefs_display_header_set_dialog ();

  gtk_widget_show (dispheader.window);
}

static void
prefs_display_header_create (void)
{
  GtkWidget *window;
  GtkWidget *vbox;
  GtkWidget *btn_hbox;
  GtkWidget *confirm_area;
  GtkWidget *ok_btn;
  GtkWidget *cancel_btn;

  GtkWidget *vbox1;

  GtkWidget *hbox1;
  GtkWidget *hdr_label;
  GtkWidget *hdr_combo;

  GtkWidget *btn_vbox;
  GtkWidget *reg_btn;
  GtkWidget *del_btn;
  GtkWidget *up_btn;
  GtkWidget *down_btn;

  GtkWidget *list_hbox;
  GtkWidget *list_hbox1;
  GtkWidget *list_hbox2;
  GtkWidget *list_scrolledwin;
  GtkWidget *headers_list;
  GtkWidget *hidden_headers_list;

  GtkListStore *store;
  GtkTreeViewColumn *col;
  GtkCellRenderer *renderer;

  GtkWidget *checkbtn_other_headers;

  debug_print ("Creating display header setting window...\n");

  window = gtk_window_new (GTK_WINDOW_TOPLEVEL);
  gtk_container_set_border_width (GTK_CONTAINER (window), 8);
  gtk_window_set_position (GTK_WINDOW (window), GTK_WIN_POS_CENTER);
  gtk_window_set_modal (GTK_WINDOW (window), TRUE);

  vbox = gtk_box_new (GTK_ORIENTATION_VERTICAL, 6);
  gtk_container_add (GTK_CONTAINER (window), vbox);

  btn_hbox = gtk_box_new (GTK_ORIENTATION_HORIZONTAL, 8);
  gtk_box_pack_end (GTK_BOX (vbox), btn_hbox, FALSE, FALSE, 0);

  yam_stock_button_set_create (&confirm_area, &ok_btn, "yam-ok", &cancel_btn, "yam-cancel", NULL, NULL);
  gtk_box_pack_end (GTK_BOX (btn_hbox), confirm_area, FALSE, FALSE, 0);
  gtk_widget_grab_default (ok_btn);

  gtk_window_set_title (GTK_WINDOW (window), _("Display header setting"));
  MANAGE_WINDOW_SIGNALS_CONNECT (window);
  g_signal_connect (G_OBJECT (window), "delete_event", G_CALLBACK (prefs_display_header_deleted), NULL);
  g_signal_connect (G_OBJECT (window), "key_press_event", G_CALLBACK (prefs_display_header_key_pressed), NULL);
  g_signal_connect (G_OBJECT (ok_btn), "clicked", G_CALLBACK (prefs_display_header_ok), NULL);
  g_signal_connect (G_OBJECT (cancel_btn), "clicked", G_CALLBACK (prefs_display_header_cancel), NULL);

  vbox1 = gtk_box_new (GTK_ORIENTATION_VERTICAL, VSPACING);
  gtk_box_pack_start (GTK_BOX (vbox), vbox1, TRUE, TRUE, 0);
  gtk_container_set_border_width (GTK_CONTAINER (vbox1), 2);

  hbox1 = gtk_box_new (GTK_ORIENTATION_HORIZONTAL, 8);
  gtk_box_pack_start (GTK_BOX (vbox1), hbox1, FALSE, TRUE, 0);

  hdr_label = gtk_label_new (_("Header name"));
  gtk_box_pack_start (GTK_BOX (hbox1), hdr_label, FALSE, FALSE, 0);

  hdr_combo = gtk_combo_box_text_new_with_entry ();
  gtk_box_pack_start (GTK_BOX (hbox1), hdr_combo, TRUE, TRUE, 0);
  gtk_widget_set_size_request (hdr_combo, 150, -1);

  gtk_combo_box_text_append_text (GTK_COMBO_BOX_TEXT (hdr_combo), "From");
  gtk_combo_box_text_append_text (GTK_COMBO_BOX_TEXT (hdr_combo), "To");
  gtk_combo_box_text_append_text (GTK_COMBO_BOX_TEXT (hdr_combo), "Cc");
  gtk_combo_box_text_append_text (GTK_COMBO_BOX_TEXT (hdr_combo), "Subject");
  gtk_combo_box_text_append_text (GTK_COMBO_BOX_TEXT (hdr_combo), "Date");
  gtk_combo_box_text_append_text (GTK_COMBO_BOX_TEXT (hdr_combo), "Reply-To");
  gtk_combo_box_text_append_text (GTK_COMBO_BOX_TEXT (hdr_combo), "Sender");
  gtk_combo_box_text_append_text (GTK_COMBO_BOX_TEXT (hdr_combo), "User-Agent");
  gtk_combo_box_text_append_text (GTK_COMBO_BOX_TEXT (hdr_combo), "X-Mailer");

  list_hbox = gtk_box_new (GTK_ORIENTATION_HORIZONTAL, 10);
  gtk_box_pack_start (GTK_BOX (vbox1), list_hbox, TRUE, TRUE, 0);

  /* display headers list */

  list_hbox1 = gtk_box_new (GTK_ORIENTATION_HORIZONTAL, 8);
  gtk_box_pack_start (GTK_BOX (list_hbox), list_hbox1, TRUE, TRUE, 0);

  list_scrolledwin = gtk_scrolled_window_new (NULL, NULL);
  gtk_widget_set_size_request (list_scrolledwin, 200, 210);
  gtk_box_pack_start (GTK_BOX (list_hbox1), list_scrolledwin, TRUE, TRUE, 0);
  gtk_scrolled_window_set_policy (GTK_SCROLLED_WINDOW (list_scrolledwin), GTK_POLICY_AUTOMATIC, GTK_POLICY_AUTOMATIC);

  store = gtk_list_store_new (2, G_TYPE_STRING, G_TYPE_POINTER);
  headers_list = gtk_tree_view_new_with_model (GTK_TREE_MODEL (store));
  gtk_tree_view_set_headers_visible (GTK_TREE_VIEW (headers_list), TRUE);
  gtk_tree_selection_set_mode (gtk_tree_view_get_selection (GTK_TREE_VIEW (headers_list)), GTK_SELECTION_BROWSE);
  gtk_container_add (GTK_CONTAINER (list_scrolledwin), headers_list);
  g_object_unref (store);

  renderer = gtk_cell_renderer_text_new ();
  col = gtk_tree_view_column_new_with_attributes (_("Displayed Headers"), renderer, "text", 0, NULL);
  gtk_tree_view_column_set_expand (col, TRUE);
  gtk_tree_view_append_column (GTK_TREE_VIEW (headers_list), col);

  btn_vbox = gtk_box_new (GTK_ORIENTATION_VERTICAL, 8);
  gtk_box_pack_start (GTK_BOX (list_hbox1), btn_vbox, FALSE, FALSE, 0);

  reg_btn = gtk_button_new_with_label (_("Add"));
  gtk_box_pack_start (GTK_BOX (btn_vbox), reg_btn, FALSE, TRUE, 0);
  g_signal_connect (G_OBJECT (reg_btn), "clicked",
                    G_CALLBACK (prefs_display_header_register_cb), GINT_TO_POINTER (FALSE));
  del_btn = gtk_button_new_with_label (_("Delete"));
  gtk_box_pack_start (GTK_BOX (btn_vbox), del_btn, FALSE, TRUE, 0);
  g_signal_connect (G_OBJECT (del_btn), "clicked", G_CALLBACK (prefs_display_header_delete_cb), headers_list);

  up_btn = gtk_button_new_with_label (_("Up"));
  gtk_box_pack_start (GTK_BOX (btn_vbox), up_btn, FALSE, FALSE, 0);
  g_signal_connect (G_OBJECT (up_btn), "clicked", G_CALLBACK (prefs_display_header_up), NULL);

  down_btn = gtk_button_new_with_label (_("Down"));
  gtk_box_pack_start (GTK_BOX (btn_vbox), down_btn, FALSE, FALSE, 0);
  g_signal_connect (G_OBJECT (down_btn), "clicked", G_CALLBACK (prefs_display_header_down), NULL);

  /* hidden headers list */

  list_hbox2 = gtk_box_new (GTK_ORIENTATION_HORIZONTAL, 8);
  gtk_box_pack_start (GTK_BOX (list_hbox), list_hbox2, TRUE, TRUE, 0);

  list_scrolledwin = gtk_scrolled_window_new (NULL, NULL);
  gtk_widget_set_size_request (list_scrolledwin, 200, 210);
  gtk_box_pack_start (GTK_BOX (list_hbox2), list_scrolledwin, TRUE, TRUE, 0);
  gtk_scrolled_window_set_policy (GTK_SCROLLED_WINDOW (list_scrolledwin), GTK_POLICY_AUTOMATIC, GTK_POLICY_AUTOMATIC);

  store = gtk_list_store_new (2, G_TYPE_STRING, G_TYPE_POINTER);
  hidden_headers_list = gtk_tree_view_new_with_model (GTK_TREE_MODEL (store));
  gtk_tree_view_set_headers_visible (GTK_TREE_VIEW (hidden_headers_list), TRUE);
  gtk_tree_selection_set_mode (gtk_tree_view_get_selection (GTK_TREE_VIEW (hidden_headers_list)), GTK_SELECTION_BROWSE);
  gtk_container_add (GTK_CONTAINER (list_scrolledwin), hidden_headers_list);
  g_object_unref (store);

  renderer = gtk_cell_renderer_text_new ();
  col = gtk_tree_view_column_new_with_attributes (_("Hidden Headers"), renderer, "text", 0, NULL);
  gtk_tree_view_column_set_expand (col, TRUE);
  gtk_tree_view_append_column (GTK_TREE_VIEW (hidden_headers_list), col);

  btn_vbox = gtk_box_new (GTK_ORIENTATION_VERTICAL, 8);
  gtk_box_pack_start (GTK_BOX (list_hbox2), btn_vbox, FALSE, FALSE, 0);

  reg_btn = gtk_button_new_with_label (_("Add"));
  gtk_box_pack_start (GTK_BOX (btn_vbox), reg_btn, FALSE, TRUE, 0);
  g_signal_connect (G_OBJECT (reg_btn), "clicked", G_CALLBACK (prefs_display_header_register_cb), GINT_TO_POINTER (TRUE));

  del_btn = gtk_button_new_with_label (_("Delete"));
  gtk_box_pack_start (GTK_BOX (btn_vbox), del_btn, FALSE, TRUE, 0);
  g_signal_connect (G_OBJECT (del_btn), "clicked", G_CALLBACK (prefs_display_header_delete_cb), hidden_headers_list);

  PACK_CHECK_BUTTON (btn_hbox, checkbtn_other_headers, _("Show all unspecified headers"));
  SET_TOGGLE_SENSITIVITY (checkbtn_other_headers, list_hbox2);

  gtk_widget_show_all (window);

  dispheader.window = window;

  dispheader.confirm_area = confirm_area;
  dispheader.ok_btn = ok_btn;
  dispheader.cancel_btn = cancel_btn;

  dispheader.hdr_combo = hdr_combo;
  dispheader.hdr_entry = gtk_bin_get_child (GTK_BIN (hdr_combo));

  dispheader.headers_list = headers_list;
  dispheader.hidden_headers_list = hidden_headers_list;

  dispheader.other_headers = checkbtn_other_headers;
}

void
prefs_display_header_read_config (void)
{
  gchar *rcpath;
  FILE *fp;
  gchar buf[PREFSBUFSIZE];
  DisplayHeaderProp *dp;

  debug_print ("Reading configuration for displaying headers...\n");

  rcpath = g_strconcat (get_rc_dir (), G_DIR_SEPARATOR_S, DISPLAY_HEADER_RC, NULL);
  if ((fp = g_fopen (rcpath, "rb")) == NULL)
    {
      if (ENOENT != errno)
        FILE_OP_ERROR (rcpath, "fopen");
      g_free (rcpath);
      prefs_common.disphdr_list = NULL;
      prefs_display_header_set_default ();
      return;
    }
  g_free (rcpath);

  /* remove all previous headers list */
  while (prefs_common.disphdr_list != NULL)
    {
      dp = (DisplayHeaderProp *) prefs_common.disphdr_list->data;
      display_header_prop_free (dp);
      prefs_common.disphdr_list = g_slist_remove (prefs_common.disphdr_list, dp);
    }

  while (fgets (buf, sizeof (buf), fp) != NULL)
    {
      g_strchomp (buf);
      dp = display_header_prop_read_str (buf);
      if (dp)
        prefs_common.disphdr_list = g_slist_append (prefs_common.disphdr_list, dp);
    }

  fclose (fp);
}

void
prefs_display_header_write_config (void)
{
  gchar *rcpath;
  PrefFile *pfile;
  GSList *cur;

  debug_print ("Writing configuration for displaying headers...\n");

  rcpath = g_strconcat (get_rc_dir (), G_DIR_SEPARATOR_S, DISPLAY_HEADER_RC, NULL);

  if ((pfile = prefs_file_open (rcpath)) == NULL)
    {
      g_warning (_("failed to write configuration to file\n"));
      g_free (rcpath);
      return;
    }

  for (cur = prefs_common.disphdr_list; cur != NULL; cur = cur->next)
    {
      DisplayHeaderProp *dp = (DisplayHeaderProp *) cur->data;
      gchar *dpstr;

      dpstr = display_header_prop_get_str (dp);
      if (fputs (dpstr, pfile->fp) == EOF || fputc ('\n', pfile->fp) == EOF)
        {
          FILE_OP_ERROR (rcpath, "fputs || fputc");
          prefs_file_close_revert (pfile);
          g_free (rcpath);
          g_free (dpstr);
          return;
        }
      g_free (dpstr);
    }

  g_free (rcpath);

  if (prefs_file_close (pfile) < 0)
    {
      g_warning (_("failed to write configuration to file\n"));
      return;
    }
}

static void
prefs_display_header_set_dialog (void)
{
  GtkListStore *hstore;
  GtkListStore *hhstore;
  GtkTreeIter iter;
  GSList *cur;

  gtk_widget_freeze_child_notify (dispheader.headers_list);
  gtk_widget_freeze_child_notify (dispheader.hidden_headers_list);

  hstore = GTK_LIST_STORE (gtk_tree_view_get_model (GTK_TREE_VIEW (dispheader.headers_list)));
  hhstore = GTK_LIST_STORE (gtk_tree_view_get_model (GTK_TREE_VIEW (dispheader.hidden_headers_list)));

  gtk_list_store_clear (hstore);
  gtk_list_store_clear (hhstore);

  for (cur = prefs_common.disphdr_list; cur != NULL; cur = cur->next)
    {
      DisplayHeaderProp *dp = (DisplayHeaderProp *) cur->data;

      if (dp->hidden)
        {
          gtk_list_store_append (hhstore, &iter);
          gtk_list_store_set (hhstore, &iter, 0, dp->name, 1, dp, -1);
        }
      else
        {
          gtk_list_store_append (hstore, &iter);
          gtk_list_store_set (hstore, &iter, 0, dp->name, 1, dp, -1);
        }
    }

  gtk_widget_thaw_child_notify (dispheader.hidden_headers_list);
  gtk_widget_thaw_child_notify (dispheader.headers_list);

  gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (dispheader.other_headers), prefs_common.show_other_header);
}

static void
prefs_display_header_set_list ()
{
  GtkTreeModel *model;
  GtkTreeIter iter;
  DisplayHeaderProp *dp;

  g_slist_free (prefs_common.disphdr_list);
  prefs_common.disphdr_list = NULL;

  model = gtk_tree_view_get_model (GTK_TREE_VIEW (dispheader.headers_list));
  if (gtk_tree_model_get_iter_first (model, &iter))
    {
      do
        {
          gtk_tree_model_get (model, &iter, 1, &dp, -1);
          prefs_common.disphdr_list = g_slist_append (prefs_common.disphdr_list, dp);
        }
      while (gtk_tree_model_iter_next (model, &iter));
    }

  model = gtk_tree_view_get_model (GTK_TREE_VIEW (dispheader.hidden_headers_list));
  if (gtk_tree_model_get_iter_first (model, &iter))
    {
      do
        {
          gtk_tree_model_get (model, &iter, 1, &dp, -1);
          prefs_common.disphdr_list = g_slist_append (prefs_common.disphdr_list, dp);
        }
      while (gtk_tree_model_iter_next (model, &iter));
    }
}

static gboolean
prefs_display_header_find_header (GtkTreeModel *model, const gchar * header)
{
  GtkTreeIter iter;
  DisplayHeaderProp *dp;

  if (gtk_tree_model_get_iter_first (model, &iter))
    {
      do
        {
          gtk_tree_model_get (model, &iter, 1, &dp, -1);
          if (g_ascii_strcasecmp (dp->name, header) == 0)
            return TRUE;
        }
      while (gtk_tree_model_iter_next (model, &iter));
    }

  return FALSE;
}

static void
prefs_display_header_list_set_row (gboolean hidden)
{
  GtkTreeModel *model;
  GtkTreeIter iter;
  DisplayHeaderProp *dp;
  const gchar *entry_text;

  entry_text = gtk_entry_get_text (GTK_ENTRY (dispheader.hdr_entry));
  if (entry_text[0] == '\0')
    {
      alertpanel_error (_("Header name is not set."));
      return;
    }

  if (hidden)
    model = gtk_tree_view_get_model (GTK_TREE_VIEW (dispheader.hidden_headers_list));
  else
    model = gtk_tree_view_get_model (GTK_TREE_VIEW (dispheader.headers_list));

  if (prefs_display_header_find_header (model, entry_text))
    {
      alertpanel_error (_("This header is already in the list."));
      return;
    }

  dp = g_new0 (DisplayHeaderProp, 1);

  dp->name = g_strdup (entry_text);
  dp->hidden = hidden;

  gtk_list_store_append (GTK_LIST_STORE (model), &iter);
  gtk_list_store_set (GTK_LIST_STORE (model), &iter, 0, dp->name, 1, dp, -1);

  prefs_display_header_set_list ();
}

static void
prefs_display_header_register_cb (GtkButton * btn, gpointer data)
{
  prefs_display_header_list_set_row (GPOINTER_TO_INT (data));
}

static void
prefs_display_header_delete_cb (GtkButton * btn, gpointer data)
{
  GtkTreeSelection *sel;
  GtkTreeModel *model;
  GtkTreeIter iter;
  DisplayHeaderProp *dp;

  sel = gtk_tree_view_get_selection (GTK_TREE_VIEW (data));

  if (!gtk_tree_selection_get_selected (sel, &model, &iter))
    return;

  gtk_tree_model_get (model, &iter, 1, &dp, -1);
  display_header_prop_free (dp);
  gtk_list_store_remove (GTK_LIST_STORE (model), &iter);
  prefs_common.disphdr_list = g_slist_remove (prefs_common.disphdr_list, dp);
}

static void
prefs_display_header_up (void)
{
  GtkTreeSelection *sel;
  GtkTreeModel *model;
  GtkTreeIter iter, pos;

  sel = gtk_tree_view_get_selection (GTK_TREE_VIEW (dispheader.headers_list));

  if (!gtk_tree_selection_get_selected (sel, &model, &iter))
    return;

  pos = iter;
  if (gtk_tree_model_iter_previous (model, &pos))
    gtk_list_store_move_before (GTK_LIST_STORE (model), &iter, &pos);

  prefs_display_header_set_list ();
}

static void
prefs_display_header_down (void)
{
  GtkTreeSelection *sel;
  GtkTreeModel *model;
  GtkTreeIter iter, pos;

  sel = gtk_tree_view_get_selection (GTK_TREE_VIEW (dispheader.headers_list));

  if (!gtk_tree_selection_get_selected (sel, &model, &iter))
    return;

  pos = iter;
  if (gtk_tree_model_iter_next (model, &pos))
    gtk_list_store_move_after (GTK_LIST_STORE (model), &iter, &pos);

  prefs_display_header_set_list ();
}

static gboolean
prefs_display_header_key_pressed (GtkWidget * widget, GdkEventKey * event, gpointer data)
{
  if (event && event->keyval == GDK_KEY_Escape)
    prefs_display_header_cancel ();
  return FALSE;
}

static void
prefs_display_header_ok (void)
{
  prefs_common.show_other_header = gtk_toggle_button_get_active (GTK_TOGGLE_BUTTON (dispheader.other_headers));
  prefs_display_header_write_config ();
  gtk_widget_hide (dispheader.window);
}

static void
prefs_display_header_cancel (void)
{
  prefs_display_header_read_config ();
  gtk_widget_hide (dispheader.window);
}

static gint
prefs_display_header_deleted (GtkWidget * widget, GdkEventAny * event, gpointer data)
{
  prefs_display_header_cancel ();
  return TRUE;
}
