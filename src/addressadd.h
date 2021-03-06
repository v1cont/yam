/*
 * YAM -- a GTK+ based, lightweight, and fast e-mail client
 * Copyright (C) 2020 Victor Ananjevsky <victor@sanana.kiev.ua>
 * Copyright (C) 2001 Match Grun
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

/*
 * Add address to address book dialog.
 */

#ifndef __ADDRESS_ADD_H__
#define __ADDRESS_ADD_H__

#include "addrindex.h"

gboolean addressadd_selection (AddressIndex * addrIndex, const gchar * name, const gchar * address,
                               const gchar * remarks);

gboolean addressadd_autoreg (AddressIndex * addrIndex, const gchar * name, const gchar * address,
                             const gchar * remarks);

#endif /* __ADDRESS_ADD_H__ */
