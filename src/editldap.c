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
 * Edit LDAP address book data.
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

#include "addressbook.h"
#include "prefs_common.h"
#include "addressitem.h"
#include "mgutils.h"
#include "syldap.h"
#include "editldap_basedn.h"
#include "manage_window.h"
#include "gtkutils.h"

#define ADDRESSBOOK_GUESS_LDAP_NAME	"MyServer"
#define ADDRESSBOOK_GUESS_LDAP_SERVER	"localhost"

static struct _LDAPEdit {
  GtkWidget *window;
  GtkWidget *notebook;
  GtkWidget *hbbox;
  GtkWidget *ok_btn;
  GtkWidget *cancel_btn;
  GtkWidget *statusbar;
  gint status_cid;
  GtkWidget *entry_name;
  GtkWidget *entry_server;
  GtkWidget *spinbtn_port;
  GtkWidget *entry_baseDN;
  GtkWidget *spinbtn_timeout;
  GtkWidget *entry_bindDN;
  GtkWidget *entry_bindPW;
  GtkWidget *entry_criteria;
  GtkWidget *spinbtn_maxentry;
} ldapedit;

/*
* Edit functions.
*/
static void
edit_ldap_status_show (gchar * msg)
{
  if (ldapedit.statusbar != NULL)
    {
      gtk_statusbar_pop (GTK_STATUSBAR (ldapedit.statusbar), ldapedit.status_cid);
      if (msg)
        gtk_statusbar_push (GTK_STATUSBAR (ldapedit.statusbar), ldapedit.status_cid, msg);
    }
}

static void
edit_ldap_ok (GtkWidget * widget, gboolean * cancelled)
{
  *cancelled = FALSE;
  gtk_main_quit ();
}

static void
edit_ldap_cancel (GtkWidget * widget, gboolean * cancelled)
{
  *cancelled = TRUE;
  gtk_main_quit ();
}

static gint
edit_ldap_delete_event (GtkWidget * widget, GdkEventAny * event, gboolean * cancelled)
{
  *cancelled = TRUE;
  gtk_main_quit ();
  return TRUE;
}

static gboolean
edit_ldap_key_pressed (GtkWidget * widget, GdkEventKey * event, gboolean * cancelled)
{
  if (event && event->keyval == GDK_KEY_Escape)
    {
      *cancelled = TRUE;
      gtk_main_quit ();
    }
  return FALSE;
}

static void
edit_ldap_switch_page (GtkWidget * widget)
{
  edit_ldap_status_show ("");
}

static void
edit_ldap_server_check (void)
{
  gchar *sHost, *sBind, *sPass;
  gint iPort, iTime;
  gchar *sMsg;
  gchar *sBaseDN = NULL;
  gint iBaseDN = 0;
  gboolean flg;

  edit_ldap_status_show ("");
  flg = FALSE;
  sHost = gtk_editable_get_chars (GTK_EDITABLE (ldapedit.entry_server), 0, -1);
  sBind = gtk_editable_get_chars (GTK_EDITABLE (ldapedit.entry_bindDN), 0, -1);
  sPass = gtk_editable_get_chars (GTK_EDITABLE (ldapedit.entry_bindPW), 0, -1);
  iPort = gtk_spin_button_get_value_as_int (GTK_SPIN_BUTTON (ldapedit.spinbtn_port));
  iTime = gtk_spin_button_get_value_as_int (GTK_SPIN_BUTTON (ldapedit.spinbtn_timeout));
  g_strchomp (sHost);
  g_strchug (sHost);
  g_strchomp (sBind);
  g_strchug (sBind);
  g_strchomp (sPass);
  g_strchug (sPass);
  if (*sHost != '\0')
    {
      /* Test connection to server */
      if (syldap_test_connect_s (sHost, iPort))
        {
          /* Attempt to read base DN */
          GList *baseDN = syldap_read_basedn_s (sHost, iPort, sBind, sPass, iTime);
          if (baseDN)
            {
              GList *node = baseDN;
              while (node)
                {
                  ++iBaseDN;
                  if (!sBaseDN)
                    sBaseDN = g_strdup (node->data);
                  node = g_list_next (node);
                }
              mgu_free_dlist (baseDN);
              baseDN = node = NULL;
            }
          flg = TRUE;
        }
    }
  g_free (sHost);
  g_free (sBind);
  g_free (sPass);

  if (sBaseDN)
    {
      /* Load search DN */
      gtk_entry_set_text (GTK_ENTRY (ldapedit.entry_baseDN), sBaseDN);
      g_free (sBaseDN);
    }

  /* Display appropriate message */
  if (flg)
    sMsg = _("Connected successfully to server");
  else
    sMsg = _("Could not connect to server");
  edit_ldap_status_show (sMsg);
}

