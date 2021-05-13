/*
 * YAM -- a GTK+ based, lightweight, and fast e-mail client
 * Copyright (C) 2020 Victor Ananjevsky <victor@sanana.kiev.ua>
 * Copyright (C) 1999-2018 Hiroyuki Yamamoto
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

#include <glib/gi18n.h>
#include <gdk/gdkkeysyms.h>
#include <gtk/gtk.h>

#include "about.h"
#include "utils.h"

void
about_show (void)
{
  GtkWidget *window;
  const gchar *authors[] = {
    "2020 Victor Ananjevsky <victor@sanana.kiev.ua>",
    "---",
    "Hiroyuki Yamamoto <hiro-y@kcn.ne.jp> (Sylpheed)",
#if USE_GPGME
    "Werner Koch <dd9jn@gnu.org> (GPGME)",
#endif
    NULL
  };
  const gchar *translators = N_("translator-credits");
  const gchar *copyright = "Copyright \xc2\xa9 2020, 2020 Victor Ananjevsky <victor@sanana.kiev.ua>\n"
    "Copyright \xc2\xa9 1999-2018 Hiroyuki Yamamoto <hiro-y@kcn.ne.jp>";
  const gchar *license =
    N_("YAM is free software; you can redistribute it and/or modify\n"
       "it under the terms of the GNU General Public License as published by\n"
       "the Free Software Foundation; either version 2 of the License, or\n"
       "(at your option) any later version.\n\n"
       "YAM is distributed in the hope that it will be useful,\n"
       "but WITHOUT ANY WARRANTY; without even the implied warranty of\n"
       "MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the\n"
       "GNU General Public License for more details.\n\n"
       "You should have received a copy of the GNU General Public License\n"
       "along with YAD. If not, see <http://www.gnu.org/licenses/>.");

  gchar *comments = g_strdup_printf (_("Yet Another Mail\n"
                                       "(lightweight and fast email client)\n"
                                       "\nBased on Sylpheed code\n\n"
#if INET6
                                       "Built with IPv6 support\n"
#endif
#if HAVE_LIBCOMPFACE
                                       "Built with compface\n"
#endif
#if USE_GPGME
                                       "Built with GnuPG\n"
#endif
#if USE_SSL
                                       "Built with OpenSSL\n"
#endif
#if USE_LDAP
                                       "Built with LDAP\n"
#endif
#if USE_GSPELL
                                       "Built with GSpell\n"
#endif
                                       "GTK+ %d.%d.%d / GLib %d.%d.%d\n"),
                                     gtk_major_version, gtk_minor_version, gtk_micro_version,
                                     glib_major_version, glib_minor_version, glib_micro_version);

  window = gtk_about_dialog_new ();
  gtk_window_set_title (GTK_WINDOW (window), _("About YAM"));
  gtk_window_set_position (GTK_WINDOW (window), GTK_WIN_POS_CENTER);

  gtk_about_dialog_set_program_name (GTK_ABOUT_DIALOG (window), PACKAGE_NAME);
  gtk_about_dialog_set_logo_icon_name (GTK_ABOUT_DIALOG (window), "yam");
  gtk_about_dialog_set_version (GTK_ABOUT_DIALOG (window), PACKAGE_VERSION);
  gtk_about_dialog_set_authors (GTK_ABOUT_DIALOG (window), authors);
  gtk_about_dialog_set_translator_credits (GTK_ABOUT_DIALOG (window), translators);
  gtk_about_dialog_set_comments (GTK_ABOUT_DIALOG (window), comments);
  gtk_about_dialog_set_website (GTK_ABOUT_DIALOG (window), PACKAGE_URL);
  gtk_about_dialog_set_license (GTK_ABOUT_DIALOG (window), license);
  gtk_about_dialog_set_copyright (GTK_ABOUT_DIALOG (window), copyright);

  gtk_dialog_run (GTK_DIALOG (window));

  gtk_widget_destroy (window);
}
