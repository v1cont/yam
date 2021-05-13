/*
 * YAM -- a GTK+ based, lightweight, and fast e-mail client
 * Copyright (C) 2020 Victor Ananjevsky <victor@sanana.kiev.ua>
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

/*
 * Edit vCard address book data.
 */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include "defs.h"

#include <glib.h>
#include <glib/gi18n.h>
#include <gdk/gdkkeysyms.h>
#include <gtk/gtk.h>

#include "addressbook.h"
#include "prefs_common.h"
#include "addressitem.h"
#include "vcard.h"
#include "mgutils.h"
#include "filesel.h"
#include "gtkutils.h"
#include "codeconv.h"
#include "manage_window.h"

#define ADDRESSBOOK_GUESS_VCARD  "MyGnomeCard"

static struct _VCardEdit {
  GtkWidget *window;
  GtkWidget *name_entry;
  GtkWidget *file_entry;
  GtkWidget *hbbox;
  GtkWidget *ok_btn;
  GtkWidget *cancel_btn;
  GtkWidget *statusbar;
  gint status_cid;
} vcardedit;

/*
* Edit functions.
*/
void
edit_vcard_status_show (gchar * msg)
{
  if (vcardedit.statusbar != NULL)
    {
      gtk_statusbar_pop (GTK_STATUSBAR (vcardedit.statusbar), vcardedit.status_cid);
      if (msg)
        gtk_statusbar_push (GTK_STATUSBAR (vcardedit.statusbar), vcardedit.status_cid, msg);
    }
}

static void
edit_vcard_ok (GtkWidget * widget, gboolean * cancelled)
{
  *cancelled = FALSE;
  gtk_main_quit ();
}

static void
edit_vcard_cancel (GtkWidget * widget, gboolean * cancelled)
{
  *cancelled = TRUE;
  gtk_main_quit ();
}

static void
edit_vcard_file_check (void)
{
  gint t;
  gchar *sFile;
  gchar *sFSFile;
  gchar *sMsg;

  sFile = gtk_editable_get_chars (GTK_EDITABLE (vcardedit.file_entry), 0, -1);
  sFSFile = conv_filename_from_utf8 (sFile);
  t = vcard_test_read_file (sFSFile);
  g_free (sFSFile);
  g_free (sFile);
  if (t == MGU_SUCCESS)
    sMsg = "";
  else if (t == MGU_BAD_FORMAT)
    sMsg = _("File does not appear to be vCard format.");
  else
    sMsg = _("Could not read file.");
  edit_vcard_status_show (sMsg);
}

static void
edit_vcard_file_select (void)
{
  gchar *sFile;
  gchar *sUTF8File;

  sFile = filesel_select_file (_("Select vCard File"), NULL, GTK_FILE_CHOOSER_ACTION_OPEN);

  if (sFile)
    {
      sUTF8File = conv_filename_to_utf8 (sFile);
      gtk_entry_set_text (GTK_ENTRY (vcardedit.file_entry), sUTF8File);
      g_free (sUTF8File);
      g_free (sFile);
      edit_vcard_file_check ();
    }
}

static gint
edit_vcard_delete_event (GtkWidget * widget, GdkEventAny * event, gboolean * cancelled)
{
  *cancelled = TRUE;
  gtk_main_quit ();
  return TRUE;
}

static gboolean
edit_vcard_key_pressed (GtkWidget * widget, GdkEventKey * event, gboolean * cancelled)
{
  if (event && event->keyval == GDK_KEY_Escape)
    {
      *cancelled = TRUE;
      gtk_main_quit ();
    }
  return FALSE;
}

