/*
 * YAM -- a GTK+ based, lightweight, and fast e-mail client
 * Copyright (C) 2020 Victor Ananjevsky <victor@sanana.kiev.ua>
 * Copyright (C) 1999-2015 Hiroyuki Yamamoto & The Sylpheed Claws Team
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

#include "itemfactory.h"
#include "prefs.h"
#include "inc.h"
#include "utils.h"
#include "gtkutils.h"
#include "manage_window.h"
#include "mainwindow.h"
#include "prefs_common.h"
#include "alertpanel.h"
#include "prefs_actions.h"
#include "action.h"

static struct Actions {
  GtkWidget *window;

  GtkWidget *confirm_area;
  GtkWidget *ok_btn;

  GtkWidget *name_entry;
  GtkWidget *cmd_entry;

  GtkWidget *actions_list;
} actions;

/* widget creating functions */
static void prefs_actions_create (MainWindow * mainwin);
static void prefs_actions_set_dialog (void);
static gint prefs_actions_list_set_row (GtkTreeIter *iter);

/* callback functions */
static void prefs_actions_help_cb (GtkWidget * w, gpointer data);
static void prefs_actions_register_cb (GtkWidget * w, gpointer data);
static void prefs_actions_substitute_cb (GtkWidget * w, gpointer data);
static void prefs_actions_delete_cb (GtkWidget * w, gpointer data);
static void prefs_actions_up (GtkWidget * w, gpointer data);
static void prefs_actions_down (GtkWidget * w, gpointer data);
static void prefs_actions_select (GtkTreeView *list, GtkTreePath *path, GtkTreeViewColumn *col, gpointer data);
static gint prefs_actions_deleted (GtkWidget * widget, GdkEventAny * event, gpointer data);
static gboolean prefs_actions_key_pressed (GtkWidget * widget, GdkEventKey * event, gpointer data);
static void prefs_actions_cancel (GtkWidget * w, gpointer data);
static void prefs_actions_ok (GtkWidget * w, gpointer data);

void
prefs_actions_open (MainWindow * mainwin)
{
  inc_lock ();

  if (!actions.window)
    prefs_actions_create (mainwin);

  manage_window_set_transient (GTK_WINDOW (actions.window));
  gtk_widget_grab_focus (actions.ok_btn);

  prefs_actions_set_dialog ();

  gtk_widget_show (actions.window);
}

