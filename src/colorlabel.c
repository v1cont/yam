/*
 * YAM -- a GTK+ based, lightweight, and fast e-mail client
 * Copyright (C) 2020 Victor Ananjevsky <victor@sanana.kiev.ua>
 * Copyright (C) 2001-2010 Hiroyuki Yamamoto & The Sylpheed Claws Team
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

/* (alfons) - based on a contribution by Satoshi Nagayasu; revised for colorful
 * menu and more YAM integration. The idea to put the code in a separate
 * file is just that it make it easier to allow "user changeable" label colors.
 */

#include "defs.h"

#include <glib/gi18n.h>
#include <gtk/gtk.h>

#include "colorlabel.h"
#include "gtkutils.h"
#include "utils.h"
#include "prefs.h"

static gchar *labels[] = {
  N_("Orange"),
  N_("Red"),
  N_("Pink"),
  N_("Sky blue"),
  N_("Blue"),
  N_("Green"),
  N_("Brown")
};

typedef enum LabelColorChangeFlags_ {
  LCCF_COLOR = 1 << 0,
  LCCF_LABEL = 1 << 1,
  LCCF_ALL = LCCF_COLOR | LCCF_LABEL
} LabelColorChangeFlags;

static char *color_label_xpm[] = {
  "26 12 2 1",
  ". c #000000",
  "# c #ffffff",
  "..........................",
  ".########################.",
  ".########################.",
  ".########################.",
  ".########################.",
  ".########################.",
  ".########################.",
  ".########################.",
  ".########################.",
  ".########################.",
  ".########################.",
  "..........................",
};
char cl[] = "# c #ffffff";

/* XXX: if you add colors, make sure you also check the procmsg.h.
 * color indices are stored as 3 bits; that explains the max. of 7 colors */
static struct {
  LabelColorChangeFlags changed;
  GdkRGBA color;

  /* XXX: note that the label member is supposed to be dynamically
   * allocated and fffreed */
  gchar *label;
  GtkWidget *widget;
  GtkWidget *label_widget;
} label_colors[] = {
  {LCCF_ALL, {1.0, 0.65, 0.0, 1.0}, NULL, NULL, NULL},   /* orange */
  {LCCF_ALL, {1.0, 0.0, 0.0, 1.0}, NULL, NULL, NULL},    /* red    */
  {LCCF_ALL, {1.0, 0.75, 0.79, 1.0}, NULL, NULL, NULL},  /* pink   */
  {LCCF_ALL, {0.0, 1.0, 1.0, 1.0}, NULL, NULL, NULL},    /* cyan   */
  {LCCF_ALL, {0.0, 0.0, 0.9, 1.0}, NULL, NULL, NULL},    /* blue   */
  {LCCF_ALL, {0.0, 0.8, 0.0, 1.0}, NULL, NULL, NULL},    /* green  */
  {LCCF_ALL, {0.65, 0.17, 0.17, 1.0}, NULL, NULL, NULL}  /* brown  */
};

#define LABEL_COLOR_WIDTH	28
#define LABEL_COLOR_HEIGHT	16

#define LABEL_COLORS_ELEMS (sizeof label_colors / sizeof label_colors[0])

#define G_RETURN_VAL_IF_INVALID_COLOR(color, val)                       \
  g_return_val_if_fail((color) >= 0 && (color) < LABEL_COLORS_ELEMS, (val))

static void colorlabel_recreate (gint);
static void colorlabel_recreate_label (gint);

gint
colorlabel_get_color_count (void)
{
  return LABEL_COLORS_ELEMS;
}

GdkRGBA
colorlabel_get_color (gint color_index)
{
  GdkRGBA invalid = { 0 };

  G_RETURN_VAL_IF_INVALID_COLOR (color_index, invalid);

  return label_colors[color_index].color;
}

const gchar *
colorlabel_get_color_text (gint color_index)
{
  G_RETURN_VAL_IF_INVALID_COLOR (color_index, NULL);

  return label_colors[color_index].label ? label_colors[color_index].label : gettext (labels[color_index]);
}

const gchar *
colorlabel_get_custom_color_text (gint color_index)
{
  G_RETURN_VAL_IF_INVALID_COLOR (color_index, NULL);

  return label_colors[color_index].label;
}