static void
edit_ldap_basedn_select (void)
{
  gchar *sHost, *sBind, *sPass, *sBase;
  gint iPort, iTime;
  gchar *selectDN;

  sHost = gtk_editable_get_chars (GTK_EDITABLE (ldapedit.entry_server), 0, -1);
  sBase = gtk_editable_get_chars (GTK_EDITABLE (ldapedit.entry_baseDN), 0, -1);
  sBind = gtk_editable_get_chars (GTK_EDITABLE (ldapedit.entry_bindDN), 0, -1);
  sPass = gtk_editable_get_chars (GTK_EDITABLE (ldapedit.entry_bindPW), 0, -1);
  iPort = gtk_spin_button_get_value_as_int (GTK_SPIN_BUTTON (ldapedit.spinbtn_port));
  iTime = gtk_spin_button_get_value_as_int (GTK_SPIN_BUTTON (ldapedit.spinbtn_timeout));
  g_strchomp (sHost);
  g_strchug (sHost);
  g_strchomp (sBind);
  g_strchug (sBind);
  g_strchomp (sPass);
  g_strchug (sPass);
  selectDN = edit_ldap_basedn_selection (sHost, iPort, sBase, iTime, sBind, sPass);
  if (selectDN)
    {
      gtk_entry_set_text (GTK_ENTRY (ldapedit.entry_baseDN), selectDN);
      g_free (selectDN);
      selectDN = NULL;
    }
  g_free (sHost);
  g_free (sBase);
  g_free (sBind);
  g_free (sPass);
}

static void
edit_ldap_search_reset (void)
{
  gtk_entry_set_text (GTK_ENTRY (ldapedit.entry_criteria), SYLDAP_DFL_CRITERIA);
}

static void
addressbook_edit_ldap_dialog_create (gboolean * cancelled)
{
  GtkWidget *window;
  GtkWidget *vbox;
  GtkWidget *notebook;
  GtkWidget *hbbox;
  GtkWidget *ok_btn;
  GtkWidget *cancel_btn;
  GtkWidget *hsbox;
  GtkWidget *statusbar;

  window = gtk_window_new (GTK_WINDOW_TOPLEVEL);
  gtk_widget_set_size_request (window, 450, -1);
  gtk_container_set_border_width (GTK_CONTAINER (window), 0);
  gtk_window_set_title (GTK_WINDOW (window), _("Edit LDAP Server"));
  gtk_window_set_position (GTK_WINDOW (window), GTK_WIN_POS_CENTER);
  gtk_window_set_modal (GTK_WINDOW (window), TRUE);
  g_signal_connect (G_OBJECT (window), "delete_event", G_CALLBACK (edit_ldap_delete_event), cancelled);
  g_signal_connect (G_OBJECT (window), "key_press_event", G_CALLBACK (edit_ldap_key_pressed), cancelled);

  vbox = gtk_box_new (GTK_ORIENTATION_VERTICAL, 6);
  /* gtk_container_set_border_width(GTK_CONTAINER(vbox), BORDER_WIDTH); */
  gtk_widget_show (vbox);
  gtk_container_add (GTK_CONTAINER (window), vbox);

  /* Notebook */
  notebook = gtk_notebook_new ();
  gtk_widget_show (notebook);
  gtk_box_pack_start (GTK_BOX (vbox), notebook, TRUE, TRUE, 0);
  gtk_container_set_border_width (GTK_CONTAINER (notebook), 6);

  /* Status line */
  hsbox = gtk_box_new (GTK_ORIENTATION_HORIZONTAL, 0);
  gtk_box_pack_end (GTK_BOX (vbox), hsbox, FALSE, FALSE, 0);
  statusbar = gtk_statusbar_new ();
  gtk_box_pack_start (GTK_BOX (hsbox), statusbar, TRUE, TRUE, 0);

  /* Button panel */
  yam_stock_button_set_create (&hbbox, &ok_btn, "yam-ok", &cancel_btn, "yam-cancel", NULL, NULL);
  gtk_box_pack_end (GTK_BOX (vbox), hbbox, FALSE, FALSE, 0);
  gtk_widget_grab_default (ok_btn);

  g_signal_connect (G_OBJECT (ok_btn), "clicked", G_CALLBACK (edit_ldap_ok), cancelled);
  g_signal_connect (G_OBJECT (cancel_btn), "clicked", G_CALLBACK (edit_ldap_cancel), cancelled);
  g_signal_connect (G_OBJECT (notebook), "switch_page", G_CALLBACK (edit_ldap_switch_page), NULL);

  gtk_widget_show_all (vbox);

  ldapedit.window = window;
  ldapedit.notebook = notebook;
  ldapedit.hbbox = hbbox;
  ldapedit.ok_btn = ok_btn;
  ldapedit.cancel_btn = cancel_btn;
  ldapedit.statusbar = statusbar;
  ldapedit.status_cid = gtk_statusbar_get_context_id (GTK_STATUSBAR (statusbar), "Edit LDAP Server Dialog");
}