static void
prefs_actions_create (MainWindow * mainwin)
{
  GtkWidget *window;
  GtkWidget *vbox;
  GtkWidget *ok_btn;
  GtkWidget *cancel_btn;
  GtkWidget *confirm_area;

  GtkWidget *vbox1;

  GtkWidget *entry_vbox;
  GtkWidget *hbox;
  GtkWidget *name_label;
  GtkWidget *name_entry;
  GtkWidget *cmd_label;
  GtkWidget *cmd_entry;

  GtkWidget *reg_hbox;
  GtkWidget *btn_hbox;
  GtkWidget *arrow;
  GtkWidget *reg_btn;
  GtkWidget *subst_btn;
  GtkWidget *del_btn;

  GtkWidget *cond_hbox;
  GtkWidget *cond_scrolledwin;
  GtkWidget *cond_list;

  GtkListStore *store;
  GtkTreeViewColumn *col;
  GtkCellRenderer *renderer;

  GtkWidget *help_vbox;
  GtkWidget *help_label;
  GtkWidget *help_toggle;

  GtkWidget *btn_vbox;
  GtkWidget *up_btn;
  GtkWidget *down_btn;

  debug_print ("Creating actions configuration window...\n");

  window = gtk_window_new (GTK_WINDOW_TOPLEVEL);

  gtk_container_set_border_width (GTK_CONTAINER (window), 8);
  gtk_window_set_position (GTK_WINDOW (window), GTK_WIN_POS_CENTER);
  gtk_window_set_modal (GTK_WINDOW (window), TRUE);
  gtk_window_set_default_size (GTK_WINDOW (window), 400, -1);

  vbox = gtk_box_new (GTK_ORIENTATION_VERTICAL, 6);
  gtk_container_add (GTK_CONTAINER (window), vbox);

  yam_stock_button_set_create (&confirm_area, &ok_btn, "yam-ok", &cancel_btn, "yam-cancel", NULL, NULL);
  gtk_box_pack_end (GTK_BOX (vbox), confirm_area, FALSE, FALSE, 0);
  gtk_widget_grab_default (ok_btn);

  gtk_window_set_title (GTK_WINDOW (window), _("Actions configuration"));
  g_signal_connect (G_OBJECT (window), "delete_event", G_CALLBACK (prefs_actions_deleted), NULL);
  g_signal_connect (G_OBJECT (window), "key_press_event", G_CALLBACK (prefs_actions_key_pressed), NULL);
  MANAGE_WINDOW_SIGNALS_CONNECT (window);
  g_signal_connect (G_OBJECT (ok_btn), "clicked", G_CALLBACK (prefs_actions_ok), mainwin);
  g_signal_connect (G_OBJECT (cancel_btn), "clicked", G_CALLBACK (prefs_actions_cancel), NULL);

  vbox1 = gtk_box_new (GTK_ORIENTATION_VERTICAL, 8);
  gtk_box_pack_start (GTK_BOX (vbox), vbox1, TRUE, TRUE, 0);
  gtk_container_set_border_width (GTK_CONTAINER (vbox1), 2);

  entry_vbox = gtk_box_new (GTK_ORIENTATION_VERTICAL, 4);
  gtk_box_pack_start (GTK_BOX (vbox1), entry_vbox, FALSE, FALSE, 0);

  hbox = gtk_box_new (GTK_ORIENTATION_HORIZONTAL, 8);
  gtk_box_pack_start (GTK_BOX (entry_vbox), hbox, FALSE, FALSE, 0);

  name_label = gtk_label_new (_("Menu name:"));
  gtk_box_pack_start (GTK_BOX (hbox), name_label, FALSE, FALSE, 0);

  name_entry = gtk_entry_new ();
  gtk_box_pack_start (GTK_BOX (hbox), name_entry, TRUE, TRUE, 0);

  hbox = gtk_box_new (GTK_ORIENTATION_HORIZONTAL, 8);
  gtk_box_pack_start (GTK_BOX (entry_vbox), hbox, TRUE, TRUE, 0);

  cmd_label = gtk_label_new (_("Command line:"));
  gtk_box_pack_start (GTK_BOX (hbox), cmd_label, FALSE, FALSE, 0);

  cmd_entry = gtk_entry_new ();
  gtk_box_pack_start (GTK_BOX (hbox), cmd_entry, TRUE, TRUE, 0);

  help_vbox = gtk_box_new (GTK_ORIENTATION_VERTICAL, 8);
  gtk_box_pack_start (GTK_BOX (vbox1), help_vbox, FALSE, FALSE, 0);

  help_label = gtk_label_new
    (_("Menu name:\n"
       " Use / in menu name to make submenus.\n"
       "Command line:\n"
       " Begin with:\n"
       "   | to send message body or selection to command\n"
       "   > to send user provided text to command\n"
       "   * to send user provided hidden text to command\n"
       " End with:\n"
       "   | to replace message body or selection with command output\n"
       "   > to insert command's output without replacing old text\n"
       "   & to run command asynchronously\n"
       " Use:\n"
       "   %f for message file name\n"
       "   %F for the list of the file names of selected messages\n"
       "   %p for the selected message part\n"
       "   %u for a user provided argument\n"
       "   %h for a user provided hidden argument\n" "   %s for the text selection"));
  gtk_label_set_xalign (GTK_LABEL (help_label), 0.0);
  gtk_label_set_justify (GTK_LABEL (help_label), GTK_JUSTIFY_LEFT);
  gtk_box_pack_start (GTK_BOX (help_vbox), help_label, FALSE, FALSE, 0);

  /* register / substitute / delete */

  reg_hbox = gtk_box_new (GTK_ORIENTATION_HORIZONTAL, 4);
  gtk_widget_show (reg_hbox);
  gtk_box_pack_start (GTK_BOX (vbox1), reg_hbox, FALSE, FALSE, 0);

  arrow = yam_arrow_new (ARROW_DOWN);
  gtk_box_pack_start (GTK_BOX (reg_hbox), arrow, FALSE, FALSE, 0);
  //gtk_widget_set_size_request (arrow, -1, 16);

  btn_hbox = gtk_box_new (GTK_ORIENTATION_HORIZONTAL, 4);
  gtk_box_pack_start (GTK_BOX (reg_hbox), btn_hbox, FALSE, FALSE, 0);

  reg_btn = gtk_button_new_with_label (_("Add"));
  gtk_box_pack_start (GTK_BOX (btn_hbox), reg_btn, FALSE, TRUE, 0);
  g_signal_connect (G_OBJECT (reg_btn), "clicked", G_CALLBACK (prefs_actions_register_cb), NULL);

  subst_btn = gtk_button_new_with_label (_(" Replace "));
  gtk_box_pack_start (GTK_BOX (btn_hbox), subst_btn, FALSE, TRUE, 0);
  g_signal_connect (G_OBJECT (subst_btn), "clicked", G_CALLBACK (prefs_actions_substitute_cb), NULL);

  del_btn = gtk_button_new_with_label (_("Delete"));
  gtk_box_pack_start (GTK_BOX (btn_hbox), del_btn, FALSE, TRUE, 0);
  g_signal_connect (G_OBJECT (del_btn), "clicked", G_CALLBACK (prefs_actions_delete_cb), NULL);

  help_toggle = gtk_toggle_button_new_with_label (_(" Syntax help "));
  gtk_box_pack_end (GTK_BOX (reg_hbox), help_toggle, FALSE, FALSE, 0);
  g_signal_connect (G_OBJECT (help_toggle), "toggled", G_CALLBACK (prefs_actions_help_cb), help_vbox);

  cond_hbox = gtk_box_new (GTK_ORIENTATION_HORIZONTAL, 8);
  gtk_box_pack_start (GTK_BOX (vbox1), cond_hbox, TRUE, TRUE, 0);

  cond_scrolledwin = gtk_scrolled_window_new (NULL, NULL);
  gtk_widget_set_size_request (cond_scrolledwin, -1, 150);
  gtk_box_pack_start (GTK_BOX (cond_hbox), cond_scrolledwin, TRUE, TRUE, 0);
  gtk_scrolled_window_set_policy (GTK_SCROLLED_WINDOW (cond_scrolledwin), GTK_POLICY_AUTOMATIC, GTK_POLICY_AUTOMATIC);

  store = gtk_list_store_new (1, G_TYPE_STRING);
  cond_list = gtk_tree_view_new_with_model (GTK_TREE_MODEL (store));
  gtk_tree_view_set_headers_visible (GTK_TREE_VIEW (cond_list), TRUE);
  gtk_tree_view_set_activate_on_single_click (GTK_TREE_VIEW (cond_list), TRUE);
  gtk_tree_selection_set_mode (gtk_tree_view_get_selection (GTK_TREE_VIEW (cond_list)), GTK_SELECTION_BROWSE);
  gtk_container_add (GTK_CONTAINER (cond_scrolledwin), cond_list);
  g_object_unref (store);

  renderer = gtk_cell_renderer_text_new ();
  col = gtk_tree_view_column_new_with_attributes (_("Registered actions"), renderer, "text", 0, NULL);
  gtk_tree_view_column_set_expand (col, TRUE);
  gtk_tree_view_append_column (GTK_TREE_VIEW (cond_list), col);

  g_signal_connect (G_OBJECT (cond_list), "row-activated", G_CALLBACK (prefs_actions_select), NULL);

  btn_vbox = gtk_box_new (GTK_ORIENTATION_VERTICAL, 8);
  gtk_box_pack_start (GTK_BOX (cond_hbox), btn_vbox, FALSE, FALSE, 0);

  up_btn = gtk_button_new_with_label (_("Up"));
  gtk_box_pack_start (GTK_BOX (btn_vbox), up_btn, FALSE, FALSE, 0);
  g_signal_connect (G_OBJECT (up_btn), "clicked", G_CALLBACK (prefs_actions_up), NULL);

  down_btn = gtk_button_new_with_label (_("Down"));
  gtk_box_pack_start (GTK_BOX (btn_vbox), down_btn, FALSE, FALSE, 0);
  g_signal_connect (G_OBJECT (down_btn), "clicked", G_CALLBACK (prefs_actions_down), NULL);

  gtk_widget_show_all (window);
  gtk_widget_hide (help_vbox);

  actions.window = window;

  actions.confirm_area = confirm_area;
  actions.ok_btn = ok_btn;

  actions.name_entry = name_entry;
  actions.cmd_entry = cmd_entry;

  actions.actions_list = cond_list;
}

