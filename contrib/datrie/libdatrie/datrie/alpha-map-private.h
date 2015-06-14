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
 * alpha-map-private.h - private APIs for alpha-map
 * Created: 2008-12-04
 * Author:  Theppitak Karoonboonyanan <thep@linux.thai.net>
 */

#ifndef __ALPHA_MAP_PRIVATE_H
#define __ALPHA_MAP_PRIVATE_H

#include <stdio.h>
#include "alpha-map.h"

AlphaMap *  alpha_map_fread_bin (FILE *file);

int         alpha_map_fwrite_bin (const AlphaMap *alpha_map, FILE *file);

TrieIndex   alpha_map_char_to_trie (const AlphaMap *alpha_map,
                                    AlphaChar       ac);

AlphaChar   alpha_map_trie_to_char (const AlphaMap *alpha_map,
                                    TrieChar        tc);

TrieChar *  alpha_map_char_to_trie_str (const AlphaMap  *alpha_map,
                                        const AlphaChar *str);

AlphaChar * alpha_map_trie_to_char_str (const AlphaMap  *alpha_map,
                                        const TrieChar  *str);


#endif /* __ALPHA_MAP_PRIVATE_H */


/*
vi:ts=4:ai:expandtab
*/

