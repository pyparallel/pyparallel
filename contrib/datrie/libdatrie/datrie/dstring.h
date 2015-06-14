/* -*- Mode: C; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
/*
 * libdatrie - Double-Array Trie Library
 * Copyright (C) 2006  Theppitak Karoonboonyanan <thep@linux.thai.net>
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

/*
 * dstring.h - Dynamic string type
 * Created: 2012-08-01
 * Author:  Theppitak Karoonboonyanan <thep@linux.thai.net>
 */

#ifndef __DSTRING_H
#define __DSTRING_H

#include "typedefs.h"

typedef struct _DString DString;

DString * dstring_new (int char_size, int n_elm);

void      dstring_free (DString *ds);

int       dstring_length (const DString *ds);

const void * dstring_get_val (const DString *ds);

void *    dstring_get_val_rw (DString *ds);

void      dstring_clear (DString *ds);

Bool      dstring_copy (DString *dst, const DString *src);

Bool      dstring_append (DString *dst, const DString *src);

Bool      dstring_append_string (DString *ds, const void *data, int len);

Bool      dstring_append_char (DString *ds, const void *data);

Bool      dstring_terminate (DString *ds);

Bool      dstring_cut_last (DString *ds);

#endif  /* __DSTRING_H */

/*
vi:ts=4:ai:expandtab
*/
