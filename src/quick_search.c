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

#include "defs.h"

#include <glib.h>
#include <glib/gi18n.h>
#include <gdk/gdkkeysyms.h>
#include <gtk/gtk.h>

#include "summaryview.h"
#include "quick_search.h"
#include "filter.h"
#include "procheader.h"
#include "menu.h"
#include "addressbook.h"
#include "prefs_common.h"

static const struct {
  QSearchCondType type;
  FilterCondType ftype;
} qsearch_cond_types[] = {
  {QS_ALL, -1},
  {QS_UNREAD, FLT_COND_UNREAD},
  {QS_MARK, FLT_COND_MARK},
  {QS_CLABEL, FLT_COND_COLOR_LABEL},
  {QS_MIME, FLT_COND_MIME},
  {QS_W1DAY, -1},
  {QS_LAST5, -1},
  {QS_LAST7, -1},
  {QS_LAST30, -1},
  {QS_IN_ADDRESSBOOK, -1},
};

static void entry_activated (GtkWidget * entry, QuickSearch * qsearch);
static gboolean entry_key_pressed (GtkWidget * treeview, GdkEventKey * event, QuickSearch * qsearch);

void
quick_search_clear_entry (QuickSearch * qsearch)
{
  qsearch->entry_entered = FALSE;
  gtk_label_set_text (GTK_LABEL (qsearch->status_label), "");
}

QuickSearch *
quick_search_create (SummaryView * summaryview)
{
  QuickSearch *qsearch;
  GtkWidget *hbox;
  GtkWidget *optmenu;
  GtkWidget *label;
  GtkWidget *entry;
  GtkWidget *status_label;

  qsearch = g_new0 (QuickSearch, 1);

  hbox = gtk_box_new (GTK_ORIENTATION_HORIZONTAL, 5);
  gtk_container_set_border_width (GTK_CONTAINER (hbox), 2);

  optmenu = gtk_combo_box_text_new ();
  gtk_box_pack_start (GTK_BOX (hbox), optmenu, FALSE, FALSE, 0);

  gtk_combo_box_text_append_text (GTK_COMBO_BOX_TEXT (optmenu), _("All"));
  gtk_combo_box_text_append_text (GTK_COMBO_BOX_TEXT (optmenu), _("Unread"));
  gtk_combo_box_text_append_text (GTK_COMBO_BOX_TEXT (optmenu), _("Marked"));
  gtk_combo_box_text_append_text (GTK_COMBO_BOX_TEXT (optmenu), _("Have color label"));
  gtk_combo_box_text_append_text (GTK_COMBO_BOX_TEXT (optmenu), _("Have attachment"));
  gtk_combo_box_text_append_text (GTK_COMBO_BOX_TEXT (optmenu), _("Within 1 day"));
  gtk_combo_box_text_append_text (GTK_COMBO_BOX_TEXT (optmenu), _("Last 5 days"));
  gtk_combo_box_text_append_text (GTK_COMBO_BOX_TEXT (optmenu), _("Last 7 days"));
  gtk_combo_box_text_append_text (GTK_COMBO_BOX_TEXT (optmenu), _("Last 30 days"));
  gtk_combo_box_text_append_text (GTK_COMBO_BOX_TEXT (optmenu), _("In addressbook"));

  label = gtk_label_new (_("Search:"));
  gtk_box_pack_start (GTK_BOX (hbox), label, FALSE, FALSE, 0);

  entry = gtk_search_entry_new ();
  gtk_entry_set_placeholder_text (GTK_ENTRY (entry), _("Search for Subject or From"));
  gtk_widget_set_size_request (entry, 250, -1);
  gtk_widget_set_vexpand (entry, TRUE);
  gtk_box_pack_start (GTK_BOX (hbox), entry, FALSE, FALSE, 0);
  g_signal_connect (G_OBJECT (entry), "activate", G_CALLBACK (entry_activated), qsearch);
  g_signal_connect (G_OBJECT (entry), "key_press_event", G_CALLBACK (entry_key_pressed), qsearch);

  gtk_widget_set_tooltip_text (entry, _("Search for Subject or From"));

  status_label = gtk_label_new (NULL);
  gtk_box_pack_start (GTK_BOX (hbox), status_label, FALSE, FALSE, 0);

  qsearch->hbox = hbox;
  qsearch->optmenu = optmenu;
  qsearch->label = label;
  qsearch->entry = entry;
  qsearch->status_label = status_label;
  qsearch->summaryview = summaryview;
  qsearch->entry_entered = FALSE;
  summaryview->qsearch = qsearch;

  gtk_widget_show_all (hbox);

  return qsearch;
}

