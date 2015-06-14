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
 * test_walk.c - Test for datrie walking operations
 * Created: 2013-10-16
 * Author:  Theppitak Karoonboonyanan <thep@linux.thai.net>
 */

#include <datrie/trie.h>
#include "utils.h"
#include <stdio.h>

/*
 * Sample trie in http://linux.thai.net/~thep/datrie/datrie.html
 *
 *           +---o-> (3) -o-> (4) -l-> [5]
 *           |
 *           |        +---i-> (7) -z-> (8) -e-> [9]
 *           |        |
 * (1) -p-> (2) -r-> (6) -e-> (10) -v-> (11) -i-> (12) -e-> (13) -w-> [14]
 *                    |         |
 *                    |         +---p-> (15) -a-> (16) -r-> (17) -e-> [18]
 *                    |
 *                    +---o-> (19) -d-> (20) -u-> (21) -c-> (22) -e-> [23]
 *                              |
 *                              +---g-> (24) -r-> (25) -e-> (26) -s-> (27) -s-> [28]
 *
 */
DictRec walk_dict[] = {
    {(AlphaChar *)L"pool",       TRIE_DATA_UNREAD},
    {(AlphaChar *)L"prize",      TRIE_DATA_UNREAD},
    {(AlphaChar *)L"preview",    TRIE_DATA_UNREAD},
    {(AlphaChar *)L"prepare",    TRIE_DATA_UNREAD},
    {(AlphaChar *)L"produce",    TRIE_DATA_UNREAD},
    {(AlphaChar *)L"progress",   TRIE_DATA_UNREAD},
    {(AlphaChar *)NULL,          TRIE_DATA_ERROR},
};

static Bool
is_walkables_include (AlphaChar c, const AlphaChar *walkables, int n_elm)
{
    while (n_elm > 0) {
        if (walkables[--n_elm] == c)
            return TRUE;
    }
    return FALSE;
}

static void
print_walkables (const AlphaChar *walkables, int n_elm)
{
    int i;

    printf ("{");
    for (i = 0; i < n_elm; i++) {
        if (i > 0) {
            printf (", ");
        }
        printf ("'%lc'", walkables[i]);
    }
    printf ("}");
}

#define ALPHABET_SIZE 256

