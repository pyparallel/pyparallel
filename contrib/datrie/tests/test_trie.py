# -*- coding: utf-8 -*-

from __future__ import absolute_import, unicode_literals

import pickle
import random
import string
import sys
import tempfile

import datrie
import pytest


def test_trie():
    trie = datrie.Trie(string.printable)
    assert trie.is_dirty()

    assert 'foo' not in trie
    assert 'Foo' not in trie

    trie['foo'] = '5'
    assert 'foo' in trie
    assert trie['foo'] == '5'

    trie['Foo'] = 10
    assert trie['Foo'] == 10
    assert trie['foo'] == '5'
    del trie['foo']

    assert 'foo' not in trie
    assert 'Foo' in trie
    assert trie['Foo'] == 10

    with pytest.raises(KeyError):
        trie['bar']


def test_trie_invalid_alphabet():
    t = datrie.Trie('abc')
    t['a'] = 'a'
    t['b'] = 'b'
    t['c'] = 'c'

    for k in 'abc':
        assert t[k] == k

    with pytest.raises(KeyError):
        t['d']

    with pytest.raises(KeyError):
        t['e']


def test_trie_save_load():
    fd, fname = tempfile.mkstemp()
    trie = datrie.Trie(string.printable)
    trie['foobar'] = 1
    trie['foovar'] = 2
    trie['baz'] = 3
    trie['fo'] = 4
    trie['Foo'] = 'vasia'
    trie.save(fname)
    del trie

    trie2 = datrie.Trie.load(fname)
    assert trie2['foobar'] == 1
    assert trie2['baz'] == 3
    assert trie2['fo'] == 4
    assert trie2['foovar'] == 2
    assert trie2['Foo'] == 'vasia'


def test_save_load_base():
    fd, fname = tempfile.mkstemp()
    trie = datrie.BaseTrie(alphabet=string.printable)
    trie['foobar'] = 1
    trie['foovar'] = 2
    trie['baz'] = 3
    trie['fo'] = 4
    trie.save(fname)

    trie2 = datrie.BaseTrie.load(fname)
    assert trie2['foobar'] == 1
    assert trie2['baz'] == 3
    assert trie2['fo'] == 4
    assert trie2['foovar'] == 2


def test_trie_file_io():
    fd, fname = tempfile.mkstemp()

    trie = datrie.BaseTrie(string.printable)
    trie['foobar'] = 1
    trie['foo'] = 2

    extra_data = ['foo', 'bar']

    with open(fname, "wb", 0) as f:
        pickle.dump(extra_data, f)
        trie.write(f)
        pickle.dump(extra_data, f)

    with open(fname, "rb", 0) as f:
        extra_data2 = pickle.load(f)
        trie2 = datrie.BaseTrie.read(f)
        extra_data3 = pickle.load(f)

    assert extra_data2 == extra_data
    assert extra_data3 == extra_data
    assert trie2['foobar'] == 1
    assert trie2['foo'] == 2
    assert len(trie2) == len(trie)


def test_trie_unicode():
    # trie for lowercase Russian characters
    trie = datrie.Trie(ranges=[('а', 'я')])
    trie['а'] = 1
    trie['б'] = 2
    trie['аб'] = 'vasia'

    assert trie['а'] == 1
    assert trie['б'] == 2
    assert trie['аб'] == 'vasia'


def test_trie_ascii():
    trie = datrie.Trie(string.ascii_letters)
    trie['x'] = 1
    trie['y'] = 'foo'
    trie['xx'] = 2

    assert trie['x'] == 1
    assert trie['y'] == 'foo'
    assert trie['xx'] == 2


def test_trie_items():
    trie = datrie.Trie(string.ascii_lowercase)
    trie['foo'] = 10
    trie['bar'] = 'foo'
    trie['foobar'] = 30
    assert trie.values() == ['foo', 10, 30]
    assert trie.items() == [('bar', 'foo'), ('foo', 10), ('foobar', 30)]
    assert trie.keys() == ['bar', 'foo', 'foobar']


def test_trie_iter():
    trie = datrie.Trie(string.ascii_lowercase)
    assert list(trie) == []

    trie['foo'] = trie['bar'] = trie['foobar'] = 42
    assert list(trie) == ['bar', 'foo', 'foobar']


