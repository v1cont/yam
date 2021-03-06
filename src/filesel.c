/*
 * YAM -- a GTK+ based, lightweight, and fast e-mail client
 * Copyright (C) 2020 Victor Ananjevsky <victor@sanana.kiev.ua>
 * Copyright (C) 1999-2014 Hiroyuki Yamamoto
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

#include <glib.h>
#include <glib/gi18n.h>
#include <gdk/gdkkeysyms.h>
#include <gtk/gtk.h>

#include "main.h"
#include "filesel.h"
#include "manage_window.h"
#include "alertpanel.h"
#include "utils.h"
#include "prefs_common.h"
#include "inc.h"


static GSList *filesel_select_file_full (const gchar * title, const gchar * file,
                                         GtkFileChooserAction action, gboolean multiple,
                                         FileselFileType * types, gint default_type, gint * selected_type);

static GtkWidget *filesel_create (const gchar * title, GtkFileChooserAction action);

static void filesel_save_expander_set_expanded (GtkWidget * dialog, gboolean expanded);
static gboolean filesel_save_expander_get_expanded (GtkWidget * dialog);

static gchar *filesel_get_filename_with_ext (const gchar * filename, const gchar * ext);

static void filesel_combo_changed_cb (GtkComboBox * combo_box, gpointer data);

static GtkFileChooserConfirmation filesel_confirm_overwrite_cb (GtkFileChooser * chooser, gpointer data);

gchar *
filesel_select_file (const gchar * title, const gchar * file, GtkFileChooserAction action)
{
  GSList *list;
  gchar *selected = NULL;

  list = filesel_select_file_full (title, file, action, FALSE, NULL, 0, NULL);
  if (list)
    {
      selected = (gchar *) list->data;
      slist_free_strings (list->next);
    }
  g_slist_free (list);

  return selected;
}

GSList *
filesel_select_files (const gchar * title, const gchar * file, GtkFileChooserAction action)
{
  return filesel_select_file_full (title, file, action, TRUE, NULL, 0, NULL);
}

static void
filesel_change_dir_for_action (GtkFileChooserAction action)
{
  const gchar *cwd = NULL;

  switch (action)
    {
    case GTK_FILE_CHOOSER_ACTION_OPEN:
      if (prefs_common.prev_open_dir && is_dir_exist (prefs_common.prev_open_dir))
        cwd = prefs_common.prev_open_dir;
      else
        {
          g_free (prefs_common.prev_open_dir);
          prefs_common.prev_open_dir = NULL;
        }
      break;
    case GTK_FILE_CHOOSER_ACTION_SAVE:
      if (prefs_common.prev_save_dir && is_dir_exist (prefs_common.prev_save_dir))
        cwd = prefs_common.prev_save_dir;
      else
        {
          g_free (prefs_common.prev_save_dir);
          prefs_common.prev_save_dir = NULL;
        }
      break;
    case GTK_FILE_CHOOSER_ACTION_SELECT_FOLDER:
      if (prefs_common.prev_folder_dir && is_dir_exist (prefs_common.prev_folder_dir))
        cwd = prefs_common.prev_folder_dir;
      else
        {
          g_free (prefs_common.prev_folder_dir);
          prefs_common.prev_folder_dir = NULL;
        }
      break;
    default:
      break;
    }

  if (cwd)
    change_dir (cwd);
  else
    change_dir (get_document_dir ());
}

static void
filesel_save_dir_for_action (GtkFileChooserAction action, const gchar * cwd)
{
  switch (action)
    {
    case GTK_FILE_CHOOSER_ACTION_OPEN:
      g_free (prefs_common.prev_open_dir);
      prefs_common.prev_open_dir = g_strdup (cwd);
      break;
    case GTK_FILE_CHOOSER_ACTION_SAVE:
      g_free (prefs_common.prev_save_dir);
      prefs_common.prev_save_dir = g_strdup (cwd);
      break;
    case GTK_FILE_CHOOSER_ACTION_SELECT_FOLDER:
      g_free (prefs_common.prev_folder_dir);
      prefs_common.prev_folder_dir = g_strdup (cwd);
      break;
    default:
      break;
    }
}

static GSList *
filesel_select_file_full (const gchar * title, const gchar * file, GtkFileChooserAction action,
                          gboolean multiple, FileselFileType * types, gint default_type, gint * selected_type)
{
  gchar *cwd;
  GtkWidget *dialog;
  gchar *prev_dir;
  static gboolean save_expander_expanded = FALSE;
  GSList *list = NULL;
  GtkWidget *combo = NULL;

  prev_dir = g_get_current_dir ();

  filesel_change_dir_for_action (action);

  dialog = filesel_create (title, action);

  manage_window_set_transient (GTK_WINDOW (dialog));

  cwd = g_get_current_dir ();
  gtk_file_chooser_set_current_folder (GTK_FILE_CHOOSER (dialog), cwd);
  g_free (cwd);
  cwd = NULL;

  if (file)
    gtk_file_chooser_set_current_name (GTK_FILE_CHOOSER (dialog), file);

  gtk_file_chooser_set_select_multiple (GTK_FILE_CHOOSER (dialog), multiple);

  if (action == GTK_FILE_CHOOSER_ACTION_SAVE && save_expander_expanded)
    filesel_save_expander_set_expanded (dialog, save_expander_expanded);

  if (action == GTK_FILE_CHOOSER_ACTION_SAVE)
    {
      gtk_file_chooser_set_do_overwrite_confirmation (GTK_FILE_CHOOSER (dialog), TRUE);
      g_signal_connect (GTK_FILE_CHOOSER (dialog), "confirm-overwrite", G_CALLBACK (filesel_confirm_overwrite_cb), NULL);
    }

  /* create types combo box */
  if (types)
    {
      GtkWidget *hbox;
      GtkWidget *label;
      gint i;

      hbox = gtk_box_new (GTK_ORIENTATION_HORIZONTAL, 12);
      label = gtk_label_new (_("File type:"));
      gtk_box_pack_start (GTK_BOX (hbox), label, FALSE, FALSE, 0);

      combo = gtk_combo_box_text_new ();
      for (i = 0; types[i].type != NULL; i++)
        gtk_combo_box_text_append_text (GTK_COMBO_BOX_TEXT (combo), types[i].type);
      gtk_box_pack_start (GTK_BOX (hbox), combo, FALSE, FALSE, 0);

      gtk_widget_show_all (hbox);
      gtk_file_chooser_set_extra_widget (GTK_FILE_CHOOSER (dialog), hbox);
      if (default_type < 0 || default_type >= i)
        default_type = 0;
      gtk_combo_box_set_active (GTK_COMBO_BOX (combo), default_type);
      g_object_set_data (G_OBJECT (combo), "types", types);
      g_signal_connect (GTK_COMBO_BOX (combo), "changed", G_CALLBACK (filesel_combo_changed_cb), dialog);

      if (file)
        {
          gchar *newfile = filesel_get_filename_with_ext (file, types[default_type].ext);
          gtk_file_chooser_set_current_name (GTK_FILE_CHOOSER (dialog), newfile);
          g_free (newfile);
        }
    }

  gtk_widget_show (dialog);

  change_dir (prev_dir);
  g_free (prev_dir);

  inc_lock ();

  if (gtk_dialog_run (GTK_DIALOG (dialog)) == GTK_RESPONSE_ACCEPT)
    {
      list = gtk_file_chooser_get_filenames (GTK_FILE_CHOOSER (dialog));
      if (list)
        {
          cwd = gtk_file_chooser_get_current_folder (GTK_FILE_CHOOSER (dialog));
          if (cwd)
            {
              filesel_save_dir_for_action (action, cwd);
              g_free (cwd);
            }
        }
    }

  inc_unlock ();

  if (combo && selected_type)
    *selected_type = gtk_combo_box_get_active (GTK_COMBO_BOX (combo));

  if (action == GTK_FILE_CHOOSER_ACTION_SAVE)
    save_expander_expanded = filesel_save_expander_get_expanded (dialog);

  manage_window_focus_out (dialog, NULL, NULL);
  gtk_widget_destroy (dialog);

  return list;
}

