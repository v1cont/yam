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

#include <glib/gi18n.h>
#include <gdk/gdkkeysyms.h>
#include <gdk/gdk.h>
#include <gtk/gtk.h>

#include <stdlib.h>
#include <string.h>
#include <stdarg.h>
#include <math.h>

#include "itemfactory.h"
#include "gtkutils.h"
#include "utils.h"
#include "codeconv.h"
#include "menu.h"

typedef struct {
  gchar *id;
  gchar *icon;
  gchar *label;
  GtkWidget *image;
} YamStock;

YamStock yam_stock_items[] = {
  { "yam-ok", "gtk-ok", "_OK", NULL },
  { "yam-cancel", "gtk-cancel", N_("_Cancel"), NULL },
  { "yam-yes", "gtk-yes", N_("_Yes"), NULL },
  { "yam-no", "gtk-no", N_("_No"), NULL },
  { "yam-close", "window-close", N_("_Close"), NULL },
  { "yam-apply", "gtk-apply", N_("_Apply"), NULL },
  { "yam-new", "document-new", N_("_New"), NULL },
  { "yam-open", "document-open", N_("_Open"), NULL },
  { "yam-save", "document-save", N_("_Save"), NULL },
  { "yam-add", "list-add", N_("_Add"), NULL },
  { "yam-clear", "edit-clear-all", N_("_Clear"), NULL },
  { "yam-copy", "edit-copy", N_("_Copy"), NULL },
  { "yam-delete", "edit-delete", N_("_Delete"), NULL },
  { "yam-edit", "gtk-edit", N_("_Edit"), NULL },
  { "yam-exec", "system-run", N_("_Execute"), NULL },
  { "yam-find", "edit-find", N_("_Find"), NULL },
  { "yam-go-back", "go-previous", N_("_Back"), NULL },
  { "yam-go-down", "go-down", N_("_Down"), NULL },
  { "yam-go-forward", "go-next", N_("_Next"), NULL },
  { "yam-go-bottom", "go-bottom", N_("_Bottom"), NULL },
  { "yam-go-top", "go-top", N_("_Top"), NULL },
  { "yam-go-up", "go-up", N_("_Up"), NULL },
  { "yam-preferences", "gtk-preferences", N_("_Preferences"), NULL },
  { "yam-print", "document-print", N_("_Print"), NULL },
  { "yam-refresh", "view-refresh", N_("_Refresh"), NULL },
  { "yam-stop", "process-stop", N_("_Stop"), NULL },
};

gboolean
yam_get_str_size (GtkWidget * widget, const gchar * str, gint * width, gint * height)
{
  PangoLayout *layout;

  g_return_val_if_fail (GTK_IS_WIDGET (widget), FALSE);

  layout = gtk_widget_create_pango_layout (widget, str);
  g_return_val_if_fail (layout, FALSE);
  pango_layout_get_pixel_size (layout, width, height);
  g_object_unref (layout);

  return TRUE;
}

gboolean
yam_get_font_size (GtkWidget * widget, gint * width, gint * height)
{
  const gchar *str = "Abcdef";
  gboolean ret;

  ret = yam_get_str_size (widget, str, width, height);
  if (ret && width)
    *width = *width / g_utf8_strlen (str, -1);

  return ret;
}

void
yam_stock_button_set_create (GtkWidget ** bbox, GtkWidget ** button1, const gchar * label1,
                               GtkWidget ** button2, const gchar * label2, GtkWidget ** button3, const gchar * label3)
{
  g_return_if_fail (bbox != NULL);
  g_return_if_fail (button1 != NULL);

  *bbox = gtk_button_box_new (GTK_ORIENTATION_HORIZONTAL);
  gtk_button_box_set_layout (GTK_BUTTON_BOX (*bbox), GTK_BUTTONBOX_END);
  gtk_box_set_spacing (GTK_BOX (*bbox), 5);

  if (button3)
    {
      *button3 = yam_button_new (label3);
      gtk_widget_set_can_default (*button3, TRUE);
      gtk_box_pack_start (GTK_BOX (*bbox), *button3, FALSE, FALSE, 0);
      gtk_widget_show (*button3);
    }

  if (button2)
    {
      *button2 = yam_button_new (label2);
      gtk_widget_set_can_default (*button2, TRUE);
      gtk_box_pack_start (GTK_BOX (*bbox), *button2, FALSE, FALSE, 0);
      gtk_widget_show (*button2);
    }

  *button1 = yam_button_new (label1);
  gtk_widget_set_can_default (*button1, TRUE);
  gtk_box_pack_start (GTK_BOX (*bbox), *button1, FALSE, FALSE, 0);
  gtk_widget_show (*button1);
}

