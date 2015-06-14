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
 * alpha-map.c - map between character codes and trie alphabet
 * Created: 2006-08-19
 * Author:  Theppitak Karoonboonyanan <thep@linux.thai.net>
 */

#include <ctype.h>
#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include <assert.h>

#include "alpha-map.h"
#include "alpha-map-private.h"
#include "fileutils.h"

/**
 * @brief Alphabet string length
 *
 * @param str   : the array of null-terminated AlphaChar string to measure
 *
 * @return the total characters in @a str.
 */
int
alpha_char_strlen (const AlphaChar *str)
{
    const AlphaChar *p;

    for (p = str; *p; p++)
        ;
    return p - str;
}

/**
 * @brief Compare alphabet strings
 *
 * @param str1, str2  : the arrays of null-terminated AlphaChar strings
 *                      to compare
 *
 * @return negative if @a str1 < @a str2;
 *         0 if @a str1 == @a str2; 
 *         positive if @a str1 > @a str2
 *
 * Available since: 0.2.7
 */
int
alpha_char_strcmp (const AlphaChar *str1, const AlphaChar *str2)
{
    while (*str1 && *str1 == *str2) {
        str1++; str2++;
    }
    if (*str1 < *str2)
        return -1;
    if (*str1 > *str2)
        return 1;
    return 0;
}

/*------------------------------*
 *    PRIVATE DATA DEFINITONS   *
 *------------------------------*/

typedef struct _AlphaRange {
    struct _AlphaRange *next;

    AlphaChar           begin;
    AlphaChar           end;
} AlphaRange;

struct _AlphaMap {
    AlphaRange     *first_range;
};

/*-----------------------------------*
 *    PRIVATE METHODS DECLARATIONS   *
 *-----------------------------------*/
static int  alpha_map_get_total_ranges (const AlphaMap *alpha_map);

/*-----------------------------*
 *    METHODS IMPLEMENTAIONS   *
 *-----------------------------*/

#define ALPHAMAP_SIGNATURE  0xD9FCD9FC

/* AlphaMap Header:
 * - INT32: signature
 * - INT32: total ranges
 *
 * Ranges:
 * - INT32: range begin
 * - INT32: range end
 */

/**
 * @brief Create new alphabet map
 *
 * @return a pointer to the newly created alphabet map, NULL on failure
 *
 * Create a new empty alphabet map. The map contents can then be added with
 * alpha_map_add_range().
 *
 *  The created object must be freed with alpha_map_free().
 */
AlphaMap *
alpha_map_new ()
{
    AlphaMap   *alpha_map;

    alpha_map = (AlphaMap *) malloc (sizeof (AlphaMap));
    if (!alpha_map)
        return NULL;

    alpha_map->first_range = NULL;

    return alpha_map;
}

/**
 * @brief Create a clone of alphabet map
 *
 * @param a_map : the source alphabet map to clone
 *
 * @return a pointer to the alphabet map clone, NULL on failure
 *
 *  The created object must be freed with alpha_map_free().
 */
AlphaMap *
alpha_map_clone (const AlphaMap *a_map)
{
    AlphaMap   *alpha_map;
    AlphaRange *range;

    alpha_map = alpha_map_new ();
    if (!alpha_map)
        return NULL;

    for (range = a_map->first_range; range; range = range->next) {
        if (alpha_map_add_range (alpha_map, range->begin, range->end) != 0) {
            alpha_map_free (alpha_map);
            return NULL;
        }
    }

    return alpha_map;
}

/**
 * @brief Free an alphabet map object
 *
 * @param alpha_map : the alphabet map object to free
 *
 * Destruct the @a alpha_map and free its allocated memory.
 */
void
alpha_map_free (AlphaMap *alpha_map)
{
    AlphaRange *p, *q;

    p = alpha_map->first_range;
    while (p) {
        q = p->next;
        free (p);
        p = q;
    }

    free (alpha_map);
}

AlphaMap *
alpha_map_fread_bin (FILE *file)
{
    long        save_pos;
    uint32      sig;
    int32       total, i;
    AlphaMap   *alpha_map;

    /* check signature */
    save_pos = ftell (file);
    if (!file_read_int32 (file, (int32 *) &sig) || ALPHAMAP_SIGNATURE != sig)
        goto exit_file_read;

    if (NULL == (alpha_map = alpha_map_new ()))
        goto exit_file_read;

    /* read number of ranges */
    if (!file_read_int32 (file, &total))
        goto exit_map_created;

    /* read character ranges */
    for (i = 0; i < total; i++) {
        int32   b, e;

        if (!file_read_int32 (file, &b) || !file_read_int32 (file, &e))
            goto exit_map_created;
        alpha_map_add_range (alpha_map, b, e);
    }

    return alpha_map;

exit_map_created:
    alpha_map_free (alpha_map);
exit_file_read:
    fseek (file, save_pos, SEEK_SET);
    return NULL;
}

static int
alpha_map_get_total_ranges (const AlphaMap *alpha_map)
{
    int         n;
    AlphaRange *range;

    for (n = 0, range = alpha_map->first_range; range; range = range->next) {
        ++n;
    }

    return n;
}

int
alpha_map_fwrite_bin (const AlphaMap *alpha_map, FILE *file)
{
    AlphaRange *range;

    if (!file_write_int32 (file, ALPHAMAP_SIGNATURE) ||
        !file_write_int32 (file, alpha_map_get_total_ranges (alpha_map)))
    {
        return -1;
    }

    for (range = alpha_map->first_range; range; range = range->next) {
        if (!file_write_int32 (file, range->begin) ||
            !file_write_int32 (file, range->end))
        {
            return -1;
        }
    }

    return 0;
}

