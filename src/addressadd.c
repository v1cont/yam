/*
 * YAM -- a GTK+ based, lightweight, and fast e-mail client
 * Copyright (C) 2020 Victor Ananjevsky <victor@sanana.kiev.ua>
 * Copyright (C) 2001 Match Grun
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

/*
 * Add address to address book dialog.
 */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include "defs.h"

#include <glib.h>
#include <glib/gi18n.h>
#include <gdk/gdkkeysyms.h>
#include <gtk/gtk.h>

#include "gtkutils.h"
#include "stock_pixmap.h"
#include "prefs_common.h"
#include "addressadd.h"
#include "addritem.h"
#include "addrbook.h"
#include "addrindex.h"
#include "editaddress.h"
#include "manage_window.h"
#include "utils.h"

typedef struct {
  AddressBookFile *book;
  ItemFolder *folder;
} FolderInfo;

static struct _AddressAdd_dlg {
  GtkWidget *window;
  GtkWidget *label_name;
  GtkWidget *label_address;
  GtkWidget *label_remarks;
  GtkWidget *ok_btn;
  GtkWidget *cancel_btn;
  GtkTreeModel *model;
  GtkTreeSelection *sel;
  FolderInfo *fiSelected;
} addressadd_dlg;

static GdkPixbuf *folder_pb = NULL;
static GdkPixbuf *book_pb = NULL;

static gboolean addressadd_cancelled;

static FolderInfo *
addressadd_create_folderinfo (AddressBookFile * abf, ItemFolder * folder)
{
  FolderInfo *fi = g_new0 (FolderInfo, 1);
  fi->book = abf;
  fi->folder = folder;
  return fi;
}

static void
addressadd_free_folderinfo (FolderInfo * fi)
{
  fi->book = NULL;
  fi->folder = NULL;
  g_free (fi);
}

static gint
addressadd_delete_event (GtkWidget * widget, GdkEventAny * event, gboolean * cancelled)
{
  addressadd_cancelled = TRUE;
  gtk_main_quit ();
  return TRUE;
}

static gboolean
addressadd_key_pressed (GtkWidget * widget, GdkEventKey * event, gboolean * cancelled)
{
  if (event && event->keyval == GDK_KEY_Escape)
    {
      addressadd_cancelled = TRUE;
      gtk_main_quit ();
    }
  return FALSE;
}

static void
addressadd_ok (GtkWidget * widget, gboolean * cancelled)
{
  addressadd_cancelled = FALSE;
  gtk_main_quit ();
}

static void
addressadd_cancel (GtkWidget * widget, gboolean * cancelled)
{
  addressadd_cancelled = TRUE;
  gtk_main_quit ();
}

static void
addressadd_folder_select (GtkTreeSelection *sel, gpointer data)
{
  GtkTreeModel *model;
  GtkTreeIter iter;

  if (gtk_tree_selection_get_selected (sel, &model, &iter))
    gtk_tree_model_get (model, &iter, 2, &addressadd_dlg.fiSelected, -1);
}

static void
addressadd_tree_button (GtkTreeView *tree, GtkTreePath *path, GtkTreeViewColumn *col, gpointer data)
{
  addressadd_cancelled = FALSE;
  gtk_main_quit ();
}