gboolean
yam_tree_model_next (GtkTreeModel * model, GtkTreeIter * iter)
{
  GtkTreeIter iter_, parent;
  gboolean valid;

  if (gtk_tree_model_iter_children (model, &iter_, iter))
    {
      *iter = iter_;
      return TRUE;
    }

  iter_ = *iter;
  if (gtk_tree_model_iter_next (model, &iter_))
    {
      *iter = iter_;
      return TRUE;
    }

  iter_ = *iter;
  valid = gtk_tree_model_iter_parent (model, &parent, &iter_);
  while (valid)
    {
      iter_ = parent;
      if (gtk_tree_model_iter_next (model, &iter_))
        {
          *iter = iter_;
          return TRUE;
        }

      iter_ = parent;
      valid = gtk_tree_model_iter_parent (model, &parent, &iter_);
    }

  return FALSE;
}

gboolean
yam_tree_model_prev (GtkTreeModel * model, GtkTreeIter * iter)
{
  GtkTreeIter iter_, child, next, parent;
  GtkTreePath *path;
  gboolean found = FALSE;

  iter_ = *iter;

  path = gtk_tree_model_get_path (model, &iter_);

  if (gtk_tree_path_prev (path))
    {
      gtk_tree_model_get_iter (model, &child, path);

      while (gtk_tree_model_iter_has_child (model, &child))
        {
          iter_ = child;
          gtk_tree_model_iter_children (model, &child, &iter_);
          next = child;
          while (gtk_tree_model_iter_next (model, &next))
            child = next;
        }

      *iter = child;
      found = TRUE;
    }
  else if (gtk_tree_model_iter_parent (model, &parent, &iter_))
    {
      *iter = parent;
      found = TRUE;
    }

  gtk_tree_path_free (path);

  return found;
}

gboolean
yam_tree_model_get_iter_last (GtkTreeModel * model, GtkTreeIter * iter)
{
  GtkTreeIter iter_, child, next;

  if (!gtk_tree_model_get_iter_first (model, &iter_))
    return FALSE;

  for (;;)
    {
      next = iter_;
      while (gtk_tree_model_iter_next (model, &next))
        iter_ = next;
      if (gtk_tree_model_iter_children (model, &child, &iter_))
        iter_ = child;
      else
        break;
    }

  *iter = iter_;
  return TRUE;
}

gboolean
yam_tree_model_find_by_column_data (GtkTreeModel * model,
                                      GtkTreeIter * iter, GtkTreeIter * start, gint col, gpointer data)
{
  gboolean valid;
  GtkTreeIter iter_;
  gpointer store_data;

  if (start)
    {
      gtk_tree_model_get (model, start, col, &store_data, -1);
      if (store_data == data)
        {
          *iter = *start;
          return TRUE;
        }
      valid = gtk_tree_model_iter_children (model, &iter_, start);
    }
  else
    valid = gtk_tree_model_get_iter_first (model, &iter_);

  while (valid)
    {
      if (yam_tree_model_find_by_column_data (model, iter, &iter_, col, data))
        return TRUE;

      valid = gtk_tree_model_iter_next (model, &iter_);
    }

  return FALSE;
}

void
yam_tree_model_foreach (GtkTreeModel * model, GtkTreeIter * start, GtkTreeModelForeachFunc func, gpointer user_data)
{
  gboolean valid = TRUE;
  GtkTreeIter iter;
  GtkTreePath *path;

  g_return_if_fail (func != NULL);

  if (!start)
    {
      gtk_tree_model_foreach (model, func, user_data);
      return;
    }

  path = gtk_tree_model_get_path (model, start);
  func (model, path, start, user_data);
  gtk_tree_path_free (path);

  valid = gtk_tree_model_iter_children (model, &iter, start);
  while (valid)
    {
      yam_tree_model_foreach (model, &iter, func, user_data);
      valid = gtk_tree_model_iter_next (model, &iter);
    }
}

