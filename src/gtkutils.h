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

#ifndef __GTKUTILS_H__
#define __GTKUTILS_H__

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <glib.h>
#include <gdk/gdk.h>
#include <gtk/gtk.h>

#include <stdlib.h>

#include "itemfactory.h"

#define GTK_EVENTS_FLUSH()                      \
  {                                             \
	while (gtk_events_pending())                \
      gtk_main_iteration();                     \
  }

#define GTK_WIDGET_PTR(wid)	(*(GtkWidget **)wid)

typedef enum {
  ARROW_LEFT,
  ARROW_RIGHT,
  ARROW_UP,
  ARROW_DOWN
} ArrowType;

gboolean yam_get_str_size (GtkWidget * widget, const gchar * str, gint * width, gint * height);
gboolean yam_get_font_size (GtkWidget * widget, gint * width, gint * height);

void yam_stock_button_set_create (GtkWidget ** bbox, GtkWidget ** button1,  const gchar * label1,
                                    GtkWidget ** button2, const gchar * label2, GtkWidget ** button3, const gchar * label3);

/* TreeView functions */

gboolean yam_tree_model_next (GtkTreeModel * model, GtkTreeIter * iter);
gboolean yam_tree_model_prev (GtkTreeModel * model, GtkTreeIter * iter);

gboolean yam_tree_model_get_iter_last (GtkTreeModel * model, GtkTreeIter * iter);

gboolean yam_tree_model_find_by_column_data
  (GtkTreeModel * model, GtkTreeIter * iter, GtkTreeIter * start, gint col, gpointer data);

void yam_tree_model_foreach (GtkTreeModel * model,
                               GtkTreeIter * start, GtkTreeModelForeachFunc func, gpointer user_data);

gboolean yam_tree_row_reference_get_iter (GtkTreeModel * model, GtkTreeRowReference * ref, GtkTreeIter * iter);
gboolean yam_tree_row_reference_equal (GtkTreeRowReference * ref1, GtkTreeRowReference * ref2);

void yam_tree_sortable_unset_sort_column_id (GtkTreeSortable * sortable);

gboolean yam_tree_view_find_collapsed_parent (GtkTreeView * treeview, GtkTreeIter * parent, GtkTreeIter * iter);
void yam_tree_view_expand_parent_all (GtkTreeView * treeview, GtkTreeIter * iter);

void yam_tree_view_vertical_autoscroll (GtkTreeView * treeview);

void yam_tree_view_fast_clear (GtkTreeView * treeview, GtkTreeStore * store);

gchar *yam_editable_get_selection (GtkEditable * editable);

void yam_entry_strip_text (GtkEntry * entry);

void yam_scrolled_window_reset_position (GtkScrolledWindow * window);

/* TextView functions */

gboolean yam_text_buffer_match_string (GtkTextBuffer * buffer, const GtkTextIter * iter,
                                         gunichar * wcs, gint len, gboolean case_sens);
gboolean yam_text_buffer_find (GtkTextBuffer * buffer, const GtkTextIter * iter,
                                 const gchar * str, gboolean case_sens, GtkTextIter * match_pos);
gboolean yam_text_buffer_find_backward (GtkTextBuffer * buffer, const GtkTextIter * iter,
                                          const gchar * str, gboolean case_sens, GtkTextIter * match_pos);

void yam_text_buffer_insert_with_tag_by_name
  (GtkTextBuffer * buffer, GtkTextIter * iter, const gchar * text, gint len, const gchar * tag);

gchar *yam_text_view_get_selection (GtkTextView * textview);

void yam_window_popup (GtkWidget * window);
gboolean yam_window_modal_exist (void);

void yam_window_move (GtkWindow * window, gint x, gint y);

void yam_widget_get_uposition (GtkWidget * widget, gint * px, gint * py);
void yam_widget_init (void);

void yam_events_flush (void);

gboolean yam_separator_row (GtkTreeModel *model, GtkTreeIter *iter, gpointer data);

GtkWidget * yam_label_title (gchar *txt);
GtkWidget * yam_label_note (gchar *txt);

GtkWidget * yam_arrow_new (ArrowType type);
void yam_arrow_set (GtkWidget *arrow, ArrowType type);

void yam_text_view_modify_font (GtkWidget *text, PangoFontDescription *desc);

GtkWidget * yam_button_new (const gchar *label);

void yam_screen_get_size (GdkWindow *win, guint *width, guint *height);

#endif /* __GTKUTILS_H__ */