GSList *
quick_search_filter (QuickSearch * qsearch, QSearchCondType type, const gchar * key)
{
  SummaryView *summaryview = qsearch->summaryview;
  FilterCondType ftype;
  FilterRule *status_rule = NULL;
  FilterRule *rule = NULL;
  FilterCond *cond;
  FilterInfo fltinfo;
  GSList *cond_list = NULL;
  GSList *rule_list = NULL;
  GSList *flt_mlist = NULL;
  GSList *cur;
  gint count = 0, total = 0;
  gchar status_text[1024];
  gboolean dmode;

  if (!summaryview->all_mlist)
    return NULL;

  debug_print ("quick_search_filter: filtering summary (type: %d)\n", type);

  switch (type)
    {
    case QS_UNREAD:
    case QS_MARK:
    case QS_CLABEL:
    case QS_MIME:
      ftype = qsearch_cond_types[type].ftype;
      cond = filter_cond_new (ftype, 0, 0, NULL, NULL);
      cond_list = g_slist_append (cond_list, cond);
      status_rule = filter_rule_new ("Status filter rule", FLT_OR, cond_list, NULL);
      break;
    case QS_W1DAY:
      cond = filter_cond_new (FLT_COND_AGE_GREATER, 0, FLT_NOT_MATCH, NULL, "1");
      cond_list = g_slist_append (cond_list, cond);
      status_rule = filter_rule_new ("Status filter rule", FLT_OR, cond_list, NULL);
      break;
    case QS_LAST5:
      cond = filter_cond_new (FLT_COND_AGE_GREATER, 0, FLT_NOT_MATCH, NULL, "5");
      cond_list = g_slist_append (cond_list, cond);
      status_rule = filter_rule_new ("Status filter rule", FLT_OR, cond_list, NULL);
      break;
    case QS_LAST7:
      cond = filter_cond_new (FLT_COND_AGE_GREATER, 0, FLT_NOT_MATCH, NULL, "7");
      cond_list = g_slist_append (cond_list, cond);
      status_rule = filter_rule_new ("Status filter rule", FLT_OR, cond_list, NULL);
      break;
    case QS_LAST30:
      cond = filter_cond_new (FLT_COND_AGE_GREATER, 0, FLT_NOT_MATCH, NULL, "30");
      cond_list = g_slist_append (cond_list, cond);
      status_rule = filter_rule_new ("Status filter rule", FLT_OR, cond_list, NULL);
      break;
    case QS_IN_ADDRESSBOOK:
      cond = filter_cond_new (FLT_COND_HEADER, FLT_IN_ADDRESSBOOK, 0, "From", NULL);
      cond_list = g_slist_append (cond_list, cond);
      status_rule = filter_rule_new ("Status filter rule", FLT_OR, cond_list, NULL);
      break;
    case QS_ALL:
    default:
      break;
    }

  if (key)
    {
      gchar **keys;
      gint i;

      keys = g_strsplit (key, " ", -1);
      for (i = 0; keys[i] != NULL; i++)
        {
          cond_list = NULL;

          if (*keys[i] == '\0')
            continue;

          cond = filter_cond_new (FLT_COND_HEADER, FLT_CONTAIN, 0, "Subject", keys[i]);
          cond_list = g_slist_append (cond_list, cond);
          cond = filter_cond_new (FLT_COND_HEADER, FLT_CONTAIN, 0, "From", keys[i]);
          cond_list = g_slist_append (cond_list, cond);
          if (FOLDER_ITEM_IS_SENT_FOLDER (summaryview->folder_item))
            {
              cond = filter_cond_new (FLT_COND_TO_OR_CC, FLT_CONTAIN, 0, NULL, keys[i]);
              cond_list = g_slist_append (cond_list, cond);
            }

          if (cond_list)
            {
              rule = filter_rule_new ("Quick search rule", FLT_OR, cond_list, NULL);
              rule_list = g_slist_append (rule_list, rule);
            }
        }
      g_strfreev (keys);
    }

  memset (&fltinfo, 0, sizeof (FilterInfo));
  dmode = get_debug_mode ();
  set_debug_mode (FALSE);

  for (cur = summaryview->all_mlist; cur != NULL; cur = cur->next)
    {
      MsgInfo *msginfo = (MsgInfo *) cur->data;
      GSList *hlist = NULL;
      gboolean matched = TRUE;

      total++;

      if (status_rule)
        {
          if (type == QS_IN_ADDRESSBOOK)
            hlist = procheader_get_header_list_from_msginfo (msginfo);
          if (!filter_match_rule (status_rule, msginfo, hlist, &fltinfo))
            {
              if (hlist)
                procheader_header_list_destroy (hlist);
              continue;
            }
        }

      if (rule_list)
        {
          GSList *rcur;

          if (!hlist)
            hlist = procheader_get_header_list_from_msginfo (msginfo);

          /* AND keyword match */
          for (rcur = rule_list; rcur != NULL; rcur = rcur->next)
            {
              rule = (FilterRule *) rcur->data;
              if (!filter_match_rule (rule, msginfo, hlist, &fltinfo))
                {
                  matched = FALSE;
                  break;
                }
            }
        }

      if (matched)
        {
          flt_mlist = g_slist_prepend (flt_mlist, msginfo);
          count++;
        }

      if (hlist)
        procheader_header_list_destroy (hlist);
    }
  flt_mlist = g_slist_reverse (flt_mlist);

  set_debug_mode (dmode);

  if (status_rule || rule)
    {
      if (count > 0)
        g_snprintf (status_text, sizeof (status_text), _("%1$d in %2$d matched"), count, total);
      else
        g_snprintf (status_text, sizeof (status_text), _("No messages matched"));
      gtk_label_set_text (GTK_LABEL (qsearch->status_label), status_text);
    }
  else
    gtk_label_set_text (GTK_LABEL (qsearch->status_label), "");

  filter_rule_list_free (rule_list);
  filter_rule_free (status_rule);

  return flt_mlist;
}

static void
entry_activated (GtkWidget * entry, QuickSearch * qsearch)
{
  gtk_editable_select_region (GTK_EDITABLE (entry), 0, -1);
  summary_qsearch (qsearch->summaryview);
}

static gboolean
entry_key_pressed (GtkWidget * treeview, GdkEventKey * event, QuickSearch * qsearch)
{
  if (event && event->keyval == GDK_KEY_Escape)
    {
      quick_search_clear_entry (qsearch);
      return TRUE;
    }
  return FALSE;
}
