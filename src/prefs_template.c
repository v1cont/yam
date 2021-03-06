/*
 * YAM templates subsystem
 * Copyright (C) 2001 Alexander Barinov
 * Copyright (C) 2001-2015 Hiroyuki Yamamoto
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

#include "defs.h"

#include <glib.h>
#include <glib/gi18n.h>
#include <gtk/gtk.h>
#include <gdk/gdkkeysyms.h>
#include <string.h>
#include <dirent.h>
#include <sys/stat.h>

#include "template.h"
#include "main.h"
#include "inc.h"
#include "utils.h"
#include "gtkutils.h"
#include "alertpanel.h"
#include "manage_window.h"
#include "prefs_common.h"
#include "prefs_common_dialog.h"
#include "compose.h"
#include "mainwindow.h"
#include "addr_compl.h"
#include "quote_fmt.h"
#include "plugin.h"

static struct Templates {
  GtkWidget *window;
  GtkWidget *list_tmpls;
  GtkWidget *entry_name;
  GtkWidget *entry_to;
  GtkWidget *entry_cc;
  GtkWidget *entry_bcc;
  GtkWidget *entry_replyto;
  GtkWidget *entry_subject;
  GtkWidget *text_value;
  GtkWidget *confirm_area;
  GtkWidget *ok_btn;
  GtkWidget *cancel_btn;

  gboolean entry_modified;
  gboolean list_modified;
} templates;

/* widget creating functions */
static void prefs_template_window_create (void);
static void prefs_template_window_setup (void);
static void prefs_template_clear (void);

static GSList *prefs_template_get_list (void);

/* callbacks */
static gint prefs_template_deleted_cb (GtkWidget * widget, GdkEventAny * event, gpointer data);
static gboolean prefs_template_key_pressed_cb (GtkWidget * widget, GdkEventKey * event, gpointer data);
static void prefs_template_changed_cb (GtkEditable * editable, gpointer data);
static void prefs_template_cancel_cb (void);
static void prefs_template_ok_cb (void);
static void prefs_template_select_cb (GtkTreeView *view, GtkTreePath *path, GtkTreeViewColumn *col, gpointer data);
static void prefs_template_register_cb (void);
static void prefs_template_substitute_cb (void);
static void prefs_template_delete_cb (void);
static void prefs_template_up_cb (void);
static void prefs_template_down_cb (void);

/* Called from mainwindow.c */
void
prefs_template_open (void)
{
  inc_lock ();

  if (!templates.window)
    prefs_template_window_create ();

  prefs_template_window_setup ();
  gtk_widget_show (templates.window);

  yam_plugin_signal_emit ("prefs-template-open", templates.window);
}

#define ADD_ENTRY(entry, str, row)                                  \
  {                                                                 \
	label1 = gtk_label_new(str);                                    \
	gtk_grid_attach(GTK_GRID(table), label1, 0, row, 1, 1);         \
	gtk_label_set_xalign(GTK_LABEL(label1), 1.0);                   \
                                                                    \
	entry = gtk_entry_new();                                        \
	gtk_grid_attach(GTK_GRID(table), entry, 1, row, 1, 1);          \
	g_signal_connect(G_OBJECT(entry), "changed",                    \
                     G_CALLBACK(prefs_template_changed_cb), NULL);  \
  }

