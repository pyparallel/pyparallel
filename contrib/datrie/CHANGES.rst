
CHANGES
=======

0.7 (2014-02-18)
----------------

* bundled libdatrie C library is updated to version 0.2.8;
* new `.suffixes()` method (thanks Ahmed T. Youssef);
* wrapper is rebuilt with Cython 0.20.1.

0.6.1 (2013-09-21)
------------------

* fixed build for Visual Studio (thanks Gabi Davar).

0.6 (2013-07-09)
----------------

* datrie is rebuilt with Cython 0.19.1;
* ``iter_prefix_values``, ``prefix_values`` and ``longest_prefix_value``
  methods for ``datrie.BaseTrie`` and ``datrie.Trie`` (thanks Jared Suttles).

0.5.1 (2013-01-30)
------------------

* Recently introduced memory leak in ``longest_prefix``
  and ``longest_prefix_item`` is fixed.

0.5 (2013-01-29)
----------------

* ``longest_prefix`` and ``longest_prefix_item`` methods are fixed;
* datrie is rebuilt with Cython 0.18;
* misleading benchmark results in README are fixed;
* State._walk is renamed to State.walk_char.

0.4.2 (2012-09-02)
------------------

* Update to latest libdatrie; this makes ``.keys()`` method a bit slower but
  removes a keys length limitation.

0.4.1 (2012-07-29)
------------------

* cPickle is used for saving/loading ``datrie.Trie`` if it is available.

0.4 (2012-07-27)
----------------

* ``libdatrie`` improvements and bugfixes, including C iterator API support;
* custom iteration support using ``datrie.State`` and ``datrie.Iterator``.
* speed improvements: ``__length__``, ``keys``, ``values`` and
  ``items`` methods should be up to 2x faster.
* keys longer than 32768 are not supported in this release.


0.3 (2012-07-21)
----------------

There are no new features or speed improvements in this release.

* ``datrie.new`` is deprecated; use ``datrie.Trie`` with the same arguments;
* small test & benchmark improvements.

0.2 (2012-07-16)
----------------

* ``datrie.Trie`` items can have any Python object as a value
  (``Trie`` from 0.1.x becomes ``datrie.BaseTrie``);
* ``longest_prefix`` and ``longest_prefix_items`` are fixed;
* ``save`` & ``load`` are rewritten;
* ``setdefault`` method.


0.1.1 (2012-07-13)
------------------

* Windows support (upstream libdatrie changes are merged);
* license is changed from LGPL v3 to LGPL v2.1 to match the libdatrie license.

0.1 (2012-07-12)
----------------

Initial release.