static void
addressadd_create (void)
{
  GtkWidget *window;
  GtkWidget *vbox;
  GtkWidget *table;
  GtkWidget *label;
  GtkWidget *label_name;
  GtkWidget *label_addr;
  GtkWidget *label_rems;
  GtkWidget *tree_folder;
  GtkWidget *tree_win;
  GtkWidget *hbbox;
  GtkWidget *ok_btn;
  GtkWidget *cancel_btn;
  GtkTreeStore *model;
  GtkTreeSelection *sel;
  GtkCellRenderer *renderer;
  GtkTreeViewColumn *column;

  stock_pixbuf_gdk (STOCK_PIXMAP_BOOK, &book_pb);
  stock_pixbuf_gdk (STOCK_PIXMAP_FOLDER_OPEN, &folder_pb);

  window = gtk_window_new (GTK_WINDOW_TOPLEVEL);
  gtk_widget_set_size_request (window, 300, 360);
  gtk_container_set_border_width (GTK_CONTAINER (window), 0);
  gtk_window_set_title (GTK_WINDOW (window), _("Add Address to Book"));
  gtk_window_set_position (GTK_WINDOW (window), GTK_WIN_POS_CENTER_ON_PARENT);
  gtk_window_set_modal (GTK_WINDOW (window), TRUE);
  gtk_widget_realize (window);
  g_signal_connect (G_OBJECT (window), "delete_event", G_CALLBACK (addressadd_delete_event), NULL);
  g_signal_connect (G_OBJECT (window), "key_press_event", G_CALLBACK (addressadd_key_pressed), NULL);

  vbox = gtk_box_new (GTK_ORIENTATION_VERTICAL, 5);
  gtk_container_add (GTK_CONTAINER (window), vbox);
  gtk_container_set_border_width (GTK_CONTAINER (vbox), 0);

  table = gtk_grid_new ();
  gtk_box_pack_start (GTK_BOX (vbox), table, FALSE, FALSE, 0);
  gtk_container_set_border_width (GTK_CONTAINER (table), 5);
  gtk_grid_set_row_spacing (GTK_GRID (table), 5);
  gtk_grid_set_column_spacing (GTK_GRID (table), 5);

  /* First row */
  label = gtk_label_new (_("Name"));
  gtk_grid_attach (GTK_GRID (table), label, 0, 0, 1, 1);
  gtk_label_set_xalign (GTK_LABEL (label), 0.0);

  label_name = gtk_label_new (NULL);
  gtk_grid_attach (GTK_GRID (table), label_name, 1, 0, 1, 1);
  gtk_label_set_xalign (GTK_LABEL (label_name), 0.0);

  /* Second row */
  label = gtk_label_new (_("Address"));
  gtk_grid_attach (GTK_GRID (table), label, 0, 1, 1, 1);
  gtk_label_set_xalign (GTK_LABEL (label), 0.0);

  label_addr = gtk_label_new (NULL);
  gtk_grid_attach (GTK_GRID (table), label_addr, 1, 1, 1, 1);
  gtk_label_set_xalign (GTK_LABEL (label_addr), 0.0);

  /* Third row */
  label = gtk_label_new (_("Remarks"));
  gtk_grid_attach (GTK_GRID (table), label, 0, 2, 1, 1);
  gtk_label_set_xalign (GTK_LABEL (label), 0.0);

  label_rems = gtk_label_new (NULL);
  gtk_grid_attach (GTK_GRID (table), label_rems, 1, 2, 1, 1);
  gtk_label_set_xalign (GTK_LABEL (label_rems), 0.0);

  /* Address book/folder tree */
  tree_win = gtk_scrolled_window_new (NULL, NULL);
  gtk_scrolled_window_set_policy (GTK_SCROLLED_WINDOW (tree_win), GTK_POLICY_AUTOMATIC, GTK_POLICY_AUTOMATIC);
  gtk_box_pack_start (GTK_BOX (vbox), tree_win, TRUE, TRUE, 0);

  model = gtk_tree_store_new (3, GDK_TYPE_PIXBUF, G_TYPE_STRING, G_TYPE_POINTER);

  tree_folder = gtk_tree_view_new_with_model (GTK_TREE_MODEL (model));
  gtk_tree_view_set_headers_visible (GTK_TREE_VIEW (tree_folder), TRUE);
  gtk_tree_view_set_enable_tree_lines (GTK_TREE_VIEW (tree_folder), TRUE);
  gtk_container_add (GTK_CONTAINER (tree_win), tree_folder);

  g_object_unref (model);

  sel = gtk_tree_view_get_selection (GTK_TREE_VIEW (tree_folder));
  gtk_tree_selection_set_mode (sel, GTK_SELECTION_BROWSE);

  column = gtk_tree_view_column_new ();
  gtk_tree_view_column_set_title (column, _("Select Address Book Folder"));

  renderer = gtk_cell_renderer_pixbuf_new ();
  gtk_tree_view_column_pack_start (column, renderer, FALSE);
  gtk_tree_view_column_set_attributes (column, renderer, "pixbuf", 0, NULL);

  renderer = gtk_cell_renderer_text_new ();
  gtk_tree_view_column_pack_start (column, renderer, TRUE);
  gtk_tree_view_column_set_attributes (column, renderer, "text", 1, NULL);

  gtk_tree_view_append_column (GTK_TREE_VIEW (tree_folder), column);

  /* Button panel */
  yam_stock_button_set_create (&hbbox, &ok_btn, "yam-ok", &cancel_btn, "yam-cancel", NULL, NULL);
  gtk_box_pack_end (GTK_BOX (vbox), hbbox, FALSE, FALSE, 0);
  gtk_container_set_border_width (GTK_CONTAINER (hbbox), 4);
  gtk_widget_grab_default (ok_btn);

  g_signal_connect (G_OBJECT (ok_btn), "clicked", G_CALLBACK (addressadd_ok), NULL);
  g_signal_connect (G_OBJECT (cancel_btn), "clicked", G_CALLBACK (addressadd_cancel), NULL);
  g_signal_connect (G_OBJECT (sel), "changed", G_CALLBACK (addressadd_folder_select), NULL);
  g_signal_connect (G_OBJECT (tree_folder), "row-activated", G_CALLBACK (addressadd_tree_button), NULL);

  addressadd_dlg.window = window;
  addressadd_dlg.label_name = label_name;
  addressadd_dlg.label_address = label_addr;
  addressadd_dlg.label_remarks = label_rems;
  addressadd_dlg.ok_btn = ok_btn;
  addressadd_dlg.cancel_btn = cancel_btn;
  addressadd_dlg.model = GTK_TREE_MODEL (model);
  addressadd_dlg.sel = sel;

  gtk_widget_show_all (vbox);
}

