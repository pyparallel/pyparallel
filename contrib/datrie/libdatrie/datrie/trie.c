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
 * trie.c - Trie data type and functions
 * Created: 2006-08-11
 * Author:  Theppitak Karoonboonyanan <thep@linux.thai.net>
 */

#include <stdlib.h>
#include <string.h>

#include "trie.h"
#include "fileutils.h"
#include "alpha-map.h"
#include "alpha-map-private.h"
#include "darray.h"
#include "tail.h"
#include "trie-string.h"

/**
 * @brief Trie structure
 */
struct _Trie {
    AlphaMap   *alpha_map;
    DArray     *da;
    Tail       *tail;

    Bool        is_dirty;
};

/**
 * @brief TrieState structure
 */
struct _TrieState {
    const Trie *trie;       /**< the corresponding trie */
    TrieIndex   index;      /**< index in double-array/tail structures */
    short       suffix_idx; /**< suffix character offset, if in suffix */
    short       is_suffix;  /**< whether it is currently in suffix part */
};

/**
 * @brief TrieIterator structure
 */
struct _TrieIterator {
    const TrieState *root;  /**< the state to start iteration from */
    TrieState       *state; /**< the current state */
    TrieString      *key;   /**< buffer for calculating the entry key */
};


/*------------------------*
 *   INTERNAL FUNCTIONS   *
 *------------------------*/

#define trie_da_is_separate(da,s)      (da_get_base ((da), (s)) < 0)
#define trie_da_get_tail_index(da,s)   (-da_get_base ((da), (s)))
#define trie_da_set_tail_index(da,s,v) (da_set_base ((da), (s), -(v)))

static TrieState * trie_state_new (const Trie *trie,
                                   TrieIndex   index,
                                   short       suffix_idx,
                                   short       is_suffix);

static Bool        trie_store_conditionally (Trie            *trie,
                                             const AlphaChar *key,
                                             TrieData         data,
                                             Bool             is_overwrite);

static Bool        trie_branch_in_branch (Trie           *trie,
                                          TrieIndex       sep_node,
                                          const TrieChar *suffix,
                                          TrieData        data);

static Bool        trie_branch_in_tail   (Trie           *trie,
                                          TrieIndex       sep_node,
                                          const TrieChar *suffix,
                                          TrieData        data);

/*-----------------------*
 *   GENERAL FUNCTIONS   *
 *-----------------------*/

/**
 * @brief Create a new trie
 *
 * @param   alpha_map   : the alphabet set for the trie
 *
 * @return a pointer to the newly created trie, NULL on failure
 *
 * Create a new empty trie object based on the given @a alpha_map alphabet
 * set. The trie contents can then be added and deleted with trie_store() and
 * trie_delete() respectively.
 *
 * The created object must be freed with trie_free().
 */
Trie *
trie_new (const AlphaMap *alpha_map)
{
    Trie *trie;

    trie = (Trie *) malloc (sizeof (Trie));
    if (!trie)
        return NULL;

    trie->alpha_map = alpha_map_clone (alpha_map);
    if (!trie->alpha_map)
        goto exit_trie_created;

    trie->da = da_new ();
    if (!trie->da)
        goto exit_alpha_map_created;

    trie->tail = tail_new ();
    if (!trie->tail)
        goto exit_da_created;

    trie->is_dirty = TRUE;
    return trie;

exit_da_created:
    da_free (trie->da);
exit_alpha_map_created:
    alpha_map_free (trie->alpha_map);
exit_trie_created:
    free (trie);
    return NULL;
}

/**
 * @brief Create a new trie by loading from a file
 *
 * @param path  : the path to the file
 *
 * @return a pointer to the created trie, NULL on failure
 *
 * Create a new trie and initialize its contents by loading from the file at
 * given @a path.
 *
 * The created object must be freed with trie_free().
 */
Trie *
trie_new_from_file (const char *path)
{
    Trie       *trie;
    FILE       *trie_file;

    trie_file = fopen (path, "r");
    if (!trie_file)
        return NULL;

    trie = trie_fread (trie_file);
    fclose (trie_file);
    return trie;
}

