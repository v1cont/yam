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
 * LDAP Base DN selection dialog.
 */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#ifdef USE_LDAP

#include "defs.h"

#include <glib.h>
#include <glib/gi18n.h>
#include <gdk/gdkkeysyms.h>
#include <gtk/gtk.h>

#include "prefs_common.h"
#include "syldap.h"
#include "mgutils.h"
#include "gtkutils.h"
#include "manage_window.h"

static struct _LDAPEdit_basedn {
  GtkWidget *window;
  GtkWidget *host_label;
  GtkWidget *port_label;
  GtkWidget *basedn_entry;
  GtkWidget *basedn_list;
  GtkWidget *hbbox;
  GtkWidget *ok_btn;
  GtkWidget *cancel_btn;
  GtkWidget *statusbar;
  gint status_cid;
} ldapedit_basedn;

static gboolean ldapedit_basedn_cancelled;
static gboolean ldapedit_basedn_bad_server;

/*
* Edit functions.
*/
static void
edit_ldap_bdn_status_show (gchar * msg)
{
  if (ldapedit_basedn.statusbar != NULL)
    {
      gtk_statusbar_pop (GTK_STATUSBAR (ldapedit_basedn.statusbar), ldapedit_basedn.status_cid);
      if (msg)
        gtk_statusbar_push (GTK_STATUSBAR (ldapedit_basedn.statusbar), ldapedit_basedn.status_cid, msg);
    }
}

static gint
edit_ldap_bdn_delete_event (GtkWidget * widget, GdkEventAny * event, gboolean * cancelled)
{
  ldapedit_basedn_cancelled = TRUE;
  gtk_main_quit ();
  return TRUE;
}

static gboolean
edit_ldap_bdn_key_pressed (GtkWidget * widget, GdkEventKey * event, gboolean * cancelled)
{
  if (event && event->keyval == GDK_KEY_Escape)
    {
      ldapedit_basedn_cancelled = TRUE;
      gtk_main_quit ();
    }
  return FALSE;
}

static void
edit_ldap_bdn_ok (GtkWidget * widget, gboolean * cancelled)
{
  ldapedit_basedn_cancelled = FALSE;
  gtk_main_quit ();
}

static void
edit_ldap_bdn_cancel (GtkWidget * widget, gboolean * cancelled)
{
  ldapedit_basedn_cancelled = TRUE;
  gtk_main_quit ();
}

static void
edit_ldap_bdn_list_select (GtkTreeSelection *sel, gpointer data)
{
  GtkTreeModel *model;
  GtkTreeIter iter;
  gchar *text = NULL;

  if (gtk_tree_selection_get_selected (sel, &model, &iter))
    {
      gtk_tree_model_get (model, &iter, 0, &text, -1);
      if (text)
        gtk_entry_set_text (GTK_ENTRY (ldapedit_basedn.basedn_entry), text);
    }
}

static void
edit_ldap_bdn_list_button (GtkTreeView *tv, GtkTreePath *path, GtkTreeViewColumn *col, gpointer data)
{
  ldapedit_basedn_cancelled = FALSE;
  gtk_main_quit ();
}

