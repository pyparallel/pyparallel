# cython: profile=False
"""
Cython wrapper for libdatrie.
"""

from cpython.version cimport PY_MAJOR_VERSION
from cython.operator import dereference as deref
#from cpython.mem cimport PyMem_Malloc as MALLOC
#from cpython.mem cimport PyMem_Free as FREE
from libc.stdlib cimport malloc as MALLOC
from libc.stdlib cimport free as FREE
from libc cimport stdio
from libc cimport string
cimport stdio_ext
cimport cdatrie

import itertools
import warnings
import sys
import tempfile
from collections import MutableMapping

try:
    import cPickle as pickle
except ImportError:
    import pickle

class DatrieError(Exception):
    pass

RAISE_KEY_ERROR = object()
RERAISE_KEY_ERROR = object()
DELETED_OBJECT = object()


cdef class BaseTrie:
    """
    Wrapper for libdatrie's trie.

    Keys are unicode strings, values are integers -2147483648 <= x <= 2147483647.
    """

    cdef AlphaMap alpha_map
    cdef cdatrie.Trie *_c_trie

    def __init__(self, alphabet=None, ranges=None, AlphaMap alpha_map=None, _create=True):
        """
        For efficiency trie needs to know what unicode symbols
        it should be able to store so this constructor requires
        either ``alphabet`` (a string/iterable with all allowed characters),
        ``ranges`` (a list of (begin, end) pairs, e.g. [('a', 'z')])
        or ``alpha_map`` (:class:`datrie.AlphaMap` instance).
        """
        if self._c_trie is not NULL:
            return

        if not _create:
            return

        if alphabet is None and ranges is None and alpha_map is None:
            raise ValueError(
                "Please provide alphabet, ranges or alpha_map argument.")

        if alpha_map is None:
            alpha_map = AlphaMap(alphabet, ranges)

        self.alpha_map = alpha_map
        self._c_trie = cdatrie.trie_new(alpha_map._c_alpha_map)
        if self._c_trie is NULL:
            raise MemoryError()

    def __dealloc__(self):
        if self._c_trie is not NULL:
            cdatrie.trie_free(self._c_trie)

    def update(self, other=(), **kwargs):
        if PY_MAJOR_VERSION == 2:
            if kwargs:
                raise TypeError("keyword arguments are not supported.")

        if hasattr(other, "keys"):
            for key in other:
                self[key] = other[key]
        else:
            for key, value in other:
                self[key] = value

        for key in kwargs:
            self[key] = kwargs[key]

    def clear(self):
        cdef AlphaMap alpha_map = self.alpha_map.copy()
        _c_trie = cdatrie.trie_new(alpha_map._c_alpha_map)
        if _c_trie is NULL:
            raise MemoryError()

        cdatrie.trie_free(self._c_trie)
        self._c_trie = _c_trie

    cpdef bint is_dirty(self):
        """
        Returns True if the trie is dirty with some pending changes
        and needs saving to synchronize with the file.
        """
        return cdatrie.trie_is_dirty(self._c_trie)

    def save(self, path):
        """
        Saves this trie.
        """
        with open(path, "wb", 0) as f:
            self.write(f)

    def write(self, f):
        """
        Writes a trie to a file. File-like objects without real
        file descriptors are not supported.
        """
        f.flush()

        cdef stdio.FILE* f_ptr = stdio_ext.fdopen(f.fileno(), "w")
        if f_ptr == NULL:
            raise IOError("Can't open file descriptor")

        cdef int res = cdatrie.trie_fwrite(self._c_trie, f_ptr)
        if res == -1:
            raise IOError("Can't write to file")

        stdio.fflush(f_ptr)

    @classmethod
    def load(cls, path):
        """
        Loads a trie from file.
        """
        with open(path, "rb", 0) as f:
            return cls.read(f)

    @classmethod
    def read(cls, f):
        """
        Creates a new Trie by reading it from file.
        File-like objects without real file descriptors are not supported.

        # XXX: does it work properly in subclasses?
        """
        cdef BaseTrie trie = cls(_create=False)
        trie._c_trie = _load_from_file(f)
        return trie

    def __reduce__(self):
        with tempfile.NamedTemporaryFile() as f:
            self.write(f)
            f.seek(0)
            state = f.read()
            return BaseTrie, (None, None, None, False), state

    def __setstate__(self, bytes state):
        assert self._c_trie is NULL
        with tempfile.NamedTemporaryFile() as f:
            f.write(state)
            f.flush()
            f.seek(0)
            self._c_trie = _load_from_file(f)

    def __setitem__(self, unicode key, cdatrie.TrieData value):
        self._setitem(key, value)

    cdef void _setitem(self, unicode key, cdatrie.TrieData value):
        cdef cdatrie.AlphaChar* c_key = new_alpha_char_from_unicode(key)
        try:
            cdatrie.trie_store(self._c_trie, c_key, value)
        finally:
            FREE(c_key)

    def __getitem__(self, unicode key):
        return self._getitem(key)

    def get(self, unicode key, default=None):
        try:
            return self._getitem(key)
        except KeyError:
            return default

    cdef cdatrie.TrieData _getitem(self, unicode key) except -1:
        cdef cdatrie.TrieData data
        cdef cdatrie.AlphaChar* c_key = new_alpha_char_from_unicode(key)

        try:
            found = cdatrie.trie_retrieve(self._c_trie, c_key, &data)
        finally:
            FREE(c_key)

        if not found:
            raise KeyError(key)
        return data

    def __contains__(self, unicode key):
        cdef cdatrie.AlphaChar* c_key = new_alpha_char_from_unicode(key)
        try:
            return cdatrie.trie_retrieve(self._c_trie, c_key, NULL)
        finally:
            FREE(c_key)

    def __delitem__(self, unicode key):
        self._delitem(key)

    def pop(self, unicode key, default=None):
        try:
            value = self[key]
            self._delitem(key)
            return value
        except KeyError:
            return default

    cpdef bint _delitem(self, unicode key) except -1:
        """
        Deletes an entry for the given key from the trie. Returns
        boolean value indicating whether the key exists and is removed.
        """
        cdef cdatrie.AlphaChar* c_key = new_alpha_char_from_unicode(key)
        try:
            found = cdatrie.trie_delete(self._c_trie, c_key)
        finally:
            FREE(c_key)

        if not found:
            raise KeyError(key)

    @staticmethod
    cdef int len_enumerator(cdatrie.AlphaChar *key, cdatrie.TrieData key_data,
                            void *counter_ptr):
        (<int *>counter_ptr)[0] += 1
        return True

    def __len__(self):
        cdef int counter = 0
        cdatrie.trie_enumerate(self._c_trie,
                               <cdatrie.TrieEnumFunc>(self.len_enumerator),
                               &counter)
        return counter

    def __richcmp__(self, other, int op):
        if op == 2:    # ==
            if other is self:
                return True
            elif not isinstance(other, BaseTrie):
                return False

            for key in self:
                if self[key] != other[key]:
                    return False

            # XXX this can be written more efficiently via explicit iterators.
            return len(self) == len(other)
        elif op == 3:  # !=
            return not (self == other)

        raise TypeError("unorderable types: {0} and {1}".format(
            self.__class__, other.__class__))

    def setdefault(self, unicode key, cdatrie.TrieData value):
        return self._setdefault(key, value)

    cdef cdatrie.TrieData _setdefault(self, unicode key, cdatrie.TrieData value):
        cdef cdatrie.AlphaChar* c_key = new_alpha_char_from_unicode(key)
        cdef cdatrie.TrieData data

        try:
            found = cdatrie.trie_retrieve(self._c_trie, c_key, &data)
            if found:
                return data
            else:
                cdatrie.trie_store(self._c_trie, c_key, value)
                return value
        finally:
            FREE(c_key)

    def iter_prefixes(self, unicode key):
        '''
        Returns an iterator over the keys of this trie that are prefixes
        of ``key``.
        '''
        cdef cdatrie.TrieState* state = cdatrie.trie_root(self._c_trie)
        if state == NULL:
            raise MemoryError()

        cdef int index = 1
        try:
            for char in key:
                if not cdatrie.trie_state_walk(state, <cdatrie.AlphaChar> char):
                    return
                if cdatrie.trie_state_is_terminal(state):
                    yield key[:index]
                index += 1
        finally:
            cdatrie.trie_state_free(state)

    def iter_prefix_items(self, unicode key):
        '''
        Returns an iterator over the items (``(key,value)`` tuples)
        of this trie that are associated with keys that are prefixes of ``key``.
        '''
        cdef cdatrie.TrieState* state = cdatrie.trie_root(self._c_trie)

        if state == NULL:
            raise MemoryError()

        cdef int index = 1
        try:
            for char in key:
                if not cdatrie.trie_state_walk(state, <cdatrie.AlphaChar> char):
                    return
                if cdatrie.trie_state_is_terminal(state): # word is found
                    yield key[:index], cdatrie.trie_state_get_terminal_data(state)
                index += 1
        finally:
            cdatrie.trie_state_free(state)

    def iter_prefix_values(self, unicode key):
        '''
        Returns an iterator over the values of this trie that are associated
        with keys that are prefixes of ``key``.
        '''
        cdef cdatrie.TrieState* state = cdatrie.trie_root(self._c_trie)

        if state == NULL:
            raise MemoryError()

        try:
            for char in key:
                if not cdatrie.trie_state_walk(state, <cdatrie.AlphaChar> char):
                    return
                if cdatrie.trie_state_is_terminal(state):
                    yield cdatrie.trie_state_get_terminal_data(state)
        finally:
            cdatrie.trie_state_free(state)

    def prefixes(self, unicode key):
        '''
        Returns a list with keys of this trie that are prefixes of ``key``.
        '''
        cdef cdatrie.TrieState* state = cdatrie.trie_root(self._c_trie)
        if state == NULL:
            raise MemoryError()

        cdef list result = []
        cdef int index = 1
        try:
            for char in key:
                if not cdatrie.trie_state_walk(state, <cdatrie.AlphaChar> char):
                    break
                if cdatrie.trie_state_is_terminal(state):
                    result.append(key[:index])
                index += 1
            return result
        finally:
            cdatrie.trie_state_free(state)

    cpdef suffixes(self, unicode prefix=u''):
        """
        Returns a list of this trie's suffixes.
        If ``prefix`` is not empty, returns only the suffixes of words prefixed by ``prefix``.
        """
        cdef bint success
        cdef list res = []
        cdef BaseState state = BaseState(self)

        if prefix is not None:
            success = state.walk(prefix)
            if not success:
                return res

        cdef BaseIterator iter = BaseIterator(state)
        while iter.next():
            res.append(iter.key())

        return res

    def prefix_items(self, unicode key):
        '''
        Returns a list of the items (``(key,value)`` tuples)
        of this trie that are associated with keys that are
        prefixes of ``key``.
        '''
        return self._prefix_items(key)

    cdef list _prefix_items(self, unicode key):
        cdef cdatrie.TrieState* state = cdatrie.trie_root(self._c_trie)

        if state == NULL:
            raise MemoryError()

        cdef list result = []
        cdef int index = 1
        try:
            for char in key:
                if not cdatrie.trie_state_walk(state, <cdatrie.AlphaChar> char):
                    break
                if cdatrie.trie_state_is_terminal(state): # word is found
                    result.append(
                        (key[:index],
                         cdatrie.trie_state_get_terminal_data(state))
                    )
                index += 1
            return result
        finally:
            cdatrie.trie_state_free(state)

    def prefix_values(self, unicode key):
        '''
        Returns a list of the values of this trie that are associated
        with keys that are prefixes of ``key``.
        '''
        return self._prefix_values(key)

    cdef list _prefix_values(self, unicode key):
        cdef cdatrie.TrieState* state = cdatrie.trie_root(self._c_trie)

        if state == NULL:
            raise MemoryError()

        cdef list result = []
        try:
            for char in key:
                if not cdatrie.trie_state_walk(state, <cdatrie.AlphaChar> char):
                    break
                if cdatrie.trie_state_is_terminal(state): # word is found
                    result.append(cdatrie.trie_state_get_terminal_data(state))
            return result
        finally:
            cdatrie.trie_state_free(state)

    def longest_prefix(self, unicode key, default=None):
        """
        Returns the longest key in this trie that is a prefix of ``key``.

        If the trie doesn't contain any prefix of ``key``:
          - if ``default`` is given, returns it,
          - otherwise raises ``KeyError``.
        """
        cdef cdatrie.TrieState* state = cdatrie.trie_root(self._c_trie)

        if state == NULL:
            raise MemoryError()

        cdef int index = 0, last_terminal_index = 0

        try:
            for ch in key:
                if not cdatrie.trie_state_walk(state, <cdatrie.AlphaChar> ch):
                    break

                index += 1
                if cdatrie.trie_state_is_terminal(state):
                    last_terminal_index = index

            if not last_terminal_index:
                if default is RAISE_KEY_ERROR:
                    raise KeyError(key)
                return default

            return key[:last_terminal_index]
        finally:
            cdatrie.trie_state_free(state)

    def longest_prefix_item(self, unicode key, default=None):
        """
        Returns the item (``(key,value)`` tuple) associated with the longest
        key in this trie that is a prefix of ``key``.

        If the trie doesn't contain any prefix of ``key``:
          - if ``default`` is given, returns it,
          - otherwise raises ``KeyError``.
        """
        return self._longest_prefix_item(key, default)

    cdef _longest_prefix_item(self, unicode key, default=None):
        cdef cdatrie.TrieState* state = cdatrie.trie_root(self._c_trie)

        if state == NULL:
            raise MemoryError()

        cdef int index = 0, last_terminal_index = 0, data

        try:
            for ch in key:
                if not cdatrie.trie_state_walk(state, <cdatrie.AlphaChar> ch):
                    break

                index += 1
                if cdatrie.trie_state_is_terminal(state):
                    last_terminal_index = index
                    data = cdatrie.trie_state_get_terminal_data(state)

            if not last_terminal_index:
                if default is RAISE_KEY_ERROR:
                    raise KeyError(key)
                return default

            return key[:last_terminal_index], data

        finally:
            cdatrie.trie_state_free(state)

    def longest_prefix_value(self, unicode key, default=None):
        """
        Returns the value associated with the longest key in this trie that is
        a prefix of ``key``.

        If the trie doesn't contain any prefix of ``key``:
          - if ``default`` is given, return it
          - otherwise raise ``KeyError``
        """
        return self._longest_prefix_value(key, default)

    cdef _longest_prefix_value(self, unicode key, default=None):
        cdef cdatrie.TrieState* state = cdatrie.trie_root(self._c_trie)

        if state == NULL:
            raise MemoryError()

        cdef int data = 0
        cdef char found = 0

        try:
            for ch in key:
                if not cdatrie.trie_state_walk(state, <cdatrie.AlphaChar> ch):
                    break

                if cdatrie.trie_state_is_terminal(state):
                    found = 1
                    data = cdatrie.trie_state_get_terminal_data(state)

            if not found:
                if default is RAISE_KEY_ERROR:
                    raise KeyError(key)
                return default

            return data

        finally:
            cdatrie.trie_state_free(state)

    def has_keys_with_prefix(self, unicode prefix):
        """
        Returns True if any key in the trie begins with ``prefix``.
        """
        cdef cdatrie.TrieState* state = cdatrie.trie_root(self._c_trie)
        if state == NULL:
            raise MemoryError()
        try:
            for char in prefix:
                if not cdatrie.trie_state_walk(state, <cdatrie.AlphaChar> char):
                    return False
            return True
        finally:
            cdatrie.trie_state_free(state)

    cpdef items(self, unicode prefix=None):
        """
        Returns a list of this trie's items (``(key,value)`` tuples).

        If ``prefix`` is not None, returns only the items
        associated with keys prefixed by ``prefix``.
        """
        cdef bint success
        cdef list res = []
        cdef BaseState state = BaseState(self)

        if prefix is not None:
            success = state.walk(prefix)
            if not success:
                return res

        cdef BaseIterator iter = BaseIterator(state)

        if prefix is None:
            while iter.next():
                res.append((iter.key(), iter.data()))
        else:
            while iter.next():
                res.append((prefix+iter.key(), iter.data()))

        return res

    def __iter__(self):
        cdef BaseIterator iter = BaseIterator(BaseState(self))
        while iter.next():
            yield iter.key()

    cpdef keys(self, unicode prefix=None):
        """
        Returns a list of this trie's keys.

        If ``prefix`` is not None, returns only the keys prefixed by ``prefix``.
        """
        cdef bint success
        cdef list res = []
        cdef BaseState state = BaseState(self)

        if prefix is not None:
            success = state.walk(prefix)
            if not success:
                return res

        cdef BaseIterator iter = BaseIterator(state)

        if prefix is None:
            while iter.next():
                res.append(iter.key())
        else:
            while iter.next():
                res.append(prefix+iter.key())

        return res

    cpdef values(self, unicode prefix=None):
        """
        Returns a list of this trie's values.

        If ``prefix`` is not None, returns only the values
        associated with keys prefixed by ``prefix``.
        """
        cdef bint success
        cdef list res = []
        cdef BaseState state = BaseState(self)

        if prefix is not None:
            success = state.walk(prefix)
            if not success:
                return res

        cdef BaseIterator iter = BaseIterator(state)
        while iter.next():
            res.append(iter.data())
        return res

    cdef _index_to_value(self, cdatrie.TrieData index):
        return index


