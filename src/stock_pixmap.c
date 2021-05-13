/*
 * YAM -- a GTK+ based, lightweight, and fast e-mail client
 * Copyright (C) 2020 Victor Ananjevsky <victor@sanana.kiev.ua>
 * Copyright (C) 1999-2013 Hiroyuki Yamamoto
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
#include <gtk/gtk.h>

#include "stock_pixmap.h"
#include "gtkutils.h"
#include "utils.h"

typedef struct _StockPixmapData StockPixmapData;

struct _StockPixmapData {
  GdkPixbuf *pixbuf;      /* icon pixbuf */
  gchar *iconname;        /* icon name from icon theme */
  GtkIconSize size;       /* size for icon from theme */
  gchar *filename;        /* filename for user icon or gresours */
  gboolean loaded;
};

static StockPixmapData pixmaps[] = {
  { NULL, NULL, GTK_ICON_SIZE_INVALID, "address.png", FALSE },
  { NULL, NULL, GTK_ICON_SIZE_INVALID, "book.png", FALSE },
  { NULL, NULL, GTK_ICON_SIZE_INVALID, "category.png", FALSE },
  { NULL, NULL, GTK_ICON_SIZE_INVALID, "clip.png", FALSE },
  { NULL, NULL, GTK_ICON_SIZE_INVALID, "complete.png", FALSE },
  { NULL, NULL, GTK_ICON_SIZE_INVALID, "continue.png", FALSE },
  { NULL, NULL, GTK_ICON_SIZE_INVALID, "deleted.png", FALSE },
  { NULL, "folder", GTK_ICON_SIZE_MENU, "folder-close.png", FALSE },
  { NULL, "folder-open", GTK_ICON_SIZE_MENU, "folder-open.png", FALSE },
  { NULL, "folder", GTK_ICON_SIZE_MENU, "folder-noselect.png", FALSE },
  { NULL, NULL, GTK_ICON_SIZE_INVALID, "error.png", FALSE },
  { NULL, NULL, GTK_ICON_SIZE_INVALID, "forwarded.png", FALSE },
  { NULL, NULL, GTK_ICON_SIZE_INVALID, "group.png", FALSE },
  { NULL, NULL, GTK_ICON_SIZE_INVALID, "html.png", FALSE },
  { NULL, NULL, GTK_ICON_SIZE_INVALID, "interface.png", FALSE },
  { NULL, NULL, GTK_ICON_SIZE_INVALID, "ldap.png", FALSE },
  { NULL, NULL, GTK_ICON_SIZE_INVALID, "linewrap.png", FALSE },
  { NULL, NULL, GTK_ICON_SIZE_INVALID, "mark.png", FALSE },
  { NULL, NULL, GTK_ICON_SIZE_INVALID, "new.png", FALSE },
  { NULL, NULL, GTK_ICON_SIZE_INVALID, "replied.png", FALSE },
  { NULL, NULL, GTK_ICON_SIZE_INVALID, "unread.png", FALSE },
  { NULL, NULL, GTK_ICON_SIZE_INVALID, "vcard.png", FALSE },
  { NULL, "gtk-connect", GTK_ICON_SIZE_MENU, "online.png", FALSE },
  { NULL, "gtk-disconnect", GTK_ICON_SIZE_MENU, "offline.png", FALSE },
  { NULL, NULL, GTK_ICON_SIZE_INVALID, "mail-small.png", FALSE },

  { NULL, NULL, GTK_ICON_SIZE_INVALID, "inbox.png", FALSE },
  { NULL, NULL, GTK_ICON_SIZE_INVALID, "outbox.png", FALSE },
  { NULL, NULL, GTK_ICON_SIZE_INVALID, "draft.png", FALSE },
  { NULL, NULL, GTK_ICON_SIZE_INVALID, "trash.png", FALSE },
  { NULL, NULL, GTK_ICON_SIZE_INVALID, "junk.png", FALSE },
  { NULL, NULL, GTK_ICON_SIZE_INVALID, "mail.png", FALSE },
  { NULL, "mail-attachment", GTK_ICON_SIZE_LARGE_TOOLBAR, "attach.png", FALSE },
  { NULL, "mail-message-new", GTK_ICON_SIZE_LARGE_TOOLBAR, "mail-compose.png", FALSE },
  { NULL, "mail-forward", GTK_ICON_SIZE_LARGE_TOOLBAR, "mail-forward.png", FALSE },
  { NULL, NULL, GTK_ICON_SIZE_INVALID, "mail-receive.png", FALSE },
  { NULL, NULL, GTK_ICON_SIZE_INVALID, "mail-receive-all.png", FALSE },
  { NULL, "mail-reply-sender", GTK_ICON_SIZE_LARGE_TOOLBAR, "mail-reply.png", FALSE },
  { NULL, "mail-reply-all", GTK_ICON_SIZE_LARGE_TOOLBAR, "mail-reply-all.png", FALSE },
  { NULL, "mail-send", GTK_ICON_SIZE_LARGE_TOOLBAR, "mail-send.png", FALSE },
  { NULL, NULL, GTK_ICON_SIZE_INVALID, "mail-send-queue.png", FALSE },
  { NULL, "insert-object", GTK_ICON_SIZE_LARGE_TOOLBAR, "insert-file.png", FALSE },
  { NULL, NULL, GTK_ICON_SIZE_INVALID, "addressbook.png", FALSE },
  { NULL, "edit-delete", GTK_ICON_SIZE_LARGE_TOOLBAR, "delete.png", FALSE },
  { NULL, "mail-mark-junk", GTK_ICON_SIZE_LARGE_TOOLBAR, "spam.png", FALSE },
  { NULL, "mail-mark-notjunk", GTK_ICON_SIZE_LARGE_TOOLBAR, "notspam.png", FALSE },
  { NULL, NULL, GTK_ICON_SIZE_INVALID, "hand-signed.png", FALSE },
  { NULL, NULL, GTK_ICON_SIZE_INVALID, "tray-nomail.png", FALSE },
  { NULL, NULL, GTK_ICON_SIZE_INVALID, "tray-newmail.png", FALSE },
  { NULL, NULL, GTK_ICON_SIZE_INVALID, "person.png", FALSE },
  { NULL, NULL, GTK_ICON_SIZE_INVALID, "folder-search.png", FALSE },

  /* for toolbar */
  { NULL, "go-down", GTK_ICON_SIZE_LARGE_TOOLBAR, "next.png", FALSE },
  { NULL, "go-up", GTK_ICON_SIZE_LARGE_TOOLBAR, "prev.png", FALSE },
  { NULL, "edit-find", GTK_ICON_SIZE_LARGE_TOOLBAR, "search.png", FALSE },
  { NULL, "document-print", GTK_ICON_SIZE_LARGE_TOOLBAR, "print.png", FALSE },
  { NULL, "process-stop", GTK_ICON_SIZE_LARGE_TOOLBAR, "stop.png", FALSE },
  { NULL, "system-run", GTK_ICON_SIZE_LARGE_TOOLBAR, "execute.png", FALSE },
  { NULL, "gtk-preferences", GTK_ICON_SIZE_LARGE_TOOLBAR, "common-prefs.png", FALSE },
  { NULL, "gtk-preferences", GTK_ICON_SIZE_LARGE_TOOLBAR, "account-prefs.png", FALSE },
  { NULL, "document-save", GTK_ICON_SIZE_LARGE_TOOLBAR, "save.png", FALSE },
  { NULL, "gtk-edit", GTK_ICON_SIZE_LARGE_TOOLBAR, "editor.png", FALSE },
};

