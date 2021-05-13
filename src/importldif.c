/*
 * YAM -- a GTK+ based, lightweight, and fast e-mail client
 * Copyright (C) 2020 Victor Ananjevsky <victor@sanana.kiev.ua>
 * Copyright (C) 1999-2017 Hiroyuki Yamamoto
 * Copyright (C) 2001 Match Grun
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

#include "addrbook.h"
#include "addressbook.h"
#include "addressitem.h"
#include "filesel.h"
#include "gtkutils.h"
#include "stock_pixmap.h"
#include "prefs_common.h"
#include "manage_window.h"
#include "mgutils.h"
#include "ldif.h"
#include "codeconv.h"
#include "utils.h"

#define IMPORTLDIF_GUESS_NAME "LDIF Import"

#define PAGE_FILE_INFO             0
#define PAGE_ATTRIBUTES            1
#define PAGE_FINISH                2

#define IMPORTLDIF_WIDTH           480
#define IMPORTLDIF_HEIGHT          300

#define FIELDS_N_COLS              3

typedef enum {
  FIELD_COL_SELECT = 0,
  FIELD_COL_FIELD = 1,
  FIELD_COL_ATTRIB = 2
} ImpLdif_FieldColPos;

static struct _ImpLdif_Dlg {
  GtkWidget *window;
  GtkWidget *notebook;
  GtkWidget *file_entry;
  GtkWidget *name_entry;
  GtkWidget *list_field;
  GtkWidget *name_ldif;
  GtkWidget *name_attrib;
  GtkWidget *check_select;
  GtkWidget *labelBook;
  GtkWidget *labelFile;
  GtkWidget *labelRecords;
  GtkWidget *btnPrev;
  GtkWidget *btnNext;
  GtkWidget *btnCancel;
  GtkWidget *statusbar;
  gint status_cid;
  gint rowCount;
  gchar *nameBook;
  gchar *fileName;
  gboolean cancelled;
} impldif_dlg;

static AddressBookFile *_importedBook_;
static AddressIndex *_imp_addressIndex_;
static LdifFile *_ldifFile_ = NULL;

static void
imp_ldif_status_show (gchar * msg)
{
  if (impldif_dlg.statusbar != NULL)
    {
      gtk_statusbar_pop (GTK_STATUSBAR (impldif_dlg.statusbar), impldif_dlg.status_cid);
      if (msg)
        gtk_statusbar_push (GTK_STATUSBAR (impldif_dlg.statusbar), impldif_dlg.status_cid, msg);
    }
}

static void
imp_ldif_message (void)
{
  gchar *sMsg = NULL;
  gint pageNum;

  pageNum = gtk_notebook_get_current_page (GTK_NOTEBOOK (impldif_dlg.notebook));
  if (pageNum == PAGE_FILE_INFO)
    sMsg = _("Please specify address book name and file to import.");
  else if (pageNum == PAGE_ATTRIBUTES)
    sMsg = _("Select and rename LDIF field names to import.");
  else if (pageNum == PAGE_FINISH)
    sMsg = _("File imported.");
  imp_ldif_status_show (sMsg);
}

static gchar *
imp_ldif_guess_file (AddressBookFile * abf)
{
  gchar *newFile = NULL;
  GList *fileList = NULL;
  gint fileNum = 1;
  fileList = addrbook_get_bookfile_list (abf);
  if (fileList)
    fileNum = 1 + abf->maxValue;
  newFile = addrbook_gen_new_file_name (fileNum);
  g_list_free (fileList);
  fileList = NULL;
  return newFile;
}

static void
imp_ldif_load_fields (LdifFile * ldf)
{
  GtkListStore *store;
  GtkTreeIter iter;
  GList *node, *list;

  store = GTK_LIST_STORE (gtk_tree_view_get_model (GTK_TREE_VIEW (impldif_dlg.list_field)));

  impldif_dlg.rowCount = 0;
  if (!ldf->accessFlag)
    return;
  gtk_list_store_clear (store);
  list = ldif_get_fieldlist (ldf);
  node = list;
  while (node)
    {
      Ldif_FieldRec *rec = node->data;

      if (!rec->reserved)
        {
          gtk_list_store_append (store, &iter);
          gtk_list_store_set (store, &iter, 0, rec->selected, 1, rec->tagName, 2, rec->userName, 3, rec, -1);
          impldif_dlg.rowCount++;
        }
      node = g_list_next (node);
    }
  g_list_free (list);
  list = NULL;
  ldif_set_accessed (ldf, FALSE);
}

static void
imp_ldif_field_list_selected (GtkTreeView *list, GtkTreePath *path, GtkTreeViewColumn *col, gpointer data)
{
  GtkTreeModel *model;
  GtkTreeIter iter;
  Ldif_FieldRec *rec;

  model = gtk_tree_view_get_model (list);
  gtk_tree_model_get_iter (model, &iter, path);
  gtk_tree_model_get (model, &iter, 3, &rec, -1);

  gtk_entry_set_text (GTK_ENTRY (impldif_dlg.name_attrib), "");
  if (rec)
    {
      gtk_label_set_text (GTK_LABEL (impldif_dlg.name_ldif), rec->tagName);
      if (rec->userName)
        gtk_entry_set_text (GTK_ENTRY (impldif_dlg.name_attrib), rec->userName);
      gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (impldif_dlg.check_select), rec->selected);
    }
  gtk_widget_grab_focus (impldif_dlg.name_attrib);
}

static gboolean
imp_ldif_field_list_toggle (GtkCellRendererToggle *cell, gchar *path_str, gpointer data)
{
  GtkTreeModel *model = (GtkTreeModel *) data;
  GtkTreeIter iter;
  GtkTreePath *path = gtk_tree_path_new_from_string (path_str);
  Ldif_FieldRec *rec;

  /* get toggled iter */
  gtk_tree_model_get_iter (model, &iter, path);
  gtk_tree_model_get (model, &iter, 3, &rec, -1);
  rec->selected ^= TRUE;
  gtk_list_store_set (GTK_LIST_STORE (model), &iter, 0, rec->selected, -1);
  gtk_tree_path_free (path);
}

