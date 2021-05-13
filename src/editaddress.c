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

#include <glib/gi18n.h>
#include <gdk/gdkkeysyms.h>
#include <gtk/gtk.h>

#include "mgutils.h"
#include "addressbook.h"
#include "addressitem.h"
#include "addritem.h"
#include "addrbook.h"
#include "manage_window.h"
#include "gtkutils.h"
#include "codeconv.h"

#include "prefs_common.h"

static struct _PersonEdit_dlg {
  GtkWidget *window;
  GtkWidget *notebook;
  GtkWidget *hbbox;
  GtkWidget *ok_btn;
  GtkWidget *cancel_btn;
  GtkWidget *statusbar;
  gint status_cid;

  /* Basic data tab */
  GtkWidget *entry_name;
  GtkWidget *entry_first;
  GtkWidget *entry_last;
  GtkWidget *entry_nick;

  /* EMail data tab */
  GtkWidget *entry_email;
  GtkWidget *entry_alias;
  GtkWidget *entry_remarks;
  GtkWidget *list_email;

  /* Attribute data tab */
  GtkWidget *entry_atname;
  GtkWidget *entry_atvalue;
  GtkWidget *list_attrib;

  gboolean editNew;
} personeditdlg;

typedef enum {
  EMAIL_COL_EMAIL = 0,
  EMAIL_COL_ALIAS = 1,
  EMAIL_COL_REMARKS = 2
} PersonEditEMailColumnPos;

typedef enum {
  ATTRIB_COL_NAME = 0,
  ATTRIB_COL_VALUE = 1
} PersonEditAttribColumnPos;

#define EDITPERSON_WIDTH      520
#define EDITPERSON_HEIGHT     340

#define EMAIL_N_COLS          3
#define EMAIL_COL_WIDTH_EMAIL 180
#define EMAIL_COL_WIDTH_ALIAS 80

#define ATTRIB_N_COLS          2
#define ATTRIB_COL_WIDTH_NAME  120
#define ATTRIB_COL_WIDTH_VALUE 180

#define PAGE_BASIC             0
#define PAGE_EMAIL             1
#define PAGE_ATTRIBUTES        2

static void
edit_person_status_show (gchar * msg)
{
  if (personeditdlg.statusbar != NULL)
    {
      gtk_statusbar_pop (GTK_STATUSBAR (personeditdlg.statusbar), personeditdlg.status_cid);
      if (msg)
        gtk_statusbar_push (GTK_STATUSBAR (personeditdlg.statusbar), personeditdlg.status_cid, msg);
    }
}

static void
edit_person_ok (GtkWidget * widget, gboolean * cancelled)
{
  *cancelled = FALSE;
  gtk_main_quit ();
}

static void
edit_person_cancel (GtkWidget * widget, gboolean * cancelled)
{
  *cancelled = TRUE;
  gtk_main_quit ();
}

static gint
edit_person_delete_event (GtkWidget * widget, GdkEventAny * event, gboolean * cancelled)
{
  *cancelled = TRUE;
  gtk_main_quit ();
  return TRUE;
}

static gboolean
edit_person_key_pressed (GtkWidget * widget, GdkEventKey * event, gboolean * cancelled)
{
  if (event && event->keyval == GDK_KEY_Escape)
    {
      *cancelled = TRUE;
      gtk_main_quit ();
    }
  return FALSE;
}

static gchar *_title_new_ = NULL;
static gchar *_title_edit_ = NULL;

static void
edit_person_set_window_title (gint pageNum)
{
  gchar *sTitle;

  if (_title_new_ == NULL)
    {
      _title_new_ = g_strdup (_("Add New Person"));
      _title_edit_ = g_strdup (_("Edit Person Details"));
    }

  if (pageNum == PAGE_BASIC)
    {
      if (personeditdlg.editNew)
        gtk_window_set_title (GTK_WINDOW (personeditdlg.window), _title_new_);
      else
        gtk_window_set_title (GTK_WINDOW (personeditdlg.window), _title_edit_);
    }
  else
    {
      if (personeditdlg.entry_name == NULL)
        sTitle = g_strdup (_title_edit_);
      else
        {
          gchar *name;
          name = gtk_editable_get_chars (GTK_EDITABLE (personeditdlg.entry_name), 0, -1);
          sTitle = g_strdup_printf ("%s - %s", _title_edit_, name);
          g_free (name);
        }
      gtk_window_set_title (GTK_WINDOW (personeditdlg.window), sTitle);
      g_free (sTitle);
    }
}

static void
edit_person_email_clear (gpointer data)
{
  gtk_entry_set_text (GTK_ENTRY (personeditdlg.entry_email), "");
  gtk_entry_set_text (GTK_ENTRY (personeditdlg.entry_alias), "");
  gtk_entry_set_text (GTK_ENTRY (personeditdlg.entry_remarks), "");
}

static void
edit_person_attrib_clear (gpointer data)
{
  gtk_entry_set_text (GTK_ENTRY (personeditdlg.entry_atname), "");
  gtk_entry_set_text (GTK_ENTRY (personeditdlg.entry_atvalue), "");
}

static void
edit_person_switch_page (GtkNotebook * notebook, GtkWidget * page, gint pageNum, gpointer user_data)
{
  edit_person_set_window_title (pageNum);
  edit_person_status_show ("");
}

