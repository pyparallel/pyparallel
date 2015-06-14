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
 * dstring.c - Dynamic string type
 * Created: 2012-08-01
 * Author:  Theppitak Karoonboonyanan <thep@linux.thai.net>
 */

#include "dstring.h"
#include "dstring-private.h"

#include "trie-private.h"
#include <string.h>
#include <stdlib.h>


DString *
dstring_new (int char_size, int n_elm)
{
    DString *ds;

    ds = (DString *) malloc (sizeof (DString));
    if (!ds)
        return NULL;

    ds->alloc_size = char_size * n_elm;
    ds->val = malloc (ds->alloc_size);
    if (!ds->val) {
        free (ds);
        return NULL;
    }

    ds->char_size = char_size;
    ds->str_len = 0;

    return ds;
}

void
dstring_free (DString *ds)
{
    free (ds->val);
    free (ds);
}

int
dstring_length (const DString *ds)
{
    return ds->str_len;
}

const void *
dstring_get_val (const DString *ds)
{
    return ds->val;
}

void *
dstring_get_val_rw (DString *ds)
{
    return ds->val;
}

void
dstring_clear (DString *ds)
{
    ds->str_len = 0;
}

static Bool
dstring_ensure_space (DString *ds, int size)
{
    if (ds->alloc_size < size) {
        int   re_size = MAX_VAL (ds->alloc_size * 2, size);
        void *re_ptr = realloc (ds->val, re_size);
        if (!re_ptr)
            return FALSE;
        ds->val = re_ptr;
        ds->alloc_size = re_size;
    }

    return TRUE;
}

Bool
dstring_copy (DString *dst, const DString *src)
{
    if (!dstring_ensure_space (dst, (src->str_len + 1) * src->char_size))
        return FALSE;

    memcpy (dst->val, src->val, (src->str_len + 1) * src->char_size);

    dst->char_size = src->char_size;
    dst->str_len = src->str_len;

    return TRUE;
}

Bool
dstring_append (DString *dst, const DString *src)
{
    if (dst->char_size != src->char_size)
        return FALSE;

    if (!dstring_ensure_space (dst, (dst->str_len + src->str_len + 1)
                                    * dst->char_size))
    {
        return FALSE;
    }

    memcpy ((char *)dst->val + (dst->char_size * dst->str_len), src->val,
            (src->str_len + 1) * dst->char_size);

    dst->str_len += src->str_len;

    return TRUE;
}

Bool
dstring_append_string (DString *ds, const void *data, int len)
{
    if (!dstring_ensure_space (ds, (ds->str_len + len + 1) * ds->char_size))
        return FALSE;

    memcpy ((char  *)ds->val + (ds->char_size * ds->str_len), data,
            ds->char_size * len);

    ds->str_len += len;

    return TRUE;
}

Bool
dstring_append_char (DString *ds, const void *data)
{
    if (!dstring_ensure_space (ds, (ds->str_len + 2) * ds->char_size))
        return FALSE;

    memcpy ((char *)ds->val + (ds->char_size * ds->str_len), data,
            ds->char_size);

    ds->str_len++;

    return TRUE;
}

Bool
dstring_terminate (DString *ds)
{
    if (!dstring_ensure_space (ds, (ds->str_len + 2) * ds->char_size))
        return FALSE;

    memset ((char *)ds->val + (ds->char_size * ds->str_len), 0, ds->char_size);

    return TRUE;
}

Bool
dstring_cut_last (DString *ds)
{
    if (0 == ds->str_len)
        return FALSE;

    ds->str_len--;

    return TRUE;
}

/*
vi:ts=4:ai:expandtab
*/