gchar *
filesel_save_as (const gchar * file)
{
  return filesel_select_file (_("Save as"), file, GTK_FILE_CHOOSER_ACTION_SAVE);
}

gchar *
filesel_save_as_type (const gchar * file, FileselFileType * types, gint default_type, gint * selected_type)
{
  GSList *list;
  gchar *filename = NULL;

  list = filesel_select_file_full (_("Save as"), file,
                                   GTK_FILE_CHOOSER_ACTION_SAVE, FALSE, types, default_type, selected_type);
  if (list)
    {
      filename = (gchar *) list->data;
      slist_free_strings (list->next);
    }
  g_slist_free (list);

  return filename;
}

gchar *
filesel_select_dir (const gchar * dir)
{
  GSList *list;
  gchar *selected = NULL;

  list = filesel_select_file_full (_("Select folder"), dir,
                                   GTK_FILE_CHOOSER_ACTION_SELECT_FOLDER, FALSE, NULL, 0, NULL);
  if (list)
    {
      selected = (gchar *) list->data;
      slist_free_strings (list->next);
    }
  g_slist_free (list);

  return selected;
}

static GtkWidget *
filesel_create (const gchar * title, GtkFileChooserAction action)
{
  GtkWidget *dialog;

  dialog = gtk_file_chooser_dialog_new (title, NULL, action, _("_Cancel"), GTK_RESPONSE_CANCEL,
                                        (action == GTK_FILE_CHOOSER_ACTION_SAVE ||
                                         action == GTK_FILE_CHOOSER_ACTION_SELECT_FOLDER) ? _("_Save") : _("_Open"), GTK_RESPONSE_ACCEPT,
                                         NULL);
  gtk_window_set_position (GTK_WINDOW (dialog), GTK_WIN_POS_CENTER_ON_PARENT);
  gtk_window_set_modal (GTK_WINDOW (dialog), TRUE);
  gtk_dialog_set_default_response (GTK_DIALOG (dialog), GTK_RESPONSE_ACCEPT);

  MANAGE_WINDOW_SIGNALS_CONNECT (dialog);

  return dialog;
}