/*
* Load list with a copy of person's email addresses.
*/
void
edit_person_load_email (ItemPerson * person)
{
  GtkListStore *store;
  GtkTreeIter iter;
  GList *node = person->listEMail;

  store = GTK_LIST_STORE (gtk_tree_view_get_model (GTK_TREE_VIEW (personeditdlg.list_email)));

  while (node)
    {
      ItemEMail *email = (ItemEMail *) node->data;

      gtk_list_store_append (store, &iter);
      gtk_list_store_set (store, &iter, 0, email->address, 1, email->obj.name, 2, email->remarks, 3, email, -1);
      node = g_list_next (node);
    }
}

static void
edit_person_email_list_selected (GtkTreeView *list, GtkTreePath *path, GtkTreeViewColumn *col, gpointer data)
{
  GtkTreeModel *model;
  GtkTreeIter iter;
  ItemEMail *email = NULL;

  model = gtk_tree_view_get_model (list);
  gtk_tree_model_get_iter (model, &iter, path);
  gtk_tree_model_get (model, &iter, 3, &email, -1);

  if (email)
    {
      if (email->address)
        gtk_entry_set_text (GTK_ENTRY (personeditdlg.entry_email), email->address);
      if (ADDRITEM_NAME (email))
        gtk_entry_set_text (GTK_ENTRY (personeditdlg.entry_alias), ADDRITEM_NAME (email));
      if (email->remarks)
        gtk_entry_set_text (GTK_ENTRY (personeditdlg.entry_remarks), email->remarks);
    }
  edit_person_status_show (NULL);
}

static void
edit_person_email_move_up (gpointer data)
{
  GtkTreeSelection *sel;
  GtkTreeModel *model;
  GtkTreeIter iter, pos;

  sel = gtk_tree_view_get_selection (GTK_TREE_VIEW (personeditdlg.list_email));

  if (!gtk_tree_selection_get_selected (sel, &model, &iter))
    return;

  pos = iter;
  if (gtk_tree_model_iter_previous (model, &pos))
    {
      gtk_list_store_move_before (GTK_LIST_STORE (model), &iter, &pos);

      edit_person_email_clear (NULL);
      edit_person_status_show (NULL);
    }
}

static void
edit_person_email_move_down (gpointer data)
{
  GtkTreeSelection *sel;
  GtkTreeModel *model;
  GtkTreeIter iter, pos;

  sel = gtk_tree_view_get_selection (GTK_TREE_VIEW (personeditdlg.list_email));

  if (!gtk_tree_selection_get_selected (sel, &model, &iter))
    return;

  pos = iter;
  if (gtk_tree_model_iter_next (model, &pos))
    {
      gtk_list_store_move_after (GTK_LIST_STORE (model), &iter, &pos);

      edit_person_email_clear (NULL);
      edit_person_status_show (NULL);
    }
}

static void
edit_person_email_delete (gpointer data)
{
  GtkTreeSelection *sel;
  GtkTreeModel *model;
  GtkTreeIter iter;

  sel = gtk_tree_view_get_selection (GTK_TREE_VIEW (personeditdlg.list_email));

  if (!gtk_tree_selection_get_selected (sel, &model, &iter))
    return;

  gtk_list_store_remove (GTK_LIST_STORE (model), &iter);

  edit_person_email_clear (NULL);
  edit_person_status_show (NULL);
}

static ItemEMail *
edit_person_email_edit (gboolean * error, ItemEMail * email)
{
  ItemEMail *retVal = NULL;
  gchar *sEmail, *sAlias, *sRemarks, *sEmail_;

  *error = TRUE;
  sEmail_ = gtk_editable_get_chars (GTK_EDITABLE (personeditdlg.entry_email), 0, -1);
  sAlias = gtk_editable_get_chars (GTK_EDITABLE (personeditdlg.entry_alias), 0, -1);
  sRemarks = gtk_editable_get_chars (GTK_EDITABLE (personeditdlg.entry_remarks), 0, -1);
  sEmail = mgu_email_check_empty (sEmail_);
  g_free (sEmail_);

  if (sEmail)
    {
      if (email == NULL)
        email = addritem_create_item_email ();
      addritem_email_set_address (email, sEmail);
      addritem_email_set_alias (email, sAlias);
      addritem_email_set_remarks (email, sRemarks);
      retVal = email;
      *error = FALSE;
    }
  else
    edit_person_status_show (_("An E-Mail address must be supplied."));

  g_free (sEmail);
  g_free (sAlias);
  g_free (sRemarks);

  return retVal;
}

static void
edit_person_email_modify (gpointer data)
{
  GtkTreeSelection *sel;
  GtkTreeModel *model;
  GtkTreeIter iter;
  gboolean errFlg = FALSE;
  ItemEMail *email = NULL;

  sel = gtk_tree_view_get_selection (GTK_TREE_VIEW (personeditdlg.list_email));

  if (!gtk_tree_selection_get_selected (sel, &model, &iter))
    return;

  gtk_tree_model_get (model, &iter, 3, &email, -1);

  if (email)
    {
      edit_person_email_edit (&errFlg, email);
      if (!errFlg)
        {
          gtk_list_store_set (GTK_LIST_STORE (model), &iter, 0, email->address, 1, email->obj.name, 2, email->remarks, -1);
          edit_person_email_clear (NULL);
        }
    }
}

static void
edit_person_email_add (gpointer data)
{
  GtkListStore *model;
  GtkTreeIter iter;
  gboolean errFlg = FALSE;
  ItemEMail *email = NULL;

  model = GTK_LIST_STORE (gtk_tree_view_get_model (GTK_TREE_VIEW (personeditdlg.list_email)));

  email = edit_person_email_edit (&errFlg, NULL);
  if (!errFlg)
    {
      gtk_list_store_append (model, &iter);
      gtk_list_store_set (model, &iter, 0, email->address, 1, email->obj.name, 2, email->remarks, 3, email, -1);
      edit_person_email_clear (NULL);
    }
}