static void
imp_ldif_modify_pressed (GtkWidget * widget, gpointer data)
{
  GtkTreeSelection *sel;
  GtkTreeModel *model;
  GtkTreeIter iter;
  Ldif_FieldRec *rec;

  sel = gtk_tree_view_get_selection (GTK_TREE_VIEW (impldif_dlg.list_field));

  if (!gtk_tree_selection_get_selected (sel, &model, &iter))
    return;

  gtk_tree_model_get (model, &iter, 3, &rec, -1);

  g_free (rec->userName);
  rec->userName = gtk_editable_get_chars (GTK_EDITABLE (impldif_dlg.name_attrib), 0, -1);
  rec->selected = gtk_toggle_button_get_active (GTK_TOGGLE_BUTTON (impldif_dlg.check_select));

  gtk_list_store_set (GTK_LIST_STORE (model), &iter, 0, rec->selected, 2, rec->userName, -1);

  gtk_tree_model_get_iter_first (model, &iter);
  gtk_tree_selection_select_iter (sel, &iter);

  gtk_label_set_text (GTK_LABEL (impldif_dlg.name_ldif), "");
  gtk_entry_set_text (GTK_ENTRY (impldif_dlg.name_attrib), "");
  gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (impldif_dlg.check_select), FALSE);
}

/*
* Move off fields page.
* return: TRUE if OK to move off page.
*/
static gboolean
imp_ldif_field_move ()
{
  gboolean retVal = FALSE;
  gchar *newFile;
  AddressBookFile *abf = NULL;

  if (_importedBook_)
    addrbook_free_book (_importedBook_);

  abf = addrbook_create_book ();
  addrbook_set_path (abf, _imp_addressIndex_->filePath);
  addrbook_set_name (abf, impldif_dlg.nameBook);
  newFile = imp_ldif_guess_file (abf);
  addrbook_set_file (abf, newFile);
  g_free (newFile);

  /* Import data into file */
  if (ldif_import_data (_ldifFile_, abf->addressCache) == MGU_SUCCESS)
    {
      addrbook_save_data (abf);
      abf->dirtyFlag = TRUE;
      _importedBook_ = abf;
      retVal = TRUE;
    }
  else
    addrbook_free_book (abf);

  return retVal;
}