static void
edit_ldap_bdn_create (void)
{
  GtkWidget *window;
  GtkWidget *vbox;
  GtkWidget *table;
  GtkWidget *label;
  GtkWidget *host_label;
  GtkWidget *port_label;
  GtkWidget *basedn_list;
  GtkWidget *lwindow;
  GtkWidget *basedn_entry;
  GtkWidget *hbbox;
  GtkWidget *ok_btn;
  GtkWidget *cancel_btn;
  GtkWidget *hsbox;
  GtkWidget *statusbar;
  GtkListStore *store;
  GtkTreeViewColumn *col;
  GtkCellRenderer *renderer;
  GtkTreeSelection *sel;

  window = gtk_window_new (GTK_WINDOW_TOPLEVEL);
  gtk_widget_set_size_request (window, 300, 270);
  gtk_container_set_border_width (GTK_CONTAINER (window), 0);
  gtk_window_set_title (GTK_WINDOW (window), _("Edit LDAP - Select Search Base"));
  gtk_window_set_position (GTK_WINDOW (window), GTK_WIN_POS_CENTER);
  gtk_window_set_modal (GTK_WINDOW (window), TRUE);
  g_signal_connect (G_OBJECT (window), "delete_event", G_CALLBACK (edit_ldap_bdn_delete_event), NULL);
  g_signal_connect (G_OBJECT (window), "key_press_event", G_CALLBACK (edit_ldap_bdn_key_pressed), NULL);

  vbox = gtk_box_new (GTK_ORIENTATION_VERTICAL, 8);
  gtk_container_add (GTK_CONTAINER (window), vbox);
  gtk_container_set_border_width (GTK_CONTAINER (vbox), 0);

  table = gtk_grid_new (3, 2, FALSE);
  gtk_box_pack_start (GTK_BOX (vbox), table, FALSE, FALSE, 0);
  gtk_container_set_border_width (GTK_CONTAINER (table), 5);
  gtk_grid_set_row_spacing (GTK_GRID (table), 5);
  gtk_grid_set_column_spacing (GTK_GRID (table), 5);

  /* First row */
  label = gtk_label_new (_("Hostname"));
  gtk_grid_attach (GTK_GRID (table), label, 0, 0, 1, 1);
  gtk_label_set_xalign (GTK_LABEL (label), 0.0);

  host_label = gtk_label_new (NULL);
  gtk_grid_attach (GTK_GRID (table), host_label, 1, 0, 1, 1);
  gtk_label_set_xalign (GTK_LABEL (label), 0.0);

  /* Second row */
  label = gtk_label_new (_("Port"));
  gtk_grid_attach (GTK_GRID (table), label, 1, 0, 1, 1);
  gtk_label_set_xalign (GTK_LABEL (label), 0.0);

  port_label = gtk_label_new (NULL);
  gtk_grid_attach (GTK_GRID (table), port_label, 1, 1, 1, 1);
  gtk_label_set_xalign (GTK_LABEL (label), 0.0);

  /* Third row */
  label = gtk_label_new (_("Search Base"));
  gtk_grid_attach (GTK_GRID (table), label, 2, 0, 1, 1);
  gtk_label_set_xalign (GTK_LABEL (label), 0.0);

  basedn_entry = gtk_entry_new ();
  gtk_grid_attach (GTK_GRID (table), basedn_entry, 2, 1, 1, 1);

  /* Basedn list */
  lwindow = gtk_scrolled_window_new (NULL, NULL);
  gtk_scrolled_window_set_policy (GTK_SCROLLED_WINDOW (lwindow), GTK_POLICY_AUTOMATIC, GTK_POLICY_ALWAYS);
  gtk_box_pack_start (GTK_BOX (vbox), lwindow, TRUE, TRUE, 0);

  store = gtk_list_store (1, G_TYPE_STRING);
  basedn_list = gtk_tree_view_new_with_model (GTK_TREE_MODEL (store));
  gtk_tree_view_set_headers_visible (GTK_TREE_VIEW (basedn_list), TRUE);
  gtk_container_add (GTK_CONTAINER (lwindow), basedn_list);
  g_object_unref (store);

  sel = gtk_tree_view_get_selection (GTK_TREE_VIEW (basedn_list));
  gtk_tree_selection_set_mode (sel, GTK_SELECTION_BROWSE);

  renderer = gtk_cell_renderer_text_new ();
  col = gtk_tree_view_column_new_with_attributes (_("Headers"), renderer, "text", 0, NULL);
  gtk_tree_view_append_column (basedn_list, col);

  /* Status line */
  hsbox = gtk_box_new (GTK_ORIENTATION_HORIZONTAL, 0);
  gtk_box_pack_end (GTK_BOX (vbox), hsbox, FALSE, FALSE, 0);
  statusbar = gtk_statusbar_new ();
  gtk_box_pack_start (GTK_BOX (hsbox), statusbar, TRUE, TRUE, 0);

  /* Button panel */
  yam_stock_button_set_create (&hbbox, &ok_btn, "yam-ok", &cancel_btn, "yam-cancel", NULL, NULL);
  gtk_box_pack_end (GTK_BOX (vbox), hbbox, FALSE, FALSE, 0);
  gtk_container_set_border_width (GTK_CONTAINER (hbbox), 0);
  gtk_widget_grab_default (ok_btn);

  g_signal_connect (G_OBJECT (ok_btn), "clicked", G_CALLBACK (edit_ldap_bdn_ok), NULL);
  g_signal_connect (G_OBJECT (cancel_btn), "clicked", G_CALLBACK (edit_ldap_bdn_cancel), NULL);
  g_signal_connect (G_OBJECT (sel), "changed", G_CALLBACK (edit_ldap_bdn_list_select), NULL);
  g_signal_connect (G_OBJECT (basedn_list), "row-activated", G_CALLBACK (edit_ldap_bdn_list_button), NULL);

  gtk_widget_show_all (vbox);

  ldapedit_basedn.window = window;
  ldapedit_basedn.host_label = host_label;
  ldapedit_basedn.port_label = port_label;
  ldapedit_basedn.basedn_entry = basedn_entry;
  ldapedit_basedn.basedn_list = basedn_list;
  ldapedit_basedn.hbbox = hbbox;
  ldapedit_basedn.ok_btn = ok_btn;
  ldapedit_basedn.cancel_btn = cancel_btn;
  ldapedit_basedn.statusbar = statusbar;
  ldapedit_basedn.status_cid = gtk_statusbar_get_context_id (GTK_STATUSBAR (statusbar), "Edit LDAP Select Base DN");
}

