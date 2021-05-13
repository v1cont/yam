/* select-keys.c - GTK+ based key selection
 *      Copyright (C) 2001 Werner Koch (dd9jn)
 *      Copyright (C) 1999-2014 Hiroyuki Yamamoto
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
#include <config.h>
#endif

#ifdef USE_GPGME
#include <stdio.h>
#include <stdlib.h>

#include <glib.h>
#include <glib/gi18n.h>
#include <gdk/gdkkeysyms.h>
#include <gtk/gtk.h>

#include "select-keys.h"
#include "utils.h"
#include "gtkutils.h"
#include "inputdialog.h"
#include "manage_window.h"
#include "alertpanel.h"

#define DIM(v) (sizeof(v)/sizeof((v)[0]))
#define DIMof(type,member)   DIM(((type *)0)->member)

enum col_titles {
  COL_ALGO,
  COL_KEYID,
  COL_NAME,
  COL_EMAIL,
  COL_VALIDITY,
  N_COL_TITLES
};

struct select_keys_s {
  int okay;
  GtkWidget *window;
  GtkLabel *toplabel;
  GtkTreeView *list;
  const char *pattern;
  unsigned int num_keys;
  gpgme_key_t *kset;
  gpgme_ctx_t select_ctx;

  GtkSortType sort_type;
  enum col_titles sort_column;
};

static void set_row (GtkWidget *list, gpgme_key_t key);
static gpgme_key_t get_row (GtkWidget *list);
static void clear_list (GtkWidget *list);
static void fill_list (struct select_keys_s *sk, const char *pattern);
static void create_dialog (struct select_keys_s *sk);
static void open_dialog (struct select_keys_s *sk);
static void close_dialog (struct select_keys_s *sk);
static gint delete_event_cb (GtkWidget *widget, GdkEventAny *event, gpointer data);
static gboolean key_pressed_cb (GtkWidget *widget, GdkEventKey *event, gpointer data);
static void select_btn_cb (GtkWidget *widget, gpointer data);
static void cancel_btn_cb (GtkWidget *widget, gpointer data);
static void other_btn_cb (GtkWidget *widget, gpointer data);
static gboolean use_untrusted (gpgme_key_t);

static void
update_progress (struct select_keys_s *sk, int running, const char *pattern)
{
  static int windmill[] = { '-', '\\', '|', '/' };
  char *buf;

  if (!pattern)
    pattern = "";
  if (!running)
    buf = g_strdup_printf (_("Please select key for \"%s\""), pattern);
  else
    buf = g_strdup_printf (_("Collecting info for \"%s\" ... %c"), pattern, windmill[running % DIM (windmill)]);
  gtk_label_set_text (sk->toplabel, buf);
  g_free (buf);
}

/**
 * gpgmegtk_recipient_selection:
 * @recp_names: A list of email addresses
 *
 * Select a list of recipients from a given list of email addresses.
 * This may pop up a window to present the user a choice, it will also
 * check that the recipients key are all valid.
 *
 * Return value: NULL on error or a list of list of recipients.
 **/
gpgme_key_t *
gpgmegtk_recipient_selection (GSList * recp_names)
{
  struct select_keys_s sk;

  memset (&sk, 0, sizeof sk);

  open_dialog (&sk);

  do
    {
      sk.pattern = recp_names ? recp_names->data : NULL;
      clear_list (GTK_WIDGET (sk.list));
      fill_list (&sk, sk.pattern);
      update_progress (&sk, 0, sk.pattern);
      gtk_main ();
      if (recp_names)
        recp_names = recp_names->next;
    }
  while (sk.okay && recp_names);

  close_dialog (&sk);

  if (!sk.okay)
    {
      g_free (sk.kset);
      sk.kset = NULL;
    }
  else
    {
      sk.kset = g_realloc (sk.kset, sizeof (gpgme_key_t) * (sk.num_keys + 1));
      sk.kset[sk.num_keys] = NULL;
    }
  return sk.kset;
}

static void
destroy_key (gpointer data)
{
  gpgme_key_t key = data;
  gpgme_key_release (key);
}

static gpgme_key_t
get_row (GtkWidget *list)
{
  GtkTreeSelection *sel;
  GtkTreeModel *model;
  GtkTreeIter iter;
  gpgme_key_t key = NULL;

  sel = gtk_tree_view_get_selection (GTK_TREE_VIEW (list));
  if (gtk_tree_selection_get_selected (sel, &model, &iter))
    gtk_tree_model_get (model, &iter, 5, &key, -1);

  return key;
}