static void
prefs_actions_help_cb (GtkWidget * w, gpointer data)
{
  if (gtk_toggle_button_get_active (GTK_TOGGLE_BUTTON (w)))
    gtk_widget_show (GTK_WIDGET (data));
  else
    gtk_widget_hide (GTK_WIDGET (data));
}

void
prefs_actions_read_config (void)
{
  gchar *rcpath;
  FILE *fp;
  gchar buf[PREFSBUFSIZE];
  gchar *act;

  debug_print ("Reading actions configurations...\n");

  rcpath = g_strconcat (get_rc_dir (), G_DIR_SEPARATOR_S, ACTIONS_RC, NULL);
  if ((fp = g_fopen (rcpath, "rb")) == NULL)
    {
      if (ENOENT != errno)
        FILE_OP_ERROR (rcpath, "fopen");
      g_free (rcpath);
      return;
    }
  g_free (rcpath);

  while (prefs_common.actions_list != NULL)
    {
      act = (gchar *) prefs_common.actions_list->data;
      prefs_common.actions_list = g_slist_remove (prefs_common.actions_list, act);
      g_free (act);
    }

  while (fgets (buf, sizeof (buf), fp) != NULL)
    {
      g_strchomp (buf);
      act = strstr (buf, ": ");
      if (act && act[2] && action_get_type (&act[2]) != ACTION_ERROR)
        prefs_common.actions_list = g_slist_append (prefs_common.actions_list, g_strdup (buf));
    }
  fclose (fp);
}

