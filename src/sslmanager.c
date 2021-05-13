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

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#if USE_SSL

#include "defs.h"

#include <glib.h>
#include <glib/gi18n.h>
#include <gtk/gtk.h>

#include "ssl.h"
#include "sslmanager.h"
#include "manage_window.h"
#include "prefs_common.h"
#include "gtkutils.h"

gint
ssl_manager_verify_cert (SockInfo * sockinfo, const gchar * hostname, X509 * server_cert, glong verify_result)
{
  GtkWidget *dialog;
  GtkWidget *hbox;
  GtkWidget *image;
  GtkWidget *vbox;
  GtkWidget *label;
  GString *message;
  gchar *subject, *issuer;
  guchar keyid[EVP_MAX_MD_SIZE];
  gchar keyidstr[EVP_MAX_MD_SIZE * 3 + 1] = "";
  guint keyidlen = 0;
  gchar *sha1_keyidstr, *md5_keyidstr;
  BIO *bio;
  gchar not_before[64] = "", not_after[64] = "";
  gint i;
  gint result;
  gboolean disable_always = FALSE;

  if (verify_result == X509_V_OK)
    return 0;

  gdk_threads_enter ();

  subject = X509_NAME_oneline (X509_get_subject_name (server_cert), NULL, 0);
  issuer = X509_NAME_oneline (X509_get_issuer_name (server_cert), NULL, 0);

  bio = BIO_new (BIO_s_mem ());
  ASN1_TIME_print (bio, X509_get_notBefore (server_cert));
  BIO_gets (bio, not_before, sizeof (not_before));
  BIO_reset (bio);
  ASN1_TIME_print (bio, X509_get_notAfter (server_cert));
  BIO_gets (bio, not_after, sizeof (not_after));
  BIO_free (bio);

  if (X509_digest (server_cert, EVP_sha1 (), keyid, &keyidlen))
    {
      for (i = 0; i < keyidlen; i++)
        g_snprintf (keyidstr + i * 3, 4, "%02x:", keyid[i]);
      keyidstr[keyidlen * 3 - 1] = '\0';
      sha1_keyidstr = g_ascii_strup (keyidstr, -1);
    }
  else
    sha1_keyidstr = g_strdup ("(cannot calculate digest)");

  if (X509_digest (server_cert, EVP_md5 (), keyid, &keyidlen))
    {
      for (i = 0; i < keyidlen; i++)
        g_snprintf (keyidstr + i * 3, 4, "%02x:", keyid[i]);
      keyidstr[keyidlen * 3 - 1] = '\0';
      md5_keyidstr = g_ascii_strup (keyidstr, -1);
    }
  else
    md5_keyidstr = g_strdup ("(cannot calculate digest)");

  message = g_string_new ("");
  g_string_append_printf (message, _("The SSL certificate of %s cannot be verified by the following reason:"), hostname);
  if (verify_result == X509_V_ERR_APPLICATION_VERIFICATION)
    g_string_append_printf (message, "\n  certificate hostname does not match\n\n");
  else
    g_string_append_printf (message, "\n  %s\n\n", X509_verify_cert_error_string (verify_result));
  g_string_append_printf (message, _("Subject: %s\n"), subject ? subject : "(unknown)");
  g_string_append_printf (message, _("Issuer: %s\n"), issuer ? issuer : "(unknown)");
  g_string_append_printf (message, _("Issued date: %s\n"), not_before);
  g_string_append_printf (message, _("Expire date: %s\n"), not_after);
  g_string_append (message, "\n");
  g_string_append_printf (message, _("SHA1 fingerprint: %s\n"), sha1_keyidstr);
  g_string_append_printf (message, _("MD5 fingerprint: %s\n"), md5_keyidstr);
  g_string_append (message, "\n");
  g_string_append (message, _("Do you accept this certificate?"));
  g_free (md5_keyidstr);
  g_free (sha1_keyidstr);
  if (issuer)
    OPENSSL_free (issuer);
  if (subject)
    OPENSSL_free (subject);

  dialog = gtk_dialog_new ();
  gtk_window_set_title (GTK_WINDOW (dialog), _("SSL certificate verify failed"));
  gtk_window_set_position (GTK_WINDOW (dialog), GTK_WIN_POS_CENTER_ON_PARENT);
  gtk_window_set_modal (GTK_WINDOW (dialog), TRUE);
  gtk_window_set_resizable (GTK_WINDOW (dialog), FALSE);
  manage_window_set_transient (GTK_WINDOW (dialog));
  gtk_widget_realize (dialog);

  hbox = gtk_box_new (GTK_ORIENTATION_HORIZONTAL, 12);
  gtk_container_set_border_width (GTK_CONTAINER (hbox), 12);
  gtk_box_pack_start (GTK_BOX (gtk_dialog_get_content_area (GTK_DIALOG (dialog))), hbox, FALSE, FALSE, 0);

  image = gtk_image_new_from_icon_name ("dialog-warning", GTK_ICON_SIZE_DIALOG);

  gtk_label_set_xalign (GTK_LABEL (image), 0.0);
  gtk_box_pack_start (GTK_BOX (hbox), image, FALSE, FALSE, 0);

  vbox = gtk_box_new (GTK_ORIENTATION_VERTICAL, 12);
  gtk_box_pack_start (GTK_BOX (hbox), vbox, TRUE, TRUE, 0);

  label = yam_label_title (_("SSL certificate verify failed"));
  gtk_box_pack_start (GTK_BOX (vbox), label, TRUE, TRUE, 0);
  gtk_label_set_xalign (GTK_LABEL (label), 0.0);
  gtk_label_set_yalign (GTK_LABEL (label), 0.0);
  gtk_label_set_line_wrap (GTK_LABEL (label), TRUE);

  label = gtk_label_new (message->str);
  g_string_free (message, TRUE);
  gtk_box_pack_start (GTK_BOX (vbox), label, TRUE, TRUE, 0);
  gtk_label_set_xalign (GTK_LABEL (label), 0.0);
  gtk_label_set_yalign (GTK_LABEL (label), 0.0);
  gtk_label_set_line_wrap (GTK_LABEL (label), TRUE);
  gtk_label_set_selectable (GTK_LABEL (label), TRUE);
  gtk_widget_set_can_focus (label, FALSE);

  /* prohibit acception of expired certificates */
  if (verify_result == X509_V_ERR_CERT_HAS_EXPIRED)
    disable_always = TRUE;

  gtk_dialog_add_buttons (GTK_DIALOG (dialog), _("_Reject"), GTK_RESPONSE_REJECT,
                          _("_Temporarily accept"), GTK_RESPONSE_OK, _("Always _accept"), GTK_RESPONSE_ACCEPT, NULL);
  gtk_dialog_set_default_response (GTK_DIALOG (dialog), GTK_RESPONSE_OK);
  if (disable_always)
    gtk_dialog_set_response_sensitive (GTK_DIALOG (dialog), GTK_RESPONSE_ACCEPT, FALSE);

  gtk_widget_show_all (dialog);

  result = gtk_dialog_run (GTK_DIALOG (dialog));
  gtk_widget_destroy (dialog);

  gdk_threads_leave ();

  switch (result)
    {
    case GTK_RESPONSE_ACCEPT:
      return 0;
    case GTK_RESPONSE_OK:
      return 1;
    case GTK_RESPONSE_REJECT:
    default:
      break;
    }

  return -1;
}

#endif /* USE_SSL */