/**
 * @brief Create a new trie by reading from an open file
 *
 * @param file  : the handle of the open file
 *
 * @return a pointer to the created trie, NULL on failure
 *
 * Create a new trie and initialize its contents by reading from the open
 * @a file. After reading, the file pointer is left at the end of the trie data.
 * This can be useful for reading embedded trie index as part of a file data.
 *
 * The created object must be freed with trie_free().
 *
 * Available since: 0.2.4
 */
Trie *
trie_fread (FILE *file)
{
    Trie       *trie;

    trie = (Trie *) malloc (sizeof (Trie));
    if (!trie)
        return NULL;

    if (NULL == (trie->alpha_map = alpha_map_fread_bin (file)))
        goto exit_trie_created;
    if (NULL == (trie->da   = da_fread (file)))
        goto exit_alpha_map_created;
    if (NULL == (trie->tail = tail_fread (file)))
        goto exit_da_created;

    trie->is_dirty = FALSE;
    return trie;

exit_da_created:
    da_free (trie->da);
exit_alpha_map_created:
    alpha_map_free (trie->alpha_map);
exit_trie_created:
    free (trie);
    return NULL;
}

/**
 * @brief Free a trie object
 *
 * @param trie  : the trie object to free
 *
 * Destruct the @a trie and free its allocated memory.
 */
void
trie_free (Trie *trie)
{
    alpha_map_free (trie->alpha_map);
    da_free (trie->da);
    tail_free (trie->tail);
    free (trie);
}

/**
 * @brief Save a trie to file
 *
 * @param trie  : the trie
 *
 * @param path  : the path to the file
 *
 * @return 0 on success, non-zero on failure
 *
 * Create a new file at the given @a path and write @a trie data to it.
 * If @a path already exists, its contents will be replaced.
 */
int
trie_save (Trie *trie, const char *path)
{
    FILE *file;
    int   res = 0;

    file = fopen (path, "w+");
    if (!file)
        return -1;

    res = trie_fwrite (trie, file);
    fclose (file);
    return res;
}

/**
 * @brief Write trie data to an open file
 *
 * @param trie  : the trie
 *
 * @param file  : the open file
 *
 * @return 0 on success, non-zero on failure
 *
 * Write @a trie data to @a file which is opened for writing.
 * After writing, the file pointer is left at the end of the trie data.
 * This can be useful for embedding trie index as part of a file data.
 *
 * Available since: 0.2.4
 */
int
trie_fwrite (Trie *trie, FILE *file)
{
    if (alpha_map_fwrite_bin (trie->alpha_map, file) != 0)
        return -1;

    if (da_fwrite (trie->da, file) != 0)
        return -1;

    if (tail_fwrite (trie->tail, file) != 0)
        return -1;

    trie->is_dirty = FALSE;

    return 0;
}

/**
 * @brief Check pending changes
 *
 * @param trie  : the trie object
 *
 * @return TRUE if there are pending changes, FALSE otherwise
 *
 * Check if the @a trie is dirty with some pending changes and needs saving
 * to synchronize with the file.
 */
Bool
trie_is_dirty (const Trie *trie)
{
    return trie->is_dirty;
}


/*------------------------------*
 *   GENERAL QUERY OPERATIONS   *
 *------------------------------*/

/**
 * @brief Retrieve an entry from trie
 *
 * @param trie   : the trie
 * @param key    : the key for the entry to retrieve
 * @param o_data : the storage for storing the entry data on return
 *
 * @return boolean value indicating the existence of the entry.
 *
 * Retrieve an entry for the given @a key from @a trie. On return,
 * if @a key is found and @a o_data is not NULL, @a *o_data is set
 * to the data associated to @a key.
 */
Bool
trie_retrieve (const Trie *trie, const AlphaChar *key, TrieData *o_data)
{
    TrieIndex        s;
    short            suffix_idx;
    const AlphaChar *p;

    /* walk through branches */
    s = da_get_root (trie->da);
    for (p = key; !trie_da_is_separate (trie->da, s); p++) {
        TrieIndex tc = alpha_map_char_to_trie (trie->alpha_map, *p);
        if (TRIE_INDEX_MAX == tc)
            return FALSE;
        if (!da_walk (trie->da, &s, (TrieChar) tc))
            return FALSE;
        if (0 == *p)
            break;
    }

    /* walk through tail */
    s = trie_da_get_tail_index (trie->da, s);
    suffix_idx = 0;
    for ( ; ; p++) {
        TrieIndex tc = alpha_map_char_to_trie (trie->alpha_map, *p);
        if (TRIE_INDEX_MAX == tc)
            return FALSE;
        if (!tail_walk_char (trie->tail, s, &suffix_idx, (TrieChar) tc))
            return FALSE;
        if (0 == *p)
            break;
    }

    /* found, set the val and return */
    if (o_data)
        *o_data = tail_get_data (trie->tail, s);
    return TRUE;
}

