/*
 * YAM -- a GTK+ based, lightweight, and fast e-mail client
 * Copyright (C) 2020 Victor Ananjevsky <victor@sanana.kiev.ua>
 * Copyright (C) 1999-2008 Hiroyuki Yamamoto
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

#ifndef __FOLDERSEL_H__
#define __FOLDERSEL_H__

#include <glib.h>
#include <gtk/gtk.h>

#include "folder.h"

typedef enum {
  FOLDER_SEL_ALL,
  FOLDER_SEL_MOVE,
  FOLDER_SEL_COPY,
  FOLDER_SEL_MOVE_FOLDER
} FolderSelectionType;

FolderItem *foldersel_folder_sel (Folder * cur_folder, FolderSelectionType type, const gchar * default_folder);
FolderItem *foldersel_folder_sel_full (Folder * cur_folder,
                                       FolderSelectionType type, const gchar * default_folder, const gchar * message);

#endif /* __FOLDERSEL_H__ */
