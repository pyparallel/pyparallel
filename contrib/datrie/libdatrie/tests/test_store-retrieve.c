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
 * test_store-retrieve.c - Test for datrie store/retrieve operations
 * Created: 2013-10-16
 * Author:  Theppitak Karoonboonyanan <thep@linux.thai.net>
 */

#include <datrie/trie.h>
#include "utils.h"
#include <stdio.h>
#include <stdlib.h>
#include <time.h>

int
main ()
{
    Trie         *test_trie;
    DictRec      *dict_p;
    TrieData      trie_data;
    Bool          is_failed;
    int           n_entries, n_dels, i;
    TrieState    *trie_root_state;
    TrieIterator *trie_it;

    msg_step ("Preparing trie");
    test_trie = en_trie_new ();
    if (!test_trie) {
        fprintf (stderr, "Fail to create test trie\n");
        goto err_trie_not_created;
    }

    /* store */
    msg_step ("Adding data to trie");
    for (dict_p = dict_src; dict_p->key; dict_p++) {
        if (!trie_store (test_trie, dict_p->key, dict_p->data)) {
            printf ("Failed to add key '%ls', data %d.\n",
                    dict_p->key, dict_p->data);
            goto err_trie_created;
        }
    }

    /* retrieve */
    msg_step ("Retrieving data from trie");
    is_failed = FALSE;
    for (dict_p = dict_src; dict_p->key; dict_p++) {
        if (!trie_retrieve (test_trie, dict_p->key, &trie_data)) {
            printf ("Failed to retrieve key '%ls'.\n", dict_p->key);
            is_failed = TRUE;
        }
        if (trie_data != dict_p->data) {
            printf ("Wrong data for key '%ls'; expected %d, got %d.\n",
                    dict_p->key, dict_p->data, trie_data);
            is_failed = TRUE;
        }
    }
    if (is_failed) {
        printf ("Trie store/retrieval test failed.\n");
        goto err_trie_created;
    }

    /* delete */
    msg_step ("Deleting some entries from trie");
    n_entries = dict_src_n_entries ();
    srand (time (NULL));
    for (n_dels = n_entries/3 + 1; n_dels > 0; n_dels--) {
        /* pick an undeleted entry */
        do {
            i = rand () % n_entries;
        } while (TRIE_DATA_READ == dict_src[i].data);

        printf ("Deleting '%ls'\n", dict_src[i].key);
        if (!trie_delete (test_trie, dict_src[i].key)) {
            printf ("Failed to delete '%ls'\n", dict_src[i].key);
            is_failed = TRUE;
        }
        dict_src[i].data = TRIE_DATA_READ;
    }
    if (is_failed) {
        printf ("Trie deletion test failed.\n");
        goto err_trie_created;
    }

    /* retrieve */
    msg_step ("Retrieving data from trie again after deletions");
    for (dict_p = dict_src; dict_p->key; dict_p++) {
        /* skip deleted entries */
        if (TRIE_DATA_READ == dict_p->data)
            continue;

        if (!trie_retrieve (test_trie, dict_p->key, &trie_data)) {
            printf ("Failed to retrieve key '%ls'.\n", dict_p->key);
            is_failed = TRUE;
        }
        if (trie_data != dict_p->data) {
            printf ("Wrong data for key '%ls'; expected %d, got %d.\n",
                    dict_p->key, dict_p->data, trie_data);
            is_failed = TRUE;
        }
    }
    if (is_failed) {
        printf ("Trie retrival-after-deletion test failed.\n");
        goto err_trie_created;
    }

    /* enumerate & check */
    msg_step ("Iterating trie contents after deletions");
    trie_root_state = trie_root (test_trie);
    if (!trie_root_state) {
        printf ("Failed to get trie root state\n");
        goto err_trie_created;
    }
    trie_it = trie_iterator_new (trie_root_state);
    if (!trie_it) {
        printf ("Failed to get trie iterator\n");
        goto err_trie_root_created;
    }

    while (trie_iterator_next (trie_it)) {
        AlphaChar *key;
        TrieData   key_data, src_data;

        key = trie_iterator_get_key (trie_it);
        if (!key) {
            printf ("Failed to get key from trie iterator\n");
            is_failed = TRUE;
            continue;
        }
        key_data = trie_iterator_get_data (trie_it);
        if (TRIE_DATA_ERROR == key_data) {
            printf ("Failed to get data from trie iterator for key '%ls'\n",
                    key);
            is_failed = TRUE;
        }
        /* mark entries found in trie */
        src_data = dict_src_get_data (key);
        if (TRIE_DATA_ERROR == src_data) {
            printf ("Extra entry in trie: key '%ls', data %d.\n",
                    key, key_data);
            is_failed = TRUE;
        } else if (src_data != key_data) {
            printf ("Data mismatch for: key '%ls', expected %d, got %d.\n",
                    key, src_data, key_data);
            is_failed = TRUE;
        } else {
            dict_src_set_data (key, TRIE_DATA_READ);
        }

        free (key);
    }

    /* check for unmarked entries, (i.e. missed in trie) */
    for (dict_p = dict_src; dict_p->key; dict_p++) {
        if (dict_p->data != TRIE_DATA_READ) {
            printf ("Entry missed in trie: key '%ls', data %d.\n",
                    dict_p->key, dict_p->data);
            is_failed = TRUE;
        }
    }

    if (is_failed) {
        printf ("Errors found in trie iteration after deletions.\n");
        goto err_trie_it_created;
    }

    trie_iterator_free (trie_it);
    trie_state_free (trie_root_state);
    trie_free (test_trie);
    return 0;

err_trie_it_created:
    trie_iterator_free (trie_it);
err_trie_root_created:
    trie_state_free (trie_root_state);
err_trie_created:
    trie_free (test_trie);
err_trie_not_created:
    return 1;
}

/*
vi:ts=4:ai:expandtab
*/
