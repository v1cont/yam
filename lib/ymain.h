/*
 * LibYAM -- E-Mail client library
 * Copyright (C) 2020 Victor Ananjevsky <victor@sanana.kiev.ua>
 * Copyright (C) 1999-2009 Hiroyuki Yamamoto
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301  USA
 */

#ifndef __SYLMAIN_H__
#define __SYLMAIN_H__

#include <glib.h>
#include <glib-object.h>

/* YamApp object */

#define YAM_TYPE_APP	(yam_app_get_type())
#define YAM_APP(obj)         (G_TYPE_CHECK_INSTANCE_CAST((obj), YAM_TYPE_APP, YamApp))
#define YAM_IS_APP(obj)      (G_TYPE_CHECK_INSTANCE_CAST((obj), YAM_TYPE_APP))
#define YAM_APP_CLASS(klass) (G_TYPE_CHECK_CLASS_CAST((klass), YAM_TYPE_APP, YamAppClass))
#define YAM_IS_APP_CLASS(klass)      (G_TYPE_CHECK_CLASS_TYPE((klass), YAM_TYPE_APP))
#define YAM_APP_GET_CLASS(obj)       (G_TYPE_INSTANCE_GET_CLASS((obj), YAM_TYPE_APP, YamAppClass))

typedef struct _YamApp YamApp;
typedef struct _YamAppClass YamAppClass;

struct _YamApp {
  GObject parent_instance;
};

struct _YamAppClass {
  GObjectClass parent_class;
};

GObject *yam_app_create (void);
GObject *yam_app_get (void);

void yam_init (void);
void yam_init_gettext (const gchar * package, const gchar * dirname);
gint yam_setup_rc_dir (void);
void yam_save_all_state (void);
void yam_cleanup (void);

#endif /* __SYLMAIN_H__ */