void
addressbook_edit_ldap_page_basic (gint pageNum, gchar * pageLbl)
{
  GtkWidget *vbox;
  GtkWidget *table;
  GtkWidget *label;
  GtkWidget *entry_name;
  GtkWidget *entry_server;
  GtkAdjustment *spinbtn_port_adj;
  GtkWidget *spinbtn_port;
  GtkWidget *entry_baseDN;
  GtkWidget *check_btn;
  GtkWidget *lookdn_btn;

  vbox = gtk_box_new (GTK_ORIENTATION_VERTICAL, 8);
  gtk_widget_show (vbox);
  gtk_container_add (GTK_CONTAINER (ldapedit.notebook), vbox);
  /* gtk_container_set_border_width( GTK_CONTAINER (vbox), BORDER_WIDTH ); */

  label = gtk_label_new (pageLbl);
  gtk_widget_show (label);
  gtk_notebook_set_tab_label (GTK_NOTEBOOK (ldapedit.notebook),
                              gtk_notebook_get_nth_page (GTK_NOTEBOOK (ldapedit.notebook), pageNum), label);

  table = gtk_grid_new ();
  gtk_box_pack_start (GTK_BOX (vbox), table, FALSE, FALSE, 0);
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
  label = gtk_label_new (_("Hostname"));
  gtk_grid_attach (GTK_GRID (table), label, 0, 1, 1, 1);
  gtk_label_set_xalign (GTK_LABEL (label), 0.0);

  entry_server = gtk_entry_new ();
  gtk_widget_set_hexpand (entry_server, TRUE);
  gtk_grid_attach (GTK_GRID (table), entry_server, 1, 1, 1, 1);

  /* Next row */
  label = gtk_label_new (_("Port"));
  gtk_grid_attach (GTK_GRID (table), label, 0, 2, 1, 1);
  gtk_label_set_xalign (GTK_LABEL (label), 0.0);

  spinbtn_port_adj = gtk_adjustment_new (389, 1, 65535, 100, 1000, 0);
  spinbtn_port = gtk_spin_button_new (GTK_ADJUSTMENT (spinbtn_port_adj), 1, 0);
  gtk_widget_set_size_request (spinbtn_port, 64, -1);
  gtk_spin_button_set_numeric (GTK_SPIN_BUTTON (spinbtn_port), TRUE);
  gtk_grid_attach (GTK_GRID (table), spinbtn_port, 1, 2, 1, 1);

  check_btn = gtk_button_new_with_label (_(" Check Server "));
  gtk_grid_attach (GTK_GRID (table), check_btn, 2, 2, 1, 1);

  /* Next row */
  label = gtk_label_new (_("Search Base"));
  gtk_grid_attach (GTK_GRID (table), label, 0, 3, 1, 1);
  gtk_label_set_xalign (GTK_LABEL (label), 0.0);

  entry_baseDN = gtk_entry_new ();
  gtk_widget_set_hexpand (entry_baseDN, TRUE);
  gtk_grid_attach (GTK_GRID (table), entry_baseDN, 1, 3, 1, 1);

  lookdn_btn = gtk_button_new_with_label ("...");
  gtk_grid_attach (GTK_GRID (table), lookdn_btn, 2, 3, 1, 1);

  g_signal_connect (G_OBJECT (check_btn), "clicked", G_CALLBACK (edit_ldap_server_check), NULL);
  g_signal_connect (G_OBJECT (lookdn_btn), "clicked", G_CALLBACK (edit_ldap_basedn_select), NULL);

  gtk_widget_show_all (vbox);

  ldapedit.entry_name = entry_name;
  ldapedit.entry_server = entry_server;
  ldapedit.spinbtn_port = spinbtn_port;
  ldapedit.entry_baseDN = entry_baseDN;
}