/*
* Load list with a copy of person's email addresses.
*/
void
edit_person_load_attrib (ItemPerson * person)
{
  GtkListStore *store;
  GtkTreeIter iter;
  GList *node = person->listEMail;

  store = GTK_LIST_STORE (gtk_tree_view_get_model (GTK_TREE_VIEW (personeditdlg.list_attrib)));

  while (node)
    {
      UserAttribute *attrib = (UserAttribute *) node->data;

      gtk_list_store_append (store, &iter);
      gtk_list_store_set (store, &iter, 0, attrib->name, 1, attrib->value, 2, attrib, -1);
      node = g_list_next (node);
    }
}

static void
edit_person_attrib_list_selected (GtkTreeView *list, GtkTreePath *path, GtkTreeViewColumn *col, gpointer data)
{
  GtkTreeModel *model;
  GtkTreeIter iter;
  UserAttribute *attrib = NULL;

  model = gtk_tree_view_get_model (list);
  gtk_tree_model_get_iter (model, &iter, path);
  gtk_tree_model_get (model, &iter, 2, &attrib, -1);

  if (attrib)
    {
      gtk_entry_set_text (GTK_ENTRY (personeditdlg.entry_atname), attrib->name);
      gtk_entry_set_text (GTK_ENTRY (personeditdlg.entry_atvalue), attrib->value);
    }
  edit_person_status_show (NULL);
}

static void
edit_person_attrib_delete (gpointer data)
{
  GtkTreeSelection *sel;
  GtkTreeModel *model;
  GtkTreeIter iter;

  sel = gtk_tree_view_get_selection (GTK_TREE_VIEW (personeditdlg.list_attrib));

  if (!gtk_tree_selection_get_selected (sel, &model, &iter))
    return;

  gtk_list_store_remove (GTK_LIST_STORE (model), &iter);

  edit_person_status_show (NULL);
}

static UserAttribute *
edit_person_attrib_edit (gboolean * error, UserAttribute * attrib)
{
  UserAttribute *retVal = NULL;
  gchar *sName, *sValue, *sName_, *sValue_;

  *error = TRUE;
  sName_ = gtk_editable_get_chars (GTK_EDITABLE (personeditdlg.entry_atname), 0, -1);
  sValue_ = gtk_editable_get_chars (GTK_EDITABLE (personeditdlg.entry_atvalue), 0, -1);
  sName = mgu_email_check_empty (sName_);
  sValue = mgu_email_check_empty (sValue_);
  g_free (sName_);
  g_free (sValue_);

  if (sName && sValue)
    {
      if (attrib == NULL)
        attrib = addritem_create_attribute ();
      addritem_attrib_set_name (attrib, sName);
      addritem_attrib_set_value (attrib, sValue);
      retVal = attrib;
      *error = FALSE;
    }
  else
    edit_person_status_show (_("A Name and Value must be supplied."));

  g_free (sName);
  g_free (sValue);

  return retVal;
}

static void
edit_person_attrib_modify (gpointer data)
{
  GtkTreeSelection *sel;
  GtkTreeModel *model;
  GtkTreeIter iter;
  gboolean errFlg = FALSE;
  UserAttribute *attrib = NULL;

  sel = gtk_tree_view_get_selection (GTK_TREE_VIEW (personeditdlg.list_attrib));

  if (!gtk_tree_selection_get_selected (sel, &model, &iter))
    return;

  gtk_tree_model_get (model, &iter, 2, &attrib, -1);

  if (attrib)
    {
      edit_person_attrib_edit (&errFlg, attrib);
      if (!errFlg)
        {
          gtk_list_store_set (GTK_LIST_STORE (model), &iter, 0, attrib->name, 1, attrib->value, -1);
          edit_person_attrib_clear (NULL);
        }
    }
}

static void
edit_person_attrib_add (gpointer data)
{
  GtkListStore *model;
  GtkTreeIter iter;
  gboolean errFlg = FALSE;
  UserAttribute *attrib = NULL;

  model = GTK_LIST_STORE (gtk_tree_view_get_model (GTK_TREE_VIEW (personeditdlg.list_email)));

  attrib = edit_person_attrib_edit (&errFlg, NULL);
  if (!errFlg)
    {
      gtk_list_store_append (model, &iter);
      gtk_list_store_set (model, &iter, 0, attrib->name, 1, attrib->value, 2, attrib, -1);
      edit_person_attrib_clear (NULL);
    }
}

