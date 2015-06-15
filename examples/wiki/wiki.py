"""
A very simple wiki search HTTP server that demonstrates useful techniques
afforded by PyParallel: the ability to load large reference data structures
into memory, and then query them as part of incoming request processing in
parallel.
"""
#===============================================================================
# Imports
#===============================================================================
import json
import async
import socket
import datrie
import string
import urllib
import numpy as np

from collections import (
    defaultdict,
)

from functools import (
    partial,
)

from numpy import (
    uint64,
)

from async import (
    rdtsc,

    sys_stats,
    socket_stats,
    memory_stats,
    context_stats,

    call_from_main_thread,
    call_from_main_thread_and_wait,

    CachingBehavior,
)

from async.http.server import (
    Request,
    HttpServer,
    RangedRequest,
)

from os.path import (
    join,
    exists,
    abspath,
    dirname,
    normpath,
)

def join_path(*args):
    return abspath(normpath(join(*args)))

#===============================================================================
# Configurables -- Change These!
#===============================================================================
# Change this to the directory containing the downloaded files.
#DATA_DIR = r'd:\data'
DATA_DIR = join_path(dirname(__file__), 'data')

# If you want to change the hostname listened on from the default (which will
# resolve to whatever IP address the computer name resolves to), do so here.
HOSTNAME = socket.gethostname()
# E.g.:
# HOSTNAME = 'localhost'
IPADDR = '0.0.0.0'
PORT = 8080

#===============================================================================
# Constants
#===============================================================================

# This file is huge when unzipped -- ~53GB.  Although, granted, it is the
# entire Wikipedia in a single file.  The bz2 version is much smaller, but
# still pretty huge.  Search the web for instructions on how to download
# from one of the wiki mirrors, then bunzip2 it, then place in the same
# data directory.
WIKI_XML_NAME = 'enwiki-20150205-pages-articles.xml'
WIKI_XML_PATH = join_path(DATA_DIR, WIKI_XML_NAME)

# The following two files can be downloaded from
# http://download.pyparallel.org.

# This is a trie keyed by every <title>xxx</title> in the wiki XML; the value
# is a 64-bit byte offset within the file where the title was found.
# Specifically, it is the offset where the '<' bit of the <title> was found.
TITLES_TRIE_PATH = join_path(DATA_DIR, 'titles.trie')
# And because this file is so huge and the data structure takes so long to
# load, we have another version that was created for titles starting with Z
# and z (I picked 'z' as I figured it would have the least-ish titles).  (This
# file was created via the save_titles_startingwith_z() method below.)
ZTITLES_TRIE_PATH = join_path(DATA_DIR, 'ztitles.trie')

# This is a sorted numpy array of uint64s representing the byte offset values
# in the trie.  When given the byte offset of a title derived from a trie
# lookup, we can find the byte offset of where the next title starts within
# the xml file.  That allows us to isolate the required byte range from the
# xml file where the particular title is defined.  Such a byte range can be
# satisfied with a ranged HTTP request.
TITLES_OFFSETS_NPY_PATH = join_path(DATA_DIR, 'titles_offsets.npy')

#===============================================================================
# Aliases
#===============================================================================
uint64_7 = uint64(7)
uint64_11 = uint64(11)
#===============================================================================
# Globals
#===============================================================================
#wiki_xml = async.open(WIKI_XML_PATH, 'r', caching=CachingBehavior.RandomAccess)

offsets = np.load(TITLES_OFFSETS_NPY_PATH)

# Use the smaller one if the larger one doesn't exist.
if not exists(TITLES_TRIE_PATH):
    TRIE_PATH = ZTITLES_TRIE_PATH
else:
    TRIE_PATH = TITLES_TRIE_PATH

print("About to load titles trie, this will take a while...")
titles = datrie.Trie.load(TRIE_PATH)

#===============================================================================
# Misc Helpers
#===============================================================================
def save_titles_startingwith_z():
    # Ok, the 11GB trie that takes 2 minutes to load is painful to develop
    # with; let's whip up a little helper that just works on titles starting
    # with 'Z'.
    path = TITLES_TRIE_PATH.replace('titles.', 'ztitles.')
    allowed = (string.printable + string.punctuation)
    ztrie = datrie.Trie(allowed)
    for c in ('Z', 'z'):
        for (key, value) in titles.items(c):
            if key in ztrie:
                existing = ztrie[key]
                for v in value:
                    if v not in existing:
                        existing.append(v)
                        existing.sort()
            else:
                ztrie[key] = value
    ztrie.save(path)