/**
 * @brief Store a value for an entry to trie
 *
 * @param trie  : the trie
 * @param key   : the key for the entry to retrieve
 * @param data  : the data associated to the entry
 *
 * @return boolean value indicating the success of the operation
 *
 * Store a @a data for the given @a key in @a trie. If @a key does not
 * exist in @a trie, it will be appended. If it does, its current data will
 * be overwritten.
 */
Bool
trie_store (Trie *trie, const AlphaChar *key, TrieData data)
{
    return trie_store_conditionally (trie, key, data, TRUE);
}

/**
 * @brief Store a value for an entry to trie only if the key is not present
 *
 * @param trie  : the trie
 * @param key   : the key for the entry to retrieve
 * @param data  : the data associated to the entry
 *
 * @return boolean value indicating the success of the operation
 *
 * Store a @a data for the given @a key in @a trie. If @a key does not
 * exist in @a trie, it will be appended. If it does, the function will
 * return failure and the existing value will not be touched.
 *
 * This can be useful for multi-thread applications, as race condition
 * can be avoided.
 *
 * Available since: 0.2.4
 */
Bool
trie_store_if_absent (Trie *trie, const AlphaChar *key, TrieData data)
{
    return trie_store_conditionally (trie, key, data, FALSE);
}

static Bool
trie_store_conditionally (Trie            *trie,
                          const AlphaChar *key,
                          TrieData         data,
                          Bool             is_overwrite)
{
    TrieIndex        s, t;
    short            suffix_idx;
    const AlphaChar *p, *sep;

    /* walk through branches */
    s = da_get_root (trie->da);
    for (p = key; !trie_da_is_separate (trie->da, s); p++) {
        TrieIndex tc = alpha_map_char_to_trie (trie->alpha_map, *p);
        if (TRIE_INDEX_MAX == tc)
            return FALSE;
        if (!da_walk (trie->da, &s, (TrieChar) tc)) {
            TrieChar *key_str;
            Bool      res;

            key_str = alpha_map_char_to_trie_str (trie->alpha_map, p);
            if (!key_str)
                return FALSE;
            res = trie_branch_in_branch (trie, s, key_str, data);
            free (key_str);

            return res;
        }
        if (0 == *p)
            break;
    }

    /* walk through tail */
    sep = p;
    t = trie_da_get_tail_index (trie->da, s);
    suffix_idx = 0;
    for ( ; ; p++) {
        TrieIndex tc = alpha_map_char_to_trie (trie->alpha_map, *p);
        if (TRIE_INDEX_MAX == tc)
            return FALSE;
        if (!tail_walk_char (trie->tail, t, &suffix_idx, (TrieChar) tc)) {
            TrieChar *tail_str;
            Bool      res;

            tail_str = alpha_map_char_to_trie_str (trie->alpha_map, sep);
            if (!tail_str)
                return FALSE;
            res = trie_branch_in_tail (trie, s, tail_str, data);
            free (tail_str);

            return res;
        }
        if (0 == *p)
            break;
    }

    /* duplicated key, overwrite val if flagged */
    if (!is_overwrite) {
        return FALSE;
    }
    tail_set_data (trie->tail, t, data);
    trie->is_dirty = TRUE;
    return TRUE;
}

static Bool
trie_branch_in_branch (Trie           *trie,
                       TrieIndex       sep_node,
                       const TrieChar *suffix,
                       TrieData        data)
{
    TrieIndex new_da, new_tail;

    new_da = da_insert_branch (trie->da, sep_node, *suffix);
    if (TRIE_INDEX_ERROR == new_da)
        return FALSE;

    if ('\0' != *suffix)
        ++suffix;

    new_tail = tail_add_suffix (trie->tail, suffix);
    tail_set_data (trie->tail, new_tail, data);
    trie_da_set_tail_index (trie->da, new_da, new_tail);

    trie->is_dirty = TRUE;
    return TRUE;
}