static void
addressbook_edit_person_dialog_create (gboolean * cancelled)
{
  GtkWidget *window;
  GtkWidget *vbox;
  GtkWidget *vnbox;
  GtkWidget *notebook;
  GtkWidget *hbbox;
  GtkWidget *ok_btn;
  GtkWidget *cancel_btn;
  GtkWidget *hsbox;
  GtkWidget *statusbar;

  window = gtk_window_new (GTK_WINDOW_TOPLEVEL);
  gtk_widget_set_size_request (window, EDITPERSON_WIDTH, EDITPERSON_HEIGHT);
  gtk_window_set_title (GTK_WINDOW (window), _("Edit Person Data"));
  gtk_window_set_position (GTK_WINDOW (window), GTK_WIN_POS_CENTER_ON_PARENT);
  gtk_window_set_modal (GTK_WINDOW (window), TRUE);
  gtk_widget_realize (window);
  g_signal_connect (G_OBJECT (window), "delete_event", G_CALLBACK (edit_person_delete_event), cancelled);
  g_signal_connect (G_OBJECT (window), "key_press_event", G_CALLBACK (edit_person_key_pressed), cancelled);

  vbox = gtk_box_new (GTK_ORIENTATION_VERTICAL, 4);
  gtk_widget_show (vbox);
  gtk_container_add (GTK_CONTAINER (window), vbox);

  vnbox = gtk_box_new (GTK_ORIENTATION_VERTICAL, 4);
  gtk_container_set_border_width (GTK_CONTAINER (vnbox), 4);
  gtk_widget_show (vnbox);
  gtk_box_pack_start (GTK_BOX (vbox), vnbox, TRUE, TRUE, 0);

  /* Notebook */
  notebook = gtk_notebook_new ();
  gtk_widget_show (notebook);
  gtk_box_pack_start (GTK_BOX (vnbox), notebook, TRUE, TRUE, 0);
  gtk_container_set_border_width (GTK_CONTAINER (notebook), 6);

  /* Status line */
  hsbox = gtk_box_new (GTK_ORIENTATION_HORIZONTAL, 0);
  gtk_box_pack_end (GTK_BOX (vbox), hsbox, FALSE, FALSE, 0);
  statusbar = gtk_statusbar_new ();
  gtk_box_pack_start (GTK_BOX (hsbox), statusbar, TRUE, TRUE, 0);

  /* Button panel */
  yam_stock_button_set_create (&hbbox, &ok_btn, "yam-ok", &cancel_btn, "yam-cancel", NULL, NULL);
  gtk_box_pack_end (GTK_BOX (vnbox), hbbox, FALSE, FALSE, 0);
  gtk_widget_grab_default (ok_btn);

  g_signal_connect (G_OBJECT (ok_btn), "clicked", G_CALLBACK (edit_person_ok), cancelled);
  g_signal_connect (G_OBJECT (cancel_btn), "clicked", G_CALLBACK (edit_person_cancel), cancelled);
  g_signal_connect (G_OBJECT (notebook), "switch_page", G_CALLBACK (edit_person_switch_page), NULL);

  gtk_widget_show_all (vbox);

  personeditdlg.window = window;
  personeditdlg.notebook = notebook;
  personeditdlg.hbbox = hbbox;
  personeditdlg.ok_btn = ok_btn;
  personeditdlg.cancel_btn = cancel_btn;
  personeditdlg.statusbar = statusbar;
  personeditdlg.status_cid = gtk_statusbar_get_context_id (GTK_STATUSBAR (statusbar), "Edit Person Dialog");
}

void
addressbook_edit_person_page_basic (gint pageNum, gchar * pageLbl)
{
  GtkWidget *vbox;
  GtkWidget *table;
  GtkWidget *label;
  GtkWidget *entry_name;
  GtkWidget *entry_fn;
  GtkWidget *entry_ln;
  GtkWidget *entry_nn;
  const gchar *locale;

  vbox = gtk_box_new (GTK_ORIENTATION_VERTICAL, 8);
  gtk_widget_show (vbox);
  gtk_container_add (GTK_CONTAINER (personeditdlg.notebook), vbox);
  gtk_container_set_border_width (GTK_CONTAINER (vbox), BORDER_WIDTH);

  label = gtk_label_new (pageLbl);
  gtk_widget_show (label);
  gtk_notebook_set_tab_label (GTK_NOTEBOOK (personeditdlg.notebook),
                              gtk_notebook_get_nth_page (GTK_NOTEBOOK (personeditdlg.notebook), pageNum), label);

  table = gtk_grid_new ();
  gtk_box_pack_start (GTK_BOX (vbox), table, FALSE, FALSE, 0);
  gtk_container_set_border_width (GTK_CONTAINER (table), 5);
  gtk_grid_set_row_spacing (GTK_GRID (table), 5);
  gtk_grid_set_column_spacing (GTK_GRID (table), 5);

#define ATTACH_ROW(text, entry, row)                        \
  {                                                         \
	label = gtk_label_new(text);                            \
	gtk_grid_attach(GTK_GRID(table), label, 0, row, 1, 1);  \
	gtk_label_set_xalign(GTK_LABEL(label), 0.0);            \
                                                            \
	entry = gtk_entry_new();                                \
    gtk_widget_set_hexpand(entry, TRUE);                    \
    gtk_grid_attach(GTK_GRID(table), entry, 1, row, 1, 1);  \
  }

  ATTACH_ROW (_("Display Name"), entry_name, 0);
  locale = conv_get_current_locale ();
  if (locale && (!g_ascii_strncasecmp (locale, "ja", 2) ||
                 !g_ascii_strncasecmp (locale, "ko", 2) ||
                 !g_ascii_strncasecmp (locale, "zh", 2)))
    {
      ATTACH_ROW (_("Last Name"), entry_ln, 1);
      ATTACH_ROW (_("First Name"), entry_fn, 2);
    }
  else
    {
      ATTACH_ROW (_("First Name"), entry_fn, 1);
      ATTACH_ROW (_("Last Name"), entry_ln, 2);
    }
  ATTACH_ROW (_("Nick Name"), entry_nn, 3);

#undef ATTACH_ROW

  gtk_widget_show_all (vbox);

  personeditdlg.entry_name = entry_name;
  personeditdlg.entry_first = entry_fn;
  personeditdlg.entry_last = entry_ln;
  personeditdlg.entry_nick = entry_nn;
}

