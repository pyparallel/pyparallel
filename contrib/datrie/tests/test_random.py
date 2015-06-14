# -*- coding: utf-8 -*-

from __future__ import absolute_import, unicode_literals

import pickle
import string
import sys

import datrie

# This is ugly, but hypothesis only supports Python2.7+ at the moment.
if sys.version_info[:2] >= (2, 7):
    from hypothesis import given, Settings
    from hypothesis.specifiers import strings

    # Reduce hypothesis input size by half.
    Settings.default.average_list_length = 5.0

    printable_strings = [strings(string.printable)]

    @given(printable_strings)
    def test_contains(words):
        trie = datrie.Trie(string.printable)
        for i, word in enumerate(set(words)):
            trie[word] = i

        for i, word in enumerate(set(words)):
            assert word in trie
            assert trie[word] == trie.get(word) == i

    @given(printable_strings)
    def test_len(words):
        trie = datrie.Trie(string.printable)
        for i, word in enumerate(set(words)):
            trie[word] = i

        assert len(trie) == len(set(words))

    @given(printable_strings)
    def test_pickle_unpickle(words):
        trie = datrie.Trie(string.printable)
        for i, word in enumerate(set(words)):
            trie[word] = i

        trie = pickle.loads(pickle.dumps(trie))
        for i, word in enumerate(set(words)):
            assert word in trie
            assert trie[word] == i

    @given(printable_strings)
    def test_pop(words):
        words = set(words)
        trie = datrie.Trie(string.printable)
        for i, word in enumerate(words):
            trie[word] = i

        for i, word in enumerate(words):
            assert trie.pop(word) == i
            assert trie.pop(word, 42) == trie.get(word, 42) == 42

    @given(printable_strings)
    def test_clear(words):
        words = set(words)
        trie = datrie.Trie(string.printable)
        for i, word in enumerate(words):
            trie[word] = i

        assert len(trie) == len(words)
        trie.clear()
        assert not trie
        assert len(trie) == 0

        # make sure the trie works afterwards.
        for i, word in enumerate(words):
            trie[word] = i
            assert trie[word] == i