int
main ()
{
    Trie       *test_trie;
    DictRec    *dict_p;
    TrieState  *s, *t, *u;
    AlphaChar   walkables[ALPHABET_SIZE];
    int         n;
    Bool        is_failed;
    TrieData    data;

    msg_step ("Preparing trie");
    test_trie = en_trie_new ();
    if (!test_trie) {
        fprintf (stderr, "Fail to create test trie\n");
        goto err_trie_not_created;
    }

    /* store */
    for (dict_p = walk_dict; dict_p->key; dict_p++) {
        if (!trie_store (test_trie, dict_p->key, dict_p->data)) {
            printf ("Failed to add key '%ls', data %d.\n",
                    dict_p->key, dict_p->data);
            goto err_trie_created;
        }
    }

    printf (
        "Now the trie structure is supposed to be:\n"
        "\n"
        "          +---o-> (3) -o-> (4) -l-> [5]\n"
        "          |\n"
        "          |        +---i-> (7) -z-> (8) -e-> [9]\n"
        "          |        |\n"
        "(1) -p-> (2) -r-> (6) -e-> (10) -v-> (11) -i-> (12) -e-> (13) -w-> [14]\n"
        "                   |         |\n"
        "                   |         +---p-> (15) -a-> (16) -r-> (17) -e-> [18]\n"
        "                   |\n"
        "                   +---o-> (19) -d-> (20) -u-> (21) -c-> (22) -e-> [23]\n"
        "                             |\n"
        "                             +---g-> (24) -r-> (25) -e-> (26) -s-> (27) -s-> [28]\n"
        "\n"
    );

    /* walk */
    msg_step ("Test walking");
    s = trie_root (test_trie);
    if (!s) {
        printf ("Failed to get trie root state\n");
        goto err_trie_created;
    }

    msg_step ("Test walking with 'p'");
    if (!trie_state_is_walkable (s, L'p')) {
        printf ("Trie state is not walkable with 'p'\n");
        goto err_trie_state_s_created;
    }
    if (!trie_state_walk (s, L'p')) {
        printf ("Failed to walk with 'p'\n");
        goto err_trie_state_s_created;
    }

    msg_step ("Now at (2), walkable chars should be {'o', 'r'}");
    is_failed = FALSE;
    n = trie_state_walkable_chars (s, walkables, ALPHABET_SIZE);
    if (2 != n) {
        printf ("Walkable chars should be exactly 2, got %d\n", n);
        is_failed = TRUE;
    }
    if (!is_walkables_include (L'o', walkables, n)) {
        printf ("Walkable chars do not include 'o'\n");
        is_failed = TRUE;
    }
    if (!is_walkables_include (L'r', walkables, n)) {
        printf ("Walkable chars do not include 'r'\n");
        is_failed = TRUE;
    }
    if (is_failed) {
        printf ("Walkables = ");
        print_walkables (walkables, n);
        printf ("\n");
        goto err_trie_state_s_created;
    }

    msg_step ("Try walking from (2) with 'o' to (3)");
    t = trie_state_clone (s);
    if (!t) {
        printf ("Failed to clone trie state\n");
        goto err_trie_state_s_created;
    }
    if (!trie_state_walk (t, L'o')) {
        printf ("Failed to walk from (2) with 'o' to (3)\n");
        goto err_trie_state_t_created;
    }
    if (!trie_state_is_single (t)) {
        printf ("(3) should be single, but isn't.\n");
        goto err_trie_state_t_created;
    }

    msg_step ("Try walking from (3) with 'o' to (4)");
    if (!trie_state_walk (t, L'o')) {
        printf ("Failed to walk from (3) with 'o' to (4)\n");
        goto err_trie_state_t_created;
    }
    if (!trie_state_is_single (t)) {
        printf ("(4) should be single, but isn't.\n");
        goto err_trie_state_t_created;
    }

    msg_step ("Try walking from (4) with 'l' to (5)");
    if (!trie_state_walk (t, L'l')) {
        printf ("Failed to walk from (4) with 'l' to (5)\n");
        goto err_trie_state_t_created;
    }
    if (!trie_state_is_terminal (t)) {
        printf ("(5) should be terminal, but isn't.\n");
        goto err_trie_state_t_created;
    }

    /* get key & data */
    msg_step ("Try getting data from (5)");
    data = trie_state_get_data (t);
    if (TRIE_DATA_ERROR == data) {
        printf ("Failed to get data from (5)\n");
        goto err_trie_state_t_created;
    }
    if (TRIE_DATA_UNREAD != data) {
        printf ("Mismatched data from (5), expected %d, got %d\n",
                TRIE_DATA_UNREAD, data);
        goto err_trie_state_t_created;
    }

    /* walk s from (2) with 'r' to (6) */
    msg_step ("Try walking from (2) with 'r' to (6)");
    if (!trie_state_walk (s, L'r')) {
        printf ("Failed to walk from (2) with 'r' to (6)\n");
        goto err_trie_state_t_created;
    }

    msg_step ("Now at (6), walkable chars should be {'e', 'i', 'o'}");
    is_failed = FALSE;
    n = trie_state_walkable_chars (s, walkables, ALPHABET_SIZE);
    if (3 != n) {
        printf ("Walkable chars should be exactly 3, got %d\n", n);
        is_failed = TRUE;
    }
    if (!is_walkables_include (L'e', walkables, n)) {
        printf ("Walkable chars do not include 'e'\n");
        is_failed = TRUE;
    }
    if (!is_walkables_include (L'i', walkables, n)) {
        printf ("Walkable chars do not include 'i'\n");
        is_failed = TRUE;
    }
    if (!is_walkables_include (L'o', walkables, n)) {
        printf ("Walkable chars do not include 'o'\n");
        is_failed = TRUE;
    }
    if (is_failed) {
        printf ("Walkables = ");
        print_walkables (walkables, n);
        printf ("\n");
        goto err_trie_state_t_created;
    }

    /* walk from s (6) with "ize" */
    msg_step ("Try walking from (6) with 'i' to (7)");
    trie_state_copy (t, s);
    if (!trie_state_walk (t, L'i')) {
        printf ("Failed to walk from (6) with 'i' to (7)\n");
        goto err_trie_state_t_created;
    }
    msg_step ("Try walking from (7) with 'z' to (8)");
    if (!trie_state_walk (t, L'z')) {
        printf ("Failed to walk from (7) with 'z' to (8)\n");
        goto err_trie_state_t_created;
    }
    if (!trie_state_is_single (t)) {
        printf ("(7) should be single, but isn't.\n");
        goto err_trie_state_t_created;
    }
    msg_step ("Try walking from (8) with 'e' to (9)");
    if (!trie_state_walk (t, L'e')) {
        printf ("Failed to walk from (8) with 'e' to (9)\n");
        goto err_trie_state_t_created;
    }
    if (!trie_state_is_terminal (t)) {
        printf ("(9) should be terminal, but isn't.\n");
        goto err_trie_state_t_created;
    }

    msg_step ("Try getting data from (9)");
    data = trie_state_get_data (t);
    if (TRIE_DATA_ERROR == data) {
        printf ("Failed to get data from (9)\n");
        goto err_trie_state_t_created;
    }
    if (TRIE_DATA_UNREAD != data) {
        printf ("Mismatched data from (9), expected %d, got %d\n",
                TRIE_DATA_UNREAD, data);
        goto err_trie_state_t_created;
    }

    /* walk from u = s (6) with 'e' to (10) */
    msg_step ("Try walking from (6) with 'e' to (10)");
    u = trie_state_clone (s);
    if (!u) {
        printf ("Failed to clone trie state\n");
        goto err_trie_state_t_created;
    }
    if (!trie_state_walk (u, L'e')) {
        printf ("Failed to walk from (6) with 'e' to (10)\n");
        goto err_trie_state_u_created;
    }

    /* walkable chars from (10) should be {'p', 'v'} */
    msg_step ("Now at (10), walkable chars should be {'p', 'v'}");
    is_failed = FALSE;
    n = trie_state_walkable_chars (u, walkables, ALPHABET_SIZE);
    if (2 != n) {
        printf ("Walkable chars should be exactly 2, got %d\n", n);
        is_failed = TRUE;
    }
    if (!is_walkables_include (L'p', walkables, n)) {
        printf ("Walkable chars do not include 'p'\n");
        is_failed = TRUE;
    }
    if (!is_walkables_include (L'v', walkables, n)) {
        printf ("Walkable chars do not include 'v'\n");
        is_failed = TRUE;
    }
    if (is_failed) {
        printf ("Walkables = ");
        print_walkables (walkables, n);
        printf ("\n");
        goto err_trie_state_u_created;
    }

    /* walk from u (10) with "view" */
    msg_step ("Try walking from (10) with 'v' to (11)");
    trie_state_copy (t, u);
    if (!trie_state_walk (t, L'v')) {
        printf ("Failed to walk from (10) with 'v' to (11)\n");
        goto err_trie_state_u_created;
    }
    if (!trie_state_is_single (t)) {
        printf ("(11) should be single, but isn't.\n");
        goto err_trie_state_u_created;
    }
    msg_step ("Try walking from (11) with 'i' to (12)");
    if (!trie_state_walk (t, L'i')) {
        printf ("Failed to walk from (11) with 'i' to (12)\n");
        goto err_trie_state_u_created;
    }
    msg_step ("Try walking from (12) with 'e' to (13)");
    if (!trie_state_walk (t, L'e')) {
        printf ("Failed to walk from (12) with 'e' to (13)\n");
        goto err_trie_state_u_created;
    }
    msg_step ("Try walking from (13) with 'w' to (14)");
    if (!trie_state_walk (t, L'w')) {
        printf ("Failed to walk from (13) with 'w' to (14)\n");
        goto err_trie_state_u_created;
    }
    if (!trie_state_is_terminal (t)) {
        printf ("(14) should be terminal, but isn't.\n");
        goto err_trie_state_u_created;
    }

    msg_step ("Try getting data from (14)");
    data = trie_state_get_data (t);
    if (TRIE_DATA_ERROR == data) {
        printf ("Failed to get data from (14)\n");
        goto err_trie_state_u_created;
    }
    if (TRIE_DATA_UNREAD != data) {
        printf ("Mismatched data from (14), expected %d, got %d\n",
                TRIE_DATA_UNREAD, data);
        goto err_trie_state_u_created;
    }

    /* walk from u (10) with "pare" */
    msg_step ("Try walking from (10) with 'p' to (15)");
    trie_state_copy (t, u);
    if (!trie_state_walk (t, L'p')) {
        printf ("Failed to walk from (10) with 'p' to (15)\n");
        goto err_trie_state_u_created;
    }
    if (!trie_state_is_single (t)) {
        printf ("(15) should be single, but isn't.\n");
        goto err_trie_state_u_created;
    }
    msg_step ("Try walking from (15) with 'a' to (16)");
    if (!trie_state_walk (t, L'a')) {
        printf ("Failed to walk from (15) with 'a' to (16)\n");
        goto err_trie_state_u_created;
    }
    msg_step ("Try walking from (16) with 'r' to (17)");
    if (!trie_state_walk (t, L'r')) {
        printf ("Failed to walk from (16) with 'r' to (17)\n");
        goto err_trie_state_u_created;
    }
    msg_step ("Try walking from (17) with 'e' to (18)");
    if (!trie_state_walk (t, L'e')) {
        printf ("Failed to walk from (17) with 'e' to (18)\n");
        goto err_trie_state_u_created;
    }
    if (!trie_state_is_terminal (t)) {
        printf ("(18) should be terminal, but isn't.\n");
        goto err_trie_state_u_created;
    }

    msg_step ("Try getting data from (18)");
    data = trie_state_get_data (t);
    if (TRIE_DATA_ERROR == data) {
        printf ("Failed to get data from (18)\n");
        goto err_trie_state_u_created;
    }
    if (TRIE_DATA_UNREAD != data) {
        printf ("Mismatched data from (18), expected %d, got %d\n",
                TRIE_DATA_UNREAD, data);
        goto err_trie_state_u_created;
    }

    trie_state_free (u);

    /* walk s from (6) with 'o' to (19) */
    msg_step ("Try walking from (6) with 'o' to (19)");
    if (!trie_state_walk (s, L'o')) {
        printf ("Failed to walk from (6) with 'o' to (19)\n");
        goto err_trie_state_t_created;
    }

    msg_step ("Now at (19), walkable chars should be {'d', 'g'}");
    is_failed = FALSE;
    n = trie_state_walkable_chars (s, walkables, ALPHABET_SIZE);
    if (2 != n) {
        printf ("Walkable chars should be exactly 2, got %d\n", n);
        is_failed = TRUE;
    }
    if (!is_walkables_include (L'd', walkables, n)) {
        printf ("Walkable chars do not include 'd'\n");
        is_failed = TRUE;
    }
    if (!is_walkables_include (L'g', walkables, n)) {
        printf ("Walkable chars do not include 'g'\n");
        is_failed = TRUE;
    }
    if (is_failed) {
        printf ("Walkables = ");
        print_walkables (walkables, n);
        printf ("\n");
        goto err_trie_state_t_created;
    }

    /* walk from s (19) with "duce" */
    msg_step ("Try walking from (19) with 'd' to (20)");
    trie_state_copy (t, s);
    if (!trie_state_walk (t, L'd')) {
        printf ("Failed to walk from (19) with 'd' to (20)\n");
        goto err_trie_state_t_created;
    }
    if (!trie_state_is_single (t)) {
        printf ("(20) should be single, but isn't.\n");
        goto err_trie_state_t_created;
    }
    msg_step ("Try walking from (20) with 'u' to (21)");
    if (!trie_state_walk (t, L'u')) {
        printf ("Failed to walk from (20) with 'u' to (21)\n");
        goto err_trie_state_t_created;
    }
    msg_step ("Try walking from (21) with 'c' to (22)");
    if (!trie_state_walk (t, L'c')) {
        printf ("Failed to walk from (21) with 'c' to (22)\n");
        goto err_trie_state_t_created;
    }
    msg_step ("Try walking from (22) with 'e' to (23)");
    if (!trie_state_walk (t, L'e')) {
        printf ("Failed to walk from (22) with 'e' to (23)\n");
        goto err_trie_state_t_created;
    }
    if (!trie_state_is_terminal (t)) {
        printf ("(23) should be terminal, but isn't.\n");
        goto err_trie_state_t_created;
    }

    msg_step ("Try getting data from (23)");
    data = trie_state_get_data (t);
    if (TRIE_DATA_ERROR == data) {
        printf ("Failed to get data from (23)\n");
        goto err_trie_state_t_created;
    }
    if (TRIE_DATA_UNREAD != data) {
        printf ("Mismatched data from (23), expected %d, got %d\n",
                TRIE_DATA_UNREAD, data);
        goto err_trie_state_t_created;
    }

    trie_state_free (t);

    /* walk from s (19) with "gress" */
    msg_step ("Try walking from (19) with 'g' to (24)");
    if (!trie_state_walk (s, L'g')) {
        printf ("Failed to walk from (19) with 'g' to (24)\n");
        goto err_trie_state_s_created;
    }
    if (!trie_state_is_single (s)) {
        printf ("(24) should be single, but isn't.\n");
        goto err_trie_state_s_created;
    }
    msg_step ("Try walking from (24) with 'r' to (25)");
    if (!trie_state_walk (s, L'r')) {
        printf ("Failed to walk from (24) with 'r' to (25)\n");
        goto err_trie_state_s_created;
    }
    msg_step ("Try walking from (25) with 'e' to (26)");
    if (!trie_state_walk (s, L'e')) {
        printf ("Failed to walk from (25) with 'e' to (26)\n");
        goto err_trie_state_s_created;
    }
    msg_step ("Try walking from (26) with 's' to (27)");
    if (!trie_state_walk (s, L's')) {
        printf ("Failed to walk from (26) with 's' to (27)\n");
        goto err_trie_state_s_created;
    }
    msg_step ("Try walking from (27) with 's' to (28)");
    if (!trie_state_walk (s, L's')) {
        printf ("Failed to walk from (27) with 's' to (28)\n");
        goto err_trie_state_s_created;
    }
    if (!trie_state_is_terminal (s)) {
        printf ("(28) should be terminal, but isn't.\n");
        goto err_trie_state_s_created;
    }

    msg_step ("Try getting data from (28)");
    data = trie_state_get_data (s);
    if (TRIE_DATA_ERROR == data) {
        printf ("Failed to get data from (28)\n");
        goto err_trie_state_s_created;
    }
    if (TRIE_DATA_UNREAD != data) {
        printf ("Mismatched data from (28), expected %d, got %d\n",
                TRIE_DATA_UNREAD, data);
        goto err_trie_state_s_created;
    }

    trie_state_free (s);
    trie_free (test_trie);
    return 0;

err_trie_state_u_created:
    trie_state_free (u);
err_trie_state_t_created:
    trie_state_free (t);
err_trie_state_s_created:
    trie_state_free (s);
err_trie_created:
    trie_free (test_trie);
err_trie_not_created:
    return 1;
}

/*
vi:ts=4:ai:expandtab
*/