void
edit_ldap_bdn_load_data (const gchar * hostName, const gint iPort, const gint tov, const gchar * bindDN,
                         const gchar * bindPW)
{
  gchar *sHost;
  gchar *sMsg = NULL;
  gchar sPort[20];
  gboolean flgConn;
  gboolean flgDN;
  GtkListStore *store;
  GtkTreeIter iter;

  store = GTK_LIST_STORE (gtk_tree_view_get_model (GTK_TREE_VIEW (ldapedit_basedn.basedn_list)));

  edit_ldap_bdn_status_show ("");
  gtk_list_store_clear (store);
  ldapedit_basedn_bad_server = TRUE;
  flgConn = flgDN = FALSE;
  sHost = g_strdup (hostName);
  sprintf (sPort, "%d", iPort);
  gtk_label_set_text (GTK_LABEL (ldapedit_basedn.host_label), hostName);
  gtk_label_set_text (GTK_LABEL (ldapedit_basedn.port_label), sPort);
  if (*sHost != '\0')
    {
      /* Test connection to server */
      if (syldap_test_connect_s (sHost, iPort))
        {
          /* Attempt to read base DN */
          GList *baseDN = syldap_read_basedn_s (sHost, iPort, bindDN, bindPW, tov);
          if (baseDN)
            {
              GList *node = baseDN;
              while (node)
                {
                  gtk_list_store_append (store, &iter);
                  gtk_list_store_set (store, &iter, 0, node->data, -1);
                  node = g_list_next (node);
                  flgDN = TRUE;
                }
              mgu_free_dlist (baseDN);
              baseDN = node = NULL;
            }
          ldapedit_basedn_bad_server = FALSE;
          flgConn = TRUE;
        }
    }
  g_free (sHost);

  /* Display appropriate message */
  if (flgConn)
    {
      if (!flgDN)
        sMsg = _("Could not read Search Base(s) from server - please set manually");
    }
  else
    sMsg = _("Could not connect to server");

  edit_ldap_bdn_status_show (sMsg);
}

gchar *
edit_ldap_basedn_selection (const gchar * hostName, const gint port, gchar * baseDN, const gint tov,
                            const gchar * bindDN, const gchar * bindPW)
{
  gchar *retVal = NULL;

  ldapedit_basedn_cancelled = FALSE;
  if (!ldapedit_basedn.window)
    edit_ldap_bdn_create ();
  gtk_widget_grab_focus (ldapedit_basedn.ok_btn);
  gtk_widget_show (ldapedit_basedn.window);
  manage_window_set_transient (GTK_WINDOW (ldapedit_basedn.window));

  edit_ldap_bdn_status_show ("");
  edit_ldap_bdn_load_data (hostName, port, tov, bindDN, bindPW);
  gtk_widget_show (ldapedit_basedn.window);

  gtk_entry_set_text (GTK_ENTRY (ldapedit_basedn.basedn_entry), baseDN);

  gtk_main ();
  gtk_widget_hide (ldapedit_basedn.window);
  if (ldapedit_basedn_cancelled)
    return NULL;
  if (ldapedit_basedn_bad_server)
    return NULL;

  retVal = gtk_editable_get_chars (GTK_EDITABLE (ldapedit_basedn.basedn_entry), 0, -1);
  g_strchomp (retVal);
  g_strchug (retVal);
  if (*retVal == '\0')
    {
      g_free (retVal);
      retVal = NULL;
    }
  return retVal;
}

#endif /* USE_LDAP */

/*
* End of Source.
*/