static Bool
trie_branch_in_tail   (Trie           *trie,
                       TrieIndex       sep_node,
                       const TrieChar *suffix,
                       TrieData        data)
{
    TrieIndex       old_tail, old_da, s;
    const TrieChar *old_suffix, *p;

    /* adjust separate point in old path */
    old_tail = trie_da_get_tail_index (trie->da, sep_node);
    old_suffix = tail_get_suffix (trie->tail, old_tail);
    if (!old_suffix)
        return FALSE;

    for (p = old_suffix, s = sep_node; *p == *suffix; p++, suffix++) {
        TrieIndex t = da_insert_branch (trie->da, s, *p);
        if (TRIE_INDEX_ERROR == t)
            goto fail;
        s = t;
    }

    old_da = da_insert_branch (trie->da, s, *p);
    if (TRIE_INDEX_ERROR == old_da)
        goto fail;

    if ('\0' != *p)
        ++p;
    tail_set_suffix (trie->tail, old_tail, p);
    trie_da_set_tail_index (trie->da, old_da, old_tail);

    /* insert the new branch at the new separate point */
    return trie_branch_in_branch (trie, s, suffix, data);

fail:
    /* failed, undo previous insertions and return error */
    da_prune_upto (trie->da, sep_node, s);
    trie_da_set_tail_index (trie->da, sep_node, old_tail);
    return FALSE;
}

/**
 * @brief Delete an entry from trie
 *
 * @param trie  : the trie
 * @param key   : the key for the entry to delete
 *
 * @return boolean value indicating whether the key exists and is removed
 *
 * Delete an entry for the given @a key from @a trie.
 */
Bool
trie_delete (Trie *trie, const AlphaChar *key)
{
    TrieIndex        s, t;
    short            suffix_idx;
    const AlphaChar *p;

    /* walk through branches */
    s = da_get_root (trie->da);
    for (p = key; !trie_da_is_separate (trie->da, s); p++) {
        TrieIndex tc = alpha_map_char_to_trie (trie->alpha_map, *p);
        if (TRIE_INDEX_MAX == tc)
            return FALSE;
        if (!da_walk (trie->da, &s, (TrieChar) tc))
            return FALSE;
        if (0 == *p)
            break;
    }

    /* walk through tail */
    t = trie_da_get_tail_index (trie->da, s);
    suffix_idx = 0;
    for ( ; ; p++) {
        TrieIndex tc = alpha_map_char_to_trie (trie->alpha_map, *p);
        if (TRIE_INDEX_MAX == tc)
            return FALSE;
        if (!tail_walk_char (trie->tail, t, &suffix_idx, (TrieChar) tc))
            return FALSE;
        if (0 == *p)
            break;
    }

    tail_delete (trie->tail, t);
    da_set_base (trie->da, s, TRIE_INDEX_ERROR);
    da_prune (trie->da, s);

    trie->is_dirty = TRUE;
    return TRUE;
}

/**
 * @brief Enumerate entries in trie
 *
 * @param trie       : the trie
 * @param enum_func  : the callback function to be called on each key
 * @param user_data  : user-supplied data to send as an argument to @a enum_func
 *
 * @return boolean value indicating whether all the keys are visited
 *
 * Enumerate all entries in trie. For each entry, the user-supplied
 * @a enum_func callback function is called, with the entry key and data.
 * Returning FALSE from such callback will stop enumeration and return FALSE.
 */
Bool
trie_enumerate (const Trie *trie, TrieEnumFunc enum_func, void *user_data)
{
    TrieState      *root;
    TrieIterator   *iter;
    Bool            cont = TRUE;

    root = trie_root (trie);
    if (!root)
        return FALSE;

    iter = trie_iterator_new (root);
    if (!iter)
        goto exit_root_created;

    while (cont && trie_iterator_next (iter)) {
        AlphaChar *key = trie_iterator_get_key (iter);
        TrieData   data = trie_iterator_get_data (iter);
        cont = (*enum_func) (key, data, user_data);
        free (key);
    }

    trie_iterator_free (iter);
    trie_state_free (root);

    return cont;

exit_root_created:
    trie_state_free (root);
    return FALSE;
}