cdef class Trie(BaseTrie):
    """
    Wrapper for libdatrie's trie.
    Keys are unicode strings, values are Python objects.
    """

    cdef list _values

    def __init__(self, alphabet=None, ranges=None, AlphaMap alpha_map=None, _create=True):
        """
        For efficiency trie needs to know what unicode symbols
        it should be able to store so this constructor requires
        either ``alphabet`` (a string/iterable with all allowed characters),
        ``ranges`` (a list of (begin, end) pairs, e.g. [('a', 'z')])
        or ``alpha_map`` (:class:`datrie.AlphaMap` instance).
        """
        self._values = []
        super(Trie, self).__init__(alphabet, ranges, alpha_map, _create)

    def __reduce__(self):
        with tempfile.NamedTemporaryFile() as f:
            self.write(f)
            pickle.dump(self._values, f)
            f.seek(0)
            state = f.read()
            return Trie, (None, None, None, False), state

    def __setstate__(self, bytes state):
        assert self._c_trie is NULL
        with tempfile.NamedTemporaryFile() as f:
            f.write(state)
            f.flush()
            f.seek(0)
            self._c_trie = _load_from_file(f)
            self._values = pickle.load(f)

    def __getitem__(self, unicode key):
        cdef cdatrie.TrieData index = self._getitem(key)
        return self._values[index]

    def __setitem__(self, unicode key, object value):
        cdef cdatrie.TrieData next_index = len(self._values)
        cdef cdatrie.TrieData index = self._setdefault(key, next_index)
        if index == next_index:
            self._values.append(value)   # insert
        else:
            self._values[index] = value  # update

    def setdefault(self, unicode key, object value):
        cdef cdatrie.TrieData next_index = len(self._values)
        cdef cdatrie.TrieData index = self._setdefault(key, next_index)
        if index == next_index:
            self._values.append(value)   # insert
            return value
        else:
            return self._values[index]   # lookup

    def __delitem__(self, unicode key):
        # XXX: this could be faster (key is encoded twice here)
        cdef cdatrie.TrieData index = self._getitem(key)
        self._values[index] = DELETED_OBJECT
        self._delitem(key)

    def write(self, f):
        """
        Writes a trie to a file. File-like objects without real
        file descriptors are not supported.
        """
        super(Trie, self).write(f)
        pickle.dump(self._values, f)

    @classmethod
    def read(cls, f):
        """
        Creates a new Trie by reading it from file.
        File-like objects without real file descriptors are not supported.
        """
        cdef Trie trie = super(Trie, cls).read(f)
        trie._values = pickle.load(f)
        return trie

    cpdef items(self, unicode prefix=None):
        """
        Returns a list of this trie's items (``(key,value)`` tuples).

        If ``prefix`` is not None, returns only the items
        associated with keys prefixed by ``prefix``.
        """

        # the following code is
        #
        #    [(k, self._values[v]) for (k,v) in BaseTrie.items(self, prefix)]
        #
        # but inlined for speed.

        cdef bint success
        cdef list res = []
        cdef BaseState state = BaseState(self)

        if prefix is not None:
            success = state.walk(prefix)
            if not success:
                return res

        cdef BaseIterator iter = BaseIterator(state)

        if prefix is None:
            while iter.next():
                res.append((iter.key(), self._values[iter.data()]))
        else:
            while iter.next():
                res.append((prefix+iter.key(), self._values[iter.data()]))

        return res

    cpdef values(self, unicode prefix=None):
        """
        Returns a list of this trie's values.

        If ``prefix`` is not None, returns only the values
        associated with keys prefixed by ``prefix``.
        """

        # the following code is
        #
        #     [self._values[v] for v in BaseTrie.values(self, prefix)]
        #
        # but inlined for speed.

        cdef list res = []
        cdef BaseState state = BaseState(self)
        cdef bint success

        if prefix is not None:
            success = state.walk(prefix)
            if not success:
                return res

        cdef BaseIterator iter = BaseIterator(state)

        while iter.next():
            res.append(self._values[iter.data()])

        return res

    def longest_prefix_item(self, unicode key, default=None):
        """
        Returns the item (``(key,value)`` tuple) associated with the longest
        key in this trie that is a prefix of ``key``.

        If the trie doesn't contain any prefix of ``key``:
          - if ``default`` is given, returns it,
          - otherwise raises ``KeyError``.
        """
        cdef res = self._longest_prefix_item(key, RERAISE_KEY_ERROR)
        if res is RERAISE_KEY_ERROR: # error
            if default is RAISE_KEY_ERROR:
                raise KeyError(key)
            return default

        return res[0], self._values[res[1]]

    def longest_prefix_value(self, unicode key, default=None):
        """
        Returns the value associated with the longest key in this trie that is
        a prefix of ``key``.

        If the trie doesn't contain any prefix of ``key``:
          - if ``default`` is given, return it
          - otherwise raise ``KeyError``
        """
        cdef res = self._longest_prefix_value(key, RERAISE_KEY_ERROR)
        if res is RERAISE_KEY_ERROR: # error
            if default is RAISE_KEY_ERROR:
                raise KeyError(key)
            return default

        return self._values[res]

    def prefix_items(self, unicode key):
        '''
        Returns a list of the items (``(key,value)`` tuples)
        of this trie that are associated with keys that are
        prefixes of ``key``.
        '''
        return [(k, self._values[v]) for (k, v) in self._prefix_items(key)]

    def iter_prefix_items(self, unicode key):
        for k, v in super(Trie, self).iter_prefix_items(key):
            yield k, self._values[v]

    def prefix_values(self, unicode key):
        '''
        Returns a list of the values of this trie that are associated
        with keys that are prefixes of ``key``.
        '''
        return [self._values[v] for v in self._prefix_values(key)]

    def iter_prefix_values(self, unicode key):
        for v in super(Trie, self).iter_prefix_values(key):
            yield self._values[v]

    cdef _index_to_value(self, cdatrie.TrieData index):
        return self._values[index]