gboolean
yam_tree_row_reference_get_iter (GtkTreeModel * model, GtkTreeRowReference * ref, GtkTreeIter * iter)
{
  GtkTreePath *path;
  gboolean valid = FALSE;

  if (ref)
    {
      path = gtk_tree_row_reference_get_path (ref);
      if (path)
        {
          valid = gtk_tree_model_get_iter (model, iter, path);
          gtk_tree_path_free (path);
        }
    }

  return valid;
}

gboolean
yam_tree_row_reference_equal (GtkTreeRowReference * ref1, GtkTreeRowReference * ref2)
{
  GtkTreePath *path1, *path2;
  gint result;

  if (ref1 == NULL || ref2 == NULL)
    return FALSE;

  path1 = gtk_tree_row_reference_get_path (ref1);
  if (!path1)
    return FALSE;
  path2 = gtk_tree_row_reference_get_path (ref2);
  if (!path2)
    {
      gtk_tree_path_free (path1);
      return FALSE;
    }

  result = gtk_tree_path_compare (path1, path2);

  gtk_tree_path_free (path2);
  gtk_tree_path_free (path1);

  return (result == 0);
}

void
yam_tree_sortable_unset_sort_column_id (GtkTreeSortable * sortable)
{
  gtk_tree_sortable_set_sort_column_id (sortable, GTK_TREE_SORTABLE_UNSORTED_SORT_COLUMN_ID, GTK_SORT_ASCENDING);
}

gboolean
yam_tree_view_find_collapsed_parent (GtkTreeView * treeview, GtkTreeIter * parent, GtkTreeIter * iter)
{
  GtkTreeModel *model;
  GtkTreeIter iter_, parent_;
  GtkTreePath *path;
  gboolean valid;

  if (!iter)
    return FALSE;

  model = gtk_tree_view_get_model (treeview);
  valid = gtk_tree_model_iter_parent (model, &parent_, iter);

  while (valid)
    {
      path = gtk_tree_model_get_path (model, &parent_);
      if (!gtk_tree_view_row_expanded (treeview, path))
        {
          *parent = parent_;
          gtk_tree_path_free (path);
          return TRUE;
        }
      gtk_tree_path_free (path);
      iter_ = parent_;
      valid = gtk_tree_model_iter_parent (model, &parent_, &iter_);
    }

  return FALSE;
}

void
yam_tree_view_expand_parent_all (GtkTreeView * treeview, GtkTreeIter * iter)
{
  GtkTreeModel *model;
  GtkTreeIter parent;
  GtkTreePath *path;

  model = gtk_tree_view_get_model (treeview);

  if (gtk_tree_model_iter_parent (model, &parent, iter))
    {
      path = gtk_tree_model_get_path (model, &parent);
      gtk_tree_view_expand_to_path (treeview, path);
      gtk_tree_path_free (path);
    }
}

#define SCROLL_EDGE_SIZE 15

/* borrowed from gtktreeview.c */
void
yam_tree_view_vertical_autoscroll (GtkTreeView * treeview)
{
  GdkRectangle visible_rect;
  gint y, wy;
  gint offset;
  GtkAdjustment *vadj;
  gfloat value;

  gdk_window_get_pointer (gtk_tree_view_get_bin_window (treeview), NULL, &wy, NULL);
  gtk_tree_view_convert_widget_to_tree_coords (treeview, 0, wy, NULL, &y);

  gtk_tree_view_get_visible_rect (treeview, &visible_rect);

  /* see if we are near the edge. */
  offset = y - (visible_rect.y + 2 * SCROLL_EDGE_SIZE);
  if (offset > 0)
    {
      offset = y - (visible_rect.y + visible_rect.height - 2 * SCROLL_EDGE_SIZE);
      if (offset < 0)
        return;
    }

  vadj = gtk_scrollable_get_vadjustment (GTK_SCROLLABLE (treeview));
  value = CLAMP (gtk_adjustment_get_value (vadj) + offset, 0.0,
                 gtk_adjustment_get_upper (vadj) - gtk_adjustment_get_page_size (vadj));
  gtk_adjustment_set_value (vadj, value);
}