void
prefs_actions_write_config (void)
{
  gchar *rcpath;
  PrefFile *pfile;
  GSList *cur;

  debug_print ("Writing actions configuration...\n");

  rcpath = g_strconcat (get_rc_dir (), G_DIR_SEPARATOR_S, ACTIONS_RC, NULL);
  if ((pfile = prefs_file_open (rcpath)) == NULL)
    {
      g_warning ("failed to write configuration to file\n");
      g_free (rcpath);
      return;
    }

  for (cur = prefs_common.actions_list; cur != NULL; cur = cur->next)
    {
      gchar *act = (gchar *) cur->data;
      if (fputs (act, pfile->fp) == EOF || fputc ('\n', pfile->fp) == EOF)
        {
          FILE_OP_ERROR (rcpath, "fputs || fputc");
          prefs_file_close_revert (pfile);
          g_free (rcpath);
          return;
        }
    }

  g_free (rcpath);

  if (prefs_file_close (pfile) < 0)
    {
      g_warning ("failed to write configuration to file\n");
      return;
    }
}

static void
prefs_actions_set_dialog (void)
{
  GtkListStore *store;
  GtkTreeIter iter;
  GSList *cur;

  store = GTK_LIST_STORE (gtk_tree_view_get_model (GTK_TREE_VIEW (actions.actions_list)));

  gtk_widget_freeze_child_notify (actions.actions_list);

  gtk_list_store_clear (store);

  for (cur = prefs_common.actions_list; cur != NULL; cur = cur->next)
    {
      gtk_list_store_append (store, &iter);
      gtk_list_store_set (store, &iter, 0, cur->data, -1);
    }

  gtk_widget_thaw_child_notify (actions.actions_list);
}

