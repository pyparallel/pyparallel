AM_CPPFLAGS = -I$(top_srcdir)

TESTS_ENVIRONMENT = top_builddir=$(top_builddir)

TESTS = \
	test_walk \
	test_iterator \
	test_store-retrieve \
	test_file \
	test_nonalpha \
	test_null_trie \
	$(NULL)

check_PROGRAMS = \
	test_walk \
	test_iterator \
	test_store-retrieve \
	test_file \
	test_nonalpha \
	test_null_trie \
	$(NULL)

noinst_HEADERS = \
	utils.h \
	$(NULL)

test_walk_SOURCES = \
	test_walk.c \
	utils.c \
	$(NULL)
test_walk_LDADD = $(top_builddir)/datrie/libdatrie.la

test_iterator_SOURCES = \
	test_iterator.c \
	utils.c \
	$(NULL)
test_iterator_LDADD = $(top_builddir)/datrie/libdatrie.la

test_store_retrieve_SOURCES = \
	test_store-retrieve.c \
	utils.c \
	$(NULL)
test_store_retrieve_LDADD = $(top_builddir)/datrie/libdatrie.la

test_file_SOURCES = \
	test_file.c \
	utils.c \
	$(NULL)
test_file_LDADD = $(top_builddir)/datrie/libdatrie.la

test_nonalpha_SOURCES = \
	test_nonalpha.c \
	utils.c \
	$(NULL)
test_nonalpha_LDADD = $(top_builddir)/datrie/libdatrie.la

test_null_trie_SOURCES = \
	test_null_trie.c \
	utils.c \
	$(NULL)
test_null_trie_LDADD = $(top_builddir)/datrie/libdatrie.la