cdef class _TrieState:
    cdef cdatrie.TrieState* _state
    cdef BaseTrie _trie

    def __cinit__(self, BaseTrie trie):
        self._state = cdatrie.trie_root(trie._c_trie)
        if self._state is NULL:
            raise MemoryError()
        self._trie = trie

    def __dealloc__(self):
        if self._state is not NULL:
            cdatrie.trie_state_free(self._state)

    cpdef walk(self, unicode to):
        cdef bint res
        for ch in to:
            if not self.walk_char(<cdatrie.AlphaChar> ch):
                return False
        return True

    cdef bint walk_char(self, cdatrie.AlphaChar char):
        """
        Walks the trie stepwise, using a given character ``char``.
        On return, the state is updated to the new state if successfully walked.
        Returns boolean value indicating the success of the walk.
        """
        return cdatrie.trie_state_walk(self._state, char)

    cpdef copy_to(self, _TrieState state):
        """ Copies trie state to another """
        cdatrie.trie_state_copy(state._state, self._state)

    cpdef rewind(self):
        """ Puts the state at root """
        cdatrie.trie_state_rewind(self._state)

    cpdef bint is_terminal(self):
        return cdatrie.trie_state_is_terminal(self._state)

    cpdef bint is_single(self):
        return cdatrie.trie_state_is_single(self._state)

    cpdef bint is_leaf(self):
        return cdatrie.trie_state_is_leaf(self._state)

    def __unicode__(self):
        return u"data:%d, term:%s, leaf:%s, single: %s" % (
            self.data(),
            self.is_terminal(),
            self.is_leaf(),
            self.is_single(),
        )

    def __repr__(self):
        return self.__unicode__()  # XXX: this is incorrect under Python 2.x