static void
prefs_template_window_create (void)
{
  /* window structure ;) */
  GtkWidget *window;
  GtkWidget *vpaned;
  GtkWidget *vbox1;
  GtkWidget *hbox1;
  GtkWidget *label1;
  GtkWidget *entry_name;
  GtkWidget *table;
  GtkWidget *entry_to;
  GtkWidget *entry_cc;
  GtkWidget *entry_bcc;
  GtkWidget *entry_replyto;
  GtkWidget *entry_subject;
  GtkWidget *scroll2;
  GtkWidget *text_value;
  GtkWidget *vbox2;
  GtkWidget *hbox2;
  GtkWidget *arrow1;
  GtkWidget *hbox3;
  GtkWidget *reg_btn;
  GtkWidget *subst_btn;
  GtkWidget *del_btn;
  GtkWidget *desc_btn;
  GtkWidget *hbox4;
  GtkWidget *scroll1;
  GtkWidget *list_tmpl;
  GtkWidget *vbox3;
  GtkWidget *vbox4;
  GtkWidget *up_btn;
  GtkWidget *down_btn;
  GtkWidget *confirm_area;
  GtkWidget *ok_btn;
  GtkWidget *cancel_btn;

  GtkListStore *store;
  GtkCellRenderer *renderer;
  GtkTreeViewColumn *col;

  GtkTextBuffer *buffer;

  /* main window */
  window = gtk_window_new (GTK_WINDOW_TOPLEVEL);
  gtk_window_set_title (GTK_WINDOW (window), _("Templates"));
  gtk_window_set_position (GTK_WINDOW (window), GTK_WIN_POS_CENTER);
  gtk_window_set_modal (GTK_WINDOW (window), TRUE);

  /* vpaned to separate template settings from templates list */
  vpaned = gtk_paned_new (GTK_ORIENTATION_VERTICAL);
  gtk_container_add (GTK_CONTAINER (window), vpaned);

  /* vbox to handle template name and content */
  vbox1 = gtk_box_new (GTK_ORIENTATION_VERTICAL, 6);
  gtk_container_set_border_width (GTK_CONTAINER (vbox1), 8);
  gtk_paned_pack1 (GTK_PANED (vpaned), vbox1, FALSE, FALSE);

  hbox1 = gtk_box_new (GTK_ORIENTATION_HORIZONTAL, 8);
  gtk_box_pack_start (GTK_BOX (vbox1), hbox1, FALSE, FALSE, 0);

  label1 = gtk_label_new (_("Template name"));
  gtk_box_pack_start (GTK_BOX (hbox1), label1, FALSE, FALSE, 0);

  entry_name = gtk_entry_new ();
  gtk_box_pack_start (GTK_BOX (hbox1), entry_name, TRUE, TRUE, 0);
  g_signal_connect (G_OBJECT (entry_name), "changed", G_CALLBACK (prefs_template_changed_cb), NULL);

  /* table for headers */
  table = gtk_grid_new ();
  gtk_box_pack_start (GTK_BOX (vbox1), table, FALSE, FALSE, 0);
  gtk_grid_set_row_spacing (GTK_GRID (table), 5);
  gtk_grid_set_column_spacing (GTK_GRID (table), 5);

  ADD_ENTRY (entry_to, _("To:"), 0);
  address_completion_register_entry (GTK_ENTRY (entry_to));
  ADD_ENTRY (entry_cc, _("Cc:"), 1);
  address_completion_register_entry (GTK_ENTRY (entry_cc));
  ADD_ENTRY (entry_bcc, _("Bcc:"), 2);
  address_completion_register_entry (GTK_ENTRY (entry_bcc));
  ADD_ENTRY (entry_replyto, _("Reply-To:"), 3);
  address_completion_register_entry (GTK_ENTRY (entry_replyto));
  ADD_ENTRY (entry_subject, _("Subject:"), 4);

#undef ADD_ENTRY

  /* template content */
  scroll2 = gtk_scrolled_window_new (NULL, NULL);
  gtk_scrolled_window_set_policy (GTK_SCROLLED_WINDOW (scroll2), GTK_POLICY_NEVER, GTK_POLICY_AUTOMATIC);
  gtk_scrolled_window_set_shadow_type (GTK_SCROLLED_WINDOW (scroll2), GTK_SHADOW_IN);
  gtk_box_pack_start (GTK_BOX (vbox1), scroll2, TRUE, TRUE, 0);

  text_value = gtk_text_view_new ();
  gtk_widget_set_size_request (text_value, 360, 120);
  gtk_container_add (GTK_CONTAINER (scroll2), text_value);
  gtk_text_view_set_editable (GTK_TEXT_VIEW (text_value), TRUE);
  buffer = gtk_text_view_get_buffer (GTK_TEXT_VIEW (text_value));
  g_signal_connect (G_OBJECT (buffer), "changed", G_CALLBACK (prefs_template_changed_cb), NULL);

  /* vbox for buttons and templates list */
  vbox2 = gtk_box_new (GTK_ORIENTATION_VERTICAL, 6);
  gtk_container_set_border_width (GTK_CONTAINER (vbox2), 8);
  gtk_paned_pack2 (GTK_PANED (vpaned), vbox2, TRUE, FALSE);

  /* register | substitute | delete */
  hbox2 = gtk_box_new (GTK_ORIENTATION_HORIZONTAL, 4);
  gtk_box_pack_start (GTK_BOX (vbox2), hbox2, FALSE, FALSE, 0);

  arrow1 = yam_arrow_new (ARROW_DOWN);
  gtk_box_pack_start (GTK_BOX (hbox2), arrow1, FALSE, FALSE, 0);

  hbox3 = gtk_box_new (GTK_ORIENTATION_HORIZONTAL, 4);
  gtk_box_pack_start (GTK_BOX (hbox2), hbox3, FALSE, FALSE, 0);

  reg_btn = gtk_button_new_with_label (_("Register"));
  gtk_box_pack_start (GTK_BOX (hbox3), reg_btn, FALSE, TRUE, 0);
  g_signal_connect (G_OBJECT (reg_btn), "clicked", G_CALLBACK (prefs_template_register_cb), NULL);

  subst_btn = gtk_button_new_with_label (_(" Substitute "));
  gtk_box_pack_start (GTK_BOX (hbox3), subst_btn, FALSE, TRUE, 0);
  g_signal_connect (G_OBJECT (subst_btn), "clicked", G_CALLBACK (prefs_template_substitute_cb), NULL);

  del_btn = gtk_button_new_with_label (_("Delete"));
  gtk_box_pack_start (GTK_BOX (hbox3), del_btn, FALSE, TRUE, 0);
  g_signal_connect (G_OBJECT (del_btn), "clicked", G_CALLBACK (prefs_template_delete_cb), NULL);

  desc_btn = gtk_button_new_with_label (_(" Symbols "));
  gtk_box_pack_end (GTK_BOX (hbox2), desc_btn, FALSE, FALSE, 0);
  g_signal_connect (G_OBJECT (desc_btn), "clicked", G_CALLBACK (prefs_quote_description), NULL);

  /* templates list */
  hbox4 = gtk_box_new (GTK_ORIENTATION_HORIZONTAL, 8);
  gtk_box_pack_start (GTK_BOX (vbox2), hbox4, TRUE, TRUE, 0);

  scroll1 = gtk_scrolled_window_new (NULL, NULL);
  gtk_box_pack_start (GTK_BOX (hbox4), scroll1, TRUE, TRUE, 0);
  gtk_scrolled_window_set_policy (GTK_SCROLLED_WINDOW (scroll1), GTK_POLICY_AUTOMATIC, GTK_POLICY_AUTOMATIC);

  store = gtk_list_store_new (2, G_TYPE_STRING, G_TYPE_POINTER);
  list_tmpl = gtk_tree_view_new_with_model (GTK_TREE_MODEL (store));
  gtk_tree_view_set_search_column (GTK_TREE_VIEW (list_tmpl), 0);
  gtk_tree_view_set_activate_on_single_click (GTK_TREE_VIEW (list_tmpl), TRUE);
  gtk_tree_view_set_headers_visible (GTK_TREE_VIEW (list_tmpl), TRUE);
  gtk_tree_selection_set_mode (gtk_tree_view_get_selection (GTK_TREE_VIEW (list_tmpl)), GTK_SELECTION_BROWSE);
  gtk_widget_set_can_focus (list_tmpl, FALSE);
  g_signal_connect (G_OBJECT (list_tmpl), "row-activated", G_CALLBACK (prefs_template_select_cb), NULL);
  g_object_unref (store);

  renderer = gtk_cell_renderer_text_new ();
  col = gtk_tree_view_column_new_with_attributes (_("Registered templates"), renderer, "text", 0, NULL);
  gtk_tree_view_append_column (GTK_TREE_VIEW (list_tmpl), col);

  vbox3 = gtk_box_new (GTK_ORIENTATION_VERTICAL, 0);
  gtk_box_pack_start (GTK_BOX (hbox4), vbox3, FALSE, FALSE, 0);

  vbox4 = gtk_box_new (GTK_ORIENTATION_VERTICAL, 8);
  gtk_box_pack_start (GTK_BOX (vbox3), vbox4, TRUE, FALSE, 0);

  up_btn = gtk_button_new_with_label (_("Up"));
  gtk_box_pack_start (GTK_BOX (vbox4), up_btn, FALSE, FALSE, 0);
  g_signal_connect (G_OBJECT (up_btn), "clicked", G_CALLBACK (prefs_template_up_cb), NULL);

  down_btn = gtk_button_new_with_label (_("Down"));
  gtk_box_pack_start (GTK_BOX (vbox4), down_btn, FALSE, FALSE, 0);
  g_signal_connect (G_OBJECT (down_btn), "clicked", G_CALLBACK (prefs_template_down_cb), NULL);

  /* ok | cancel */
  yam_stock_button_set_create (&confirm_area, &ok_btn, "yam-ok", &cancel_btn, "yam-cancel", NULL, NULL);
  gtk_box_pack_end (GTK_BOX (vbox2), confirm_area, FALSE, FALSE, 0);
  gtk_widget_grab_default (ok_btn);

  g_signal_connect (G_OBJECT (window), "delete_event", G_CALLBACK (prefs_template_deleted_cb), NULL);
  g_signal_connect (G_OBJECT (window), "key_press_event", G_CALLBACK (prefs_template_key_pressed_cb), NULL);
  MANAGE_WINDOW_SIGNALS_CONNECT (window);
  g_signal_connect (G_OBJECT (ok_btn), "clicked", G_CALLBACK (prefs_template_ok_cb), NULL);
  g_signal_connect (G_OBJECT (cancel_btn), "clicked", G_CALLBACK (prefs_template_cancel_cb), NULL);

  gtk_widget_show_all (vpaned);

  address_completion_start (window);

  templates.window = window;
  templates.list_tmpls = list_tmpl;
  templates.entry_name = entry_name;
  templates.entry_to = entry_to;
  templates.entry_cc = entry_cc;
  templates.entry_bcc = entry_bcc;
  templates.entry_replyto = entry_replyto;
  templates.entry_subject = entry_subject;
  templates.text_value = text_value;
  templates.confirm_area = confirm_area;
  templates.ok_btn = ok_btn;
  templates.cancel_btn = cancel_btn;
  templates.entry_modified = FALSE;
  templates.list_modified = FALSE;
}

