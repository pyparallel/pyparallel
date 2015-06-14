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
 * test_file.c - Test for datrie file operations
 * Created: 2013-10-16
 * Author:  Theppitak Karoonboonyanan <thep@linux.thai.net>
 */

#include <datrie/trie.h>
#include "utils.h"
#include <stdio.h>
#include <unistd.h>

#define TRIE_FILENAME "test.tri"

static Bool
trie_enum_mark_rec (const AlphaChar *key, TrieData key_data, void *user_data)
{
    Bool *is_failed = (Bool *)user_data;
    TrieData src_data;

    src_data = dict_src_get_data (key);
    if (TRIE_DATA_ERROR == src_data) {
        printf ("Extra entry in file: key '%ls', data %d.\n", key, key_data);
        *is_failed = TRUE;
    } else if (src_data != key_data) {
        printf ("Data mismatch for: key '%ls', expected %d, got %d.\n",
                key, src_data, key_data);
        *is_failed = TRUE;
    } else {
        dict_src_set_data (key, TRIE_DATA_READ);
    }

    return TRUE;
}

int
main ()
{
    Trie    *test_trie;
    DictRec *dict_p;
    Bool     is_failed;

    msg_step ("Preparing trie");
    test_trie = en_trie_new ();
    if (!test_trie) {
        printf ("Failed to allocate test trie.\n");
        goto err_trie_not_created;
    }

    /* add/remove some words */
    for (dict_p = dict_src; dict_p->key; dict_p++) {
        if (!trie_store (test_trie, dict_p->key, dict_p->data)) {
            printf ("Failed to add key '%ls', data %d.\n",
                    dict_p->key, dict_p->data);
            goto err_trie_created;
        }
    }

    /* save & close */
    msg_step ("Saving trie to file");
    unlink (TRIE_FILENAME);  /* error ignored */
    if (trie_save (test_trie, TRIE_FILENAME) != 0) {
        printf ("Failed to save trie to file '%s'.\n", TRIE_FILENAME);
        goto err_trie_created;
    }
    trie_free (test_trie);

    /* reload from file */
    msg_step ("Reloading trie from the saved file");
    test_trie = trie_new_from_file (TRIE_FILENAME);
    if (!test_trie) {
        printf ("Failed to reload saved trie from '%s'.\n",
                 TRIE_FILENAME);
        goto err_trie_saved;
    }

    /* enumerate & check */
    msg_step ("Checking trie contents");
    is_failed = FALSE;
    /* mark entries found in file */
    if (!trie_enumerate (test_trie, trie_enum_mark_rec, (void *)&is_failed)) {
        printf ("Failed to enumerate trie file contents.\n");
        goto err_trie_saved;
    }
    /* check for unmarked entries, (i.e. missed in file) */
    for (dict_p = dict_src; dict_p->key; dict_p++) {
        if (dict_p->data != TRIE_DATA_READ) {
            printf ("Entry missed in file: key '%ls', data %d.\n",
                    dict_p->key, dict_p->data);
            is_failed = TRUE;
        }
    }
    if (is_failed) {
        printf ("Errors found in trie saved contents.\n");
        goto err_trie_saved;
    }

    unlink (TRIE_FILENAME);
    trie_free (test_trie);
    return 0;

err_trie_saved:
    unlink (TRIE_FILENAME);
err_trie_created:
    trie_free (test_trie);
err_trie_not_created:
    return 1;
}

/*
vi:ts=4:ai:expandtab
*/