cdef class BaseState(_TrieState):
    """
    cdatrie.TrieState wrapper. It can be used for custom trie traversal.
    """
    cpdef int data(self):
        return cdatrie.trie_state_get_terminal_data(self._state)


cdef class State(_TrieState):

    def __cinit__(self, Trie trie): # this is overriden for extra type check
        self._state = cdatrie.trie_root(trie._c_trie)
        if self._state is NULL:
            raise MemoryError()
        self._trie = trie

    cpdef data(self):
        cdef cdatrie.TrieData data = cdatrie.trie_state_get_terminal_data(self._state)
        return self._trie._index_to_value(data)


cdef class _TrieIterator:
    cdef cdatrie.TrieIterator* _iter
    cdef _TrieState _root

    def __cinit__(self, _TrieState state):
        self._root = state # prevent garbage collection of state
        self._iter = cdatrie.trie_iterator_new(state._state)
        if self._iter is NULL:
            raise MemoryError()

    def __dealloc__(self):
        if self._iter is not NULL:
            cdatrie.trie_iterator_free(self._iter)

    cpdef bint next(self):
        return cdatrie.trie_iterator_next(self._iter)

    cpdef unicode key(self):
        cdef cdatrie.AlphaChar* key = cdatrie.trie_iterator_get_key(self._iter)
        try:
            return unicode_from_alpha_char(key)
        finally:
            FREE(key)