void
colorlabel_set_color_text (gint color_index, const gchar * label)
{
  if (label_colors[color_index].label)
    g_free (label_colors[color_index].label);

  label_colors[color_index].label = g_strdup (label);
  label_colors[color_index].changed |= LCCF_LABEL;
}

GtkWidget *
colorlabel_create_color_widget (GdkRGBA color)
{
  GdkPixbuf *pb;
  GtkWidget *widget;

  snprintf (cl, 12, "# c #%.2X%.2X%.2X", (int) (color.red * 255), (int) (color.green * 255), (int) (color.blue * 255));
  color_label_xpm[2] = cl;
  pb = gdk_pixbuf_new_from_xpm_data ((const gchar **) color_label_xpm);
  widget = gtk_image_new_from_pixbuf (pb);
  g_object_unref (pb);
  return widget;
}

/* XXX: this function to check if menus with colors and labels should
 * be recreated */
gboolean
colorlabel_changed (void)
{
  gint n;

  for (n = 0; n < LABEL_COLORS_ELEMS; n++)
    {
      if (label_colors[n].changed)
        return TRUE;
    }

  return FALSE;
}

/* XXX: colorlabel_recreate_XXX are there to make sure everything
 * is initialized ok, without having to call a global _xxx_init_
 * function */
static void
colorlabel_recreate_color (gint color)
{
  GtkWidget *widget;

  if (!(label_colors[color].changed & LCCF_COLOR))
    return;

  widget = colorlabel_create_color_widget (label_colors[color].color);
  g_return_if_fail (widget);

  if (label_colors[color].widget)
    gtk_widget_destroy (label_colors[color].widget);

  label_colors[color].widget = widget;
  label_colors[color].changed &= ~LCCF_COLOR;
}

static void
colorlabel_recreate_label (gint color)
{
  const gchar *text;

  if (!(label_colors[color].changed & LCCF_LABEL))
    return;

  text = colorlabel_get_color_text (color);

  if (label_colors[color].label_widget)
    gtk_label_set_text (GTK_LABEL (label_colors[color].label_widget), text);
  else
    label_colors[color].label_widget = gtk_label_new (text);

  label_colors[color].changed &= ~LCCF_LABEL;
}

/* XXX: call this function everytime when you're doing important
 * stuff with the label_colors[] array */
static void
colorlabel_recreate (gint color)
{
  colorlabel_recreate_label (color);
  colorlabel_recreate_color (color);
}

static void
colorlabel_recreate_all (void)
{
  gint n;

  for (n = 0; n < LABEL_COLORS_ELEMS; n++)
    colorlabel_recreate (n);
}

/* colorlabel_create_check_color_menu_item() - creates a color
 * menu item with a check box */
GtkWidget *
colorlabel_create_check_color_menu_item (gint color_index)
{
  GtkWidget *hbox;
  GtkWidget *vbox;
  GtkWidget *item;

  G_RETURN_VAL_IF_INVALID_COLOR (color_index, NULL);

  item = gtk_check_menu_item_new ();

  colorlabel_recreate (color_index);

  hbox = gtk_box_new (GTK_ORIENTATION_HORIZONTAL, 0);
  gtk_widget_show (hbox);
  gtk_container_add (GTK_CONTAINER (item), hbox);

  vbox = gtk_box_new (GTK_ORIENTATION_VERTICAL, 0);
  gtk_widget_show (vbox);
  gtk_container_set_border_width (GTK_CONTAINER (vbox), 1);

  gtk_container_add (GTK_CONTAINER (vbox), label_colors[color_index].widget);
  gtk_widget_show (label_colors[color_index].widget);

  gtk_box_pack_start (GTK_BOX (hbox), vbox, FALSE, FALSE, 0);
  gtk_box_pack_start (GTK_BOX (hbox), label_colors[color_index].label_widget, FALSE, FALSE, 4);
  gtk_widget_show (label_colors[color_index].label_widget);

  return item;
}

/* colorlabel_create_color_menu() - creates a color menu without
 * checkitems, probably for use in combo items */