static void
set_row (GtkWidget *list, gpgme_key_t key)
{
  const char *s;
  const char *text[N_COL_TITLES];
  char *algo_buf;
  GtkTreeModel *model;
  GtkTreeIter iter;

  /* first check whether the key is capable of encryption which is not
   * the case for revoked, expired or sign-only keys */
  if (!key->can_encrypt)
    return;

  algo_buf = g_strdup_printf ("%du/%s", key->subkeys->length, gpgme_pubkey_algo_name (key->subkeys->pubkey_algo));
  text[COL_ALGO] = algo_buf;

  s = key->subkeys->keyid;
  if (strlen (s) == 16)
    s += 8;                     /* show only the short keyID */
  text[COL_KEYID] = s;

  s = key->uids->name;
  text[COL_NAME] = s;

  s = key->uids->email;
  text[COL_EMAIL] = s;

  switch (key->uids->validity)
    {
    case GPGME_VALIDITY_UNDEFINED:
      s = "q";
      break;
    case GPGME_VALIDITY_NEVER:
      s = "n";
      break;
    case GPGME_VALIDITY_MARGINAL:
      s = "m";
      break;
    case GPGME_VALIDITY_FULL:
      s = "f";
      break;
    case GPGME_VALIDITY_ULTIMATE:
      s = "u";
      break;
    case GPGME_VALIDITY_UNKNOWN:
    default:
      s = "?";
      break;
    }
  text[COL_VALIDITY] = s;

  model = gtk_tree_view_get_model (GTK_TREE_VIEW (list));

  gtk_list_store_append (GTK_LIST_STORE (model), &iter);
  gtk_list_store_set (GTK_LIST_STORE (model), &iter, 0, text[0], 1, text[1], 2, text[2], 3, text[3], 4, text[4], 5, key, -1);

  g_free (algo_buf);
}

static void
fill_list (struct select_keys_s *sk, const char *pattern)
{
  GtkWidget *list;
  gpgme_ctx_t ctx;
  gpgme_error_t err;
  gpgme_key_t key;
  int running = 0;

  g_return_if_fail (sk);
  list = GTK_WIDGET (sk->list);
  g_return_if_fail (list);

  debug_print ("select_keys:fill_list: pattern '%s'\n", pattern ? pattern : "");

  err = gpgme_new (&ctx);
  g_assert (!err);

  sk->select_ctx = ctx;

  update_progress (sk, ++running, pattern);
  while (gtk_events_pending ())
    gtk_main_iteration ();

  err = gpgme_op_keylist_start (ctx, pattern, 0);
  if (err)
    {
      debug_print ("** gpgme_op_keylist_start(%s) failed: %s", pattern ? pattern : "", gpgme_strerror (err));
      sk->select_ctx = NULL;
      gpgme_release (ctx);
      return;
    }
  update_progress (sk, ++running, pattern);
  while (!(err = gpgme_op_keylist_next (ctx, &key)))
    {
      debug_print ("%% %s:%d:  insert\n", __FILE__, __LINE__);
      set_row (list, key);
      key = NULL;
      update_progress (sk, ++running, pattern);
      while (gtk_events_pending ())
        gtk_main_iteration ();
    }
  debug_print ("%% %s:%d:  ready\n", __FILE__, __LINE__);
  if (gpgme_err_code (err) != GPG_ERR_EOF)
    {
      debug_print ("** gpgme_op_keylist_next failed: %s", gpgme_strerror (err));
      gpgme_op_keylist_end (ctx);
    }
  sk->select_ctx = NULL;
  gpgme_release (ctx);
}

static void
clear_list (GtkWidget *list)
{
  GtkTreeModel *model;
  GtkTreeIter iter;

  model = gtk_tree_view_get_model (GTK_TREE_VIEW (list));

  if (gtk_tree_model_get_iter_first (model, &iter))
    {
      do
        {
          gchar *str = NULL;
          gpgme_key_t key;

          gtk_tree_model_get (model, &iter, 0, &str, 5, &key, -1);
          g_free (str);
          destroy_key (key);
        }
      while (gtk_tree_model_iter_next (model, &iter));
    }

  gtk_list_store_clear (GTK_LIST_STORE (model));
}

