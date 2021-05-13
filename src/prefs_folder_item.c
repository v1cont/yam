/*
 * YAM -- a GTK+ based, lightweight, and fast e-mail client
 * Copyright (C) 2020 Victor Ananjevsky <victor@sanana.kiev.ua>
 * Copyright (C) 1999-2006 Hiroyuki Yamamoto
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

#include "folder.h"
#include "prefs.h"
#include "prefs_ui.h"
#include "prefs_folder_item.h"
#include "prefs_account.h"
#include "account.h"
#include "manage_window.h"
#include "folderview.h"
#include "mainwindow.h"
#include "inc.h"
#include "menu.h"

typedef struct _PrefsFolderItemDialog PrefsFolderItemDialog;

struct _PrefsFolderItemDialog {
  PrefsDialog *dialog;
  FolderItem *item;

  /* General */
  GtkWidget *name_entry;
  GtkWidget *id_label;
  GtkWidget *path_label;
  GtkWidget *type_optmenu;

  GtkWidget *trim_summary_subj_chkbtn;
  GtkWidget *trim_compose_subj_chkbtn;

  /* Compose */
  GtkWidget *account_optmenu;
  GtkWidget *ac_apply_sub_chkbtn;
  GtkWidget *to_entry;
  GtkWidget *on_reply_chkbtn;
  GtkWidget *cc_entry;
  GtkWidget *bcc_entry;
  GtkWidget *replyto_entry;
};

static PrefsFolderItemDialog *prefs_folder_item_create (FolderItem * item);
static void prefs_folder_item_general_create (PrefsFolderItemDialog * dialog);
static void prefs_folder_item_compose_create (PrefsFolderItemDialog * dialog);
static void prefs_folder_item_set_dialog (PrefsFolderItemDialog * dialog);

static void prefs_folder_item_ok_cb (GtkWidget * widget, PrefsFolderItemDialog * dialog);
static void prefs_folder_item_apply_cb (GtkWidget * widget, PrefsFolderItemDialog * dialog);
static void prefs_folder_item_cancel_cb (GtkWidget * widget, PrefsFolderItemDialog * dialog);
static gint prefs_folder_item_delete_cb (GtkWidget * widget, GdkEventAny * event, PrefsFolderItemDialog * dialog);
static gboolean prefs_folder_item_key_press_cb
  (GtkWidget * widget, GdkEventKey * event, PrefsFolderItemDialog * dialog);

void
prefs_folder_item_open (FolderItem * item)
{
  PrefsFolderItemDialog *dialog;

  g_return_if_fail (item != NULL);

  inc_lock ();

  dialog = prefs_folder_item_create (item);

  manage_window_set_transient (GTK_WINDOW (dialog->dialog->window));

  prefs_folder_item_set_dialog (dialog);

  gtk_widget_show_all (dialog->dialog->window);
}

PrefsFolderItemDialog *
prefs_folder_item_create (FolderItem * item)
{
  PrefsFolderItemDialog *new_dialog;
  PrefsDialog *dialog;

  new_dialog = g_new0 (PrefsFolderItemDialog, 1);

  dialog = g_new0 (PrefsDialog, 1);
  prefs_dialog_create (dialog);

  gtk_window_set_title (GTK_WINDOW (dialog->window), _("Folder properties"));
  gtk_widget_realize (dialog->window);
  g_signal_connect (G_OBJECT (dialog->window), "delete_event", G_CALLBACK (prefs_folder_item_delete_cb), new_dialog);
  g_signal_connect (G_OBJECT (dialog->window), "key_press_event",
                    G_CALLBACK (prefs_folder_item_key_press_cb), new_dialog);
  MANAGE_WINDOW_SIGNALS_CONNECT (dialog->window);

  g_signal_connect (G_OBJECT (dialog->ok_btn), "clicked", G_CALLBACK (prefs_folder_item_ok_cb), new_dialog);
  g_signal_connect (G_OBJECT (dialog->apply_btn), "clicked", G_CALLBACK (prefs_folder_item_apply_cb), new_dialog);
  g_signal_connect (G_OBJECT (dialog->cancel_btn), "clicked", G_CALLBACK (prefs_folder_item_cancel_cb), new_dialog);

  new_dialog->dialog = dialog;
  new_dialog->item = item;

  prefs_folder_item_general_create (new_dialog);
  prefs_folder_item_compose_create (new_dialog);

  SET_NOTEBOOK_LABEL (dialog->notebook, _("General"), 0);
  SET_NOTEBOOK_LABEL (dialog->notebook, _("Compose"), 1);

  return new_dialog;
}