static void
addressbook_edit_vcard_create (gboolean * cancelled)
{
  GtkWidget *window;
  GtkWidget *vbox;
  GtkWidget *table;
  GtkWidget *label;
  GtkWidget *name_entry;
  GtkWidget *file_entry;
  GtkWidget *hbbox;
  GtkWidget *ok_btn;
  GtkWidget *cancel_btn;
  GtkWidget *check_btn;
  GtkWidget *file_btn;
  GtkWidget *statusbar;

  window = gtk_window_new (GTK_WINDOW_TOPLEVEL);
  gtk_widget_set_size_request (window, 450, -1);
  gtk_container_set_border_width (GTK_CONTAINER (window), 0);
  gtk_window_set_title (GTK_WINDOW (window), _("Edit vCard Entry"));
  gtk_window_set_position (GTK_WINDOW (window), GTK_WIN_POS_CENTER);
  gtk_window_set_modal (GTK_WINDOW (window), TRUE);
  g_signal_connect (G_OBJECT (window), "delete_event", G_CALLBACK (edit_vcard_delete_event), cancelled);
  g_signal_connect (G_OBJECT (window), "key_press_event", G_CALLBACK (edit_vcard_key_pressed), cancelled);

  vbox = gtk_box_new (GTK_ORIENTATION_VERTICAL, 8);
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

  name_entry = gtk_entry_new ();
  gtk_widget_set_hexpand (name_entry, TRUE);
  gtk_grid_attach (GTK_GRID (table), name_entry, 0, 1, 1, 1);

  check_btn = gtk_button_new_with_label (_(" Check File "));
  gtk_grid_attach (GTK_GRID (table), check_btn, 0, 2, 1, 1);

  /* Second row */
  label = gtk_label_new (_("File"));
  gtk_grid_attach (GTK_GRID (table), label, 1, 0, 1, 1);
  gtk_label_set_xalign (GTK_LABEL (label), 0.0);

  file_entry = gtk_entry_new ();
  gtk_widget_set_hexpand (file_entry, TRUE);
  gtk_grid_attach (GTK_GRID (table), file_entry, 1, 1, 1, 1);

  file_btn = gtk_button_new_with_label ("...");
  gtk_grid_attach (GTK_GRID (table), file_btn, 1, 2, 1, 1);

  /* Status line */
  statusbar = gtk_statusbar_new ();
  gtk_box_pack_start (GTK_BOX (vbox), statusbar, TRUE, TRUE, 0);

  /* Button panel */
  yam_stock_button_set_create (&hbbox, &ok_btn, "yam-ok", &cancel_btn, "yam-cancel", NULL, NULL);
  gtk_box_pack_end (GTK_BOX (vbox), hbbox, FALSE, FALSE, 0);
  gtk_container_set_border_width (GTK_CONTAINER (hbbox), 0);
  gtk_widget_grab_default (ok_btn);

  g_signal_connect (G_OBJECT (ok_btn), "clicked", G_CALLBACK (edit_vcard_ok), cancelled);
  g_signal_connect (G_OBJECT (cancel_btn), "clicked", G_CALLBACK (edit_vcard_cancel), cancelled);
  g_signal_connect (G_OBJECT (file_btn), "clicked", G_CALLBACK (edit_vcard_file_select), NULL);
  g_signal_connect (G_OBJECT (check_btn), "clicked", G_CALLBACK (edit_vcard_file_check), NULL);

  gtk_widget_show_all (vbox);

  vcardedit.window = window;
  vcardedit.name_entry = name_entry;
  vcardedit.file_entry = file_entry;
  vcardedit.hbbox = hbbox;
  vcardedit.ok_btn = ok_btn;
  vcardedit.cancel_btn = cancel_btn;
  vcardedit.statusbar = statusbar;
  vcardedit.status_cid = gtk_statusbar_get_context_id (GTK_STATUSBAR (statusbar), "Edit vCard Dialog");
}

AdapterDSource *
addressbook_edit_vcard (AddressIndex * addrIndex, AdapterDSource * ads)
{
  static gboolean cancelled;
  gchar *sName;
  gchar *sFile;
  gchar *sFSFile;
  AddressDataSource *ds = NULL;
  VCardFile *vcf = NULL;
  gboolean fin;

  if (!vcardedit.window)
    addressbook_edit_vcard_create (&cancelled);
  gtk_widget_grab_focus (vcardedit.ok_btn);
  gtk_widget_grab_focus (vcardedit.name_entry);
  gtk_widget_show (vcardedit.window);
  manage_window_set_transient (GTK_WINDOW (vcardedit.window));

  edit_vcard_status_show ("");
  if (ads)
    {
      ds = ads->dataSource;
      vcf = ds->rawDataSource;
      if (vcf->name)
        gtk_entry_set_text (GTK_ENTRY (vcardedit.name_entry), vcf->name);
      if (vcf->path)
        gtk_entry_set_text (GTK_ENTRY (vcardedit.file_entry), vcf->path);
      gtk_window_set_title (GTK_WINDOW (vcardedit.window), _("Edit vCard Entry"));
    }
  else
    {
      gtk_entry_set_text (GTK_ENTRY (vcardedit.name_entry), ADDRESSBOOK_GUESS_VCARD);
      gtk_entry_set_text (GTK_ENTRY (vcardedit.file_entry), vcard_find_gnomecard ());
      gtk_window_set_title (GTK_WINDOW (vcardedit.window), _("Add New vCard Entry"));
    }

  gtk_main ();
  gtk_widget_hide (vcardedit.window);
  if (cancelled == TRUE)
    return NULL;

  fin = FALSE;
  sName = gtk_editable_get_chars (GTK_EDITABLE (vcardedit.name_entry), 0, -1);
  sFile = gtk_editable_get_chars (GTK_EDITABLE (vcardedit.file_entry), 0, -1);
  sFSFile = conv_filename_from_utf8 (sFile);
  if (*sName == '\0')
    fin = TRUE;
  if (*sFile == '\0')
    fin = TRUE;
  if (!sFSFile || *sFSFile == '\0')
    fin = TRUE;

  if (!fin)
    {
      if (!ads)
        {
          vcf = vcard_create ();
          ds = addrindex_index_add_datasource (addrIndex, ADDR_IF_VCARD, vcf);
          ads = addressbook_create_ds_adapter (ds, ADDR_VCARD, NULL);
        }
      addressbook_ads_set_name (ads, sName);
      vcard_set_name (vcf, sName);
      vcard_set_file (vcf, sFSFile);
    }
  g_free (sFSFile);
  g_free (sFile);
  g_free (sName);

  return ads;
}

/*
* End of Source.
*/