static void
container_foreach_cb (GtkWidget * widget, gpointer data)
{
  GtkWidget **expander = (GtkWidget **) data;

  if (*expander == NULL)
    {
      if (GTK_IS_EXPANDER (widget))
        *expander = widget;
      else if (GTK_IS_CONTAINER (widget))
        gtk_container_foreach (GTK_CONTAINER (widget), container_foreach_cb, data);
    }
}

static void
filesel_save_expander_set_expanded (GtkWidget * dialog, gboolean expanded)
{
  GtkWidget *expander = NULL;

  gtk_container_foreach (GTK_CONTAINER (dialog), container_foreach_cb, &expander);
  if (expander)
    gtk_expander_set_expanded (GTK_EXPANDER (expander), expanded);
}

static gboolean
filesel_save_expander_get_expanded (GtkWidget * dialog)
{
  GtkWidget *expander = NULL;

  gtk_container_foreach (GTK_CONTAINER (dialog), container_foreach_cb, &expander);
  if (expander)
    return gtk_expander_get_expanded (GTK_EXPANDER (expander));
  else
    return FALSE;
}

static gchar *
filesel_get_filename_with_ext (const gchar * filename, const gchar * ext)
{
  gchar *base;
  gchar *new_filename;
  gchar *p;

  base = g_path_get_basename (filename);
  p = strrchr (base, '.');
  if (p)
    *p = '\0';
  new_filename = g_strconcat (base, ".", ext, NULL);
  debug_print ("new_filename: %s\n", new_filename);
  g_free (base);

  return new_filename;
}

static void
filesel_combo_changed_cb (GtkComboBox * combo_box, gpointer data)
{
  GtkFileChooser *chooser = data;
  gint active;
  gchar *filename;
  gchar *new_filename;
  FileselFileType *types;

  active = gtk_combo_box_get_active (combo_box);
  filename = gtk_file_chooser_get_filename (chooser);
  if (!filename)
    return;
  types = g_object_get_data (G_OBJECT (combo_box), "types");
  debug_print ("active: %d filename: %s\n", active, filename);
  debug_print ("type ext: %s\n", types[active].ext);
  new_filename = filesel_get_filename_with_ext (filename, types[active].ext);
  gtk_file_chooser_set_current_name (chooser, new_filename);
  g_free (new_filename);
  g_free (filename);
}

static GtkFileChooserConfirmation filesel_confirm_overwrite_cb (GtkFileChooser * chooser, gpointer data) {
  gchar *filename;
  GtkFileChooserConfirmation ret = GTK_FILE_CHOOSER_CONFIRMATION_ACCEPT_FILENAME;

  filename = gtk_file_chooser_get_filename (chooser);

  if (filename && is_file_exist (filename))
    {
      AlertValue aval = alertpanel (_("Overwrite existing file"),
                         _("The file already exists. Do you want to replace it?"), "yam-yes", "yam-no", NULL);
      if (G_ALERTDEFAULT == aval)
        ret = GTK_FILE_CHOOSER_CONFIRMATION_ACCEPT_FILENAME;
      else
        ret = GTK_FILE_CHOOSER_CONFIRMATION_SELECT_AGAIN;
    }

  g_free (filename);

  return ret;
}