void
addressbook_edit_ldap_page_extended (gint pageNum, gchar * pageLbl)
{
  GtkWidget *vbox;
  GtkWidget *table;
  GtkWidget *label;
  GtkWidget *entry_bindDN;
  GtkWidget *entry_bindPW;
  GtkWidget *entry_criteria;
  GtkAdjustment *spinbtn_timeout_adj;
  GtkWidget *spinbtn_timeout;
  GtkAdjustment *spinbtn_maxentry_adj;
  GtkWidget *spinbtn_maxentry;
  GtkWidget *reset_btn;

  vbox = gtk_box_new (GTK_ORIENTATION_VERTICAL, 8);
  gtk_widget_show (vbox);
  gtk_container_add (GTK_CONTAINER (ldapedit.notebook), vbox);
  /* gtk_container_set_border_width( GTK_CONTAINER (vbox), BORDER_WIDTH ); */

  label = gtk_label_new (pageLbl);
  gtk_widget_show (label);
  gtk_notebook_set_tab_label (GTK_NOTEBOOK (ldapedit.notebook),
                              gtk_notebook_get_nth_page (GTK_NOTEBOOK (ldapedit.notebook), pageNum), label);

  table = gtk_grid_new ();
  gtk_box_pack_start (GTK_BOX (vbox), table, FALSE, FALSE, 0);
  gtk_container_set_border_width (GTK_CONTAINER (table), 5);
  gtk_grid_set_row_spacing (GTK_GRID (table), 5);
  gtk_grid_set_column_spacing (GTK_GRID (table), 5);

  /* First row */
  label = gtk_label_new (_("Search Criteria"));
  gtk_grid_attach (GTK_GRID (table), label, 0, 0, 1, 1);
  gtk_label_set_xalign (GTK_LABEL (label), 0.0);

  entry_criteria = gtk_entry_new ();
  gtk_grid_attach (GTK_GRID (table), entry_criteria, 1, 0, 1, 1);

  reset_btn = gtk_button_new_with_label (_(" Reset "));
  gtk_grid_attach (GTK_GRID (table), reset_btn, 2, 0, 1, 1);

  /* Next row */
  label = gtk_label_new (_("Bind DN"));
  gtk_grid_attach (GTK_GRID (table), label, 0, 1, 1, 1);
  gtk_label_set_xalign (GTK_LABEL (label), 0.0);

  entry_bindDN = gtk_entry_new ();
  gtk_grid_attach (GTK_GRID (table), entry_bindDN, 1, 1, 1, 1);

  /* Next row */
  label = gtk_label_new (_("Bind Password"));
  gtk_grid_attach (GTK_GRID (table), label, 0, 2, 1, 1);
  gtk_label_set_xalign (GTK_LABEL (label), 0.0);

  entry_bindPW = gtk_entry_new ();
  gtk_grid_attach (GTK_GRID (table), entry_bindPW, 1, 2, 1, 1);
  gtk_entry_set_visibility (GTK_ENTRY (entry_bindPW), FALSE);

  /* Next row */
  label = gtk_label_new (_("Timeout (secs)"));
  gtk_grid_attach (GTK_GRID (table), label, 0, 3, 1, 1);
  gtk_label_set_xalign (GTK_LABEL (label), 0.0);

  spinbtn_timeout_adj = gtk_adjustment_new (0, 0, 300, 1, 10, 0);
  spinbtn_timeout = gtk_spin_button_new (GTK_ADJUSTMENT (spinbtn_timeout_adj), 1, 0);
  gtk_widget_set_size_request (spinbtn_timeout, 64, -1);
  gtk_spin_button_set_numeric (GTK_SPIN_BUTTON (spinbtn_timeout), TRUE);
  gtk_grid_attach (GTK_GRID (table), spinbtn_timeout, 1, 3, 1, 1);

  /* Next row */
  label = gtk_label_new (_("Maximum Entries"));
  gtk_grid_attach (GTK_GRID (table), label, 0, 4, 1, 1);
  gtk_label_set_xalign (GTK_LABEL (label), 0.0);

  spinbtn_maxentry_adj = gtk_adjustment_new (0, 0, 500, 1, 10, 0);
  spinbtn_maxentry = gtk_spin_button_new (GTK_ADJUSTMENT (spinbtn_maxentry_adj), 1, 0);
  gtk_widget_set_size_request (spinbtn_maxentry, 64, -1);
  gtk_spin_button_set_numeric (GTK_SPIN_BUTTON (spinbtn_maxentry), TRUE);
  gtk_grid_attach (GTK_GRID (table), spinbtn_maxentry, 1, 4, 1, 1);

  g_signal_connect (G_OBJECT (reset_btn), "clicked", G_CALLBACK (edit_ldap_search_reset), NULL);

  gtk_widget_show_all (vbox);

  ldapedit.entry_criteria = entry_criteria;
  ldapedit.entry_bindDN = entry_bindDN;
  ldapedit.entry_bindPW = entry_bindPW;
  ldapedit.spinbtn_timeout = spinbtn_timeout;
  ldapedit.spinbtn_maxentry = spinbtn_maxentry;
}