void
yam_tree_view_fast_clear (GtkTreeView * treeview, GtkTreeStore * store)
{
  gtk_tree_view_set_model (treeview, NULL);
  gtk_tree_store_clear (store);
  gtk_tree_view_set_model (treeview, GTK_TREE_MODEL (store));
}

gchar *
yam_editable_get_selection (GtkEditable * editable)
{
  gint start_pos, end_pos;
  gboolean found;

  g_return_val_if_fail (GTK_IS_EDITABLE (editable), NULL);

  found = gtk_editable_get_selection_bounds (editable, &start_pos, &end_pos);
  if (found)
    return gtk_editable_get_chars (editable, start_pos, end_pos);
  else
    return NULL;
}

void
yam_entry_strip_text (GtkEntry * entry)
{
  gchar *text;
  gint len;

  text = gtk_editable_get_chars (GTK_EDITABLE (entry), 0, -1);
  len = strlen (text);
  g_strstrip (text);
  if (len > strlen (text))
    gtk_entry_set_text (entry, text);
  g_free (text);
}

void
yam_scrolled_window_reset_position (GtkScrolledWindow * window)
{
  GtkAdjustment *adj;

  adj = gtk_scrolled_window_get_hadjustment (GTK_SCROLLED_WINDOW (window));
  gtk_adjustment_set_value (adj, gtk_adjustment_get_lower (adj));
  adj = gtk_scrolled_window_get_vadjustment (GTK_SCROLLED_WINDOW (window));
  gtk_adjustment_set_value (adj, gtk_adjustment_get_lower (adj));
}

gboolean
yam_text_buffer_match_string (GtkTextBuffer * textbuf,
                                const GtkTextIter * iter, gunichar * wcs, gint len, gboolean case_sens)
{
  GtkTextIter start_iter, end_iter;
  gchar *utf8str, *p;
  gint match_count;

  start_iter = end_iter = *iter;
  gtk_text_iter_forward_chars (&end_iter, len);

  utf8str = gtk_text_buffer_get_text (textbuf, &start_iter, &end_iter, FALSE);
  if (!utf8str)
    return FALSE;

  if ((gint) g_utf8_strlen (utf8str, -1) != len)
    {
      g_free (utf8str);
      return FALSE;
    }

  for (p = utf8str, match_count = 0; *p != '\0' && match_count < len; p = g_utf8_next_char (p), match_count++)
    {
      gunichar wc;

      wc = g_utf8_get_char (p);

      if (case_sens)
        {
          if (wc != wcs[match_count])
            break;
        }
      else
        {
          if (g_unichar_tolower (wc) != g_unichar_tolower (wcs[match_count]))
            break;
        }
    }

  g_free (utf8str);

  if (match_count == len)
    return TRUE;
  else
    return FALSE;
}

gboolean
yam_text_buffer_find (GtkTextBuffer * buffer, const GtkTextIter * iter,
                        const gchar * str, gboolean case_sens, GtkTextIter * match_pos)
{
  gunichar *wcs;
  gint len;
  glong items_read = 0, items_written = 0;
  GError *error = NULL;
  GtkTextIter iter_;
  gboolean found = FALSE;

  wcs = g_utf8_to_ucs4 (str, -1, &items_read, &items_written, &error);
  if (error != NULL)
    {
      g_warning ("An error occurred while converting a string from UTF-8 to UCS-4: %s\n", error->message);
      g_error_free (error);
    }
  if (!wcs || items_written <= 0)
    return FALSE;
  len = (gint) items_written;

  iter_ = *iter;
  do
    {
      found = yam_text_buffer_match_string (buffer, &iter_, wcs, len, case_sens);
      if (found)
        {
          *match_pos = iter_;
          break;
        }
    }
  while (gtk_text_iter_forward_char (&iter_));

  g_free (wcs);

  return found;
}