static void
prefs_template_window_setup (void)
{
  GtkTreeIter iter;
  GSList *tmpl_list;
  GSList *cur;
  Template *tmpl;
  GtkListStore *model = GTK_LIST_STORE (gtk_tree_view_get_model (GTK_TREE_VIEW (templates.list_tmpls)));

  manage_window_set_transient (GTK_WINDOW (templates.window));
  gtk_widget_grab_focus (templates.ok_btn);

  gtk_list_store_clear (model);

  gtk_list_store_append (model, &iter);
  gtk_list_store_set (model, &iter, 0, _("(New)"), 1, NULL, -1);

  tmpl_list = template_read_config ();
  for (cur = tmpl_list; cur != NULL; cur = cur->next)
    {
      tmpl = (Template *) cur->data;
      gtk_list_store_append (model, &iter);
      gtk_list_store_set (model, &iter, 0, tmpl->name, 1, tmpl, -1);
    }
  g_slist_free (tmpl_list);

  templates.entry_modified = FALSE;
  templates.list_modified = FALSE;
}

static void
prefs_template_clear (void)
{
  GtkTreeIter iter;
  Template *tmpl;
  GtkTreeModel *model = gtk_tree_view_get_model (GTK_TREE_VIEW (templates.list_tmpls));

  if (gtk_tree_model_get_iter_first (model, &iter))
    {
      do
        {
          gtk_tree_model_get (model, &iter, 1, &tmpl, -1);
          template_free (tmpl);
        }
      while (gtk_tree_model_iter_next (model, &iter));
    }

  gtk_list_store_clear (GTK_LIST_STORE (model));
}