/*
* Move off fields page.
* return: TRUE if OK to move off page.
*/
static gboolean
imp_ldif_file_move ()
{
  gboolean retVal = FALSE;
  gchar *sName;
  gchar *sFile;
  gchar *sMsg = NULL;
  gboolean errFlag = FALSE;

  sFile = gtk_editable_get_chars (GTK_EDITABLE (impldif_dlg.file_entry), 0, -1);
  g_strchug (sFile);
  g_strchomp (sFile);

  sName = gtk_editable_get_chars (GTK_EDITABLE (impldif_dlg.name_entry), 0, -1);
  g_strchug (sName);
  g_strchomp (sName);

  g_free (impldif_dlg.nameBook);
  g_free (impldif_dlg.fileName);
  impldif_dlg.nameBook = sName;
  impldif_dlg.fileName = sFile;

  gtk_entry_set_text (GTK_ENTRY (impldif_dlg.file_entry), sFile);
  gtk_entry_set_text (GTK_ENTRY (impldif_dlg.name_entry), sName);

  if (*sFile == '\0' || strlen (sFile) < 1)
    {
      sMsg = _("Please select a file.");
      gtk_widget_grab_focus (impldif_dlg.file_entry);
      errFlag = TRUE;
    }

  if (*sName == '\0' || strlen (sName) < 1)
    {
      if (!errFlag)
        sMsg = _("Address book name must be supplied.");
      gtk_widget_grab_focus (impldif_dlg.name_entry);
      errFlag = TRUE;
    }

  if (!errFlag)
    {
      gchar *sFSFile;
      /* Read attribute list */
      sFSFile = conv_filename_from_utf8 (sFile);
      ldif_set_file (_ldifFile_, sFSFile);
      g_free (sFSFile);
      if (ldif_read_tags (_ldifFile_) == MGU_SUCCESS)
        {
          /* Load fields */
          /* ldif_print_file( _ldifFile_, stdout ); */
          imp_ldif_load_fields (_ldifFile_);
          retVal = TRUE;
        }
      else
        sMsg = _("Error reading LDIF fields.");
    }
  imp_ldif_status_show (sMsg);

  return retVal;
}

/*
 * Display finish page.
 */
static void
imp_ldif_finish_show ()
{
  gchar *sMsg;
  gchar *name;

  name = gtk_editable_get_chars (GTK_EDITABLE (impldif_dlg.name_entry), 0, -1);
  gtk_label_set_text (GTK_LABEL (impldif_dlg.labelBook), name);
  g_free (name);
  gtk_label_set_text (GTK_LABEL (impldif_dlg.labelFile), _ldifFile_->path);
  gtk_label_set_text (GTK_LABEL (impldif_dlg.labelRecords), itos (_ldifFile_->importCount));
  gtk_widget_set_sensitive (impldif_dlg.btnPrev, FALSE);
  gtk_widget_set_sensitive (impldif_dlg.btnNext, FALSE);
  if (_ldifFile_->retVal == MGU_SUCCESS)
    sMsg = _("LDIF file imported successfully.");
  else
    sMsg = mgu_error2string (_ldifFile_->retVal);
  imp_ldif_status_show (sMsg);
  gtk_button_set_label (GTK_BUTTON (impldif_dlg.btnCancel), _("_Close"));
  gtk_widget_grab_focus (impldif_dlg.btnCancel);
}

static void
imp_ldif_prev (GtkWidget * widget)
{
  gint pageNum;

  pageNum = gtk_notebook_get_current_page (GTK_NOTEBOOK (impldif_dlg.notebook));
  if (pageNum == PAGE_ATTRIBUTES)
    {
      /* Goto file page stuff */
      gtk_notebook_set_current_page (GTK_NOTEBOOK (impldif_dlg.notebook), PAGE_FILE_INFO);
      gtk_widget_set_sensitive (impldif_dlg.btnPrev, FALSE);
    }
  imp_ldif_message ();
}

static void
imp_ldif_next (GtkWidget * widget)
{
  gint pageNum;

  pageNum = gtk_notebook_get_current_page (GTK_NOTEBOOK (impldif_dlg.notebook));
  if (pageNum == PAGE_FILE_INFO)
    {
      /* Goto attributes stuff */
      if (imp_ldif_file_move ())
        {
          gtk_notebook_set_current_page (GTK_NOTEBOOK (impldif_dlg.notebook), PAGE_ATTRIBUTES);
          imp_ldif_message ();
          gtk_widget_set_sensitive (impldif_dlg.btnPrev, TRUE);
        }
      else
        gtk_widget_set_sensitive (impldif_dlg.btnPrev, FALSE);
    }
  else if (pageNum == PAGE_ATTRIBUTES)
    {
      /* Goto finish stuff */
      if (imp_ldif_field_move ())
        {
          gtk_notebook_set_current_page (GTK_NOTEBOOK (impldif_dlg.notebook), PAGE_FINISH);
          imp_ldif_finish_show ();
        }
    }
}