gboolean
yam_text_buffer_find_backward (GtkTextBuffer * buffer,
                                 const GtkTextIter * iter,
                                 const gchar * str, gboolean case_sens, GtkTextIter * match_pos)
{
  gunichar *wcs;
  gint len;
  glong items_read = 0, items_written = 0;
  GError *error = NULL;
  GtkTextIter iter_;
  gboolean found = FALSE;

  wcs = g_utf8_to_ucs4 (str, -1, &items_read, &items_written, &error);
  if (error != NULL)
    {
      g_warning ("An error occurred while converting a string from UTF-8 to UCS-4: %s\n", error->message);
      g_error_free (error);
    }
  if (!wcs || items_written <= 0)
    return FALSE;
  len = (gint) items_written;

  iter_ = *iter;
  while (gtk_text_iter_backward_char (&iter_))
    {
      found = yam_text_buffer_match_string (buffer, &iter_, wcs, len, case_sens);
      if (found)
        {
          *match_pos = iter_;
          break;
        }
    }

  g_free (wcs);

  return found;
}

#define MAX_TEXT_LINE_LEN	8190

void
yam_text_buffer_insert_with_tag_by_name (GtkTextBuffer * buffer,
                                           GtkTextIter * iter, const gchar * text, gint len, const gchar * tag)
{
  if (len < 0)
    len = strlen (text);

  gtk_text_buffer_insert_with_tags_by_name (buffer, iter, text, len, tag, NULL);

  if (len > 0 && text[len - 1] != '\n')
    {
      /* somehow returns invalid value first (bug?),
         so call it twice */
      gtk_text_iter_get_chars_in_line (iter);
      if (gtk_text_iter_get_chars_in_line (iter) > MAX_TEXT_LINE_LEN)
        {
          gtk_text_buffer_insert_with_tags_by_name (buffer, iter, "\n", 1, tag, NULL);
        }
    }
}

gchar *
yam_text_view_get_selection (GtkTextView * textview)
{
  GtkTextBuffer *buffer;
  GtkTextIter start_iter, end_iter;
  gboolean found;

  g_return_val_if_fail (GTK_IS_TEXT_VIEW (textview), NULL);

  buffer = gtk_text_view_get_buffer (textview);
  found = gtk_text_buffer_get_selection_bounds (buffer, &start_iter, &end_iter);
  if (found)
    return gtk_text_buffer_get_text (buffer, &start_iter, &end_iter, FALSE);
  else
    return NULL;
}

void
yam_window_popup (GtkWidget * window)
{
  guint x, y, sx, sy, new_x, new_y;

  g_return_if_fail (window != NULL);
  g_return_if_fail (gtk_widget_get_window (window) != NULL);

  yam_screen_get_size (gtk_widget_get_window (window), &sx, &sy);

  gdk_window_get_origin (gtk_widget_get_window (window), &x, &y);
  new_x = x % sx;
  if (new_x < 0)
    new_x = 0;
  new_y = y % sy;
  if (new_y < 0)
    new_y = 0;
  if (new_x != x || new_y != y)
    gdk_window_move (gtk_widget_get_window (window), new_x, new_y);

  gtk_widget_show (window);
  gtk_window_present (GTK_WINDOW (window));
}

gboolean
yam_window_modal_exist (void)
{
  GList *window_list, *cur;
  gboolean exist = FALSE;

  window_list = gtk_window_list_toplevels ();
  for (cur = window_list; cur != NULL; cur = cur->next)
    {
      GtkWidget *window = GTK_WIDGET (cur->data);

      if (gtk_widget_get_visible (window) && gtk_window_get_modal (GTK_WINDOW (window)))
        {
          exist = TRUE;
          break;
        }
    }
  g_list_free (window_list);

  return exist;
}

/* ensure that the window is displayed on screen */
void
yam_window_move (GtkWindow * window, gint x, gint y)
{
  guint sx, sy;

  g_return_if_fail (window != NULL);

  yam_screen_get_size (gtk_widget_get_window (GTK_WIDGET (window)), &sx, &sy);

  if (x < 0)
    x = 0;
  if (y < 0)
    y = 0;
  x %= sx;
  y %= sy;

  gtk_window_move (window, x, y);
}

void
yam_widget_get_uposition (GtkWidget * widget, gint * px, gint * py)
{
  gint x, y;
  gint sx, sy;

  g_return_if_fail (widget != NULL);
  g_return_if_fail (gtk_widget_get_window (widget) != NULL);

  yam_screen_get_size (gtk_widget_get_window (widget), &sx, &sy);

  /* gdk_window_get_root_origin ever return *rootwindow*'s position */
  gdk_window_get_root_origin (gtk_widget_get_window (widget), &x, &y);

  x %= sx;
  if (x < 0)
    x = 0;
  y %= sy;
  if (y < 0)
    y = 0;
  *px = x;
  *py = y;
}

