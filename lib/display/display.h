/*
 * Copyright (C) 2001 Sistina Software
 *
 * This LVM library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Library General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This LVM library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Library General Public License for more details.
 *
 * You should have received a copy of the GNU Library General Public
 * License along with this LVM library; if not, write to the Free
 * Software Foundation, Inc., 59 Temple Place - Suite 330, Boston,
 * MA 02111-1307, USA
 *
 */

#ifndef _LVM_DISPLAY_H
#define _LVM_DISPLAY_H

typedef	enum {SIZE_LONG=0, SIZE_SHORT=1} size_len_t;

/* Specify size in KB */
char *display_size(unsigned long long size, size_len_t sl);

char *display_uuid(char *uuidstr);

#endif