static void
prefs_actions_set_list (void)
{
  GtkTreeModel *model;
  GtkTreeIter iter;
  gchar *action;

  g_slist_free (prefs_common.actions_list);
  prefs_common.actions_list = NULL;

  model = gtk_tree_view_get_model (GTK_TREE_VIEW (actions.actions_list));
  if (gtk_tree_model_get_iter_first (model, &iter))
    {
      do
        {
          gtk_tree_model_get (model, &iter, 0, &action, -1);
          prefs_common.actions_list = g_slist_append (prefs_common.actions_list, action);
        }
      while (gtk_tree_model_iter_next (model, &iter));
    }
}

#define GET_ENTRY(entry) \
	entry_text = gtk_entry_get_text(GTK_ENTRY(entry))

static gint
prefs_actions_list_set_row (GtkTreeIter *iter)
{
  GtkListStore *store;
  const gchar *entry_text;
  gint len;
  gchar action[PREFSBUFSIZE];

  GET_ENTRY (actions.name_entry);
  if (entry_text[0] == '\0')
    {
      alertpanel_error (_("Menu name is not set."));
      return -1;
    }

  if (strchr (entry_text, ':'))
    {
      alertpanel_error (_("Colon ':' is not allowed in the menu name."));
      return -1;
    }

  strncpy (action, entry_text, PREFSBUFSIZE - 1);
  g_strstrip (action);

  /* Keep space for the ': ' delimiter */
  len = strlen (action) + 2;
  if (len >= PREFSBUFSIZE - 1)
    {
      alertpanel_error (_("Menu name is too long."));
      return -1;
    }

  strcat (action, ": ");

  GET_ENTRY (actions.cmd_entry);

  if (entry_text[0] == '\0')
    {
      alertpanel_error (_("Command line not set."));
      return -1;
    }

  if (len + strlen (entry_text) >= PREFSBUFSIZE - 1)
    {
      alertpanel_error (_("Menu name and command are too long."));
      return -1;
    }

  if (action_get_type (entry_text) == ACTION_ERROR)
    {
      alertpanel_error (_("The command\n%s\nhas a syntax error."), entry_text);
      return -1;
    }

  strcat (action, entry_text);

  store = GTK_LIST_STORE (gtk_tree_view_get_model (GTK_TREE_VIEW (actions.actions_list)));

  if (iter)
    {
      gchar *old_action;
      gtk_tree_model_get (GTK_TREE_MODEL (store), iter, 1, &old_action, -1);
      g_free (old_action);
      gtk_list_store_set (store, iter, 0, action, -1);
    }
  else
    {
      GtkTreeIter new;
      gtk_list_store_append (store, &new);
      gtk_list_store_set (store, &new, 0, action, -1);
    }

  prefs_actions_set_list ();

  return 0;
}

/* callback functions */

static void
prefs_actions_register_cb (GtkWidget * w, gpointer data)
{
  prefs_actions_list_set_row (NULL);
}

static void
prefs_actions_substitute_cb (GtkWidget * w, gpointer data)
{
  GtkTreeSelection *sel;
  GtkTreeModel *model;
  GtkTreeIter iter;
  gchar *action;

  sel = gtk_tree_view_get_selection (GTK_TREE_VIEW (actions.actions_list));
  if (!gtk_tree_selection_get_selected (sel, &model, &iter))
    return;

  gtk_tree_model_get (model, &iter, 0, &action, -1);
  if (!action)
    return;

  prefs_actions_list_set_row (&iter);
}