static void
imp_ldif_cancel (GtkWidget * widget, gpointer data)
{
  gint pageNum;

  pageNum = gtk_notebook_get_current_page (GTK_NOTEBOOK (impldif_dlg.notebook));
  if (pageNum != PAGE_FINISH)
    impldif_dlg.cancelled = TRUE;
  gtk_main_quit ();
}

static void
imp_ldif_file_select (void)
{
  gchar *sSelFile;

  sSelFile = filesel_select_file (_("Select LDIF File"), NULL, GTK_FILE_CHOOSER_ACTION_OPEN);
  if (sSelFile)
    {
      gchar *sUTF8File;
      sUTF8File = conv_filename_to_utf8 (sSelFile);
      gtk_entry_set_text (GTK_ENTRY (impldif_dlg.file_entry), sUTF8File);
      g_free (sUTF8File);
      g_free (sSelFile);
    }
}

static gint
imp_ldif_delete_event (GtkWidget * widget, GdkEventAny * event, gpointer data)
{
  imp_ldif_cancel (widget, data);
  return TRUE;
}

static gboolean
imp_ldif_key_pressed (GtkWidget * widget, GdkEventKey * event, gpointer data)
{
  if (event && event->keyval == GDK_KEY_Escape)
    imp_ldif_cancel (widget, data);
  return FALSE;
}

static void
imp_ldif_page_file (gint pageNum, gchar * pageLbl)
{
  GtkWidget *vbox;
  GtkWidget *table;
  GtkWidget *label;
  GtkWidget *file_entry;
  GtkWidget *name_entry;
  GtkWidget *file_btn;

  vbox = gtk_box_new (GTK_ORIENTATION_VERTICAL, 8);
  gtk_container_add (GTK_CONTAINER (impldif_dlg.notebook), vbox);
  gtk_container_set_border_width (GTK_CONTAINER (vbox), BORDER_WIDTH);

  label = gtk_label_new (pageLbl);
  gtk_widget_show (label);
  gtk_notebook_set_tab_label (GTK_NOTEBOOK (impldif_dlg.notebook),
                              gtk_notebook_get_nth_page (GTK_NOTEBOOK (impldif_dlg.notebook), pageNum), label);

  table = gtk_grid_new ();
  gtk_box_pack_start (GTK_BOX (vbox), table, FALSE, FALSE, 0);
  gtk_container_set_border_width (GTK_CONTAINER (table), 5);
  gtk_grid_set_row_spacing (GTK_GRID (table), 5);
  gtk_grid_set_column_spacing (GTK_GRID (table), 5);

  /* First row */
  label = gtk_label_new (_("Address Book"));
  gtk_grid_attach (GTK_GRID (table), label, 0, 0, 1, 1);
  gtk_label_set_xalign (GTK_LABEL (label), 0.0);

  name_entry = gtk_entry_new ();
  gtk_widget_set_hexpand (name_entry, TRUE);
  gtk_grid_attach (GTK_GRID (table), name_entry, 1, 0, 1, 1);

  /* Second row */
  label = gtk_label_new (_("File Name"));
  gtk_grid_attach (GTK_GRID (table), label, 0, 1, 1, 1);
  gtk_label_set_xalign (GTK_LABEL (label), 0.0);

  file_entry = gtk_entry_new ();
  gtk_widget_set_hexpand (file_entry, TRUE);
  gtk_grid_attach (GTK_GRID (table), file_entry, 1, 1, 1, 1);

  file_btn = gtk_button_new_with_label ("...");
  gtk_grid_attach (GTK_GRID (table), file_btn, 2, 1, 1, 1);

  gtk_widget_show_all (vbox);

  /* Button handler */
  g_signal_connect (G_OBJECT (file_btn), "clicked", G_CALLBACK (imp_ldif_file_select), NULL);

  impldif_dlg.file_entry = file_entry;
  impldif_dlg.name_entry = name_entry;
}