def json_serialization(request=None, obj=None):
    """
    Helper method for converting a dict `obj` into a JSON response for the
    incoming `request`.
    """
    transport = None
    if not request:
        request = Request(transport=None, data=None)
    else:
        transport = request.transport
    if not obj:
        obj = {}
    #async.debug('obj: %r' % obj)
    response = request.response
    response.code = 200
    response.message = 'OK'
    response.content_type = 'application/json; charset=UTF-8'
    response.body = json.dumps(obj)

    return request

def text_serialization(request=None, text=None):
    transport = None
    if not request:
        request = Request(transport=None, data=None)
    else:
        transport = request.transport
    if not text:
        text = 'Hello, World!'
    response = request.response
    response.code = 200
    response.message = 'OK'
    response.content_type = 'text/plain; charset=UTF-8'
    response.body = text

    return request


#===============================================================================
# Offset Helpers
#===============================================================================

# Three implementations of the same functionality: given a key, look up all
# items in the trie starting with that key, then return the relevant offsets
# for each one, such that a client can then issue a ranged HTTP request for
# the bytes returned.
#   >>> results = titles.items('Ap')
#   >>> len(results)
#   16333
#
# So, how long does it take to construct the result set for 16,333 hits?
# (That is, 16,333 Wikipedia pages whose page title starts with 'Ap'.)
#
#   >>> from ctk.util import timer
#   >>> with timer.timeit():
#   ...     _ = dumps(get_page_offsets_for_key2('Ap'))
#   ...
#   274ms

#   >>> with timer.timeit():
#   ...     _ = dumps(get_page_offsets_for_key('Ap'))
#   ...
#   278ms

#   >>> with timer.timeit():
#   ...     _ = dumps(get_page_offsets_for_key3('Ap'))
#   ...
#   256ms

def get_page_offsets_for_key(search_string):
    items = titles.items(search_string)
    results = defaultdict(list)
    for (key, value) in items:
        for v in value:
            o = uint64(v if v > 0 else v*-1)
            ix = offsets.searchsorted(o, side='right')
            results[key].append((int(o-uint64_7), int(offsets[ix]-uint64_11)))
    return results or None

def get_page_offsets_for_key2(search_string):
    items = titles.items(search_string)
    if not items:
        return None
    results = [ [None, None, None] for _ in range(0, len(items)) ]
    assert len(results) == len(items), (len(results), len(items))
    for (i, pair) in enumerate(items):
        (key, value) = pair
        for (j, v) in enumerate(value):
            rx = i + j
            o = uint64(v if v > 0 else v*-1)
            ix = offsets.searchsorted(o, side='right')
            results[rx][0] = key
            results[rx][1] = int(o-uint64_7)
            results[rx][2] = int(offsets[ix]-uint64_11)
    return results

def get_page_offsets_for_key3(search_string):
    results = []
    items = titles.items(search_string)
    if not items:
        return results
    for (key, value) in items:
        v = value[0]
        o = uint64(v if v > 0 else v*-1)
        ix = offsets.searchsorted(o, side='right')
        results.append((key, int(o-uint64_7), int(offsets[ix]-uint64_11)))
    return results

#===============================================================================
# Web Helpers
#===============================================================================
def exact_title(title):
    if title in titles:
        return json.dumps([[title, ] + [ t for t in titles[title] ]])
    else:
        return json.dumps([])

#===============================================================================
# Classes
#===============================================================================
class NotTrie(dict):
    def longest_prefix_value(self, path):
        p = path[1:]
        if not p:
            return
        ix = p.find('/')
        if ix == -1:
            return self.get(path)
        key = path[:ix+1]
        return self.get(key)

#routes = NotTrie()