static void
create_dialog (struct select_keys_s *sk)
{
  GtkWidget *window;
  GtkWidget *vbox, *vbox2, *hbox;
  GtkWidget *bbox;
  GtkWidget *scrolledwin;
  GtkWidget *list;
  GtkWidget *label;
  GtkWidget *select_btn, *cancel_btn, *other_btn;
  GtkListStore *store;
  GtkTreeViewColumn *col;
  GtkCellRenderer *renderer;

  g_assert (!sk->window);
  window = gtk_window_new (GTK_WINDOW_TOPLEVEL);
  gtk_widget_set_size_request (window, 520, 280);
  gtk_container_set_border_width (GTK_CONTAINER (window), 8);
  gtk_window_set_title (GTK_WINDOW (window), _("Select encryption keys"));
  gtk_window_set_position (GTK_WINDOW (window), GTK_WIN_POS_CENTER);
  gtk_window_set_modal (GTK_WINDOW (window), TRUE);
  g_signal_connect (G_OBJECT (window), "delete_event", G_CALLBACK (delete_event_cb), sk);
  g_signal_connect (G_OBJECT (window), "key_press_event", G_CALLBACK (key_pressed_cb), sk);
  MANAGE_WINDOW_SIGNALS_CONNECT (window);

  vbox = gtk_box_new (GTK_ORIENTATION_VERTICAL, 8);
  gtk_container_add (GTK_CONTAINER (window), vbox);

  hbox = gtk_box_new (GTK_ORIENTATION_HORIZONTAL, 4);
  gtk_box_pack_start (GTK_BOX (vbox), hbox, FALSE, FALSE, 0);
  label = gtk_label_new (NULL);
  gtk_box_pack_start (GTK_BOX (hbox), label, FALSE, FALSE, 0);

  hbox = gtk_box_new (GTK_ORIENTATION_HORIZONTAL, 8);
  gtk_box_pack_start (GTK_BOX (vbox), hbox, TRUE, TRUE, 0);
  gtk_container_set_border_width (GTK_CONTAINER (hbox), 2);

  scrolledwin = gtk_scrolled_window_new (NULL, NULL);
  gtk_box_pack_start (GTK_BOX (hbox), scrolledwin, TRUE, TRUE, 0);
  gtk_scrolled_window_set_policy (GTK_SCROLLED_WINDOW (scrolledwin), GTK_POLICY_AUTOMATIC, GTK_POLICY_AUTOMATIC);

  store = gtk_list_store_new (6, G_TYPE_STRING, G_TYPE_STRING, G_TYPE_STRING, G_TYPE_STRING, G_TYPE_STRING, G_TYPE_POINTER);
  list = gtk_tree_view_new_with_model (GTK_TREE_MODEL (store));
  gtk_tree_view_set_headers_visible (GTK_TREE_VIEW (list), TRUE);
  gtk_tree_selection_set_mode (gtk_tree_view_get_selection (GTK_TREE_VIEW (list)), GTK_SELECTION_BROWSE);
  gtk_container_add (GTK_CONTAINER (scrolledwin), list);
  g_object_unref (store);

  renderer = gtk_cell_renderer_text_new ();
  col = gtk_tree_view_column_new_with_attributes (_("Size"), renderer, "text", COL_ALGO, NULL);
  gtk_tree_view_append_column (GTK_TREE_VIEW (list), col);

  renderer = gtk_cell_renderer_text_new ();
  col = gtk_tree_view_column_new_with_attributes (_("Key ID"), renderer, "text", COL_KEYID, NULL);
  gtk_tree_view_append_column (GTK_TREE_VIEW (list), col);

  renderer = gtk_cell_renderer_text_new ();
  col = gtk_tree_view_column_new_with_attributes (_("Name"), renderer, "text", COL_NAME, NULL);
  gtk_tree_view_column_set_expand (col, TRUE);
  gtk_tree_view_column_set_sort_column_id (col, COL_NAME);
  gtk_tree_view_append_column (GTK_TREE_VIEW (list), col);

  renderer = gtk_cell_renderer_text_new ();
  col = gtk_tree_view_column_new_with_attributes (_("Address"), renderer, "text", COL_EMAIL, NULL);
  gtk_tree_view_column_set_expand (col, TRUE);
  gtk_tree_view_column_set_sort_column_id (col, COL_EMAIL);
  gtk_tree_view_append_column (GTK_TREE_VIEW (list), col);

  renderer = gtk_cell_renderer_text_new ();
  col = gtk_tree_view_column_new_with_attributes (_("Val"), renderer, "text", COL_VALIDITY, NULL);
  gtk_tree_view_append_column (GTK_TREE_VIEW (list), col);

  hbox = gtk_box_new (GTK_ORIENTATION_HORIZONTAL, 8);
  gtk_box_pack_end (GTK_BOX (vbox), hbox, FALSE, FALSE, 0);

  yam_stock_button_set_create (&bbox, &select_btn, _("Select"), &cancel_btn, "yam-cancel", &other_btn, _("Other"));
  gtk_box_pack_end (GTK_BOX (hbox), bbox, FALSE, FALSE, 0);
  gtk_widget_grab_default (select_btn);
  gtk_widget_grab_focus (select_btn);

  g_signal_connect (G_OBJECT (select_btn), "clicked", G_CALLBACK (select_btn_cb), sk);
  g_signal_connect (G_OBJECT (cancel_btn), "clicked", G_CALLBACK (cancel_btn_cb), sk);
  g_signal_connect (G_OBJECT (other_btn), "clicked", G_CALLBACK (other_btn_cb), sk);

  vbox2 = gtk_box_new (GTK_ORIENTATION_VERTICAL, 4);
  gtk_box_pack_start (GTK_BOX (hbox), vbox2, FALSE, FALSE, 0);

  gtk_widget_show_all (window);

  sk->window = window;
  sk->toplabel = GTK_LABEL (label);
  sk->list = GTK_TREE_VIEW (list);
}