static void
prefs_folder_item_general_create (PrefsFolderItemDialog * dialog)
{
  GtkWidget *vbox;
  GtkWidget *table;
  GtkWidget *hbox;
  GtkWidget *label;
  GtkWidget *name_entry;
  GtkWidget *id_label;
  GtkWidget *path_label;
  GtkWidget *optmenu;
  GtkWidget *vbox2;
  GtkWidget *trim_summary_subj_chkbtn;
  GtkWidget *trim_compose_subj_chkbtn;

  vbox = gtk_box_new (GTK_ORIENTATION_VERTICAL, VSPACING);
  gtk_container_add (GTK_CONTAINER (dialog->dialog->notebook), vbox);
  gtk_container_set_border_width (GTK_CONTAINER (vbox), VBOX_BORDER);

  table = gtk_grid_new ();
  gtk_box_pack_start (GTK_BOX (vbox), table, FALSE, FALSE, 0);
  gtk_grid_set_row_spacing (GTK_GRID (table), 5);
  gtk_grid_set_column_spacing (GTK_GRID (table), 5);

  label = gtk_label_new (_("Name"));
  gtk_grid_attach (GTK_GRID (table), label, 0, 0, 1, 1);
  gtk_label_set_xalign (GTK_LABEL (label), 1.0);

  name_entry = gtk_entry_new ();
  gtk_editable_set_editable (GTK_EDITABLE (name_entry), FALSE);
  gtk_widget_set_size_request (name_entry, 200, -1);
  gtk_grid_attach (GTK_GRID (table), name_entry, 1, 0, 1, 1);

  label = gtk_label_new (_("Identifier"));
  gtk_grid_attach (GTK_GRID (table), label, 0, 1, 1, 1);
  gtk_label_set_xalign (GTK_LABEL (label), 1.0);

  id_label = gtk_label_new (NULL);
  gtk_label_set_selectable (GTK_LABEL (id_label), TRUE);
  gtk_label_set_xalign (GTK_LABEL (id_label), 0.0);
  gtk_label_set_justify (GTK_LABEL (id_label), GTK_JUSTIFY_LEFT);
  gtk_label_set_ellipsize (GTK_LABEL (id_label), PANGO_ELLIPSIZE_MIDDLE);
  gtk_grid_attach (GTK_GRID (table), id_label, 1, 1, 1, 1);

  label = gtk_label_new (_("Path"));
  gtk_grid_attach (GTK_GRID (table), label, 0, 2, 1, 1);
  gtk_label_set_xalign (GTK_LABEL (label), 1.0);

  path_label = gtk_label_new (NULL);
  gtk_label_set_selectable (GTK_LABEL (path_label), TRUE);
  gtk_label_set_xalign (GTK_LABEL (path_label), 0.0);
  gtk_label_set_justify (GTK_LABEL (path_label), GTK_JUSTIFY_LEFT);
  gtk_label_set_ellipsize (GTK_LABEL (path_label), PANGO_ELLIPSIZE_MIDDLE);
  gtk_grid_attach (GTK_GRID (table), path_label, 1, 2, 1, 1);

  label = gtk_label_new (_("Type"));
  gtk_grid_attach (GTK_GRID (table), label, 0, 3, 1, 1);
  gtk_label_set_xalign (GTK_LABEL (label), 1.0);

  hbox = gtk_box_new (GTK_ORIENTATION_HORIZONTAL, 8);
  gtk_widget_show (hbox);
  gtk_grid_attach (GTK_GRID (table), hbox, 1, 3, 1, 1);

  optmenu = gtk_combo_box_text_new ();
  gtk_box_pack_start (GTK_BOX (hbox), optmenu, FALSE, FALSE, 0);

  gtk_combo_box_text_append_text (GTK_COMBO_BOX_TEXT (optmenu), _("Normal"));
  gtk_combo_box_text_append_text (GTK_COMBO_BOX_TEXT (optmenu), _("Inbox"));
  gtk_combo_box_text_append_text (GTK_COMBO_BOX_TEXT (optmenu), _("Sent"));
  gtk_combo_box_text_append_text (GTK_COMBO_BOX_TEXT (optmenu), _("Drafts"));
  gtk_combo_box_text_append_text (GTK_COMBO_BOX_TEXT (optmenu), _("Queue"));
  gtk_combo_box_text_append_text (GTK_COMBO_BOX_TEXT (optmenu), _("Trash"));
  gtk_combo_box_text_append_text (GTK_COMBO_BOX_TEXT (optmenu), _("Junk"));

  vbox2 = gtk_box_new (GTK_ORIENTATION_VERTICAL, 0);
  gtk_box_pack_start (GTK_BOX (vbox), vbox2, FALSE, FALSE, 0);

  PACK_CHECK_BUTTON (vbox2, trim_summary_subj_chkbtn,
                     _("Don't display [...] or (...) at the beginning of subject in summary"));
  PACK_CHECK_BUTTON (vbox2, trim_compose_subj_chkbtn, _("Delete [...] or (...) at the beginning of subject on reply"));

  if (!dialog->item->parent)
    {
      gtk_widget_set_sensitive (optmenu, FALSE);
      gtk_widget_set_sensitive (vbox2, FALSE);
    }
  if (dialog->item->stype == F_VIRTUAL)
    gtk_widget_set_sensitive (optmenu, FALSE);

  dialog->name_entry = name_entry;
  dialog->id_label = id_label;
  dialog->path_label = path_label;
  dialog->type_optmenu = optmenu;
  dialog->trim_summary_subj_chkbtn = trim_summary_subj_chkbtn;
  dialog->trim_compose_subj_chkbtn = trim_compose_subj_chkbtn;
}