static void
imp_ldif_page_fields (gint pageNum, gchar * pageLbl)
{
  GtkWidget *vbox;
  GtkWidget *vboxt;
  GtkWidget *vboxb;
  GtkWidget *buttonMod;

  GtkWidget *table;
  GtkWidget *label;
  GtkWidget *list_swin;
  GtkWidget *list_field;
  GtkWidget *name_ldif;
  GtkWidget *name_attrib;
  GtkWidget *check_select;

  GtkListStore *store;
  GtkTreeViewColumn *col;
  GtkCellRenderer *renderer;

  vbox = gtk_box_new (GTK_ORIENTATION_VERTICAL, 8);
  gtk_container_add (GTK_CONTAINER (impldif_dlg.notebook), vbox);
  gtk_container_set_border_width (GTK_CONTAINER (vbox), 4);

  label = gtk_label_new (pageLbl);
  gtk_widget_show (label);
  gtk_notebook_set_tab_label (GTK_NOTEBOOK (impldif_dlg.notebook),
                              gtk_notebook_get_nth_page (GTK_NOTEBOOK (impldif_dlg.notebook), pageNum), label);

  /* Upper area - Field list */
  vboxt = gtk_box_new (GTK_ORIENTATION_VERTICAL, 4);
  gtk_container_add (GTK_CONTAINER (vbox), vboxt);

  list_swin = gtk_scrolled_window_new (NULL, NULL);
  gtk_container_add (GTK_CONTAINER (vboxt), list_swin);
  gtk_scrolled_window_set_policy (GTK_SCROLLED_WINDOW (list_swin), GTK_POLICY_AUTOMATIC, GTK_POLICY_ALWAYS);

  store = gtk_list_store_new (4, G_TYPE_BOOLEAN, G_TYPE_STRING, G_TYPE_STRING, G_TYPE_POINTER);
  list_field = gtk_tree_view_new_with_model (GTK_TREE_MODEL (store));
  gtk_tree_view_set_headers_visible (GTK_TREE_VIEW (list_field), TRUE);
  gtk_tree_selection_set_mode (gtk_tree_view_get_selection (GTK_TREE_VIEW (list_field)), GTK_SELECTION_BROWSE);
  gtk_container_add (GTK_CONTAINER (list_swin), list_field);
  g_object_unref (store);

  renderer = gtk_cell_renderer_toggle_new ();
  g_signal_connect (renderer, "toggled", G_CALLBACK (imp_ldif_field_list_toggle), store);

  col = gtk_tree_view_column_new_with_attributes ("S", renderer, "active", 0, NULL);
  /* set this column to a fixed sizing (of 20 pixels) */
  gtk_tree_view_column_set_sizing (GTK_TREE_VIEW_COLUMN (col), GTK_TREE_VIEW_COLUMN_FIXED);
  gtk_tree_view_column_set_fixed_width (GTK_TREE_VIEW_COLUMN (col), 20);
  gtk_tree_view_append_column (GTK_TREE_VIEW (list_field), col);

  renderer = gtk_cell_renderer_text_new ();
  col = gtk_tree_view_column_new_with_attributes (_("LDIF Field"), renderer, "text", 1, NULL);
  gtk_tree_view_column_set_expand (col, TRUE);
  gtk_tree_view_append_column (GTK_TREE_VIEW (list_field), col);

  renderer = gtk_cell_renderer_text_new ();
  col = gtk_tree_view_column_new_with_attributes (_("Attribute Name"), renderer, "text", 2, NULL);
  gtk_tree_view_column_set_expand (col, TRUE);
  gtk_tree_view_append_column (GTK_TREE_VIEW (list_field), col);

  /* Lower area - Edit area */
  vboxb = gtk_box_new (GTK_ORIENTATION_VERTICAL, 4);
  gtk_box_pack_end (GTK_BOX (vbox), vboxb, FALSE, FALSE, 2);

  /* Data entry area */
  table = gtk_grid_new ();
  gtk_box_pack_start (GTK_BOX (vboxb), table, FALSE, FALSE, 0);
  gtk_grid_set_row_spacing (GTK_GRID (table), 5);
  gtk_grid_set_column_spacing (GTK_GRID (table), 5);

  /* First row */
  label = gtk_label_new (_("LDIF Field"));
  gtk_grid_attach (GTK_GRID (table), label, 0, 0, 1, 1);
  gtk_label_set_xalign (GTK_LABEL (label), 0.0);

  name_ldif = gtk_label_new (NULL);
  gtk_label_set_xalign (GTK_LABEL (name_ldif), 0.01);
  gtk_grid_attach (GTK_GRID (table), name_ldif, 1, 0, 1, 1);

  /* Second row */
  label = gtk_label_new (_("Attribute"));
  gtk_grid_attach (GTK_GRID (table), label, 0, 1, 1, 1);
  gtk_label_set_xalign (GTK_LABEL (label), 0.0);

  name_attrib = gtk_entry_new ();
  gtk_grid_attach (GTK_GRID (table), name_attrib, 1, 1, 1, 1);

  /* Next row */
  label = gtk_label_new (_("Select"));
  gtk_grid_attach (GTK_GRID (table), label, 0, 2, 1, 1);
  gtk_label_set_xalign (GTK_LABEL (label), 0.0);

  check_select = gtk_check_button_new ();
  gtk_grid_attach (GTK_GRID (table), check_select, 1, 2, 1, 1);

  buttonMod = gtk_button_new_with_label (_("Modify"));
  gtk_grid_attach (GTK_GRID (table), buttonMod, 1, 3, 1, 1);

  gtk_widget_show_all (vbox);

  /* Event handlers */
  g_signal_connect (G_OBJECT (list_field), "row-activated", G_CALLBACK (imp_ldif_field_list_selected), NULL);
  g_signal_connect (G_OBJECT (buttonMod), "clicked", G_CALLBACK (imp_ldif_modify_pressed), NULL);

  impldif_dlg.list_field = list_field;
  impldif_dlg.name_ldif = name_ldif;
  impldif_dlg.name_attrib = name_attrib;
  impldif_dlg.check_select = check_select;
}