cdef class BaseIterator(_TrieIterator):
    """
    cdatrie.TrieIterator wrapper. It can be used for custom datrie.BaseTrie
    traversal.
    """
    cpdef cdatrie.TrieData data(self):
        return cdatrie.trie_iterator_get_data(self._iter)


cdef class Iterator(_TrieIterator):
    """
    cdatrie.TrieIterator wrapper. It can be used for custom datrie.Trie
    traversal.
    """
    def __cinit__(self, State state): # this is overriden for extra type check
        self._root = state # prevent garbage collection of state
        self._iter = cdatrie.trie_iterator_new(state._state)
        if self._iter is NULL:
            raise MemoryError()

    cpdef data(self):
        cdef cdatrie.TrieData data = cdatrie.trie_iterator_get_data(self._iter)
        return self._root._trie._index_to_value(data)


cdef (cdatrie.Trie* ) _load_from_file(f) except NULL:
    cdef int fd = f.fileno()
    cdef stdio.FILE* f_ptr = stdio_ext.fdopen(fd, "r")
    if f_ptr == NULL:
        raise IOError()
    cdef cdatrie.Trie* trie = cdatrie.trie_fread(f_ptr)
    if trie == NULL:
        raise DatrieError("Can't load trie from stream")

    cdef int f_pos = stdio.ftell(f_ptr)
    f.seek(f_pos)

    return trie