static gint
prefs_template_deleted_cb (GtkWidget * widget, GdkEventAny * event, gpointer data)
{
  prefs_template_cancel_cb ();
  return TRUE;
}

static gboolean
prefs_template_key_pressed_cb (GtkWidget * widget, GdkEventKey * event, gpointer data)
{
  if (event && event->keyval == GDK_KEY_Escape)
    prefs_template_cancel_cb ();
  return FALSE;
}

static void
prefs_template_changed_cb (GtkEditable * editable, gpointer data)
{
  templates.entry_modified = TRUE;
}

static void
prefs_template_ok_cb (void)
{
  GSList *tmpl_list;

  if (templates.entry_modified)
    {
      if (alertpanel (_("Template is modified"),
                      _("Current modification is not applied. Finish without saving it?"),
                      "yam-yes", "yam-no", NULL) != G_ALERTDEFAULT)
        return;
    }

  tmpl_list = prefs_template_get_list ();
  template_set_config (tmpl_list);
  compose_reflect_prefs_all ();
  gtk_list_store_clear (GTK_LIST_STORE (gtk_tree_view_get_model (GTK_TREE_VIEW (templates.list_tmpls))));
  gtk_widget_hide (templates.window);
  main_window_popup (main_window_get ());
  inc_unlock ();
}