static void
imp_ldif_page_finish (gint pageNum, gchar * pageLbl)
{
  GtkWidget *vbox;
  GtkWidget *table;
  GtkWidget *label;
  GtkWidget *labelBook;
  GtkWidget *labelFile;
  GtkWidget *labelRecs;

  vbox = gtk_box_new (GTK_ORIENTATION_VERTICAL, 8);
  gtk_container_add (GTK_CONTAINER (impldif_dlg.notebook), vbox);
  gtk_container_set_border_width (GTK_CONTAINER (vbox), BORDER_WIDTH);

  label = gtk_label_new (pageLbl);
  gtk_widget_show (label);
  gtk_notebook_set_tab_label (GTK_NOTEBOOK (impldif_dlg.notebook),
                              gtk_notebook_get_nth_page (GTK_NOTEBOOK (impldif_dlg.notebook), pageNum), label);

  table = gtk_grid_new ();
  gtk_box_pack_start (GTK_BOX (vbox), table, FALSE, FALSE, 0);
  gtk_container_set_border_width (GTK_CONTAINER (table), 5);
  gtk_grid_set_row_spacing (GTK_GRID (table), 5);
  gtk_grid_set_column_spacing (GTK_GRID (table), 5);

  /* First row */
  label = gtk_label_new (_("Address Book :"));
  gtk_grid_attach (GTK_GRID (table), label, 0, 0, 1, 1);
  gtk_label_set_xalign (GTK_LABEL (label), 1.0);

  labelBook = gtk_label_new (NULL);
  gtk_grid_attach (GTK_GRID (table), labelBook, 1, 0, 1, 1);
  gtk_label_set_xalign (GTK_LABEL (labelBook), 0.0);

  /* Second row */
  label = gtk_label_new (_("File Name :"));
  gtk_grid_attach (GTK_GRID (table), label, 0, 1, 1, 1);
  gtk_label_set_xalign (GTK_LABEL (label), 1.0);

  labelFile = gtk_label_new (NULL);
  gtk_grid_attach (GTK_GRID (table), labelFile, 1, 1, 1, 1);
  gtk_label_set_xalign (GTK_LABEL (labelFile), 0.0);

  /* Third row */
  label = gtk_label_new (_("Records :"));
  gtk_grid_attach (GTK_GRID (table), label, 0, 2, 1, 1);
  gtk_label_set_xalign (GTK_LABEL (label), 1.0);

  labelRecs = gtk_label_new (NULL);
  gtk_grid_attach (GTK_GRID (table), labelRecs, 1, 2, 1, 1);
  gtk_label_set_xalign (GTK_LABEL (labelRecs), 0.0);

  impldif_dlg.labelBook = labelBook;
  impldif_dlg.labelFile = labelFile;
  impldif_dlg.labelRecords = labelRecs;
}