static void
addressbook_edit_ldap_create (gboolean * cancelled)
{
  gint page = 0;
  addressbook_edit_ldap_dialog_create (cancelled);
  addressbook_edit_ldap_page_basic (page++, _("Basic"));
  addressbook_edit_ldap_page_extended (page++, _("Extended"));
  gtk_widget_show_all (ldapedit.window);
}

AdapterDSource *
addressbook_edit_ldap (AddressIndex * addrIndex, AdapterDSource * ads)
{
  static gboolean cancelled;
  gchar *sName, *sHost, *sBase, *sBind, *sPass, *sCrit;
  gint iPort, iMaxE, iTime;
  AddressDataSource *ds = NULL;
  SyldapServer *server = NULL;
  gboolean fin;

  if (!ldapedit.window)
    addressbook_edit_ldap_create (&cancelled);
  gtk_notebook_set_current_page (GTK_NOTEBOOK (ldapedit.notebook), 0);
  gtk_widget_grab_focus (ldapedit.ok_btn);
  gtk_widget_grab_focus (ldapedit.entry_name);
  gtk_widget_show (ldapedit.window);
  manage_window_set_transient (GTK_WINDOW (ldapedit.window));

  edit_ldap_status_show ("");
  if (ads)
    {
      ds = ads->dataSource;
      server = ds->rawDataSource;
      if (server->name)
        gtk_entry_set_text (GTK_ENTRY (ldapedit.entry_name), server->name);
      if (server->hostName)
        gtk_entry_set_text (GTK_ENTRY (ldapedit.entry_server), server->hostName);
      gtk_spin_button_set_value (GTK_SPIN_BUTTON (ldapedit.spinbtn_port), server->port);
      gtk_spin_button_set_value (GTK_SPIN_BUTTON (ldapedit.spinbtn_timeout), server->timeOut);
      if (server->baseDN)
        gtk_entry_set_text (GTK_ENTRY (ldapedit.entry_baseDN), server->baseDN);
      if (server->searchCriteria)
        gtk_entry_set_text (GTK_ENTRY (ldapedit.entry_criteria), server->searchCriteria);
      if (server->bindDN)
        gtk_entry_set_text (GTK_ENTRY (ldapedit.entry_bindDN), server->bindDN);
      if (server->bindPass)
        gtk_entry_set_text (GTK_ENTRY (ldapedit.entry_bindPW), server->bindPass);
      gtk_spin_button_set_value (GTK_SPIN_BUTTON (ldapedit.spinbtn_maxentry), server->maxEntries);
      gtk_window_set_title (GTK_WINDOW (ldapedit.window), _("Edit LDAP Server"));
    }
  else
    {
      gtk_entry_set_text (GTK_ENTRY (ldapedit.entry_name), ADDRESSBOOK_GUESS_LDAP_NAME);
      gtk_entry_set_text (GTK_ENTRY (ldapedit.entry_server), ADDRESSBOOK_GUESS_LDAP_SERVER);
      gtk_spin_button_set_value (GTK_SPIN_BUTTON (ldapedit.spinbtn_port), SYLDAP_DFL_PORT);
      gtk_spin_button_set_value (GTK_SPIN_BUTTON (ldapedit.spinbtn_timeout), SYLDAP_DFL_TIMEOUT);
      gtk_entry_set_text (GTK_ENTRY (ldapedit.entry_baseDN), "");
      gtk_entry_set_text (GTK_ENTRY (ldapedit.entry_criteria), SYLDAP_DFL_CRITERIA);
      gtk_entry_set_text (GTK_ENTRY (ldapedit.entry_bindDN), "");
      gtk_entry_set_text (GTK_ENTRY (ldapedit.entry_bindPW), "");
      gtk_spin_button_set_value (GTK_SPIN_BUTTON (ldapedit.spinbtn_maxentry), SYLDAP_MAX_ENTRIES);
      gtk_window_set_title (GTK_WINDOW (ldapedit.window), _("Add New LDAP Server"));
    }

  gtk_main ();
  gtk_widget_hide (ldapedit.window);
  if (cancelled == TRUE)
    return NULL;

  fin = FALSE;
  sName = gtk_editable_get_chars (GTK_EDITABLE (ldapedit.entry_name), 0, -1);
  sHost = gtk_editable_get_chars (GTK_EDITABLE (ldapedit.entry_server), 0, -1);
  iPort = gtk_spin_button_get_value_as_int (GTK_SPIN_BUTTON (ldapedit.spinbtn_port));
  iTime = gtk_spin_button_get_value_as_int (GTK_SPIN_BUTTON (ldapedit.spinbtn_timeout));
  sBase = gtk_editable_get_chars (GTK_EDITABLE (ldapedit.entry_baseDN), 0, -1);
  sCrit = gtk_editable_get_chars (GTK_EDITABLE (ldapedit.entry_criteria), 0, -1);
  sBind = gtk_editable_get_chars (GTK_EDITABLE (ldapedit.entry_bindDN), 0, -1);
  sPass = gtk_editable_get_chars (GTK_EDITABLE (ldapedit.entry_bindPW), 0, -1);
  iMaxE = gtk_spin_button_get_value_as_int (GTK_SPIN_BUTTON (ldapedit.spinbtn_maxentry));

  if (*sName == '\0')
    fin = TRUE;
  if (*sHost == '\0')
    fin = TRUE;
  if (*sBase == '\0')
    fin = TRUE;

  if (!fin)
    {
      if (!ads)
        {
          server = syldap_create ();
          ds = addrindex_index_add_datasource (addrIndex, ADDR_IF_LDAP, server);
          ads = addressbook_create_ds_adapter (ds, ADDR_LDAP, NULL);
        }
      addressbook_ads_set_name (ads, sName);
      syldap_set_name (server, sName);
      syldap_set_host (server, sHost);
      syldap_set_port (server, iPort);
      syldap_set_base_dn (server, sBase);
      syldap_set_bind_dn (server, sBind);
      syldap_set_bind_password (server, sPass);
      syldap_set_search_criteria (server, sCrit);
      syldap_set_max_entries (server, iMaxE);
      syldap_set_timeout (server, iTime);
    }
  g_free (sName);
  g_free (sHost);
  g_free (sBase);
  g_free (sBind);
  g_free (sPass);
  g_free (sCrit);

  return ads;
}

#endif /* USE_LDAP */

/*
* End of Source.
*/
