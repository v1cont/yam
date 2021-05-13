/*
 * YAM -- a GTK+ based, lightweight, and fast e-mail client
 * Copyright (C) 2020 Victor Ananjevsky <victor@sanana.kiev.ua>
 * Copyright (C) 1999-2017 Hiroyuki Yamamoto
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

#include <stdio.h>

#include "addrbook.h"
#include "addressbook.h"
#include "addressitem.h"
#include "filesel.h"
#include "gtkutils.h"
#include "stock_pixmap.h"
#include "prefs_common.h"
#include "manage_window.h"
#include "mgutils.h"
#include "importcsv.h"
#include "codeconv.h"
#include "utils.h"

#define IMPORTCSV_GUESS_NAME "CSV Import"

#define PAGE_FILE_INFO             0
#define PAGE_ATTRIBUTES            1
#define PAGE_FINISH                2

#define IMPORTCSV_WIDTH           480
#define IMPORTCSV_HEIGHT          320

#define FIELDS_N_COLS              3

typedef enum {
  FIELD_COL_SELECT = 0,
  FIELD_COL_FIELD = 1,
  FIELD_COL_ATTRIB = 2
} ImpCSV_FieldColPos;

static struct _ImpCSVDlg {
  GtkWidget *window;
  GtkWidget *notebook;
  GtkWidget *file_entry;
  GtkWidget *name_entry;
  GtkWidget *comma_radiobtn;
  GtkWidget *tab_radiobtn;
  GtkWidget *list_field;
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
  gchar delimiter;
  gboolean cancelled;
} impcsv_dlg;

typedef enum {
  ATTR_FIRST_NAME,
  ATTR_LAST_NAME,
  ATTR_DISPLAY_NAME,
  ATTR_NICK_NAME,
  ATTR_EMAIL_ADDRESS,
  ATTR_REMARKS,
  ATTR_ALIAS,
  N_CSV_ATTRIB
} ImpCSVAttribIndex;

static struct _ImpCSVAttrib {
  gchar *name;
  gchar *value;
  gint col;
  gboolean enabled;
} imp_csv_attrib[] = {
  {N_("First Name"), NULL, 0, TRUE},
  {N_("Last Name"), NULL, 1, TRUE},
  {N_("Display Name"), NULL, 2, TRUE},
  {N_("Nick Name"), NULL, 3, TRUE},
  {N_("E-Mail Address"), NULL, 4, TRUE},
  {N_("Remarks"), NULL, 5, TRUE},
  {N_("Alias"), NULL, 6, TRUE}
};

static AddressBookFile *_importedBook_;
static AddressIndex *_imp_addressIndex_;
static gint importCount = 0;
static gint result;

static void
imp_csv_status_show (gchar * msg)
{
  if (impcsv_dlg.statusbar != NULL)
    {
      gtk_statusbar_pop (GTK_STATUSBAR (impcsv_dlg.statusbar), impcsv_dlg.status_cid);
      if (msg)
        gtk_statusbar_push (GTK_STATUSBAR (impcsv_dlg.statusbar), impcsv_dlg.status_cid, msg);
    }
}

static void
imp_csv_message (void)
{
  gchar *sMsg = NULL;
  gint pageNum;

  pageNum = gtk_notebook_get_current_page (GTK_NOTEBOOK (impcsv_dlg.notebook));
  if (pageNum == PAGE_FILE_INFO)
    sMsg = _("Please specify address book name and file to import.");
  else if (pageNum == PAGE_ATTRIBUTES)
    sMsg = _("Select and reorder CSV field names to import.");
  else if (pageNum == PAGE_FINISH)
    sMsg = _("File imported.");
  imp_csv_status_show (sMsg);
}

static gchar *
imp_csv_guess_file (AddressBookFile * abf)
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

static gboolean
imp_csv_load_fields (gchar * sFile)
{
  GtkListStore *model;
  GtkTreeIter iter;
  FILE *fp;
  gchar buf[BUFFSIZE];
  CharSet enc;

  g_return_val_if_fail (sFile != NULL, FALSE);

  model = GTK_LIST_STORE (gtk_tree_view_get_model (GTK_TREE_VIEW (impcsv_dlg.list_field)));

  impcsv_dlg.rowCount = 0;
  gtk_list_store_clear (model);

  enc = conv_check_file_encoding (sFile);

  if ((fp = g_fopen (sFile, "rb")) == NULL)
    return FALSE;

  if (fgets (buf, sizeof (buf), fp) != NULL)
    {
      gchar *str;
      gchar **strv;
      gint i;
      guint fields_len;
      guint data_len = 0;
      guint len;

      strretchomp (buf);
      if (enc == C_UTF_8)
        str = g_strdup (buf);
      else
        str = conv_localetodisp (buf, NULL);
      strv = strsplit_csv (str, impcsv_dlg.delimiter, 0);
      fields_len = sizeof (imp_csv_attrib) / sizeof (imp_csv_attrib[0]);
      while (strv[data_len])
        ++data_len;
      len = MAX (fields_len, data_len);

      gtk_widget_freeze_child_notify (impcsv_dlg.list_field);

      for (i = 0; i < len; i++)
        {
          gchar *f, *a;

          if (i < data_len)
            f = strv[i];
          else
            f = "";
          if (i < fields_len)
            a = gettext (imp_csv_attrib[i].name);
          else
            a = "";

          if (i < fields_len)
            {
              imp_csv_attrib[i].value = NULL;
              imp_csv_attrib[i].col = i;
            }

          gtk_list_store_append (model, &iter);
          gtk_list_store_set (model, &iter, 0, imp_csv_attrib[i].enabled, 1, f, 2, a, 3, imp_csv_attrib[i], -1);
        }

      gtk_widget_thaw_child_notify (impcsv_dlg.list_field);
      gtk_widget_queue_resize (impcsv_dlg.list_field);

      g_strfreev (strv);
      g_free (str);
    }

  fclose (fp);

  return TRUE;
}

static void
imp_csv_field_list_selected (GtkTreeView *list, GtkTreePath *path,GtkTreeViewColumn *col, gpointer data)
{
  GtkTreeSelection *sel;
  GtkTreeModel *model;
  GtkTreeIter iter;
  struct _ImpCSVAttrib *attr;

  sel = gtk_tree_view_get_selection (list);

  if (!gtk_tree_selection_get_selected (sel, &model, &iter))
    return;

  gtk_tree_model_get (model, &iter, 3, &attr, -1);
  attr->enabled ^= TRUE;
  gtk_list_store_set (GTK_LIST_STORE (model), &iter, 0, attr->enabled, -1);
}

static void
imp_csv_field_list_up (GtkWidget * button, gpointer data)
{
  GtkTreeSelection *sel;
  GtkTreeModel *model;
  GtkTreeIter iter, pos;
  struct _ImpCSVAttrib *src_attr;
  struct _ImpCSVAttrib *dest_attr;

  sel = gtk_tree_view_get_selection (GTK_TREE_VIEW (impcsv_dlg.list_field));
  if (!gtk_tree_selection_get_selected (sel, &model, &iter))
    return;

  pos = iter;
  if (!gtk_tree_model_iter_previous (model, &pos))
    return;

  gtk_tree_model_get (model, &iter, 3, &src_attr, -1);
  gtk_tree_model_get (model, &iter, 3, &dest_attr, -1);

  if (src_attr)
    src_attr->col--;
  if (dest_attr)
    dest_attr->col++;

  gtk_list_store_move_before (GTK_LIST_STORE (model), &iter, &pos);
}

static void
imp_csv_field_list_down (GtkWidget * button, gpointer data)
{

  GtkTreeSelection *sel;
  GtkTreeModel *model;
  GtkTreeIter iter, pos;
  struct _ImpCSVAttrib *src_attr;
  struct _ImpCSVAttrib *dest_attr;

  sel = gtk_tree_view_get_selection (GTK_TREE_VIEW (impcsv_dlg.list_field));
  if (!gtk_tree_selection_get_selected (sel, &model, &iter))
    return;

  pos = iter;
  if (!gtk_tree_model_iter_next (model, &pos))
    return;

  gtk_tree_model_get (model, &iter, 3, &src_attr, -1);
  gtk_tree_model_get (model, &iter, 3, &dest_attr, -1);

  if (src_attr)
    src_attr->col++;
  if (dest_attr)
    dest_attr->col--;

  gtk_list_store_move_after (GTK_LIST_STORE (model), &iter, &pos);
}

static gint
imp_csv_import_data (gchar * csvFile, AddressCache * cache)
{
  FILE *fp;
  gchar buf[BUFFSIZE];
  gint i;
  gchar **strv;
  CharSet enc;
  guint fields_len;
  gchar *firstName = NULL;
  gchar *lastName = NULL;
  gchar *fullName = NULL;
  gchar *nickName = NULL;
  gchar *address = NULL;
  gchar *remarks = NULL;
  gchar *alias = NULL;
  ItemPerson *person;
  ItemEMail *email;
  gint count = 0;

  g_return_val_if_fail (csvFile != NULL, MGU_BAD_ARGS);

  addrcache_clear (cache);
  cache->dataRead = FALSE;

  enc = conv_check_file_encoding (csvFile);

  if ((fp = g_fopen (csvFile, "rb")) == NULL)
    return MGU_OPEN_FILE;

  fields_len = sizeof (imp_csv_attrib) / sizeof (imp_csv_attrib[0]);

  while (fgets (buf, sizeof (buf), fp) != NULL)
    {
      gchar *str;
      guint col, cols = 0;

      strretchomp (buf);
      if (enc == C_UTF_8)
        str = g_strdup (buf);
      else
        str = conv_localetodisp (buf, NULL);
      strv = strsplit_csv (str, impcsv_dlg.delimiter, 0);
      while (strv[cols])
        ++cols;

      for (i = 0; i < fields_len; i++)
        {
          if (!imp_csv_attrib[i].enabled)
            continue;
          col = imp_csv_attrib[i].col;
          if (col >= cols || !*strv[col])
            continue;
          imp_csv_attrib[i].value = strv[col];
        }

      firstName = imp_csv_attrib[ATTR_FIRST_NAME].value;
      lastName = imp_csv_attrib[ATTR_LAST_NAME].value;
      fullName = imp_csv_attrib[ATTR_DISPLAY_NAME].value;
      nickName = imp_csv_attrib[ATTR_NICK_NAME].value;
      address = imp_csv_attrib[ATTR_EMAIL_ADDRESS].value;
      remarks = imp_csv_attrib[ATTR_REMARKS].value;
      alias = imp_csv_attrib[ATTR_ALIAS].value;

      if (!fullName && !firstName && !lastName && address)
        fullName = address;

      person = addritem_create_item_person ();
      addritem_person_set_common_name (person, fullName);
      addritem_person_set_first_name (person, firstName);
      addritem_person_set_last_name (person, lastName);
      addritem_person_set_nick_name (person, nickName);
      addrcache_id_person (cache, person);
      addrcache_add_person (cache, person);

      if (address)
        {
          email = addritem_create_item_email ();
          addritem_email_set_address (email, address);
          addritem_email_set_remarks (email, remarks);
          addritem_email_set_alias (email, alias);
          addrcache_id_email (cache, email);
          addrcache_person_add_email (cache, person, email);
        }

      for (i = 0; i < fields_len; i++)
        imp_csv_attrib[i].value = NULL;
      g_strfreev (strv);
      g_free (str);

      count++;
    }

  fclose (fp);

  cache->modified = FALSE;
  cache->dataRead = TRUE;
  importCount = count;

  return MGU_SUCCESS;
}

/*
* Move off fields page.
* return: TRUE if OK to move off page.
*/
static gboolean
imp_csv_field_move ()
{
  gboolean retVal = FALSE;
  gchar *newFile;
  AddressBookFile *abf = NULL;

  if (_importedBook_)
    addrbook_free_book (_importedBook_);

  abf = addrbook_create_book ();
  addrbook_set_path (abf, _imp_addressIndex_->filePath);
  addrbook_set_name (abf, impcsv_dlg.nameBook);
  newFile = imp_csv_guess_file (abf);
  addrbook_set_file (abf, newFile);
  g_free (newFile);

  /* Import data into file */
  if (imp_csv_import_data (impcsv_dlg.fileName, abf->addressCache) == MGU_SUCCESS)
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
imp_csv_file_move ()
{
  gboolean retVal = FALSE;
  gchar *sName;
  gchar *sFile;
  gchar *sMsg = NULL;
  gboolean errFlag = FALSE;

  sFile = gtk_editable_get_chars (GTK_EDITABLE (impcsv_dlg.file_entry), 0, -1);
  g_strchug (sFile);
  g_strchomp (sFile);

  sName = gtk_editable_get_chars (GTK_EDITABLE (impcsv_dlg.name_entry), 0, -1);
  g_strchug (sName);
  g_strchomp (sName);

  g_free (impcsv_dlg.nameBook);
  g_free (impcsv_dlg.fileName);
  impcsv_dlg.nameBook = sName;
  impcsv_dlg.fileName = sFile;

  gtk_entry_set_text (GTK_ENTRY (impcsv_dlg.file_entry), sFile);
  gtk_entry_set_text (GTK_ENTRY (impcsv_dlg.name_entry), sName);

  if (gtk_toggle_button_get_active (GTK_TOGGLE_BUTTON (impcsv_dlg.comma_radiobtn)))
    impcsv_dlg.delimiter = ',';
  else
    impcsv_dlg.delimiter = '\t';

  if (*sFile == '\0' || strlen (sFile) < 1)
    {
      sMsg = _("Please select a file.");
      gtk_widget_grab_focus (impcsv_dlg.file_entry);
      errFlag = TRUE;
    }

  if (*sName == '\0' || strlen (sName) < 1)
    {
      if (!errFlag)
        sMsg = _("Address book name must be supplied.");
      gtk_widget_grab_focus (impcsv_dlg.name_entry);
      errFlag = TRUE;
    }

  if (!errFlag)
    {
      gchar *sFSFile;
      sFSFile = conv_filename_from_utf8 (sFile);
      if (!imp_csv_load_fields (sFSFile))
        sMsg = _("Error reading CSV fields.");
      else
        retVal = TRUE;
      g_free (sFSFile);
    }
  imp_csv_status_show (sMsg);

  return retVal;
}

/*
 * Display finish page.
 */
static void
imp_csv_finish_show ()
{
  gchar *sMsg;
  gchar *name;

  name = gtk_editable_get_chars (GTK_EDITABLE (impcsv_dlg.name_entry), 0, -1);
  gtk_label_set_text (GTK_LABEL (impcsv_dlg.labelBook), name);
  g_free (name);
  gtk_label_set_text (GTK_LABEL (impcsv_dlg.labelFile), impcsv_dlg.fileName);
  gtk_label_set_text (GTK_LABEL (impcsv_dlg.labelRecords), itos (importCount));
  gtk_widget_set_sensitive (impcsv_dlg.btnPrev, FALSE);
  gtk_widget_set_sensitive (impcsv_dlg.btnNext, FALSE);
  if (result == MGU_SUCCESS)
    sMsg = _("CSV file imported successfully.");
  else
    sMsg = mgu_error2string (result);
  imp_csv_status_show (sMsg);
  gtk_button_set_label (GTK_BUTTON (impcsv_dlg.btnCancel), _("_Close"));
  gtk_widget_grab_focus (impcsv_dlg.btnCancel);
}

static void
imp_csv_prev (GtkWidget * widget)
{
  gint pageNum;

  pageNum = gtk_notebook_get_current_page (GTK_NOTEBOOK (impcsv_dlg.notebook));
  if (pageNum == PAGE_ATTRIBUTES)
    {
      /* Goto file page stuff */
      gtk_notebook_set_current_page (GTK_NOTEBOOK (impcsv_dlg.notebook), PAGE_FILE_INFO);
      gtk_widget_set_sensitive (impcsv_dlg.btnPrev, FALSE);
    }
  imp_csv_message ();
}

static void
imp_csv_next (GtkWidget * widget)
{
  gint pageNum;

  pageNum = gtk_notebook_get_current_page (GTK_NOTEBOOK (impcsv_dlg.notebook));
  if (pageNum == PAGE_FILE_INFO)
    {
      /* Goto attributes stuff */
      if (imp_csv_file_move ())
        {
          gtk_notebook_set_current_page (GTK_NOTEBOOK (impcsv_dlg.notebook), PAGE_ATTRIBUTES);
          imp_csv_message ();
          gtk_widget_set_sensitive (impcsv_dlg.btnPrev, TRUE);
        }
      else
        gtk_widget_set_sensitive (impcsv_dlg.btnPrev, FALSE);
    }
  else if (pageNum == PAGE_ATTRIBUTES)
    {
      /* Goto finish stuff */
      if (imp_csv_field_move ())
        {
          gtk_notebook_set_current_page (GTK_NOTEBOOK (impcsv_dlg.notebook), PAGE_FINISH);
          imp_csv_finish_show ();
        }
    }
}

static void
imp_csv_cancel (GtkWidget * widget, gpointer data)
{
  gint pageNum;

  pageNum = gtk_notebook_get_current_page (GTK_NOTEBOOK (impcsv_dlg.notebook));
  if (pageNum != PAGE_FINISH)
    impcsv_dlg.cancelled = TRUE;
  gtk_main_quit ();
}

static void
imp_csv_file_select (void)
{
  gchar *sSelFile;

  sSelFile = filesel_select_file (_("Select CSV File"), NULL, GTK_FILE_CHOOSER_ACTION_OPEN);
  if (sSelFile)
    {
      gchar *sUTF8File;
      sUTF8File = conv_filename_to_utf8 (sSelFile);
      gtk_entry_set_text (GTK_ENTRY (impcsv_dlg.file_entry), sUTF8File);
      g_free (sUTF8File);
      g_free (sSelFile);
    }
}

static gint
imp_csv_delete_event (GtkWidget * widget, GdkEventAny * event, gpointer data)
{
  imp_csv_cancel (widget, data);
  return TRUE;
}

static gboolean
imp_csv_key_pressed (GtkWidget * widget, GdkEventKey * event, gpointer data)
{
  if (event && event->keyval == GDK_KEY_Escape)
    imp_csv_cancel (widget, data);
  return FALSE;
}

static void
imp_csv_page_file (gint pageNum, gchar * pageLbl)
{
  GtkWidget *vbox;
  GtkWidget *table;
  GtkWidget *label;
  GtkWidget *file_entry;
  GtkWidget *name_entry;
  GtkWidget *file_btn;
  GtkWidget *hbox;
  GtkWidget *comma_radiobtn;
  GtkWidget *tab_radiobtn;

  vbox = gtk_box_new (GTK_ORIENTATION_VERTICAL, 4);
  gtk_container_add (GTK_CONTAINER (impcsv_dlg.notebook), vbox);
  gtk_container_set_border_width (GTK_CONTAINER (vbox), 4);

  label = gtk_label_new (pageLbl);
  gtk_widget_show (label);
  gtk_notebook_set_tab_label (GTK_NOTEBOOK (impcsv_dlg.notebook),
                              gtk_notebook_get_nth_page (GTK_NOTEBOOK (impcsv_dlg.notebook), pageNum), label);

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

  hbox = gtk_box_new (GTK_ORIENTATION_HORIZONTAL, 8);
  gtk_box_pack_start (GTK_BOX (vbox), hbox, FALSE, FALSE, 0);
  gtk_container_set_border_width (GTK_CONTAINER (hbox), 4);

  comma_radiobtn = gtk_radio_button_new_with_label (NULL, _("Comma-separated"));
  gtk_box_pack_start (GTK_BOX (hbox), comma_radiobtn, FALSE, FALSE, 0);

  tab_radiobtn = gtk_radio_button_new_with_label_from_widget (GTK_RADIO_BUTTON (comma_radiobtn), _("Tab-separated"));
  gtk_box_pack_start (GTK_BOX (hbox), tab_radiobtn, FALSE, FALSE, 0);

  gtk_widget_show_all (vbox);

  /* Button handler */
  g_signal_connect (G_OBJECT (file_btn), "clicked", G_CALLBACK (imp_csv_file_select), NULL);

  impcsv_dlg.file_entry = file_entry;
  impcsv_dlg.name_entry = name_entry;
  impcsv_dlg.comma_radiobtn = comma_radiobtn;
  impcsv_dlg.tab_radiobtn = tab_radiobtn;
}

static void
state_toggled (GtkCellRendererToggle *cell, gchar *path_str, gpointer data)
{
  GtkTreeModel *model = (GtkTreeModel *) data;
  GtkTreeIter iter;
  GtkTreePath *path = gtk_tree_path_new_from_string (path_str);
  struct _ImpCSVAttrib *attr;

  /* get toggled iter */
  gtk_tree_model_get_iter (model, &iter, path);
  gtk_tree_model_get (model, &iter, 3, &attr, -1);
  attr->enabled ^= TRUE;
  gtk_list_store_set (GTK_LIST_STORE (model), &iter, 0, attr->enabled, -1);
  gtk_tree_path_free (path);
}


static void
imp_csv_page_fields (gint pageNum, gchar * pageLbl)
{
  GtkWidget *vbox;
  GtkWidget *hbox;

  GtkWidget *label;
  GtkWidget *list_swin;
  GtkWidget *list_field;

  GtkListStore *store;
  GtkTreeViewColumn *col;
  GtkCellRenderer *renderer;

  GtkWidget *btn_vbox;
  GtkWidget *btn_vbox1;
  GtkWidget *up_btn;
  GtkWidget *down_btn;

  vbox = gtk_box_new (GTK_ORIENTATION_VERTICAL, 4);
  gtk_container_add (GTK_CONTAINER (impcsv_dlg.notebook), vbox);
  gtk_container_set_border_width (GTK_CONTAINER (vbox), 4);

  label = gtk_label_new (pageLbl);
  gtk_widget_show (label);
  gtk_notebook_set_tab_label (GTK_NOTEBOOK (impcsv_dlg.notebook),
                              gtk_notebook_get_nth_page (GTK_NOTEBOOK (impcsv_dlg.notebook), pageNum), label);

  hbox = gtk_box_new (GTK_ORIENTATION_HORIZONTAL, 0);
  gtk_box_pack_start (GTK_BOX (vbox), hbox, FALSE, FALSE, 4);

  label = gtk_label_new (_("Reorder address book fields with the Up and Down button."));
  gtk_box_pack_start (GTK_BOX (hbox), label, FALSE, FALSE, 0);
  gtk_label_set_justify (GTK_LABEL (label), GTK_JUSTIFY_LEFT);
  gtk_label_set_line_wrap (GTK_LABEL (label), TRUE);

  hbox = gtk_box_new (GTK_ORIENTATION_HORIZONTAL, 4);
  gtk_box_pack_start (GTK_BOX (vbox), hbox, TRUE, TRUE, 0);

  list_swin = gtk_scrolled_window_new (NULL, NULL);
  gtk_box_pack_start (GTK_BOX (hbox), list_swin, TRUE, TRUE, 0);
  gtk_scrolled_window_set_policy (GTK_SCROLLED_WINDOW (list_swin), GTK_POLICY_AUTOMATIC, GTK_POLICY_ALWAYS);

  store = gtk_list_store_new (4, G_TYPE_BOOLEAN, G_TYPE_STRING, G_TYPE_STRING, G_TYPE_POINTER);
  list_field = gtk_tree_view_new_with_model (GTK_TREE_MODEL (store));
  gtk_tree_view_set_headers_visible (GTK_TREE_VIEW (list_field), TRUE);
  gtk_tree_selection_set_mode (gtk_tree_view_get_selection (GTK_TREE_VIEW (list_field)), GTK_SELECTION_BROWSE);
  gtk_container_add (GTK_CONTAINER (list_swin), list_field);
  g_object_unref (store);

  renderer = gtk_cell_renderer_toggle_new ();
  g_signal_connect (renderer, "toggled", G_CALLBACK (state_toggled), store);

  col = gtk_tree_view_column_new_with_attributes ("S", renderer, "active", 0, NULL);
  /* set this column to a fixed sizing (of 20 pixels) */
  gtk_tree_view_column_set_sizing (GTK_TREE_VIEW_COLUMN (col), GTK_TREE_VIEW_COLUMN_FIXED);
  gtk_tree_view_column_set_fixed_width (GTK_TREE_VIEW_COLUMN (col), 20);
  gtk_tree_view_append_column (GTK_TREE_VIEW (list_field), col);

  renderer = gtk_cell_renderer_text_new ();
  col = gtk_tree_view_column_new_with_attributes (_("CSV Field"), renderer, "text", 1, NULL);
  gtk_tree_view_column_set_expand (col, TRUE);
  gtk_tree_view_append_column (GTK_TREE_VIEW (list_field), col);

  renderer = gtk_cell_renderer_text_new ();
  col = gtk_tree_view_column_new_with_attributes (_("Address Book Field"), renderer, "text", 2, NULL);
  gtk_tree_view_column_set_expand (col, TRUE);
  gtk_tree_view_append_column (GTK_TREE_VIEW (list_field), col);

  btn_vbox = gtk_box_new (GTK_ORIENTATION_VERTICAL, 0);
  gtk_box_pack_start (GTK_BOX (hbox), btn_vbox, FALSE, FALSE, 0);
  gtk_container_set_border_width (GTK_CONTAINER (btn_vbox), 4);

  btn_vbox1 = gtk_box_new (GTK_ORIENTATION_VERTICAL, 8);
  gtk_box_pack_start (GTK_BOX (btn_vbox), btn_vbox1, TRUE, FALSE, 0);

  up_btn = gtk_button_new_with_label (_("Up"));
  gtk_box_pack_start (GTK_BOX (btn_vbox1), up_btn, FALSE, FALSE, 0);
  down_btn = gtk_button_new_with_label (_("Down"));
  gtk_box_pack_start (GTK_BOX (btn_vbox1), down_btn, FALSE, FALSE, 0);

  gtk_widget_show_all (vbox);

  g_signal_connect (G_OBJECT (list_field), "row-activated", G_CALLBACK (imp_csv_field_list_selected), NULL);
  g_signal_connect (G_OBJECT (up_btn), "clicked", G_CALLBACK (imp_csv_field_list_up), NULL);
  g_signal_connect (G_OBJECT (down_btn), "clicked", G_CALLBACK (imp_csv_field_list_down), NULL);

  impcsv_dlg.list_field = list_field;
}

static void
imp_csv_page_finish (gint pageNum, gchar * pageLbl)
{
  GtkWidget *vbox;
  GtkWidget *table;
  GtkWidget *label;
  GtkWidget *labelBook;
  GtkWidget *labelFile;
  GtkWidget *labelRecs;

  vbox = gtk_box_new (GTK_ORIENTATION_VERTICAL, 8);
  gtk_container_add (GTK_CONTAINER (impcsv_dlg.notebook), vbox);
  gtk_container_set_border_width (GTK_CONTAINER (vbox), BORDER_WIDTH);

  label = gtk_label_new (pageLbl);
  gtk_widget_show (label);
  gtk_notebook_set_tab_label (GTK_NOTEBOOK (impcsv_dlg.notebook),
                              gtk_notebook_get_nth_page (GTK_NOTEBOOK (impcsv_dlg.notebook), pageNum), label);

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
  gtk_label_set_line_wrap (GTK_LABEL (labelFile), TRUE);

  /* Third row */
  label = gtk_label_new (_("Records :"));
  gtk_grid_attach (GTK_GRID (table), label, 0, 2, 1, 1);
  gtk_label_set_xalign (GTK_LABEL (label), 1.0);

  labelRecs = gtk_label_new (NULL);
  gtk_grid_attach (GTK_GRID (table), labelRecs, 1, 2, 1, 1);
  gtk_label_set_xalign (GTK_LABEL (labelRecs), 0.0);

  impcsv_dlg.labelBook = labelBook;
  impcsv_dlg.labelFile = labelFile;
  impcsv_dlg.labelRecords = labelRecs;
}

static void
imp_csv_dialog_create ()
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
  gtk_widget_set_size_request (window, IMPORTCSV_WIDTH, IMPORTCSV_HEIGHT);
  gtk_window_set_title (GTK_WINDOW (window), _("Import CSV file into Address Book"));
  gtk_window_set_position (GTK_WINDOW (window), GTK_WIN_POS_CENTER);
  gtk_window_set_modal (GTK_WINDOW (window), TRUE);
  g_signal_connect (G_OBJECT (window), "delete_event", G_CALLBACK (imp_csv_delete_event), NULL);
  g_signal_connect (G_OBJECT (window), "key_press_event", G_CALLBACK (imp_csv_key_pressed), NULL);
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
  g_signal_connect (G_OBJECT (btnPrev), "clicked", G_CALLBACK (imp_csv_prev), NULL);
  g_signal_connect (G_OBJECT (btnNext), "clicked", G_CALLBACK (imp_csv_next), NULL);
  g_signal_connect (G_OBJECT (btnCancel), "clicked", G_CALLBACK (imp_csv_cancel), NULL);

  gtk_widget_show_all (vbox);

  impcsv_dlg.window = window;
  impcsv_dlg.notebook = notebook;
  impcsv_dlg.btnPrev = btnPrev;
  impcsv_dlg.btnNext = btnNext;
  impcsv_dlg.btnCancel = btnCancel;
  impcsv_dlg.statusbar = statusbar;
  impcsv_dlg.status_cid = gtk_statusbar_get_context_id (GTK_STATUSBAR (statusbar), "Import CSV Dialog");
}