void
addressbook_edit_person_page_email (gint pageNum, gchar * pageLbl)
{
  GtkWidget *vbox;
  GtkWidget *hbox;
  GtkWidget *vboxl;
  GtkWidget *vboxb;
  GtkWidget *vbuttonbox;
  GtkWidget *buttonUp;
  GtkWidget *buttonDown;
  GtkWidget *buttonDel;
  GtkWidget *buttonMod;
  GtkWidget *buttonAdd;
  GtkWidget *buttonClr;

  GtkListStore *store;
  GtkTreeViewColumn *col;
  GtkCellRenderer *renderer;

  GtkWidget *table;
  GtkWidget *label;
  GtkWidget *list_swin;
  GtkWidget *list;
  GtkWidget *entry_email;
  GtkWidget *entry_alias;
  GtkWidget *entry_remarks;

  vbox = gtk_box_new (GTK_ORIENTATION_VERTICAL, 8);
  gtk_widget_show (vbox);
  gtk_container_add (GTK_CONTAINER (personeditdlg.notebook), vbox);
  gtk_container_set_border_width (GTK_CONTAINER (vbox), BORDER_WIDTH);

  label = gtk_label_new (pageLbl);
  gtk_widget_show (label);
  gtk_notebook_set_tab_label (GTK_NOTEBOOK (personeditdlg.notebook),
                              gtk_notebook_get_nth_page (GTK_NOTEBOOK (personeditdlg.notebook), pageNum), label);

  /* Split into two areas */
  hbox = gtk_box_new (GTK_ORIENTATION_HORIZONTAL, 0);
  gtk_container_add (GTK_CONTAINER (vbox), hbox);

  /* EMail list */
  vboxl = gtk_box_new (GTK_ORIENTATION_VERTICAL, 4);
  gtk_container_add (GTK_CONTAINER (hbox), vboxl);
  gtk_container_set_border_width (GTK_CONTAINER (vboxl), 4);

  /* Address list */
  list_swin = gtk_scrolled_window_new (NULL, NULL);
  gtk_container_add (GTK_CONTAINER (vboxl), list_swin);
  gtk_scrolled_window_set_policy (GTK_SCROLLED_WINDOW (list_swin), GTK_POLICY_AUTOMATIC, GTK_POLICY_ALWAYS);

  store = gtk_list_store_new (4, G_TYPE_STRING, G_TYPE_STRING, G_TYPE_STRING, G_TYPE_POINTER);
  list = gtk_tree_view_new_with_model (GTK_TREE_MODEL (store));
  gtk_tree_view_set_headers_visible (GTK_TREE_VIEW (list), TRUE);
  gtk_tree_view_set_activate_on_single_click (GTK_TREE_VIEW (list), TRUE);
  gtk_tree_selection_set_mode (gtk_tree_view_get_selection (GTK_TREE_VIEW (list)), GTK_SELECTION_BROWSE);
  gtk_container_add (GTK_CONTAINER (list_swin), list);
  g_object_unref (store);

  renderer = gtk_cell_renderer_text_new ();
  col = gtk_tree_view_column_new_with_attributes (_("E-Mail Address"), renderer, "text", 0, NULL);
  gtk_tree_view_column_set_expand (col, TRUE);
  gtk_tree_view_append_column (GTK_TREE_VIEW (list), col);

  renderer = gtk_cell_renderer_text_new ();
  col = gtk_tree_view_column_new_with_attributes (_("Alias"), renderer, "text", 1, NULL);
  gtk_tree_view_column_set_expand (col, TRUE);
  gtk_tree_view_append_column (GTK_TREE_VIEW (list), col);

  renderer = gtk_cell_renderer_text_new ();
  col = gtk_tree_view_column_new_with_attributes (_("Remarks"), renderer, "text", 2, NULL);
  gtk_tree_view_column_set_expand (col, TRUE);
  gtk_tree_view_append_column (GTK_TREE_VIEW (list), col);

  /* Data entry area */
  table = gtk_grid_new ();
  gtk_box_pack_start (GTK_BOX (vboxl), table, FALSE, FALSE, 0);
  gtk_container_set_border_width (GTK_CONTAINER (table), 5);
  gtk_grid_set_row_spacing (GTK_GRID (table), 5);
  gtk_grid_set_column_spacing (GTK_GRID (table), 5);

  /* First row */
  label = gtk_label_new (_("E-Mail Address"));
  gtk_grid_attach (GTK_GRID (table), label, 0, 0, 1, 1);
  gtk_label_set_xalign (GTK_LABEL (label), 0.0);

  entry_email = gtk_entry_new ();
  gtk_widget_set_hexpand (entry_email, TRUE);
  gtk_grid_attach (GTK_GRID (table), entry_email, 1, 0, 1, 1);

  /* Next row */
  label = gtk_label_new (_("Alias"));
  gtk_grid_attach (GTK_GRID (table), label, 0, 1, 1, 1);
  gtk_label_set_xalign (GTK_LABEL (label), 0.0);

  entry_alias = gtk_entry_new ();
  gtk_widget_set_hexpand (entry_alias, TRUE);
  gtk_grid_attach (GTK_GRID (table), entry_alias, 1, 1, 1, 1);

  /* Next row */
  label = gtk_label_new (_("Remarks"));
  gtk_grid_attach (GTK_GRID (table), label, 0, 2, 1, 1);
  gtk_label_set_xalign (GTK_LABEL (label), 0.0);

  entry_remarks = gtk_entry_new ();
  gtk_widget_set_hexpand (entry_remarks, TRUE);
  gtk_grid_attach (GTK_GRID (table), entry_remarks, 1, 2, 1, 1);

  /* Button box */
  vboxb = gtk_box_new (GTK_ORIENTATION_VERTICAL, 4);
  gtk_box_pack_start (GTK_BOX (hbox), vboxb, FALSE, FALSE, 2);

  vbuttonbox = gtk_button_box_new (GTK_ORIENTATION_VERTICAL);
  gtk_button_box_set_layout (GTK_BUTTON_BOX (vbuttonbox), GTK_BUTTONBOX_START);
  gtk_box_set_spacing (GTK_BOX (vbuttonbox), 8);
  gtk_container_set_border_width (GTK_CONTAINER (vbuttonbox), 4);
  gtk_container_add (GTK_CONTAINER (vboxb), vbuttonbox);

  /* Buttons */
  buttonUp = gtk_button_new_with_label (_("Move Up"));
  gtk_container_add (GTK_CONTAINER (vbuttonbox), buttonUp);

  buttonDown = gtk_button_new_with_label (_("Move Down"));
  gtk_container_add (GTK_CONTAINER (vbuttonbox), buttonDown);

  buttonDel = gtk_button_new_with_label (_("Delete"));
  gtk_container_add (GTK_CONTAINER (vbuttonbox), buttonDel);

  buttonMod = gtk_button_new_with_label (_("Modify"));
  gtk_container_add (GTK_CONTAINER (vbuttonbox), buttonMod);

  buttonAdd = gtk_button_new_with_label (_("Add"));
  gtk_container_add (GTK_CONTAINER (vbuttonbox), buttonAdd);

  buttonClr = gtk_button_new_with_label (_("Clear"));
  gtk_container_add (GTK_CONTAINER (vbuttonbox), buttonClr);

  gtk_widget_show_all (vbox);

  /* Event handlers */
  g_signal_connect (G_OBJECT (list), "row-activated", G_CALLBACK (edit_person_email_list_selected), NULL);
  g_signal_connect (G_OBJECT (buttonUp), "clicked", G_CALLBACK (edit_person_email_move_up), NULL);
  g_signal_connect (G_OBJECT (buttonDown), "clicked", G_CALLBACK (edit_person_email_move_down), NULL);
  g_signal_connect (G_OBJECT (buttonDel), "clicked", G_CALLBACK (edit_person_email_delete), NULL);
  g_signal_connect (G_OBJECT (buttonMod), "clicked", G_CALLBACK (edit_person_email_modify), NULL);
  g_signal_connect (G_OBJECT (buttonAdd), "clicked", G_CALLBACK (edit_person_email_add), NULL);
  g_signal_connect (G_OBJECT (buttonClr), "clicked", G_CALLBACK (edit_person_email_clear), NULL);

  personeditdlg.list_email = list;
  personeditdlg.entry_email = entry_email;
  personeditdlg.entry_alias = entry_alias;
  personeditdlg.entry_remarks = entry_remarks;
}

