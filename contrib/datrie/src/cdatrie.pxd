# cython: profile=False
from libc cimport stdio

cdef extern from "../libdatrie/datrie/triedefs.h":
    ctypedef int AlphaChar # it should be utf32 letter
    ctypedef unsigned char TrieChar  # 1 byte
    ctypedef int TrieIndex
    ctypedef int TrieData  # int

cdef extern from "../libdatrie/datrie/alpha-map.h":

    struct AlphaMap:
        pass

    AlphaMap * alpha_map_new()
    void alpha_map_free (AlphaMap *alpha_map)
    AlphaMap * alpha_map_clone (AlphaMap *a_map)

    int alpha_map_add_range (AlphaMap *alpha_map, AlphaChar begin, AlphaChar end)
    int alpha_char_strlen (AlphaChar *str)


cdef extern from "../libdatrie/datrie/trie.h":

    ctypedef struct Trie:
        pass

    ctypedef struct TrieState:
        pass

    ctypedef struct TrieIterator:
        pass

    ctypedef int TrieData

    ctypedef bint (*TrieEnumFunc) (AlphaChar *key,
                                   TrieData key_data,
                                   void *user_data)

    int TRIE_CHAR_TERM
    int TRIE_DATA_ERROR

    # ========== GENERAL FUNCTIONS ==========

    Trie * trie_new (AlphaMap *alpha_map)

    Trie * trie_new_from_file (char *path)

    Trie * trie_fread (stdio.FILE *file)

    void trie_free (Trie *trie)

    int trie_save (Trie *trie, char *path)

    int trie_fwrite (Trie *trie, stdio.FILE *file)

    bint trie_is_dirty (Trie *trie)


    # =========== GENERAL QUERY OPERATIONS =========

    bint trie_retrieve (Trie *trie, AlphaChar *key, TrieData *o_data)

    bint trie_store (Trie *trie, AlphaChar *key, TrieData data)

    bint trie_store_if_absent (Trie *trie, AlphaChar *key, TrieData data)

    bint trie_delete (Trie *trie, AlphaChar *key)

    bint trie_enumerate (Trie *trie, TrieEnumFunc enum_func, void *user_data);

    # ======== STEPWISE QUERY OPERATIONS ========

    TrieState * trie_root (Trie *trie)


    # ========= TRIE STATE ===============

    TrieState * trie_state_clone (TrieState *s)

    void trie_state_copy (TrieState *dst, TrieState *src)

    void trie_state_free (TrieState *s)

    void trie_state_rewind (TrieState *s)

    bint trie_state_walk (TrieState *s, AlphaChar c)

    bint trie_state_is_walkable (TrieState *s, AlphaChar c)

    bint trie_state_is_terminal(TrieState * s)

    bint trie_state_is_single (TrieState *s)

    bint trie_state_is_leaf(TrieState* s)

    TrieData trie_state_get_data (TrieState *s)

    TrieData trie_state_get_terminal_data (TrieState *s)


    # ============== ITERATION ===================

    TrieIterator*   trie_iterator_new (TrieState *s)

    void            trie_iterator_free (TrieIterator *iter)

    bint            trie_iterator_next (TrieIterator *iter)

    AlphaChar *     trie_iterator_get_key (TrieIterator *iter)

    TrieData        trie_iterator_get_data (TrieIterator *iter)