/*-------------------------------*
 *   STEPWISE QUERY OPERATIONS   *
 *-------------------------------*/

/**
 * @brief Get root state of a trie
 *
 * @param trie : the trie
 *
 * @return the root state of the trie
 *
 * Get root state of @a trie, for stepwise walking.
 *
 * The returned state is allocated and must be freed with trie_state_free()
 */
TrieState *
trie_root (const Trie *trie)
{
    return trie_state_new (trie, da_get_root (trie->da), 0, FALSE);
}

/*----------------*
 *   TRIE STATE   *
 *----------------*/

static TrieState *
trie_state_new (const Trie *trie,
                TrieIndex   index,
                short       suffix_idx,
                short       is_suffix)
{
    TrieState *s;

    s = (TrieState *) malloc (sizeof (TrieState));
    if (!s)
        return NULL;

    s->trie       = trie;
    s->index      = index;
    s->suffix_idx = suffix_idx;
    s->is_suffix  = is_suffix;

    return s;
}

/**
 * @brief Copy trie state to another
 *
 * @param dst  : the destination state
 * @param src  : the source state
 *
 * Copy trie state data from @a src to @a dst. All existing data in @a dst
 * is overwritten.
 */
void
trie_state_copy (TrieState *dst, const TrieState *src)
{
    /* May be deep copy if necessary, not the case for now */
    *dst = *src;
}

/**
 * @brief Clone a trie state
 *
 * @param s    : the state to clone
 *
 * @return an duplicated instance of @a s
 *
 * Make a copy of trie state.
 *
 * The returned state is allocated and must be freed with trie_state_free()
 */
TrieState *
trie_state_clone (const TrieState *s)
{
    return trie_state_new (s->trie, s->index, s->suffix_idx, s->is_suffix);
}

/**
 * @brief Free a trie state
 *
 * @param s    : the state to free
 *
 * Free the trie state.
 */
void
trie_state_free (TrieState *s)
{
    free (s);
}

/**
 * @brief Rewind a trie state
 *
 * @param s    : the state to rewind
 *
 * Put the state at root.
 */
void
trie_state_rewind (TrieState *s)
{
    s->index      = da_get_root (s->trie->da);
    s->is_suffix  = FALSE;
}

/**
 * @brief Walk the trie from the state
 *
 * @param s    : current state
 * @param c    : key character for walking
 *
 * @return boolean value indicating the success of the walk
 *
 * Walk the trie stepwise, using a given character @a c.
 * On return, the state @a s is updated to the new state if successfully walked.
 */
Bool
trie_state_walk (TrieState *s, AlphaChar c)
{
    TrieIndex tc = alpha_map_char_to_trie (s->trie->alpha_map, c);
    if (TRIE_INDEX_MAX == tc)
        return FALSE;

    if (!s->is_suffix) {
        Bool ret;

        ret = da_walk (s->trie->da, &s->index, (TrieChar) tc);

        if (ret && trie_da_is_separate (s->trie->da, s->index)) {
            s->index = trie_da_get_tail_index (s->trie->da, s->index);
            s->suffix_idx = 0;
            s->is_suffix = TRUE;
        }

        return ret;
    } else {
        return tail_walk_char (s->trie->tail, s->index, &s->suffix_idx,
                               (TrieChar) tc);
    }
}

/**
 * @brief Test walkability of character from state
 *
 * @param s    : the state to check
 * @param c    : the input character
 *
 * @return boolean indicating walkability
 *
 * Test if there is a transition from state @a s with input character @a c.
 */
Bool
trie_state_is_walkable (const TrieState *s, AlphaChar c)
{
    TrieIndex tc = alpha_map_char_to_trie (s->trie->alpha_map, c);
    if (TRIE_INDEX_MAX == tc)
        return FALSE;

    if (!s->is_suffix)
        return da_is_walkable (s->trie->da, s->index, (TrieChar) tc);
    else
        return tail_is_walkable_char (s->trie->tail, s->index, s->suffix_idx,
                                      (TrieChar) tc);
}