def test_trie_comparison():
    trie = datrie.Trie(string.ascii_lowercase)
    assert trie == trie
    assert trie == datrie.Trie(string.ascii_lowercase)

    other = datrie.Trie(string.ascii_lowercase)
    trie['foo'] = 42
    other['foo'] = 24
    assert trie != other

    other['foo'] = trie['foo']
    assert trie == other

    other['bar'] = 42
    assert trie != other

    with pytest.raises(TypeError):
        trie < other  # same for other comparisons


def test_trie_update():
    trie = datrie.Trie(string.ascii_lowercase)
    trie.update([("foo", 42)])
    assert trie["foo"] == 42

    trie.update({"bar": 123})
    assert trie["bar"] == 123

    if sys.version_info[0] == 2:
        with pytest.raises(TypeError):
            trie.update(bar=24)
    else:
        trie.update(bar=24)
        assert trie["bar"] == 24



def test_trie_suffixes():
    trie = datrie.Trie(string.ascii_lowercase)

    trie['pro'] = 1
    trie['prof'] = 2
    trie['product'] = 3
    trie['production'] = 4
    trie['producer'] = 5
    trie['producers'] = 6
    trie['productivity'] = 7

    assert trie.suffixes('pro') == [
        '', 'ducer', 'ducers', 'duct', 'duction', 'ductivity', 'f'
    ]


def test_trie_len():
    trie = datrie.Trie(string.ascii_lowercase)
    words = ['foo', 'f', 'faa', 'bar', 'foobar']
    for word in words:
        trie[word] = None
    assert len(trie) == len(words)

    # Calling len on an empty trie caused segfault, see #17 on GitHub.
    trie = datrie.Trie(string.ascii_lowercase)
    assert len(trie) == 0


def test_setdefault():
    trie = datrie.Trie(string.ascii_lowercase)
    assert trie.setdefault('foo', 5) == 5
    assert trie.setdefault('foo', 4) == 5
    assert trie.setdefault('foo', 5) == 5
    assert trie.setdefault('bar', 'vasia') == 'vasia'
    assert trie.setdefault('bar', 3) == 'vasia'
    assert trie.setdefault('bar', 7) == 'vasia'


class TestPrefixLookups(object):
    def _trie(self):
        trie = datrie.Trie(string.ascii_lowercase)
        trie['foo'] = 10
        trie['bar'] = 20
        trie['foobar'] = 30
        trie['foovar'] = 40
        trie['foobarzartic'] = None
        return trie

    def test_trie_keys_prefix(self):
        trie = self._trie()
        assert trie.keys('foobarz') == ['foobarzartic']
        assert trie.keys('foobarzart') == ['foobarzartic']
        assert trie.keys('foo') == ['foo', 'foobar', 'foobarzartic', 'foovar']
        assert trie.keys('foobar') == ['foobar', 'foobarzartic']
        assert trie.keys('') == [
            'bar', 'foo', 'foobar', 'foobarzartic', 'foovar'
        ]
        assert trie.keys('x') == []

    def test_trie_items_prefix(self):
        trie = self._trie()
        assert trie.items('foobarz') == [('foobarzartic', None)]
        assert trie.items('foobarzart') == [('foobarzartic', None)]
        assert trie.items('foo') == [
            ('foo', 10), ('foobar', 30), ('foobarzartic', None), ('foovar', 40)
        ]
        assert trie.items('foobar') == [('foobar', 30), ('foobarzartic', None)]
        assert trie.items('') == [
            ('bar', 20), ('foo', 10), ('foobar', 30),
            ('foobarzartic', None), ('foovar', 40)
        ]
        assert trie.items('x') == []

    def test_trie_values_prefix(self):
        trie = self._trie()
        assert trie.values('foobarz') == [None]
        assert trie.values('foobarzart') == [None]
        assert trie.values('foo') == [10, 30, None, 40]
        assert trie.values('foobar') == [30, None]
        assert trie.values('') == [20, 10, 30, None, 40]
        assert trie.values('x') == []