static void
prefs_template_cancel_cb (void)
{
  if (templates.entry_modified || templates.list_modified)
    {
      if (alertpanel (_("Templates are modified"),
                      _("Really discard modification to templates?"),
                      "yam-yes", "yam-no", NULL) != G_ALERTDEFAULT)
        return;
    }

  prefs_template_clear ();
  gtk_widget_hide (templates.window);
  main_window_popup (main_window_get ());
  inc_unlock ();
}

static void
prefs_template_select_cb (GtkTreeView *view, GtkTreePath *path,  GtkTreeViewColumn *column, gpointer data)
{
  Template *tmpl;
  Template tmpl_def;
  GtkTextBuffer *buffer;
  GtkTextIter iter;
  GtkTreeModel *model;
  GtkTreeIter iter2;

  tmpl_def.name = _("Template");
  tmpl_def.subject = "";
  tmpl_def.to = "";
  tmpl_def.cc = "";
  tmpl_def.bcc = "";
  tmpl_def.replyto = "";
  tmpl_def.value = "";

  model = gtk_tree_view_get_model (view);
  gtk_tree_model_get_iter (model, &iter2, path);
  gtk_tree_model_get (model, &iter2, 1, &tmpl, -1);

  if (!tmpl)
    tmpl = &tmpl_def;

  gtk_entry_set_text (GTK_ENTRY (templates.entry_name), tmpl->name);
  gtk_entry_set_text (GTK_ENTRY (templates.entry_to), tmpl->to ? tmpl->to : "");
  gtk_entry_set_text (GTK_ENTRY (templates.entry_cc), tmpl->cc ? tmpl->cc : "");
  gtk_entry_set_text (GTK_ENTRY (templates.entry_bcc), tmpl->bcc ? tmpl->bcc : "");
  gtk_entry_set_text (GTK_ENTRY (templates.entry_replyto), tmpl->replyto ? tmpl->replyto : "");
  gtk_entry_set_text (GTK_ENTRY (templates.entry_subject), tmpl->subject ? tmpl->subject : "");

  buffer = gtk_text_view_get_buffer (GTK_TEXT_VIEW (templates.text_value));
  gtk_text_buffer_set_text (buffer, "", 0);
  gtk_text_buffer_get_start_iter (buffer, &iter);
  gtk_text_buffer_insert (buffer, &iter, tmpl->value, -1);

  templates.entry_modified = FALSE;
}