/**
 * @brief Get all walkable characters from state
 *
 * @param s     : the state to get
 * @param chars : the storage for the result
 * @param chars_nelm : the size of @a chars[] in number of elements
 *
 * @return total walkable characters
 *
 * Get the list of all walkable characters from state @a s. At most
 * @a chars_nelm walkable characters are stored in @a chars[] on return.
 *
 * The function returns the actual number of walkable characters from @a s.
 * Note that this may not equal the number of characters stored in @a chars[]
 * if @a chars_nelm is less than the actual number.
 *
 * Available since: 0.2.6
 */
int
trie_state_walkable_chars (const TrieState  *s,
                           AlphaChar         chars[],
                           int               chars_nelm)
{
    int syms_num = 0;

    if (!s->is_suffix) {
        Symbols *syms = da_output_symbols (s->trie->da, s->index);
        int i;

        syms_num = symbols_num (syms);
        for (i = 0; i < syms_num && i < chars_nelm; i++) {
            TrieChar tc = symbols_get (syms, i);
            chars[i] = alpha_map_trie_to_char (s->trie->alpha_map, tc);
        }

        symbols_free (syms);
    } else {
        const TrieChar *suffix = tail_get_suffix (s->trie->tail, s->index);
        chars[0] = alpha_map_trie_to_char (s->trie->alpha_map,
                                           suffix[s->suffix_idx]);
        syms_num = 1;
    }

    return syms_num;
}

/**
 * @brief Check for single path
 *
 * @param s    : the state to check
 *
 * @return boolean value indicating whether it is in a single path
 *
 * Check if the given state is in a single path, that is, there is no other
 * branch from it to leaf.
 */
Bool
trie_state_is_single (const TrieState *s)
{
    return s->is_suffix;
}

/**
 * @brief Get data from leaf state
 *
 * @param s    : a leaf state
 *
 * @return the data associated with the leaf state @a s,
 *         or TRIE_DATA_ERROR if @a s is not a leaf state
 *
 * Get value from a leaf state of trie. Getting value from a non-leaf state
 * will result in TRIE_DATA_ERROR.
 */
TrieData
trie_state_get_data (const TrieState *s)
{
    return trie_state_is_leaf (s) ? tail_get_data (s->trie->tail, s->index)
                                  : TRIE_DATA_ERROR;
}

/**
 * @brief Get data from terminal state
 *
 * @param s    : a terminal state
 *
 * @return the data associated with the terminal state @a s,
 *         or TRIE_DATA_ERROR if @a s is not a terminal state
 *
 */
TrieData
trie_state_get_terminal_data (const TrieState *s)
{
    TrieIndex        tail_index;
    TrieIndex index = s->index;

    if (!s)
        return TRIE_DATA_ERROR;

    if (!s->is_suffix){
        if (!trie_da_is_separate(s->trie->da, index)) {
            /* walk to a terminal char to get the data */
            Bool ret = da_walk (s->trie->da, &index, TRIE_CHAR_TERM);
            if (!ret) {
                return TRIE_DATA_ERROR;
            }
        }
        tail_index = trie_da_get_tail_index (s->trie->da, index);
    }
    else {
        tail_index = s->index;
    }

    return tail_get_data (s->trie->tail, tail_index);
}


/*---------------------*
 *   ENTRY ITERATION   *
 *---------------------*/

/**
 * @brief Create a new trie iterator
 *
 * @param  s  : the TrieState to start iteration from
 *
 * @return a pointer to the newly created TrieIterator, or NULL on failure
 *
 * Create a new trie iterator for iterating entries of a sub-trie rooted at
 * state @a s.
 *
 * Use it with the result of trie_root() to iterate the whole trie.
 *
 * The created object must be freed with trie_iterator_free().
 *
 * Available since: 0.2.6
 */
TrieIterator *
trie_iterator_new (TrieState *s)
{
    TrieIterator *iter;

    iter = (TrieIterator *) malloc (sizeof (TrieIterator));
    if (!iter)
        return NULL;

    iter->root = s;
    iter->state = NULL;
    iter->key = NULL;

    return iter;
}

/**
 * @brief Free a trie iterator
 *
 * @param  iter  : the trie iterator to free
 *
 * Destruct the iterator @a iter and free its allocated memory.
 *
 * Available since: 0.2.6
 */
void
trie_iterator_free (TrieIterator *iter)
{
    if (iter->state) {
        trie_state_free (iter->state);
    }
    if (iter->key) {
        trie_string_free (iter->key);
    }
    free (iter);
}

