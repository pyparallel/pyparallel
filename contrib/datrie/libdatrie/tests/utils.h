/* -*- Mode: C; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
/*
 * libdatrie - Double-Array Trie Library
 * Copyright (C) 2013  Theppitak Karoonboonyanan <thep@linux.thai.net>
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
 * utils.h - Utility functions for datrie test cases
 * Created: 2013-10-16
 * Author:  Theppitak Karoonboonyanan <thep@linux.thai.net>
 */

#include <datrie/trie.h>

/*---------------------*
 *  Debugging helpers  *
 *---------------------*/
void msg_step (const char *msg);

/*-------------------------*
 *  Trie creation helpers  *
 *-------------------------*/
Trie * en_trie_new ();

/*---------------------------*
 *  Dict source for testing  *
 *---------------------------*/
typedef struct _DictRec DictRec;
struct _DictRec {
    AlphaChar *key;
    TrieData   data;
};

#define TRIE_DATA_UNREAD    1
#define TRIE_DATA_READ      2

extern DictRec dict_src[];

int      dict_src_n_entries ();
TrieData dict_src_get_data (const AlphaChar *key);
int      dict_src_set_data (const AlphaChar *key, TrieData data);

/*
vi:ts=4:ai:expandtab
*/
