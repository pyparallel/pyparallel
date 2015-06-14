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
 * trie-string.h - Dynamic string type for Trie alphabets
 * Created: 2012-08-02
 * Author:  Theppitak Karoonboonyanan <thep@linux.thai.net>
 */

#ifndef __TRIE_STRING_H
#define __TRIE_STRING_H

#include "dstring.h"
#include "triedefs.h"

typedef struct _TrieString TrieString;

TrieString * trie_string_new (int n_elm);

void      trie_string_free (TrieString *ts);

int       trie_string_length (const TrieString *ts);

const void * trie_string_get_val (const TrieString *ts);

void *    trie_string_get_val_rw (TrieString *ts);

void      trie_string_clear (TrieString *ts);

Bool      trie_string_copy (TrieString *dst, const TrieString *src);

Bool      trie_string_append (TrieString *dst, const TrieString *src);

Bool      trie_string_append_string (TrieString *ts, const TrieChar *str);

Bool      trie_string_append_char (TrieString *ts, TrieChar tc);

Bool      trie_string_terminate (TrieString *ts);

Bool      trie_string_cut_last (TrieString *ts);


#endif  /* __TRIE_STRING_H */

/*
vi:ts=4:ai:expandtab
*/

