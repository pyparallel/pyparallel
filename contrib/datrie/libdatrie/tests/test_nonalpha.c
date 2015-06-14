/* -*- Mode: C; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
/*
 * libdatrie - Double-Array Trie Library
 * Copyright (C) 2014  Theppitak Karoonboonyanan <thep@linux.thai.net>
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
 * test_nonalpha.c - Test for datrie behaviors on non-alphabet inputs
 * Created: 2014-01-06
 * Author:  Theppitak Karoonboonyanan <thep@linux.thai.net>
 */

#include <datrie/trie.h>
#include "utils.h"
#include <stdio.h>

const AlphaChar *nonalpha_src[] = {
    (AlphaChar *)L"a6acus",
    (AlphaChar *)L"a5acus",
    NULL
};

int
main ()
{
    Trie             *test_trie;
    DictRec          *dict_p;
    const AlphaChar **nonalpha_key;
    TrieData          trie_data;
    Bool              is_fail;

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

    /* test storing keys with non-alphabet chars */
    is_fail = FALSE;
    for (nonalpha_key = nonalpha_src; *nonalpha_key; nonalpha_key++) {
        if (trie_retrieve (test_trie, *nonalpha_key, &trie_data)) {
            printf ("False duplication on key '%ls', with existing data %d.\n",
                    *nonalpha_key, trie_data);
            is_fail = TRUE;
        }
        if (trie_store (test_trie, *nonalpha_key, TRIE_DATA_UNREAD)) {
            printf ("Wrongly added key '%ls' containing non-alphanet char\n",
                    *nonalpha_key);
            is_fail = TRUE;
        }
    }

    if (is_fail)
        goto err_trie_created;

    trie_free (test_trie);
    return 0;

err_trie_created:
    trie_free (test_trie);
err_trie_not_created:
    return 1;
}

/*
vi:ts=4:ai:expandtab
*/