static void
open_dialog (struct select_keys_s *sk)
{
  if (!sk->window)
    create_dialog (sk);
  manage_window_set_transient (GTK_WINDOW (sk->window));
  sk->okay = 0;
  sk->sort_column = N_COL_TITLES;       /* use an invalid value */
  sk->sort_type = GTK_SORT_ASCENDING;
  gtk_widget_show (sk->window);
}

static void
close_dialog (struct select_keys_s *sk)
{
  g_return_if_fail (sk);
  gtk_widget_destroy (sk->window);
  sk->window = NULL;
}

static gint
delete_event_cb (GtkWidget * widget, GdkEventAny * event, gpointer data)
{
  struct select_keys_s *sk = data;

  sk->okay = 0;
  gtk_main_quit ();

  return TRUE;
}

static gboolean
key_pressed_cb (GtkWidget * widget, GdkEventKey * event, gpointer data)
{
  struct select_keys_s *sk = data;

  g_return_val_if_fail (sk, FALSE);
  if (event && event->keyval == GDK_KEY_Escape)
    {
      sk->okay = 0;
      gtk_main_quit ();
    }
  return FALSE;
}

static void
select_btn_cb (GtkWidget * widget, gpointer data)
{
  struct select_keys_s *sk = data;
  gboolean use_key;
  gpgme_key_t key;

  g_return_if_fail (sk);

  key = get_row (GTK_WIDGET (sk->list));
  if (key)
    {
      if (key->uids->validity < GPGME_VALIDITY_FULL)
        {
          use_key = use_untrusted (key);
          if (!use_key)
            {
              debug_print ("** Key untrusted, will not encrypt");
              return;
            }
        }
      sk->kset = g_realloc (sk->kset, sizeof (gpgme_key_t) * (sk->num_keys + 1));
      gpgme_key_ref (key);
      sk->kset[sk->num_keys] = key;
      sk->num_keys++;
      sk->okay = 1;
      gtk_main_quit ();
    }
  else
    debug_print ("** nothing selected");
}

static void
cancel_btn_cb (GtkWidget * widget, gpointer data)
{
  struct select_keys_s *sk = data;

  g_return_if_fail (sk);
  sk->okay = 0;
  if (sk->select_ctx)
    gpgme_cancel (sk->select_ctx);
  gtk_main_quit ();
}

static void
other_btn_cb (GtkWidget * widget, gpointer data)
{
  struct select_keys_s *sk = data;
  char *uid;

  g_return_if_fail (sk);
  uid = input_dialog (_("Add key"), _("Enter another user or key ID:"), NULL);
  if (!uid)
    return;
  fill_list (sk, uid);
  update_progress (sk, 0, sk->pattern);
  g_free (uid);
}

static gboolean
use_untrusted (gpgme_key_t key)
{
  AlertValue aval;

  aval = alertpanel (_("Trust key"),
                     _("The selected key is not fully trusted.\n"
                       "If you choose to encrypt the message with this key you don't\n"
                       "know for sure that it will go to the person you mean it to.\n"
                       "Do you trust it enough to use it anyway?"), "yam-yes", "yam-no", NULL);
  return (aval == G_ALERTDEFAULT);
}

#endif /*USE_GPGME */