void
yam_events_flush (void)
{
  GTK_EVENTS_FLUSH ();
}

gboolean
yam_separator_row (GtkTreeModel *model, GtkTreeIter *iter, gpointer data)
{
  gchar *lbl;
  gtk_tree_model_get (model, iter, 0, &lbl, -1);
  return (lbl == NULL);
}

GtkWidget *
yam_label_title (gchar *txt)
{
  GtkWidget *lbl;

  lbl = gtk_label_new (NULL);

  if (txt)
    {
      gchar *str;

      str = g_strdup_printf ("<span size=\"large\" font_weight=\"bold\">%s</span>", txt);
      gtk_label_set_markup (GTK_LABEL (lbl), str);
      g_free (str);
    }

  return lbl;
}

GtkWidget *
yam_label_note (gchar *txt)
{
  GtkWidget *lbl;

  lbl = gtk_label_new (NULL);

  if (txt)
    {
      gchar *str;

      str = g_strdup_printf ("<span size=\"small\">%s</span>", txt);
      gtk_label_set_markup (GTK_LABEL (lbl), str);
      g_free (str);
    }

  return lbl;
}

GtkWidget *
yam_arrow_new (ArrowType type)
{
  GtkWidget *arrow;

  arrow = gtk_image_new ();
  yam_arrow_set (arrow, type);

  return arrow;
}

void
yam_arrow_set (GtkWidget *arrow, ArrowType type)
{
  switch (type)
    {
    case ARROW_LEFT:
      gtk_image_set_from_icon_name (GTK_IMAGE (arrow), "pan-left-symbolic", GTK_ICON_SIZE_MENU);
      break;
    case ARROW_RIGHT:
      gtk_image_set_from_icon_name (GTK_IMAGE (arrow), "pan-right-symbolic", GTK_ICON_SIZE_MENU);
      break;
    case ARROW_UP:
      gtk_image_set_from_icon_name (GTK_IMAGE (arrow), "pan-up-symbolic", GTK_ICON_SIZE_MENU);
      break;
    case ARROW_DOWN:
      gtk_image_set_from_icon_name (GTK_IMAGE (arrow), "pan-down-symbolic", GTK_ICON_SIZE_MENU);
      break;
    default:
      g_warning ("Unknown arrow type");
    }
}