static void
prefs_folder_item_compose_create (PrefsFolderItemDialog * dialog)
{
  GtkWidget *vbox;
  GtkWidget *frame;
  GtkWidget *account_vbox;
  GtkWidget *table;
  GtkWidget *hbox;
  GtkWidget *label;
  GtkWidget *optmenu;
  GtkWidget *ac_apply_sub_chkbtn;
  GtkWidget *to_entry;
  GtkWidget *on_reply_chkbtn;
  GtkWidget *cc_entry;
  GtkWidget *bcc_entry;
  GtkWidget *replyto_entry;
  GList *list;

  vbox = gtk_box_new (GTK_ORIENTATION_VERTICAL, VSPACING);
  gtk_container_add (GTK_CONTAINER (dialog->dialog->notebook), vbox);
  gtk_container_set_border_width (GTK_CONTAINER (vbox), VBOX_BORDER);

  PACK_FRAME (vbox, frame, _("Account"));

  account_vbox = gtk_box_new (GTK_ORIENTATION_VERTICAL, VSPACING_NARROW);
  gtk_container_add (GTK_CONTAINER (frame), account_vbox);
  gtk_container_set_border_width (GTK_CONTAINER (account_vbox), 8);

  table = gtk_grid_new ();
  gtk_box_pack_start (GTK_BOX (account_vbox), table, FALSE, FALSE, 0);
  gtk_grid_set_row_spacing (GTK_GRID (table), VSPACING_NARROW);
  gtk_grid_set_column_spacing (GTK_GRID (table), 5);

  label = gtk_label_new (_("Account"));
  gtk_grid_attach (GTK_GRID (table), label, 0, 0, 1, 1);
  gtk_label_set_xalign (GTK_LABEL (label), 1.0);

  optmenu = gtk_combo_box_text_new ();
  gtk_grid_attach (GTK_GRID (table), optmenu, 1, 0, 1, 1);

  gtk_combo_box_text_append_text (GTK_COMBO_BOX_TEXT (optmenu), _("None"));
  for (list = account_get_list (); list != NULL; list = list->next)
    {
      gchar *text;
      PrefsAccount *ac = list->data;

      text = g_strdup_printf ("%s: %s", ac->account_name, ac->address);
      gtk_combo_box_text_append_text (GTK_COMBO_BOX_TEXT (optmenu), text);
    }

  PACK_CHECK_BUTTON (account_vbox, ac_apply_sub_chkbtn, _("Apply to subfolders"));

  PACK_FRAME (vbox, frame, _("Automatically set the following addresses"));

  table = gtk_grid_new ();
  gtk_container_add (GTK_CONTAINER (frame), table);
  gtk_container_set_border_width (GTK_CONTAINER (table), 5);
  gtk_grid_set_row_spacing (GTK_GRID (table), VSPACING_NARROW);
  gtk_grid_set_column_spacing (GTK_GRID (table), 5);

  label = gtk_label_new (_("To:"));
  gtk_grid_attach (GTK_GRID (table), label, 0, 0, 1, 1);
  gtk_label_set_xalign (GTK_LABEL (label), 1.0);

  hbox = gtk_box_new (GTK_ORIENTATION_HORIZONTAL, 8);
  gtk_widget_show (hbox);
  gtk_grid_attach (GTK_GRID (table), hbox, 1, 0, 1, 1);

  to_entry = gtk_entry_new ();
  gtk_widget_set_hexpand (to_entry, TRUE);
  gtk_widget_set_size_request (to_entry, 200, -1);
  gtk_box_pack_start (GTK_BOX (hbox), to_entry, TRUE, TRUE, 0);

  PACK_CHECK_BUTTON (hbox, on_reply_chkbtn, _("use also on reply"));

  label = gtk_label_new (_("Cc:"));
  gtk_grid_attach (GTK_GRID (table), label, 0, 1, 1, 1);
  gtk_label_set_xalign (GTK_LABEL (label), 1.0);

  cc_entry = gtk_entry_new ();
  gtk_widget_set_hexpand (cc_entry, TRUE);
  gtk_widget_set_size_request (cc_entry, 200, -1);
  gtk_grid_attach (GTK_GRID (table), cc_entry, 1, 1, 1, 1);

  label = gtk_label_new (_("Bcc:"));
  gtk_grid_attach (GTK_GRID (table), label, 0, 2, 1, 1);
  gtk_label_set_xalign (GTK_LABEL (label), 1.0);

  bcc_entry = gtk_entry_new ();
  gtk_widget_set_hexpand (bcc_entry, TRUE);
  gtk_widget_set_size_request (bcc_entry, 200, -1);
  gtk_grid_attach (GTK_GRID (table), bcc_entry, 1, 2, 1, 1);

  label = gtk_label_new (_("Reply-To:"));
  gtk_grid_attach (GTK_GRID (table), label, 0, 3, 1, 1);
  gtk_label_set_xalign (GTK_LABEL (label), 1.0);

  replyto_entry = gtk_entry_new ();
  gtk_widget_set_hexpand (replyto_entry, TRUE);
  gtk_widget_set_size_request (replyto_entry, 200, -1);
  gtk_grid_attach (GTK_GRID (table), replyto_entry, 1, 3, 1, 1);

  if (!dialog->item->parent)
    {
      gtk_widget_set_sensitive (frame, FALSE);
      gtk_widget_set_sensitive (ac_apply_sub_chkbtn, FALSE);
    }

  dialog->account_optmenu = optmenu;
  dialog->ac_apply_sub_chkbtn = ac_apply_sub_chkbtn;
  dialog->to_entry = to_entry;
  dialog->on_reply_chkbtn = on_reply_chkbtn;
  dialog->cc_entry = cc_entry;
  dialog->bcc_entry = bcc_entry;
  dialog->replyto_entry = replyto_entry;
}

