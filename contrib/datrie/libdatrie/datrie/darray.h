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
 * darray.h - Double-array trie structure
 * Created: 2006-08-11
 * Author:  Theppitak Karoonboonyanan <thep@linux.thai.net>
 */

#ifndef __DARRAY_H
#define __DARRAY_H

#include "triedefs.h"
#include "trie-string.h"

/**
 * @file darray.h
 * @brief Double-array trie structure
 */

/**
 * @brief Symbol set structure type
 */
typedef struct _Symbols Symbols;

Symbols *    symbols_new ();
void         symbols_free (Symbols *syms);
void         symbols_add (Symbols *syms, TrieChar c);
int          symbols_num (const Symbols *syms);
TrieChar     symbols_get (const Symbols *syms, int index);

/**
 * @brief Double-array structure type
 */
typedef struct _DArray  DArray;


DArray * da_new ();

DArray * da_fread (FILE *file);

void     da_free (DArray *d);

int      da_fwrite (const DArray *d, FILE *file);


TrieIndex  da_get_root (const DArray *d);


TrieIndex  da_get_base (const DArray *d, TrieIndex s);

TrieIndex  da_get_check (const DArray *d, TrieIndex s);


void       da_set_base (DArray *d, TrieIndex s, TrieIndex val);

void       da_set_check (DArray *d, TrieIndex s, TrieIndex val);

Bool       da_walk (const DArray *d, TrieIndex *s, TrieChar c);

Symbols *  da_output_symbols  (const DArray *d, TrieIndex s);

/**
 * @brief Test walkability in double-array structure
 *
 * @param d : the double-array structure
 * @param s : current state
 * @param c : the input character
 *
 * @return boolean indicating walkability
 *
 * Test if there is a transition from state @a s with input character @a c.
 */
/*
Bool       da_is_walkable (DArray *d, TrieIndex s, TrieChar c);
*/
#define    da_is_walkable(d,s,c) \
    (da_get_check ((d), da_get_base ((d), (s)) + (c)) == (s))

TrieIndex  da_insert_branch (DArray *d, TrieIndex s, TrieChar c);

void       da_prune (DArray *d, TrieIndex s);

void       da_prune_upto (DArray *d, TrieIndex p, TrieIndex s);

TrieIndex  da_first_separate (DArray *d, TrieIndex root, TrieString *keybuff);

TrieIndex  da_next_separate (DArray     *d,
                             TrieIndex   root,
                             TrieIndex   sep,
                             TrieString *keybuff);

#endif  /* __DARRAY_H */

/*
vi:ts=4:ai:expandtab
*/