/**
 * @brief Add a range to alphabet map
 *
 * @param alpha_map : the alphabet map object
 * @param begin     : the first character of the range
 * @param end       : the last character of the range
 *
 * @return 0 on success, non-zero on failure
 *
 * Add a range of character codes from @a begin to @a end to the
 * alphabet set.
 */
int
alpha_map_add_range (AlphaMap *alpha_map, AlphaChar begin, AlphaChar end)
{
    AlphaRange *q, *r, *begin_node, *end_node;

    if (begin > end)
        return -1;

    begin_node = end_node = 0;

    /* Skip first ranges till 'begin' is covered */
    for (q = 0, r = alpha_map->first_range;
         r && r->begin <= begin;
         q = r, r = r->next)
    {
        if (begin <= r->end) {
            /* 'r' covers 'begin' -> take 'r' as beginning point */
            begin_node = r;
            break;
        }
        if (r->end + 1 == begin) {
            /* 'begin' is next to 'r'-end
             * -> extend 'r'-end to cover 'begin'
             */
            r->end = begin;
            begin_node = r;
            break;
        }
    }
    if (!begin_node && r && r->begin <= end + 1) {
        /* ['begin', 'end'] overlaps into 'r'-begin
         * or 'r' is next to 'end' if r->begin == end + 1
         * -> extend 'r'-begin to include the range
         */
        r->begin = begin;
        begin_node = r;
    }
    /* Run upto the first range that exceeds 'end' */
    while (r && r->begin <= end + 1) {
        if (end <= r->end) {
            /* 'r' covers 'end' -> take 'r' as ending point */
            end_node = r;
        } else if (r != begin_node) {
            /* ['begin', 'end'] covers the whole 'r' -> remove 'r' */
            if (q) {
                q->next = r->next;
                free (r);
                r = q->next;
            } else {
                alpha_map->first_range = r->next;
                free (r);
                r = alpha_map->first_range;
            }
            continue;
        }
        q = r;
        r = r->next;
    }
    if (!end_node && q && begin <= q->end) {
        /* ['begin', 'end'] overlaps 'q' at the end
         * -> extend 'q'-end to include the range
         */
        q->end = end;
        end_node = q;
    }

    if (begin_node && end_node) {
        if (begin_node != end_node) {
            /* Merge begin_node and end_node ranges together */
            assert (begin_node->next == end_node);
            begin_node->end = end_node->end;
            begin_node->next = end_node->next;
            free (end_node);
        }
    } else if (!begin_node && !end_node) {
        /* ['begin', 'end'] overlaps with none of the ranges
         * -> insert a new range
         */
        AlphaRange *range = (AlphaRange *) malloc (sizeof (AlphaRange));

        if (!range)
            return -1;

        range->begin = begin;
        range->end   = end;

        /* insert it between 'q' and 'r' */
        if (q) {
            q->next = range;
        } else {
            alpha_map->first_range = range;
        }
        range->next = r;
    }

    return 0;
}

TrieIndex
alpha_map_char_to_trie (const AlphaMap *alpha_map, AlphaChar ac)
{
    TrieIndex   alpha_begin;
    AlphaRange *range;

    if (0 == ac)
        return 0;

    alpha_begin = 1;
    for (range = alpha_map->first_range; range; range = range->next) {
        if (range->begin <= ac && ac <= range->end)
            return alpha_begin + (ac - range->begin);

        alpha_begin += range->end - range->begin + 1;
    }

    return TRIE_INDEX_MAX;
}

AlphaChar
alpha_map_trie_to_char (const AlphaMap *alpha_map, TrieChar tc)
{
    TrieChar    alpha_begin;
    AlphaRange *range;

    if (0 == tc)
        return 0;

    alpha_begin = 1;
    for (range = alpha_map->first_range; range; range = range->next) {
        if (tc <= alpha_begin + (range->end - range->begin))
            return range->begin + (tc - alpha_begin);

        alpha_begin += range->end - range->begin + 1;
    }

    return ALPHA_CHAR_ERROR;
}

TrieChar *
alpha_map_char_to_trie_str (const AlphaMap *alpha_map, const AlphaChar *str)
{
    TrieChar   *trie_str, *p;

    trie_str = (TrieChar *) malloc (alpha_char_strlen (str) + 1);
    if (!trie_str)
        return NULL;

    for (p = trie_str; *str; p++, str++) {
        TrieIndex tc = alpha_map_char_to_trie (alpha_map, *str);
        if (TRIE_INDEX_MAX == tc)
            goto error_str_allocated;
        *p = (TrieChar) tc;
    }
    *p = 0;

    return trie_str;

error_str_allocated:
    free (trie_str);
    return NULL;
}

AlphaChar *
alpha_map_trie_to_char_str (const AlphaMap *alpha_map, const TrieChar *str)
{
    AlphaChar  *alpha_str, *p;

    alpha_str = (AlphaChar *) malloc ((strlen ((const char *)str) + 1)
                                      * sizeof (AlphaChar));
    if (!alpha_str)
        return NULL;

    for (p = alpha_str; *str; p++, str++) {
        *p = (AlphaChar) alpha_map_trie_to_char (alpha_map, *str);
    }
    *p = 0;

    return alpha_str;
}

/*
vi:ts=4:ai:expandtab
*/