static void
addressadd_model_clear (GtkTreeModel *model, GtkTreeIter *root)
{
  GtkTreeIter iter;

  if (gtk_tree_model_iter_children (model, &iter, root))
    {
      do
        {
          if (gtk_tree_model_iter_has_child (model, &iter))
            addressadd_model_clear (model, &iter);
          else
            {
              FolderInfo *fi;
              gchar *name;

              gtk_tree_model_get (model, &iter, 1, &name, 2, &fi, -1);
              g_free (name);
              addressadd_free_folderinfo (fi);
              gtk_tree_store_remove (GTK_TREE_STORE (model), &iter);
            }
        }
      while (gtk_tree_model_iter_next (model, &iter));
    }
}

static void
addressadd_load_folder (GtkTreeIter *parent, ItemFolder *parentFolder, FolderInfo *fiParent)
{
  GList *list;
  ItemFolder *folder;
  FolderInfo *fi;
  GtkTreeIter iter;
  GtkTreeStore *model = GTK_TREE_STORE (addressadd_dlg.model);

  list = parentFolder->listFolder;
  while (list)
    {
      folder = list->data;

      fi = addressadd_create_folderinfo (fiParent->book, folder);

      gtk_tree_store_append (model, &iter, parent);
      gtk_tree_store_set (model, &iter, 0, folder_pb, 1, g_strdup (ADDRITEM_NAME (folder)), 2, fi, -1);

      addressadd_load_folder (&iter, folder, fi);

      list = g_list_next (list);
    }
}

static void
addressadd_load_data (AddressIndex * addrIndex)
{
  AddressDataSource *ds;
  GList *list, *nodeDS;
  gchar *dsName;
  ItemFolder *rootFolder;
  AddressBookFile *abf;
  FolderInfo *fi;
  GtkTreeStore *model = GTK_TREE_STORE (addressadd_dlg.model);
  GtkTreeIter iter;

  addressadd_model_clear (addressadd_dlg.model, NULL);

  list = addrindex_get_interface_list (addrIndex);
  while (list)
    {
      AddressInterface *iface = list->data;
      if (iface->type == ADDR_IF_BOOK)
        {
          nodeDS = iface->listSource;
          while (nodeDS)
            {
              ds = nodeDS->data;
              if (!strcmp (addrindex_ds_get_name (ds), ADDR_DS_AUTOREG))
                dsName = g_strdup (_("Auto-registered address"));
              else
                dsName = g_strdup (addrindex_ds_get_name (ds));

              /* Read address book */
              if (!addrindex_ds_get_read_flag (ds))
                addrindex_ds_read_data (ds);

              abf = ds->rawDataSource;
              fi = addressadd_create_folderinfo (abf, NULL);

              /* Add node for address book */
              gtk_tree_store_append (model, &iter, NULL);
              gtk_tree_store_set (model, &iter, 0, book_pb, 1, dsName, 2, fi, -1);

              rootFolder = addrindex_ds_get_root_folder (ds);
              addressadd_load_folder (&iter, rootFolder, fi);

              nodeDS = g_list_next (nodeDS);
            }
        }
      list = g_list_next (list);
    }
}