static GSList *
prefs_template_get_list (void)
{
  GSList *tmpl_list = NULL;
  Template *tmpl;
  GtkTreeIter iter;
  GtkTreeModel *model = gtk_tree_view_get_model (GTK_TREE_VIEW (templates.list_tmpls));
  gint row = 0;

  if (gtk_tree_model_get_iter_first (model, &iter))
    {
      do
        {
          gtk_tree_model_get (model, &iter, 1, &tmpl, -1);
          if (tmpl)
            {
              tmpl->tmplid = row;
              tmpl_list = g_slist_append (tmpl_list, tmpl);
              row++;
            }
        }
      while (gtk_tree_model_iter_next (model, &iter));
    }

  return tmpl_list;
}

static void
prefs_template_list_set_row (GtkTreeIter *iter)
{
  Template *tmpl;
  Template *tmp_tmpl;
  GtkTextBuffer *buffer;
  GtkTextIter start, end;
  GtkListStore *model;
  gchar *name;
  gchar *to;
  gchar *cc;
  gchar *bcc;
  gchar *replyto;
  gchar *subject;
  gchar *value;

  buffer = gtk_text_view_get_buffer (GTK_TEXT_VIEW (templates.text_value));
  gtk_text_buffer_get_start_iter (buffer, &start);
  gtk_text_buffer_get_end_iter (buffer, &end);
  value = gtk_text_buffer_get_text (buffer, &start, &end, FALSE);

  if (value && *value != '\0')
    {
      gchar *parsed_buf;
      MsgInfo dummyinfo;

      memset (&dummyinfo, 0, sizeof (MsgInfo));
      quote_fmt_init (&dummyinfo, NULL, NULL);
      quote_fmt_scan_string (value);
      quote_fmt_parse ();
      parsed_buf = quote_fmt_get_buffer ();
      if (!parsed_buf)
        {
          alertpanel_error (_("Template format error."));
          g_free (value);
          return;
        }
    }

  name = gtk_editable_get_chars (GTK_EDITABLE (templates.entry_name), 0, -1);
  to = gtk_editable_get_chars (GTK_EDITABLE (templates.entry_to), 0, -1);
  cc = gtk_editable_get_chars (GTK_EDITABLE (templates.entry_cc), 0, -1);
  bcc = gtk_editable_get_chars (GTK_EDITABLE (templates.entry_bcc), 0, -1);
  replyto = gtk_editable_get_chars (GTK_EDITABLE (templates.entry_replyto), 0, -1);
  subject = gtk_editable_get_chars (GTK_EDITABLE (templates.entry_subject), 0, -1);

#define NULLIFY_IF_EMPTY(val)                   \
  if (val && *val == '\0') {                    \
    g_free(val);                                \
    val = NULL;                                 \
  }

  NULLIFY_IF_EMPTY (to);
  NULLIFY_IF_EMPTY (cc);
  NULLIFY_IF_EMPTY (bcc);
  NULLIFY_IF_EMPTY (replyto);
  NULLIFY_IF_EMPTY (subject);

#undef NULLIFY_IF_EMPTY

  tmpl = g_new (Template, 1);
  tmpl->name = name;
  tmpl->to = to;
  tmpl->cc = cc;
  tmpl->bcc = bcc;
  tmpl->replyto = replyto;
  tmpl->subject = subject;
  tmpl->value = value;

  model = GTK_LIST_STORE (gtk_tree_view_get_model (GTK_TREE_VIEW (templates.list_tmpls)));

  if (!iter)
    {
      GtkTreeIter iter2;

      gtk_list_store_append (model, &iter2);
      gtk_list_store_set (model, &iter2, 0, name, 1, tmpl, -1);
    }
  else
    {
      gtk_tree_model_get (GTK_TREE_MODEL (model), iter, 1, &tmp_tmpl, -1);
      gtk_list_store_set (model, iter, 0, name, 1, tmpl, -1);

      if (tmp_tmpl)
        template_free (tmp_tmpl);
    }

  templates.entry_modified = FALSE;
  templates.list_modified = TRUE;
}