void
colorlabel_create_color_menu (GtkWidget *combo)
{
  GtkListStore *model;
  GtkCellRenderer *renderer;
  gint i;

  /* create model and cell */
  model = gtk_list_store_new (2, GDK_TYPE_PIXBUF, G_TYPE_STRING);
  gtk_combo_box_set_model (GTK_COMBO_BOX (combo), GTK_TREE_MODEL (model));

  renderer = gtk_cell_renderer_pixbuf_new ();
  gtk_cell_layout_pack_start (GTK_CELL_LAYOUT (combo), renderer, FALSE);
  gtk_cell_layout_set_attributes (GTK_CELL_LAYOUT (combo), renderer, "pixbuf", 0, NULL);

  renderer = gtk_cell_renderer_text_new ();
  gtk_cell_layout_pack_start (GTK_CELL_LAYOUT (combo), renderer, TRUE);
  gtk_cell_layout_set_attributes (GTK_CELL_LAYOUT (combo), renderer, "text", 1, NULL);

  /* fill the model */
  colorlabel_recreate_all ();

  for (i = 0; i < LABEL_COLORS_ELEMS; i++)
    {
      GdkPixbuf *pb;
      GtkTreeIter iter;

      snprintf (cl, 12, "# c #%.2X%.2X%.2X", (int) (label_colors[i].color.red * 255), (int) (label_colors[i].color.green * 255),
                (int) (label_colors[i].color.blue * 255));
      color_label_xpm[2] = cl;

      pb = gdk_pixbuf_new_from_xpm_data ((const char **) color_label_xpm);

      gtk_list_store_append (model, &iter);
      gtk_list_store_set (model, &iter, 0, pb, 1, colorlabel_get_color_text (i), -1);

      g_object_unref (pb);
    }

  g_object_unref (model);
}

guint
colorlabel_get_color_menu_active_item (GtkWidget * menu)
{
  GtkWidget *menuitem;
  guint color;

  g_return_val_if_fail (g_object_get_data (G_OBJECT (menu), "label_color_menu"), 0);
  menuitem = gtk_menu_get_active (GTK_MENU (menu));
  color = GPOINTER_TO_UINT (g_object_get_data (G_OBJECT (menuitem), "color"));
  return color;
}

void
colorlabel_update_menu (void)
{
  gint i;

  for (i = 0; i < LABEL_COLORS_ELEMS; i++)
    {
      if (label_colors[i].widget && label_colors[i].changed)
        colorlabel_recreate (i);
    }
}

gint
colorlabel_read_config (void)
{
  gchar *path;
  FILE *fp;
  gint i;
  gchar buf[PREFSBUFSIZE];

  path = g_strconcat (get_rc_dir (), G_DIR_SEPARATOR_S, "colorlabelrc", NULL);
  if ((fp = g_fopen (path, "rb")) == NULL)
    {
      g_free (path);
      return -1;
    }

  for (i = 0; i < LABEL_COLORS_ELEMS; i++)
    {
      if (fgets (buf, sizeof (buf), fp) == NULL)
        break;
      g_strstrip (buf);
      if (buf[0] != '\0')
        colorlabel_set_color_text (i, buf);
    }

  fclose (fp);
  g_free (path);

  return 0;
}

gint
colorlabel_write_config (void)
{
  gchar *path;
  PrefFile *pfile;
  gint i;
  gint ret;
  const gchar *text;

  path = g_strconcat (get_rc_dir (), G_DIR_SEPARATOR_S, "colorlabelrc", NULL);
  if ((pfile = prefs_file_open (path)) == NULL)
    {
      g_warning ("failed to write colorlabelrc");
      g_free (path);
      return -1;
    }

  for (i = 0; i < LABEL_COLORS_ELEMS; i++)
    {
      text = colorlabel_get_custom_color_text (i);
      ret = 0;
      if (text)
        ret = fputs (text, pfile->fp);

      if (ret == EOF || fputc ('\n', pfile->fp) == EOF)
        {
          FILE_OP_ERROR (path, "fputs || fputc");
          prefs_file_close_revert (pfile);
          g_free (path);
          return -1;
        }
    }

  if (prefs_file_close (pfile) < 0)
    {
      g_warning ("failed to write colorlabelrc");
      g_free (path);
      return -1;
    }

  g_free (path);

  return 0;
}