void
addressbook_edit_person_page_attrib (gint pageNum, gchar * pageLbl)
{
  GtkWidget *vbox;
  GtkWidget *hbox;
  GtkWidget *vboxl;
  GtkWidget *vboxb;
  GtkWidget *vbuttonbox;
  GtkWidget *buttonDel;
  GtkWidget *buttonMod;
  GtkWidget *buttonAdd;
  GtkWidget *buttonClr;

  GtkListStore *store;
  GtkTreeViewColumn *col;
  GtkCellRenderer *renderer;

  GtkWidget *table;
  GtkWidget *label;
  GtkWidget *list_swin;
  GtkWidget *list;
  GtkWidget *entry_name;
  GtkWidget *entry_value;

  vbox = gtk_box_new (GTK_ORIENTATION_VERTICAL, 8);
  gtk_widget_show (vbox);
  gtk_container_add (GTK_CONTAINER (personeditdlg.notebook), vbox);
  gtk_container_set_border_width (GTK_CONTAINER (vbox), BORDER_WIDTH);

  label = gtk_label_new (pageLbl);
  gtk_widget_show (label);
  gtk_notebook_set_tab_label (GTK_NOTEBOOK (personeditdlg.notebook),
                              gtk_notebook_get_nth_page (GTK_NOTEBOOK (personeditdlg.notebook), pageNum), label);

  /* Split into two areas */
  hbox = gtk_box_new (GTK_ORIENTATION_HORIZONTAL, 0);
  gtk_container_add (GTK_CONTAINER (vbox), hbox);

  /* Attribute list */
  vboxl = gtk_box_new (GTK_ORIENTATION_VERTICAL, 4);
  gtk_container_add (GTK_CONTAINER (hbox), vboxl);
  gtk_container_set_border_width (GTK_CONTAINER (vboxl), 4);

  /* Address list */
  list_swin = gtk_scrolled_window_new (NULL, NULL);
  gtk_container_add (GTK_CONTAINER (vboxl), list_swin);
  gtk_scrolled_window_set_policy (GTK_SCROLLED_WINDOW (list_swin), GTK_POLICY_AUTOMATIC, GTK_POLICY_ALWAYS);

  store = gtk_list_store_new (3, G_TYPE_STRING, G_TYPE_STRING, G_TYPE_POINTER);
  list = gtk_tree_view_new_with_model (GTK_TREE_MODEL (store));
  gtk_tree_view_set_headers_visible (GTK_TREE_VIEW (list), TRUE);
  gtk_tree_view_set_activate_on_single_click (GTK_TREE_VIEW (list), TRUE);
  gtk_tree_selection_set_mode (gtk_tree_view_get_selection (GTK_TREE_VIEW (list)), GTK_SELECTION_BROWSE);
  gtk_container_add (GTK_CONTAINER (list_swin), list);
  g_object_unref (store);

  renderer = gtk_cell_renderer_text_new ();
  col = gtk_tree_view_column_new_with_attributes (_("Name"), renderer, "text", 0, NULL);
  gtk_tree_view_column_set_expand (col, TRUE);
  gtk_tree_view_append_column (GTK_TREE_VIEW (list), col);

  renderer = gtk_cell_renderer_text_new ();
  col = gtk_tree_view_column_new_with_attributes (_("Value"), renderer, "text", 1, NULL);
  gtk_tree_view_column_set_expand (col, TRUE);
  gtk_tree_view_append_column (GTK_TREE_VIEW (list), col);

  /* Data entry area */
  table = gtk_grid_new ();
  gtk_box_pack_start (GTK_BOX (vboxl), table, FALSE, FALSE, 0);
  gtk_container_set_border_width (GTK_CONTAINER (table), 5);
  gtk_grid_set_row_spacing (GTK_GRID (table), 5);
  gtk_grid_set_column_spacing (GTK_GRID (table), 5);

  /* First row */
  label = gtk_label_new (_("Name"));
  gtk_grid_attach (GTK_GRID (table), label, 0, 0, 1, 1);
  gtk_label_set_xalign (GTK_LABEL (label), 0.0);

  entry_name = gtk_entry_new ();
  gtk_widget_set_hexpand (entry_name, TRUE);
  gtk_grid_attach (GTK_GRID (table), entry_name, 1, 0, 1, 1);

  /* Next row */
  label = gtk_label_new (_("Value"));
  gtk_grid_attach (GTK_GRID (table), label, 0, 1, 1, 1);
  gtk_label_set_xalign (GTK_LABEL (label), 0.0);

  entry_value = gtk_entry_new ();
  gtk_widget_set_hexpand (entry_value, TRUE);
  gtk_grid_attach (GTK_GRID (table), entry_value, 1, 1, 1, 1);

  /* Button box */
  vboxb = gtk_box_new (GTK_ORIENTATION_VERTICAL, 4);
  gtk_box_pack_start (GTK_BOX (hbox), vboxb, FALSE, FALSE, 2);

  vbuttonbox = gtk_button_box_new (GTK_ORIENTATION_VERTICAL);
  gtk_button_box_set_layout (GTK_BUTTON_BOX (vbuttonbox), GTK_BUTTONBOX_START);
  gtk_box_set_spacing (GTK_BOX (vbuttonbox), 8);
  gtk_container_set_border_width (GTK_CONTAINER (vbuttonbox), 4);
  gtk_container_add (GTK_CONTAINER (vboxb), vbuttonbox);

  /* Buttons */
  buttonDel = gtk_button_new_with_label (_("Delete"));
  gtk_container_add (GTK_CONTAINER (vbuttonbox), buttonDel);

  buttonMod = gtk_button_new_with_label (_("Modify"));
  gtk_container_add (GTK_CONTAINER (vbuttonbox), buttonMod);

  buttonAdd = gtk_button_new_with_label (_("Add"));
  gtk_container_add (GTK_CONTAINER (vbuttonbox), buttonAdd);

  buttonClr = gtk_button_new_with_label (_("Clear"));
  gtk_container_add (GTK_CONTAINER (vbuttonbox), buttonClr);

  gtk_widget_show_all (vbox);

  /* Event handlers */
  g_signal_connect (G_OBJECT (list), "row-activated", G_CALLBACK (edit_person_attrib_list_selected), NULL);
  g_signal_connect (G_OBJECT (buttonDel), "clicked", G_CALLBACK (edit_person_attrib_delete), NULL);
  g_signal_connect (G_OBJECT (buttonMod), "clicked", G_CALLBACK (edit_person_attrib_modify), NULL);
  g_signal_connect (G_OBJECT (buttonAdd), "clicked", G_CALLBACK (edit_person_attrib_add), NULL);
  g_signal_connect (G_OBJECT (buttonClr), "clicked", G_CALLBACK (edit_person_attrib_clear), NULL);

  personeditdlg.list_attrib = list;
  personeditdlg.entry_atname = entry_name;
  personeditdlg.entry_atvalue = entry_value;
}