static void
imp_ldif_dialog_create ()
{
  GtkWidget *window;
  GtkWidget *vbox;
  GtkWidget *vnbox;
  GtkWidget *notebook;
  GtkWidget *hbbox;
  GtkWidget *btnPrev;
  GtkWidget *btnNext;
  GtkWidget *btnCancel;
  GtkWidget *hsbox;
  GtkWidget *statusbar;

  window = gtk_window_new (GTK_WINDOW_TOPLEVEL);
  gtk_widget_set_size_request (window, IMPORTLDIF_WIDTH, IMPORTLDIF_HEIGHT);
  gtk_window_set_title (GTK_WINDOW (window), _("Import LDIF file into Address Book"));
  gtk_window_set_position (GTK_WINDOW (window), GTK_WIN_POS_CENTER);
  gtk_window_set_modal (GTK_WINDOW (window), TRUE);
  g_signal_connect (G_OBJECT (window), "delete_event", G_CALLBACK (imp_ldif_delete_event), NULL);
  g_signal_connect (G_OBJECT (window), "key_press_event", G_CALLBACK (imp_ldif_key_pressed), NULL);
  MANAGE_WINDOW_SIGNALS_CONNECT (window);

  vbox = gtk_box_new (GTK_ORIENTATION_VERTICAL, 4);
  gtk_widget_show (vbox);
  gtk_container_add (GTK_CONTAINER (window), vbox);

  vnbox = gtk_box_new (GTK_ORIENTATION_VERTICAL, 4);
  gtk_container_set_border_width (GTK_CONTAINER (vnbox), 4);
  gtk_widget_show (vnbox);
  gtk_box_pack_start (GTK_BOX (vbox), vnbox, TRUE, TRUE, 0);

  /* Notebook */
  notebook = gtk_notebook_new ();
  gtk_notebook_set_show_tabs (GTK_NOTEBOOK (notebook), FALSE);
  gtk_widget_show (notebook);
  gtk_box_pack_start (GTK_BOX (vnbox), notebook, TRUE, TRUE, 0);
  gtk_container_set_border_width (GTK_CONTAINER (notebook), 6);

  /* Status line */
  hsbox = gtk_box_new (GTK_ORIENTATION_HORIZONTAL, 0);
  gtk_box_pack_end (GTK_BOX (vbox), hsbox, FALSE, FALSE, 0);
  statusbar = gtk_statusbar_new ();
  gtk_box_pack_start (GTK_BOX (hsbox), statusbar, TRUE, TRUE, 0);

  /* Button panel */
  yam_stock_button_set_create (&hbbox, &btnNext, _("Next"), &btnPrev, _("Prev"), &btnCancel, "yam-cancel");
  gtk_box_pack_end (GTK_BOX (vnbox), hbbox, FALSE, FALSE, 0);
  gtk_widget_grab_default (btnNext);

  /* Button handlers */
  g_signal_connect (G_OBJECT (btnPrev), "clicked", G_CALLBACK (imp_ldif_prev), NULL);
  g_signal_connect (G_OBJECT (btnNext), "clicked", G_CALLBACK (imp_ldif_next), NULL);
  g_signal_connect (G_OBJECT (btnCancel), "clicked", G_CALLBACK (imp_ldif_cancel), NULL);

  gtk_widget_show_all (vbox);

  impldif_dlg.window = window;
  impldif_dlg.notebook = notebook;
  impldif_dlg.btnPrev = btnPrev;
  impldif_dlg.btnNext = btnNext;
  impldif_dlg.btnCancel = btnCancel;
  impldif_dlg.statusbar = statusbar;
  impldif_dlg.status_cid = gtk_statusbar_get_context_id (GTK_STATUSBAR (statusbar), "Import LDIF Dialog");

}

static void
imp_ldif_create ()
{
  imp_ldif_dialog_create ();
  imp_ldif_page_file (PAGE_FILE_INFO, _("File Info"));
  imp_ldif_page_fields (PAGE_ATTRIBUTES, _("Attributes"));
  imp_ldif_page_finish (PAGE_FINISH, _("Finish"));
  gtk_widget_show_all (impldif_dlg.window);
}