static void
imp_csv_create ()
{
  imp_csv_dialog_create ();
  imp_csv_page_file (PAGE_FILE_INFO, _("File Info"));
  imp_csv_page_fields (PAGE_ATTRIBUTES, _("Fields"));
  imp_csv_page_finish (PAGE_FINISH, _("Finish"));
  gtk_widget_show_all (impcsv_dlg.window);
}

AddressBookFile *
addressbook_imp_csv (AddressIndex * addrIndex)
{
  _importedBook_ = NULL;
  _imp_addressIndex_ = addrIndex;

  if (!impcsv_dlg.window)
    imp_csv_create ();
  impcsv_dlg.cancelled = FALSE;
  manage_window_set_transient (GTK_WINDOW (impcsv_dlg.window));
  gtk_widget_grab_default (impcsv_dlg.btnNext);

  gtk_entry_set_text (GTK_ENTRY (impcsv_dlg.name_entry), IMPORTCSV_GUESS_NAME);
  gtk_entry_set_text (GTK_ENTRY (impcsv_dlg.file_entry), "");
  gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (impcsv_dlg.comma_radiobtn), TRUE);
  gtk_list_store_clear (GTK_LIST_STORE (gtk_tree_view_get_model (GTK_TREE_VIEW (impcsv_dlg.list_field))));
  gtk_notebook_set_current_page (GTK_NOTEBOOK (impcsv_dlg.notebook), PAGE_FILE_INFO);
  gtk_widget_set_sensitive (impcsv_dlg.btnPrev, FALSE);
  gtk_widget_set_sensitive (impcsv_dlg.btnNext, TRUE);
  gtk_button_set_label (GTK_BUTTON (impcsv_dlg.btnCancel), _("_Cancel"));
  imp_csv_message ();
  gtk_widget_grab_focus (impcsv_dlg.file_entry);

  impcsv_dlg.rowCount = 0;
  g_free (impcsv_dlg.nameBook);
  g_free (impcsv_dlg.fileName);
  impcsv_dlg.nameBook = NULL;
  impcsv_dlg.fileName = NULL;
  impcsv_dlg.delimiter = ',';
  importCount = 0;

  gtk_widget_show (impcsv_dlg.window);

  gtk_main ();
  gtk_widget_hide (impcsv_dlg.window);
  _imp_addressIndex_ = NULL;

  g_free (impcsv_dlg.nameBook);
  g_free (impcsv_dlg.fileName);
  impcsv_dlg.nameBook = NULL;
  impcsv_dlg.fileName = NULL;

  if (impcsv_dlg.cancelled == TRUE)
    return NULL;
  return _importedBook_;
}

/*
* End of Source.
*/