/**
 * @brief Move trie iterator to the next entry
 *
 * @param  iter  : an iterator
 *
 * @return boolean value indicating the availability of the entry
 *
 * Move trie iterator to the next entry.
 * On return, the iterator @a iter is updated to reference to the new entry
 * if successfully moved.
 *
 * Available since: 0.2.6
 */
Bool
trie_iterator_next (TrieIterator *iter)
{
    TrieState *s = iter->state;
    TrieIndex sep;

    /* first iteration */
    if (!s) {
        s = iter->state = trie_state_clone (iter->root);

        /* for tail state, we are already at the only entry */
        if (s->is_suffix)
            return TRUE;

        iter->key = trie_string_new (20);
        sep = da_first_separate (s->trie->da, s->index, iter->key);
        if (TRIE_INDEX_ERROR == sep)
            return FALSE;

        s->index = sep;
        return TRUE;
    }

    /* no next entry for tail state */
    if (s->is_suffix)
        return FALSE;

    /* iter->state is a separate node */
    sep = da_next_separate (s->trie->da, iter->root->index, s->index,
                            iter->key);
    if (TRIE_INDEX_ERROR == sep)
        return FALSE;

    s->index = sep;
    return TRUE;
}

/**
 * @brief Get key for a trie iterator
 *
 * @param  iter      : an iterator
 *
 * @return the allocated key string; NULL on failure
 *
 * Get key for the current entry referenced by the trie iterator @a iter.
 *
 * The return string must be freed with free().
 *
 * Available since: 0.2.6
 */
AlphaChar *
trie_iterator_get_key (const TrieIterator *iter)
{
    const TrieState *s;
    const TrieChar  *tail_str;
    AlphaChar       *alpha_key, *alpha_p;

    s = iter->state;
    if (!s)
        return NULL;

    /* if s is in tail, root == s */
    if (s->is_suffix) {
        tail_str = tail_get_suffix (s->trie->tail, s->index);
        if (!tail_str)
            return NULL;

        tail_str += s->suffix_idx;

        alpha_key = (AlphaChar *) malloc (sizeof (AlphaChar)
                                          * (strlen ((const char *)tail_str)
                                             + 1));
        alpha_p = alpha_key;
    } else {
        TrieIndex  tail_idx;
        int        i, key_len;
        const TrieChar  *key_p;

        tail_idx = trie_da_get_tail_index (s->trie->da, s->index);
        tail_str = tail_get_suffix (s->trie->tail, tail_idx);
        if (!tail_str)
            return NULL;

        key_len = trie_string_length (iter->key);
        key_p = trie_string_get_val (iter->key);
        alpha_key = (AlphaChar *) malloc (
                        sizeof (AlphaChar)
                        * (key_len + strlen ((const char *)tail_str) + 1)
                    );
        alpha_p = alpha_key;
        for (i = key_len; i > 0; i--) {
            *alpha_p++ = alpha_map_trie_to_char (s->trie->alpha_map, *key_p++);
        }
    }

    while (*tail_str) {
        *alpha_p++ = alpha_map_trie_to_char (s->trie->alpha_map, *tail_str++);
    }
    *alpha_p = 0;

    return alpha_key;
}

/**
 * @brief Get data for the entry referenced by an iterator
 *
 * @param iter  : an iterator
 *
 * @return the data associated with the entry referenced by iterator @a iter,
 *         or TRIE_DATA_ERROR if @a iter does not reference to a unique entry
 *
 * Get value for the entry referenced by an iterator. Getting value from an
 * un-iterated (or broken for any reason) iterator will result in
 * TRIE_DATA_ERROR.
 *
 * Available since: 0.2.6
 */
TrieData
trie_iterator_get_data (const TrieIterator *iter)
{
    const TrieState *s = iter->state;
    TrieIndex        tail_index;

    if (!s)
        return TRIE_DATA_ERROR;

    if (!s->is_suffix) {
        if (!trie_da_is_separate (s->trie->da, s->index))
            return TRIE_DATA_ERROR;

        tail_index = trie_da_get_tail_index (s->trie->da, s->index);
    } else {
        tail_index = s->index;
    }

    return tail_get_data (s->trie->tail, tail_index);
}

/*
vi:ts=4:ai:expandtab
*/