static void
prefs_actions_delete_cb (GtkWidget * w, gpointer data)
{
  GtkTreeSelection *sel;
  GtkTreeModel *model;
  GtkTreeIter iter;
  gchar *action;

  sel = gtk_tree_view_get_selection (GTK_TREE_VIEW (actions.actions_list));
  if (!gtk_tree_selection_get_selected (sel, &model, &iter))
    return;

  if (alertpanel (_("Delete action"),
                  _("Do you really want to delete this action?"), "yam-yes", "yam-no", NULL) != G_ALERTDEFAULT)
    return;

  gtk_tree_model_get (model, &iter, 0, &action, -1);
  g_free (action);
  gtk_list_store_remove (GTK_LIST_STORE (model), &iter);
  prefs_common.actions_list = g_slist_remove (prefs_common.actions_list, action);
}

static void
prefs_actions_up (GtkWidget *w, gpointer data)
{
  GtkTreeSelection *sel;
  GtkTreeModel *model;
  GtkTreeIter iter, pos;

  sel = gtk_tree_view_get_selection (GTK_TREE_VIEW (actions.actions_list));

  if (!gtk_tree_selection_get_selected (sel, &model, &iter))
    return;

  pos = iter;
  if (gtk_tree_model_iter_previous (model, &pos))
    gtk_list_store_move_before (GTK_LIST_STORE (model), &iter, &pos);

  prefs_actions_set_list ();
}

static void
prefs_actions_down (GtkWidget *w, gpointer data)
{
  GtkTreeSelection *sel;
  GtkTreeModel *model;
  GtkTreeIter iter, pos;

  sel = gtk_tree_view_get_selection (GTK_TREE_VIEW (actions.actions_list));

  if (!gtk_tree_selection_get_selected (sel, &model, &iter))
    return;

  pos = iter;
  if (gtk_tree_model_iter_next (model, &pos))
    gtk_list_store_move_after (GTK_LIST_STORE (model), &iter, &pos);

  prefs_actions_set_list ();
}

#define ENTRY_SET_TEXT(entry, str) \
	gtk_entry_set_text(GTK_ENTRY(entry), str ? str : "")

static void
prefs_actions_select (GtkTreeView *list, GtkTreePath *path, GtkTreeViewColumn *col, gpointer data)
{
  GtkTreeModel *model;
  GtkTreeIter iter;
  gchar *action;
  gchar *cmd;
  gchar buf[PREFSBUFSIZE];

  model = gtk_tree_view_get_model (list);
  gtk_tree_model_get_iter (model, &iter, path);
  gtk_tree_model_get (model, &iter, 0, &action, -1);

  if (!action || action[0] == '(')
    {
      gtk_entry_set_text (GTK_ENTRY (actions.name_entry), "");
      gtk_entry_set_text (GTK_ENTRY (actions.cmd_entry), "");
      return;
    }

  strncpy (buf, action, PREFSBUFSIZE - 1);
  buf[PREFSBUFSIZE - 1] = '\0';
  cmd = strstr (buf, ": ");

  if (cmd && cmd[2])
    {
      gtk_entry_set_text (GTK_ENTRY (actions.cmd_entry), &cmd[2]);
      *cmd = '\0';
      gtk_entry_set_text (GTK_ENTRY (actions.name_entry), buf);
    }
}

static gint
prefs_actions_deleted (GtkWidget * widget, GdkEventAny * event, gpointer data)
{
  prefs_actions_cancel (widget, data);
  return TRUE;
}

static gboolean
prefs_actions_key_pressed (GtkWidget * widget, GdkEventKey * event, gpointer data)
{
  if (event && event->keyval == GDK_KEY_Escape)
    prefs_actions_cancel (widget, data);
  return FALSE;
}

static void
prefs_actions_cancel (GtkWidget * w, gpointer data)
{
  prefs_actions_read_config ();
  gtk_widget_hide (actions.window);
  main_window_popup (main_window_get ());
  inc_unlock ();
}

static void
prefs_actions_ok (GtkWidget * widget, gpointer data)
{
  GtkItemFactory *ifactory;
  MainWindow *mainwin = (MainWindow *) data;

  prefs_actions_write_config ();
  ifactory = gtk_item_factory_from_widget (mainwin->menubar);
  action_update_mainwin_menu (ifactory, mainwin);
  gtk_widget_hide (actions.window);
  main_window_popup (main_window_get ());
  inc_unlock ();
}