#define SET_ENTRY(entry, str)                                       \
  gtk_entry_set_text(GTK_ENTRY(dialog->entry),                      \
                     dialog->item->str ? dialog->item->str : "")

static void
prefs_folder_item_set_dialog (PrefsFolderItemDialog * dialog)
{
  gchar *id;
  gchar *path;
  gchar *utf8_path;

  /* General */

  SET_ENTRY (name_entry, name);

  id = folder_item_get_identifier (dialog->item);
  gtk_label_set_text (GTK_LABEL (dialog->id_label), id);
  g_free (id);

  path = folder_item_get_path (dialog->item);
  utf8_path = conv_filename_to_utf8 (path);
  gtk_label_set_text (GTK_LABEL (dialog->path_label), utf8_path);
  g_free (utf8_path);
  g_free (path);

  if (FOLDER_TYPE (dialog->item->folder) == F_NEWS)
    {
      gtk_combo_box_set_active (GTK_COMBO_BOX (dialog->type_optmenu), F_NORMAL);
      gtk_widget_set_sensitive (dialog->type_optmenu, FALSE);
    }
  else
    {
      gtk_combo_box_set_active (GTK_COMBO_BOX (dialog->type_optmenu), dialog->item->stype);
      gtk_widget_set_sensitive (dialog->type_optmenu, TRUE);
    }

  gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (dialog->trim_summary_subj_chkbtn), dialog->item->trim_summary_subject);
  gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (dialog->trim_compose_subj_chkbtn), dialog->item->trim_compose_subject);

  /* Compose */

  if (dialog->item->account)
    gtk_combo_box_set_active (GTK_COMBO_BOX (dialog->account_optmenu), dialog->item->account->account_id + 1);
  else
    gtk_combo_box_set_active (GTK_COMBO_BOX (dialog->account_optmenu), 0);

  gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (dialog->ac_apply_sub_chkbtn), dialog->item->ac_apply_sub);

  SET_ENTRY (to_entry, auto_to);
  gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (dialog->on_reply_chkbtn), dialog->item->use_auto_to_on_reply);

  SET_ENTRY (cc_entry, auto_cc);
  SET_ENTRY (bcc_entry, auto_bcc);
  SET_ENTRY (replyto_entry, auto_replyto);
}