#cdef (cdatrie.Trie*) _load_from_file(path) except NULL:
#    str_path = path.encode(sys.getfilesystemencoding())
#    cdef char* c_path = str_path
#    cdef cdatrie.Trie* trie = cdatrie.trie_new_from_file(c_path)
#    if trie is NULL:
#        raise DatrieError("Can't load trie from file")
#
#    return trie


# ============================ AlphaMap & utils ================================

cdef class AlphaMap:
    """
    Alphabet map.

    For sparse data compactness, the trie alphabet set should
    be continuous, but that is usually not the case in general
    character sets. Therefore, a map between the input character
    and the low-level alphabet set for the trie is created in the
    middle. You will have to define your input character set by
    listing their continuous ranges of character codes creating a
    trie. Then, each character will be automatically assigned
    internal codes of continuous values.
    """

    cdef cdatrie.AlphaMap *_c_alpha_map

    def __cinit__(self):
        self._c_alpha_map = cdatrie.alpha_map_new()

    def __dealloc__(self):
        if self._c_alpha_map is not NULL:
            cdatrie.alpha_map_free(self._c_alpha_map)

    def __init__(self, alphabet=None, ranges=None, _create=True):
        if not _create:
            return

        if ranges is not None:
            for range in ranges:
                self.add_range(*range)

        if alphabet is not None:
            self.add_alphabet(alphabet)

    cdef AlphaMap copy(self):
        cdef AlphaMap clone = AlphaMap(_create=False)
        clone._c_alpha_map = cdatrie.alpha_map_clone(self._c_alpha_map)
        if clone._c_alpha_map is NULL:
            raise MemoryError()

        return clone

    def add_alphabet(self, alphabet):
        """
        Adds all chars from iterable to the alphabet set.
        """
        for begin, end in alphabet_to_ranges(alphabet):
            self._add_range(begin, end)

    def add_range(self, begin, end):
        """
        Add a range of character codes from ``begin`` to ``end``
        to the alphabet set.

        ``begin`` - the first character of the range;
        ``end`` - the last character of the range.
        """
        self._add_range(ord(begin), ord(end))

    cpdef _add_range(self, cdatrie.AlphaChar begin, cdatrie.AlphaChar end):
        if begin > end:
            raise DatrieError('range begin > end')
        code = cdatrie.alpha_map_add_range(self._c_alpha_map, begin, end)
        if code != 0:
            raise MemoryError()