static void
addressbook_edit_person_create (gboolean * cancelled)
{
  addressbook_edit_person_dialog_create (cancelled);
  addressbook_edit_person_page_basic (PAGE_BASIC, _("Basic Data"));
  addressbook_edit_person_page_email (PAGE_EMAIL, _("E-Mail Address"));
  addressbook_edit_person_page_attrib (PAGE_ATTRIBUTES, _("User Attributes"));
  //gtk_widget_show_all( personeditdlg.window );
}

/*
* Return list of email items.
*/
static GList *
edit_person_build_email_list ()
{
  GtkTreeModel *model;
  GtkTreeIter iter;
  GList *listEMail = NULL;

  model = gtk_tree_view_get_model (GTK_TREE_VIEW (personeditdlg.list_email));
  if (gtk_tree_model_get_iter_first (model, &iter))
    {
      ItemEMail *email;
      do
        {
          gtk_tree_model_get (model, &iter, 3, &email, -1);
          listEMail = g_list_append (listEMail, email);
        }
      while (gtk_tree_model_iter_next (model, &iter));
    }

  return listEMail;
}

/*
* Return list of attributes.
*/
static GList *
edit_person_build_attrib_list ()
{
  GtkTreeModel *model;
  GtkTreeIter iter;
  GList *listAttrib = NULL;

  model = gtk_tree_view_get_model (GTK_TREE_VIEW (personeditdlg.list_attrib));
  if (gtk_tree_model_get_iter_first (model, &iter))
    {
      UserAttribute *attrib;
      do
        {
          gtk_tree_model_get (model, &iter, 2, &attrib, -1);
          listAttrib = g_list_append (listAttrib, attrib);
        }
      while (gtk_tree_model_iter_next (model, &iter));
    }

  return listAttrib;
}

