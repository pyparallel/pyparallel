/* -*- Mode: C; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
/*
 * libdatrie - Double-Array Trie Library
 * Copyright (C) 2015  Theppitak Karoonboonyanan <theppitak@gmail.com>
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
 * test_null_trie.c - Test for datrie iteration on empty trie
 * Created: 2015-04-21
 * Author:  Theppitak Karoonboonyanan <theppitak@gmail.com>
 */

#include <datrie/trie.h>
#include "utils.h"
#include <stdio.h>
#include <stdlib.h>

int
main ()
{
    Trie         *test_trie;
    TrieState    *trie_root_state;
    TrieIterator *trie_it;
    Bool          is_failed;

    msg_step ("Preparing empty trie");
    test_trie = en_trie_new ();
    if (!test_trie) {
        fprintf (stderr, "Fail to create test trie\n");
        goto err_trie_not_created;
    }

    /* iterate & check */
    msg_step ("Iterating");
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

    is_failed = FALSE;
    while (trie_iterator_next (trie_it)) {
        AlphaChar *key;

        printf ("Got entry from empty trie, which is weird!\n");

        key = trie_iterator_get_key (trie_it);
        if (key) {
            printf ("Got key from empty trie, which is weird! (key='%ls')\n",
                    key);
            is_failed = TRUE;
            free (key);
        }
    }

    if (is_failed) {
        printf ("Errors found in empty trie iteration.\n");
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