cdef cdatrie.AlphaChar* new_alpha_char_from_unicode(unicode txt):
    """
    Converts Python unicode string to libdatrie's AlphaChar* format.
    libdatrie wants null-terminated array of 4-byte LE symbols.

    The caller should free the result of this function.
    """
    cdef int txt_len = len(txt)
    cdef int size = (txt_len + 1) * sizeof(cdatrie.AlphaChar)

    # allocate buffer
    cdef cdatrie.AlphaChar* data = <cdatrie.AlphaChar*> MALLOC(size)
    if data is NULL:
        raise MemoryError()

    # Copy text contents to buffer.
    # XXX: is it safe? The safe alternative is to decode txt
    # to utf32_le and then use memcpy to copy the content:
    #
    #    py_str = txt.encode('utf_32_le')
    #    cdef char* c_str = py_str
    #    string.memcpy(data, c_str, size-1)
    #
    # but the following is much (say 10x) faster and this
    # function is really in a hot spot.
    cdef int i = 0
    for char in txt:
        data[i] = <cdatrie.AlphaChar> char
        i+=1

    # Buffer must be null-terminated (last 4 bytes must be zero).
    data[txt_len] = 0
    return data


cdef unicode unicode_from_alpha_char(cdatrie.AlphaChar* key, int len=0):
    """
    Converts libdatrie's AlphaChar* to Python unicode.
    """
    cdef int length = len
    if length == 0:
        length = cdatrie.alpha_char_strlen(key)*sizeof(cdatrie.AlphaChar)
    cdef char* c_str = <char*> key
    return c_str[:length].decode('utf-32-le')


def to_ranges(lst):
    """
    Converts a list of numbers to a list of ranges::

    >>> numbers = [1,2,3,5,6]
    >>> list(to_ranges(numbers))
    [(1, 3), (5, 6)]
    """
    for a, b in itertools.groupby(enumerate(lst), lambda t: t[1] - t[0]):
        b = list(b)
        yield b[0][1], b[-1][1]


def alphabet_to_ranges(alphabet):
    for begin, end in to_ranges(sorted(map(ord, iter(alphabet)))):
        yield begin, end


def new(alphabet=None, ranges=None, AlphaMap alpha_map=None):
    warnings.warn('datrie.new is deprecated; please use datrie.Trie.',
                  DeprecationWarning)
    return Trie(alphabet, ranges, alpha_map)


MutableMapping.register(Trie)
MutableMapping.register(BaseTrie)