void
yam_text_view_modify_font (GtkWidget *text, PangoFontDescription *desc)
{
  GtkCssProvider *provider;
  GtkStyleContext *context;
  PangoFontMask mask;
  GString *css;

  /* make css font description (code from gedit) */
  css = g_string_new ("textview{");

  mask = pango_font_description_get_set_fields (desc);

  if ((mask & PANGO_FONT_MASK_FAMILY) != 0)
    {
      const gchar *family = pango_font_description_get_family (desc);
      g_string_append_printf (css, "font-family:\"%s\";", family);
    }

  if ((mask & PANGO_FONT_MASK_STYLE) != 0)
    {
      PangoVariant variant = pango_font_description_get_variant (desc);

      switch (variant)
        {
        case PANGO_VARIANT_NORMAL:
          g_string_append (css, "font-variant:normal;");
          break;
        case PANGO_VARIANT_SMALL_CAPS:
          g_string_append (css, "font-variant:small-caps;");
          break;
        default:
          break;
        }
    }

  if ((mask & PANGO_FONT_MASK_WEIGHT))
    {
      gint weight = pango_font_description_get_weight (desc);

      switch (weight)
        {
        case PANGO_WEIGHT_SEMILIGHT:
        case PANGO_WEIGHT_NORMAL:
          g_string_append (css, "font-weight:normal;");
          break;
        case PANGO_WEIGHT_BOLD:
          g_string_append (css, "font-weight:bold;");
          break;
        case PANGO_WEIGHT_THIN:
        case PANGO_WEIGHT_ULTRALIGHT:
        case PANGO_WEIGHT_LIGHT:
        case PANGO_WEIGHT_BOOK:
        case PANGO_WEIGHT_MEDIUM:
        case PANGO_WEIGHT_SEMIBOLD:
        case PANGO_WEIGHT_ULTRABOLD:
        case PANGO_WEIGHT_HEAVY:
        case PANGO_WEIGHT_ULTRAHEAVY:
        default:
          /* round to nearest hundred */
          weight = round (weight / 100.0) * 100;
          g_string_append_printf (css, "font-weight:%d;", weight);
          break;
        }
    }

  if ((mask & PANGO_FONT_MASK_STRETCH))
    {
      switch (pango_font_description_get_stretch (desc))
        {
        case PANGO_STRETCH_ULTRA_CONDENSED:
          g_string_append (css, "font-stretch:untra-condensed;");
          break;
        case PANGO_STRETCH_EXTRA_CONDENSED:
          g_string_append (css, "font-stretch:extra-condensed;");
          break;
        case PANGO_STRETCH_CONDENSED:
          g_string_append (css, "font-stretch:condensed;");
          break;
        case PANGO_STRETCH_SEMI_CONDENSED:
          g_string_append (css, "font_stretch:semi-condensed;");
          break;
        case PANGO_STRETCH_NORMAL:
          g_string_append (css, "font-stretch:normal;");
          break;
        case PANGO_STRETCH_SEMI_EXPANDED:
          g_string_append (css, "font-stretch:semi-expanded;");
          break;
        case PANGO_STRETCH_EXPANDED:
          g_string_append (css, "font-stretch:expanded;");
          break;
        case PANGO_STRETCH_EXTRA_EXPANDED:
          g_string_append (css, "font-stretch:extra-expanded;");
          break;
        case PANGO_STRETCH_ULTRA_EXPANDED:
          g_string_append (css, "font-stretch:untra-expanded;");
          break;
        default:
          break;
        }
    }

  if ((mask & PANGO_FONT_MASK_SIZE))
    {
      gint font_size = pango_font_description_get_size (desc) / PANGO_SCALE;
      g_string_append_printf (css, "font-size:%dpt", font_size);
    }
  g_string_append (css, "}");

  /* add css */
  provider = gtk_css_provider_new ();
  gtk_css_provider_load_from_data (provider, css->str, -1, NULL);
  context = gtk_widget_get_style_context (text);
  gtk_style_context_add_provider (context, GTK_STYLE_PROVIDER (provider),
                                  GTK_STYLE_PROVIDER_PRIORITY_APPLICATION);

  g_string_free (css, TRUE);
}

GtkWidget *
yam_button_new (const gchar *label)
{
  GtkWidget *btn;
  gboolean found = FALSE;
  gint i;

  if (g_ascii_strncasecmp (label, "yam-", 4) != 0)
    {
      /* not a stock button */
      btn = gtk_button_new_with_mnemonic (label);
      return btn;
    }

  for (i = 0; i < sizeof (yam_stock_items); i++)
    {
      if (g_ascii_strcasecmp (yam_stock_items[i].id, label) == 0)
        {
          found = TRUE;
          break;
        }
    }

  if (found)
    {
      btn = gtk_button_new_with_mnemonic (_(yam_stock_items[i].label));
      if (!yam_stock_items[i].image)
        {
          if (yam_stock_items[i].icon)
            yam_stock_items[i].image = gtk_image_new_from_icon_name (yam_stock_items[i].icon, GTK_ICON_SIZE_MENU);
        }
      if (yam_stock_items[i].image)
        {
          gtk_button_set_always_show_image (GTK_BUTTON (btn), TRUE);
          gtk_button_set_image (GTK_BUTTON (btn), yam_stock_items[i].image);
        }
    }
  else
    {
      g_warning ("Stock item with id '%s' not found\n", label);
      btn = gtk_button_new_with_mnemonic (label);
    }

  return btn;
}

void
yam_screen_get_size (GdkWindow *win, guint *width, guint *height)
{
  GdkMonitor *mon;
  GdkRectangle rect;

  mon = gdk_display_get_monitor_at_window (gdk_display_get_default (), win);
  gdk_monitor_get_workarea (mon, &rect);

  if (width)
    *width = rect.width;
  if (height)
    *height = rect.height;
}