class TestPrefixSearch(object):

    WORDS = ['producers', 'producersz', 'pr', 'pool', 'prepare', 'preview',
             'prize', 'produce', 'producer', 'progress']

    def _trie(self):
        trie = datrie.Trie(string.ascii_lowercase)
        for index, word in enumerate(self.WORDS, 1):
            trie[word] = index
        return trie

    def test_trie_iter_prefixes(self):
        trie = self._trie()
        trie['pr'] = 'foo'

        prefixes = trie.iter_prefixes('producers')
        assert list(prefixes) == ['pr', 'produce', 'producer', 'producers']

        no_prefixes = trie.iter_prefixes('vasia')
        assert list(no_prefixes) == []

        values = trie.iter_prefix_values('producers')
        assert list(values) == ['foo', 8, 9, 1]

        no_prefixes = trie.iter_prefix_values('vasia')
        assert list(no_prefixes) == []

        items = trie.iter_prefix_items('producers')
        assert next(items) == ('pr', 'foo')
        assert next(items) == ('produce', 8)
        assert next(items) == ('producer', 9)
        assert next(items) == ('producers', 1)

        no_prefixes = trie.iter_prefix_items('vasia')
        assert list(no_prefixes) == []

    def test_trie_prefixes(self):
        trie = self._trie()

        prefixes = trie.prefixes('producers')
        assert prefixes == ['pr', 'produce', 'producer', 'producers']

        values = trie.prefix_values('producers')
        assert values == [3, 8, 9, 1]

        items = trie.prefix_items('producers')
        assert items == [('pr', 3), ('produce', 8),
                         ('producer', 9), ('producers', 1)]

        assert trie.prefixes('vasia') == []
        assert trie.prefix_values('vasia') == []
        assert trie.prefix_items('vasia') == []

    def test_has_keys_with_prefix(self):
        trie = self._trie()

        for word in self.WORDS:
            assert trie.has_keys_with_prefix(word)
            assert trie.has_keys_with_prefix(word[:-1])

        assert trie.has_keys_with_prefix('p')
        assert trie.has_keys_with_prefix('poo')
        assert trie.has_keys_with_prefix('pr')
        assert trie.has_keys_with_prefix('priz')

        assert not trie.has_keys_with_prefix('prizey')
        assert not trie.has_keys_with_prefix('ops')
        assert not trie.has_keys_with_prefix('progn')

    def test_longest_prefix(self):
        trie = self._trie()

        for word in self.WORDS:
            assert trie.longest_prefix(word) == word

        assert trie.longest_prefix('pooler') == 'pool'
        assert trie.longest_prefix('producers') == 'producers'
        assert trie.longest_prefix('progressor') == 'progress'

        assert trie.longest_prefix('paol', default=None) is None
        assert trie.longest_prefix('p', default=None) is None
        assert trie.longest_prefix('z', default=None) is None

        with pytest.raises(KeyError):
            trie.longest_prefix('z')

    def test_longest_prefix_bug(self):
        trie = self._trie()
        assert trie.longest_prefix("print") == "pr"
        assert trie.longest_prefix_value("print") == 3
        assert trie.longest_prefix_item("print") == ("pr", 3)

    def test_longest_prefix_item(self):
        trie = self._trie()

        for index, word in enumerate(self.WORDS, 1):
            assert trie.longest_prefix_item(word) == (word, index)

        assert trie.longest_prefix_item('pooler') == ('pool', 4)
        assert trie.longest_prefix_item('producers') == ('producers', 1)
        assert trie.longest_prefix_item('progressor') == ('progress', 10)

        dummy = (None, None)
        assert trie.longest_prefix_item('paol', default=dummy) == dummy
        assert trie.longest_prefix_item('p', default=dummy) == dummy
        assert trie.longest_prefix_item('z', default=dummy) == dummy

        with pytest.raises(KeyError):
            trie.longest_prefix_item('z')

    def test_longest_prefix_value(self):
        trie = self._trie()

        for index, word in enumerate(self.WORDS, 1):
            assert trie.longest_prefix_value(word) == index

        assert trie.longest_prefix_value('pooler') == 4
        assert trie.longest_prefix_value('producers') == 1
        assert trie.longest_prefix_value('progressor') == 10

        assert trie.longest_prefix_value('paol', default=None) is None
        assert trie.longest_prefix_value('p', default=None) is None
        assert trie.longest_prefix_value('z', default=None) is None

        with pytest.raises(KeyError):
            trie.longest_prefix_value('z')


def test_trie_fuzzy():
    russian = 'абвгдеёжзиклмнопрстуфхцчъыьэюя'
    alphabet = russian.upper() + string.ascii_lowercase
    words = list(set([
        "".join(random.choice(alphabet) for x in range(random.randint(8, 16)))
        for y in range(1000)
    ]))

    trie = datrie.Trie(alphabet)

    enumerated_words = list(enumerate(words))

    for index, word in enumerated_words:
        trie[word] = index

    assert len(trie) == len(words)

    random.shuffle(enumerated_words)
    for index, word in enumerated_words:
        assert word in trie, word
        assert trie[word] == index, (word, index)