class route:
    routes = datrie.Trie(string.ascii_lowercase + '/')
    def __init__(self, func, *args, **kwds):
        self.func = func
        self.args = args
        self.kwds = kwds
        self.path = None
        self.funcname = func.__code__.co_name

        if not args:
            self.path = self.funcname
        else:
            self.path = args[0]

        if self.path[0] != '/':
            self.path = '/' + self.path

        self.routes[self.path] = self.funcname

    def __get__(self, obj, objtype=None):
        if not obj:
            return self.func
        return partial(self, obj)

    def __call__(self, *_args, **_kwds):
        obj = _args[0]
        request = _args[1]
        # This will be the full path received, minus query string and fragment,
        # e.g. '/offsets/Python'.
        path = request.path[len(self.path):]

        # In the case of '/offsets/Python', that'll leave us with '/Python',
        # and we want to lop off the slash.  In the case of, say, '/stats',
        # path will be empty.
        if path and path[0] == '/':
            if len(path) > 1:
                path = path[1:]
            else:
                path = ''

        # And that's it, the new path is passed to the callable as the second
        # positional parameter.  If a fragment was present, pass that next.
        # Then pass a query string as **kwds if present.
        args = []
        if path:
            path = urllib.parse.unquote(path)
            args.append(path)
        if request.fragment:
            args.append(fragment)

        try:
            result = self.func(obj, request, *args, **request.query)
            return result
        except TypeError:
            try:
                # Try without query string **kwds.
                return self.func(obj, request, *args)
            except TypeError:
                # Try without fragment.
                try:
                    return self.func(obj, request, path)
                except TypeError:
                    # And finally, try without path.
                    return self.func(obj, request)


class WikiServer(HttpServer):

    @route
    def wiki(self, request, name, **kwds):
        # Do an exact lookup if we find a match.
        if name not in titles:
            return self.error(request, 404)

        o = titles[name][0]
        o = uint64(o if o > 0 else o*-1)
        ix = offsets.searchsorted(o, side='right')
        start = int(o-uint64_7)
        end = int(offsets[ix]-uint64_11)
        range_request = '%d-%d' % (start, end)
        request.range = RangedRequest(range_request)
        request.response.content_type = 'text/xml; charset=utf-8'
        return self.sendfile(request, WIKI_XML_PATH)

    @route
    def offsets(self, request, name, limit=None):
        if not name:
            return self.error(request, 400, "Missing name")

        if len(name) < 3:
            return self.error(request, 400, "Name too short (< 3 chars)")

        return json_serialization(request, get_page_offsets_for_key3(name))

    @route
    def xml(self, request, *args, **kwds):
        if not request.range:
            return self.error(request, 400, "Ranged-request required.")
        else:
            request.response.content_type = 'text/xml; charset=utf-8'
            return self.sendfile(request, WIKI_XML_PATH)

    @route
    def stats(self, request, *args, **kwds):
        stats = {
            'system': dict(sys_stats()),
            'server': dict(socket_stats(request.transport.parent)),
            'memory': dict(memory_stats()),
            'contexts': dict(context_stats()),
            'elapsed': request.transport.elapsed(),
            'thread': async.thread_seq_id(),
        }
        if args:
            name = args[0]
            if name in stats:
                stats = { name: stats[name] }
        return json_serialization(request, stats)

    @route
    def hello(self, request, *args, **kwds):
        j = { 'args': args, 'kwds': kwds }
        return json_serialization(request, j)

    @route
    def title(self, request, name, *args, **kwds):
        items = titles.items(name)
        return json_serialization(request, items)

    @property
    def routes(self):
        return route.routes

    @route
    def elapsed(self, request, *args, **kwds):
        obj = { 'elapsed': request.transport.elapsed() }
        return json_serialization(obj)

    @route
    def json(self, request, *args, **kwds):
        return json_serialization(request, {'message': 'Hello, World!'})

    @route
    def plaintext(self, request, *args, **kwds):
        return text_serialization(request, text='Hello, World!')

#===============================================================================
# Main
#===============================================================================

def main():
    server = async.server(IPADDR, PORT)
    async.register(transport=server, protocol=WikiServer)
    async.run_once()
    return server

if __name__ == '__main__':
    server = main()
    async.run()


# vim:set ts=8 sw=4 sts=4 tw=78 et:                                            #