static void
prefs_template_register_cb (void)
{
  prefs_template_list_set_row (NULL);
}

static void
prefs_template_substitute_cb (void)
{
  Template *tmpl;
  GtkTreeSelection *sel;
  GtkTreeModel *model;
  GtkTreeIter iter;

  sel = gtk_tree_view_get_selection (GTK_TREE_VIEW (templates.list_tmpls));

  if (!gtk_tree_selection_get_selected (sel, &model, &iter))
    return;

  gtk_tree_model_get (model, &iter, 1, &tmpl, -1);
  if (!tmpl)
    return;

  prefs_template_list_set_row (&iter);
}

static void
prefs_template_delete_cb (void)
{
  Template *tmpl;
  GtkTreeSelection *sel;
  GtkTreeModel *model;
  GtkTreeIter iter;

  sel = gtk_tree_view_get_selection (GTK_TREE_VIEW (templates.list_tmpls));

  if (!gtk_tree_selection_get_selected (sel, &model, &iter))
    return;

  gtk_tree_model_get (model, &iter, 1, &tmpl, -1);
  if (!tmpl)
    return;

  if (alertpanel (_("Delete template"), _("Do you really want to delete this template?"),
                  "yam-yes", "yam-no", NULL) != G_ALERTDEFAULT)
    return;

  template_free (tmpl);
  gtk_list_store_remove (GTK_LIST_STORE (model), &iter);
  templates.list_modified = TRUE;
}

static void
prefs_template_up_cb (void)
{
  Template *tmpl;
  GtkTreeSelection *sel;
  GtkTreeModel *model;
  GtkTreeIter iter, pos;

  sel = gtk_tree_view_get_selection (GTK_TREE_VIEW (templates.list_tmpls));

  if (!gtk_tree_selection_get_selected (sel, NULL, &iter))
    return;

  model = gtk_tree_view_get_model (GTK_TREE_VIEW (templates.list_tmpls));
  pos = iter;

  if (!gtk_tree_model_iter_previous (model, &pos))
    return;

  /* check if previous is not first item*/
  gtk_tree_model_get (model, &pos, 1, &tmpl, -1);
  if (!tmpl)
    return;

  gtk_list_store_move_before (GTK_LIST_STORE (model), &iter, &pos);
  templates.list_modified = TRUE;
}

static void
prefs_template_down_cb (void)
{
  Template *tmpl;
  GtkTreeSelection *sel;
  GtkTreeModel *model;
  GtkTreeIter iter, pos;

  sel = gtk_tree_view_get_selection (GTK_TREE_VIEW (templates.list_tmpls));

  if (!gtk_tree_selection_get_selected (sel, NULL, &iter))
    return;

  model = gtk_tree_view_get_model (GTK_TREE_VIEW (templates.list_tmpls));
  pos = iter;

  if (!gtk_tree_model_iter_next (model, &pos))
    return;

  /* check if current is not first item*/
  gtk_tree_model_get (model, &iter, 1, &tmpl, -1);
  if (!tmpl)
    return;

  gtk_list_store_move_after (GTK_LIST_STORE (model), &iter, &pos);
  templates.list_modified = TRUE;
}