gboolean
addressadd_selection (AddressIndex * addrIndex, const gchar * name, const gchar * address, const gchar * remarks)
{
  GtkTreeIter iter;
  gboolean retVal = FALSE;
  ItemPerson *person = NULL;

  addressadd_cancelled = FALSE;
  if (!addressadd_dlg.window)
    addressadd_create ();
  gtk_widget_grab_focus (addressadd_dlg.ok_btn);
  manage_window_set_transient (GTK_WINDOW (addressadd_dlg.window));
  gtk_widget_show (addressadd_dlg.window);

  addressadd_dlg.fiSelected = NULL;
  addressadd_load_data (addrIndex);
  gtk_tree_model_get_iter_first (addressadd_dlg.model, &iter);
  gtk_tree_selection_select_iter (addressadd_dlg.sel, &iter);
  gtk_widget_show (addressadd_dlg.window);

  gtk_label_set_text (GTK_LABEL (addressadd_dlg.label_name), "");
  gtk_label_set_text (GTK_LABEL (addressadd_dlg.label_address), "");
  gtk_label_set_text (GTK_LABEL (addressadd_dlg.label_remarks), "");
  if (name)
    gtk_label_set_text (GTK_LABEL (addressadd_dlg.label_name), name);
  if (address)
    gtk_label_set_text (GTK_LABEL (addressadd_dlg.label_address), address);
  if (remarks)
    gtk_label_set_text (GTK_LABEL (addressadd_dlg.label_remarks), remarks);

  if (!name)
    name = "";

  gtk_main ();
  gtk_widget_hide (addressadd_dlg.window);

  if (!addressadd_cancelled)
    {
      if (addressadd_dlg.fiSelected)
        {
          FolderInfo *fi = addressadd_dlg.fiSelected;
          person = addrbook_add_contact (fi->book, fi->folder, name, address, remarks);
          if (person)
            {
              if (addressbook_edit_person (fi->book, NULL, person, FALSE) == NULL)
                addrbook_remove_person (fi->book, person);
              else
                retVal = TRUE;
            }
        }
    }

  addressadd_model_clear (addressadd_dlg.model, NULL);

  return retVal;
}

gboolean
addressadd_autoreg (AddressIndex * addrIndex, const gchar * name, const gchar * address, const gchar * remarks)
{
  ItemPerson *person = NULL;
  AddressInterface *iface = NULL;
  AddressDataSource *ds = NULL;
  AddressBookFile *abf = NULL;
  GList *node_ds;
  const gchar *ds_name;

  g_return_val_if_fail (address != NULL, FALSE);

  if (!name)
    name = "";

  iface = addrindex_get_interface (addrIndex, ADDR_IF_BOOK);
  if (!iface)
    return FALSE;

  for (node_ds = iface->listSource; node_ds != NULL; node_ds = node_ds->next)
    {
      ds = node_ds->data;
      ds_name = addrindex_ds_get_name (ds);
      if (!ds_name)
        continue;
      if (strcmp (ds_name, ADDR_DS_AUTOREG) != 0)
        continue;
      debug_print ("addressadd_autoreg: AddressDataSource: %s found\n", ds_name);

      if (!addrindex_ds_get_read_flag (ds))
        addrindex_ds_read_data (ds);
      abf = ds->rawDataSource;
    }

  if (!abf)
    return FALSE;

  person = addrbook_add_contact (abf, NULL, name, address, remarks);
  if (person)
    {
      debug_print ("addressadd_autoreg: person added: %s <%s>\n", name, address);
      return TRUE;
    }

  return FALSE;
}

/*
* End of Source.
*/
