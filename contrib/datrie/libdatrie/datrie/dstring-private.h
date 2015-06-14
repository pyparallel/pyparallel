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
 * dstring-private.h - Dynamic string type
 * Created: 2012-08-02
 * Author:  Theppitak Karoonboonyanan <thep@linux.thai.net>
 */

#ifndef __DSTRING_PRIVATE_H
#define __DSTRING_PRIVATE_H

#include "typedefs.h"


struct _DString {
    int    char_size;
    int    str_len;
    int    alloc_size;
    void * val;
};


#endif  /* __DSTRING_PRIVATE_H */

/*
vi:ts=4:ai:expandtab
*/