AddressBookFile *
addressbook_imp_ldif (AddressIndex * addrIndex)
{
  _importedBook_ = NULL;
  _imp_addressIndex_ = addrIndex;

  if (!impldif_dlg.window)
    imp_ldif_create ();
  impldif_dlg.cancelled = FALSE;
  manage_window_set_transient (GTK_WINDOW (impldif_dlg.window));
  gtk_widget_grab_default (impldif_dlg.btnNext);

  gtk_entry_set_text (GTK_ENTRY (impldif_dlg.name_entry), IMPORTLDIF_GUESS_NAME);
  gtk_entry_set_text (GTK_ENTRY (impldif_dlg.file_entry), "");
  gtk_label_set_text (GTK_LABEL (impldif_dlg.name_ldif), "");
  gtk_entry_set_text (GTK_ENTRY (impldif_dlg.name_attrib), "");
  gtk_list_store_clear (GTK_LIST_STORE (gtk_tree_view_get_model (GTK_TREE_VIEW (impldif_dlg.list_field))));
  gtk_notebook_set_current_page (GTK_NOTEBOOK (impldif_dlg.notebook), PAGE_FILE_INFO);
  gtk_widget_set_sensitive (impldif_dlg.btnPrev, FALSE);
  gtk_widget_set_sensitive (impldif_dlg.btnNext, TRUE);
  gtk_button_set_label (GTK_BUTTON (impldif_dlg.btnCancel), _("_Cancel"));
  imp_ldif_message ();
  gtk_widget_grab_focus (impldif_dlg.file_entry);

  impldif_dlg.rowCount = 0;
  g_free (impldif_dlg.nameBook);
  g_free (impldif_dlg.fileName);
  impldif_dlg.nameBook = NULL;
  impldif_dlg.fileName = NULL;

  _ldifFile_ = ldif_create ();

  gtk_widget_show (impldif_dlg.window);

  gtk_main ();
  gtk_widget_hide (impldif_dlg.window);
  ldif_free (_ldifFile_);
  _ldifFile_ = NULL;
  _imp_addressIndex_ = NULL;

  g_free (impldif_dlg.nameBook);
  g_free (impldif_dlg.fileName);
  impldif_dlg.nameBook = NULL;
  impldif_dlg.fileName = NULL;

  if (impldif_dlg.cancelled == TRUE)
    return NULL;
  return _importedBook_;
}

AddressBookFile *
addressbook_imp_ldif_file (AddressIndex * addrIndex, const gchar * file, const gchar * book_name)
{
  gchar *fsfile;
  GList *node, *list;
  gboolean ret = FALSE;

  g_return_val_if_fail (addrIndex != NULL, NULL);
  g_return_val_if_fail (file != NULL, NULL);
  g_return_val_if_fail (book_name != NULL, NULL);

  debug_print ("addressbook_imp_ldif_file: file: %s name: %s\n", file, book_name);

  _importedBook_ = NULL;
  _imp_addressIndex_ = addrIndex;
  _ldifFile_ = ldif_create ();

  fsfile = conv_filename_from_utf8 (file);
  ldif_set_file (_ldifFile_, fsfile);
  g_free (fsfile);

  if (ldif_read_tags (_ldifFile_) != MGU_SUCCESS)
    goto finish;
  list = ldif_get_fieldlist (_ldifFile_);
  node = list;
  while (node)
    {
      Ldif_FieldRec *rec = node->data;
      if (!rec->reserved)
        {
          if (g_ascii_strcasecmp (rec->tagName, "dn") != 0)
            rec->selected = TRUE;
        }
      node = g_list_next (node);
    }
  g_list_free (list);

  g_free (impldif_dlg.nameBook);
  impldif_dlg.nameBook = g_strdup (book_name);

  ret = imp_ldif_field_move ();

  g_free (impldif_dlg.nameBook);
  impldif_dlg.nameBook = NULL;

finish:
  ldif_free (_ldifFile_);
  _ldifFile_ = NULL;
  _imp_addressIndex_ = NULL;

  if (ret)
    debug_print ("addressbook_imp_ldif_file: import succeeded\n");

  return _importedBook_;
}

/*
* End of Source.
*/