static gchar *theme_dir = NULL;

GtkWidget *
stock_pixbuf_widget (StockPixmap icon)
{
  GdkPixbuf *pixbuf = NULL;
  GtkWidget *image;

  g_return_val_if_fail (icon >= 0 && icon < N_STOCK_PIXMAPS, NULL);

  stock_pixbuf_gdk (icon, &pixbuf);
  if (pixbuf)
    {
      image = gtk_image_new_from_pixbuf (pixbuf);
      g_object_unref (pixbuf);
    }
  else
    image = gtk_image_new_from_icon_name ("image-missing", GTK_ICON_SIZE_MENU);

  return image;
}

GtkWidget *
stock_pixbuf_widget_scale (StockPixmap icon, gint width, gint height)
{
  GdkPixbuf *pixbuf = NULL;
  GdkPixbuf *scaled_pixbuf;
  GtkWidget *image;

  g_return_val_if_fail (icon >= 0 && icon < N_STOCK_PIXMAPS, NULL);

  stock_pixbuf_gdk (icon, &pixbuf);
  scaled_pixbuf = gdk_pixbuf_scale_simple (pixbuf, width, height, GDK_INTERP_HYPER);
  g_object_unref (pixbuf);

  image = gtk_image_new_from_pixbuf (scaled_pixbuf);
  g_object_unref (scaled_pixbuf);

  return image;
}

static void
stock_pixbuf_load_icon (StockPixmap icon)
{
  g_return_if_fail (icon >= 0 && icon < N_STOCK_PIXMAPS);

  if (!pixmaps[icon].loaded)
    {
      gchar *path;

      path = g_build_filename (theme_dir, pixmaps[icon].filename, NULL);
      if (is_file_exist (path))
        {
          /* try to load user icon */
          pixmaps[icon].pixbuf = gdk_pixbuf_new_from_file (path, NULL);
          g_free (path);
        }
      else
        {
          /* load icon from resources */
          g_free (path);
          path = g_strdup_printf ("/yam/img/icons/%s", pixmaps[icon].filename);
          pixmaps[icon].pixbuf = gdk_pixbuf_new_from_resource (path, NULL);
        }

      if (pixmaps[icon].pixbuf == NULL && pixmaps[icon].size != GTK_ICON_SIZE_INVALID)
        {
          /* load icon from icon theme */
          pixmaps[icon].pixbuf = gtk_icon_theme_load_icon (gtk_icon_theme_get_default (), pixmaps[icon].iconname,
                                                           pixmaps[icon].size, GTK_ICON_LOOKUP_FORCE_REGULAR, NULL);
        }

      pixmaps[icon].loaded = TRUE;
    }
}

gint
stock_pixbuf_gdk (StockPixmap icon, GdkPixbuf ** pixbuf)
{
  g_return_val_if_fail (icon >= 0 && icon < N_STOCK_PIXMAPS, -1);

  stock_pixbuf_load_icon (icon);

  if (pixmaps[icon].pixbuf && pixbuf)
    {
      if (*pixbuf)
        g_object_unref (*pixbuf);
      *pixbuf = g_object_ref (pixmaps[icon].pixbuf);

      return 0;
    }

  return -1;
}

gint
stock_pixbuf_set_theme_dir (const gchar * dir)
{
  g_free (theme_dir);
  theme_dir = g_strdup (dir);

  return 0;
}