#undef SET_ENTRY

void
prefs_folder_item_destroy (PrefsFolderItemDialog * dialog)
{
  prefs_dialog_destroy (dialog->dialog);
  g_free (dialog->dialog);
  g_free (dialog);

  main_window_popup (main_window_get ());
  inc_unlock ();
}

static void
prefs_folder_item_ok_cb (GtkWidget * widget, PrefsFolderItemDialog * dialog)
{
  prefs_folder_item_apply_cb (widget, dialog);
  prefs_folder_item_destroy (dialog);
}

#define SET_DATA_FROM_ENTRY(entry, str)                                 \
  {                                                                     \
	entry_str = gtk_entry_get_text(GTK_ENTRY(dialog->entry));           \
	g_free(item->str);                                                  \
	item->str = (entry_str && *entry_str) ? g_strdup(entry_str) : NULL; \
  }

static void
prefs_folder_item_apply_cb (GtkWidget * widget, PrefsFolderItemDialog * dialog)
{
  SpecialFolderItemType type;
  FolderItem *item = dialog->item;
  Folder *folder = item->folder;
  FolderItem *prev_item = NULL;
  gint account_id;
  const gchar *entry_str;

  type = (SpecialFolderItemType) gtk_combo_box_get_active (GTK_COMBO_BOX (dialog->type_optmenu));

  if (item->stype != type && item->stype != F_VIRTUAL)
    {
      switch (type)
        {
        case F_NORMAL:
          break;
        case F_INBOX:
          if (folder->inbox)
            folder->inbox->stype = F_NORMAL;
          prev_item = folder->inbox;
          folder->inbox = item;
          break;
        case F_OUTBOX:
          if (folder->outbox)
            folder->outbox->stype = F_NORMAL;
          prev_item = folder->outbox;
          folder->outbox = item;
          break;
        case F_DRAFT:
          if (folder->draft)
            folder->draft->stype = F_NORMAL;
          prev_item = folder->draft;
          folder->draft = item;
          break;
        case F_QUEUE:
          if (folder->queue)
            folder->queue->stype = F_NORMAL;
          prev_item = folder->queue;
          folder->queue = item;
          break;
        case F_TRASH:
          if (folder->trash)
            folder->trash->stype = F_NORMAL;
          prev_item = folder->trash;
          folder->trash = item;
          break;
        case F_JUNK:
          prev_item = folder_get_junk (folder);
          if (prev_item)
            prev_item->stype = F_NORMAL;
          folder_set_junk (folder, item);
          break;
        default:
          type = item->stype;
          break;
        }

      item->stype = type;

      if (prev_item)
        folderview_update_item (prev_item, FALSE);
      folderview_update_item (item, FALSE);
    }

  item->trim_summary_subject = gtk_toggle_button_get_active (GTK_TOGGLE_BUTTON (dialog->trim_summary_subj_chkbtn));
  item->trim_compose_subject = gtk_toggle_button_get_active (GTK_TOGGLE_BUTTON (dialog->trim_compose_subj_chkbtn));

  /* account menu */
  account_id = gtk_combo_box_get_active (GTK_COMBO_BOX (dialog->account_optmenu)) - 1;
  if (account_id >= 0)
    item->account = account_find_from_id (account_id);
  else
    item->account = NULL;

  if (!item->parent && item->account)
    item->ac_apply_sub = TRUE;
  else if (item->account)
    item->ac_apply_sub = gtk_toggle_button_get_active (GTK_TOGGLE_BUTTON (dialog->ac_apply_sub_chkbtn));
  else
    item->ac_apply_sub = FALSE;

  SET_DATA_FROM_ENTRY (to_entry, auto_to);
  item->use_auto_to_on_reply = gtk_toggle_button_get_active (GTK_TOGGLE_BUTTON (dialog->on_reply_chkbtn));

  SET_DATA_FROM_ENTRY (cc_entry, auto_cc);
  SET_DATA_FROM_ENTRY (bcc_entry, auto_bcc);
  SET_DATA_FROM_ENTRY (replyto_entry, auto_replyto);
}

#undef SET_DATA_FROM_ENTRY

static void
prefs_folder_item_cancel_cb (GtkWidget * widget, PrefsFolderItemDialog * dialog)
{
  prefs_folder_item_destroy (dialog);
}

static gint
prefs_folder_item_delete_cb (GtkWidget * widget, GdkEventAny * event, PrefsFolderItemDialog * dialog)
{
  prefs_folder_item_destroy (dialog);
  return TRUE;
}

static gboolean
prefs_folder_item_key_press_cb (GtkWidget * widget, GdkEventKey * event, PrefsFolderItemDialog * dialog)
{
  if (event && event->keyval == GDK_KEY_Escape)
    prefs_folder_item_cancel_cb (widget, dialog);
  return FALSE;
}