/*
* Edit person.
* Enter: abf    Address book.
*        parent Parent folder for person (or NULL if adding to root folder). Argument is
*               only required for new objects).
*        person Person to edit, or NULL for a new person object.
*        pgMail If TRUE, E-Mail page will be activated.
* Return: Edited object, or NULL if cancelled.
*/
ItemPerson *
addressbook_edit_person (AddressBookFile * abf, ItemFolder * parent, ItemPerson * person, gboolean pgMail)
{
  static gboolean cancelled;
  GList *listEMail = NULL;
  GList *listAttrib = NULL;
  gchar *cn = NULL;

  if (!personeditdlg.window)
    addressbook_edit_person_create (&cancelled);
  gtk_widget_grab_focus (personeditdlg.ok_btn);
  gtk_widget_grab_focus (personeditdlg.entry_name);
  manage_window_set_transient (GTK_WINDOW (personeditdlg.window));
  gtk_widget_show (personeditdlg.window);

  /* Clear all fields */
  edit_person_status_show ("");
  gtk_list_store_clear (GTK_LIST_STORE (gtk_tree_view_get_model (GTK_TREE_VIEW (personeditdlg.list_email))));
  gtk_list_store_clear (GTK_LIST_STORE (gtk_tree_view_get_model (GTK_TREE_VIEW (personeditdlg.list_attrib))));
  gtk_entry_set_text (GTK_ENTRY (personeditdlg.entry_name), "");
  gtk_entry_set_text (GTK_ENTRY (personeditdlg.entry_first), "");
  gtk_entry_set_text (GTK_ENTRY (personeditdlg.entry_last), "");
  gtk_entry_set_text (GTK_ENTRY (personeditdlg.entry_nick), "");

  personeditdlg.editNew = FALSE;
  if (person)
    {
      if (ADDRITEM_NAME (person))
        gtk_entry_set_text (GTK_ENTRY (personeditdlg.entry_name), ADDRITEM_NAME (person));
      if (person->firstName)
        gtk_entry_set_text (GTK_ENTRY (personeditdlg.entry_first), person->firstName);
      if (person->lastName)
        gtk_entry_set_text (GTK_ENTRY (personeditdlg.entry_last), person->lastName);
      if (person->nickName)
        gtk_entry_set_text (GTK_ENTRY (personeditdlg.entry_nick), person->nickName);
      edit_person_load_email (person);
      edit_person_load_attrib (person);
    }
  else
    personeditdlg.editNew = TRUE;

  /* Select appropriate start page */
  if (pgMail)
    gtk_notebook_set_current_page (GTK_NOTEBOOK (personeditdlg.notebook), PAGE_EMAIL);
  else
    gtk_notebook_set_current_page (GTK_NOTEBOOK (personeditdlg.notebook), PAGE_BASIC);

  edit_person_email_clear (NULL);
  edit_person_attrib_clear (NULL);

  gtk_main ();
  gtk_widget_hide (personeditdlg.window);

  listEMail = edit_person_build_email_list ();
  listAttrib = edit_person_build_attrib_list ();
  if (cancelled)
    {
      addritem_free_list_email (listEMail);
      gtk_list_store_clear (GTK_LIST_STORE (gtk_tree_view_get_model (GTK_TREE_VIEW (personeditdlg.list_email))));
      gtk_list_store_clear (GTK_LIST_STORE (gtk_tree_view_get_model (GTK_TREE_VIEW (personeditdlg.list_attrib))));
      return NULL;
    }

  cn = gtk_editable_get_chars (GTK_EDITABLE (personeditdlg.entry_name), 0, -1);
  if (person)
    {
      /* Update email/attribute list */
      addrbook_update_address_list (abf, person, listEMail);
      addrbook_update_attrib_list (abf, person, listAttrib);
    }
  else
    {
      /* Create new person and email/attribute list */
      if (cn == NULL || *cn == '\0')
        {
          /* Wasting our time */
          if (listEMail == NULL && listAttrib == NULL)
            cancelled = TRUE;
        }
      if (!cancelled)
        {
          person = addrbook_add_address_list (abf, parent, listEMail);
          addrbook_add_attrib_list (abf, person, listAttrib);
        }
    }

  if (!cancelled)
    {
      /* Set person stuff */
      gchar *name;
      addritem_person_set_common_name (person, cn);
      name = gtk_editable_get_chars (GTK_EDITABLE (personeditdlg.entry_first), 0, -1);
      addritem_person_set_first_name (person, name);
      g_free (name);
      name = gtk_editable_get_chars (GTK_EDITABLE (personeditdlg.entry_last), 0, -1);
      addritem_person_set_last_name (person, name);
      g_free (name);
      name = gtk_editable_get_chars (GTK_EDITABLE (personeditdlg.entry_nick), 0, -1);
      addritem_person_set_nick_name (person, name);
      g_free (name);
    }
  g_free (cn);

  listEMail = NULL;

  gtk_list_store_clear (GTK_LIST_STORE (gtk_tree_view_get_model (GTK_TREE_VIEW (personeditdlg.list_email))));
  gtk_list_store_clear (GTK_LIST_STORE (gtk_tree_view_get_model (GTK_TREE_VIEW (personeditdlg.list_attrib))));

  return person;
}

/*
* End of Source.
*/
