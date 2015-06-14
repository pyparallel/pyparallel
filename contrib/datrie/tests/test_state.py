# -*- coding: utf-8 -*-

from __future__ import absolute_import, unicode_literals

import datrie


def _trie():
    trie = datrie.Trie(ranges=[(chr(0), chr(127))])
    trie['f'] = 1
    trie['fo'] = 2
    trie['fa'] = 3
    trie['faur'] = 4
    trie['fauxiiiip'] = 5
    trie['fauzox'] = 10
    trie['fauzoy'] = 20
    return trie


def test_trie_state():
    trie = _trie()
    state = datrie.State(trie)
    state.walk('f')
    assert state.data() == 1
    state.walk('o')
    assert state.data() == 2
