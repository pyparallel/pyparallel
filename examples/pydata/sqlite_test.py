#===============================================================================
# Imports
#===============================================================================
import json
import ujson
import sqlite3
import pyodbc
import datetime

import parallel

from parallel import (
    timer,
    gmtime,
    sys_stats,
    socket_stats,
    memory_stats,
    context_stats,
    thread_seq_id,
)

from parallel.http.server import (
    text_response,
    json_serialization,
    HttpServer,
)
#===============================================================================
# Globals
#===============================================================================

#===============================================================================
# Classes
#===============================================================================
def update_sys_stats():
    return ujson.dumps(sys_stats())

def update_stats(t):
    return ujson.dumps({
        'count': t.count,
        'timestamp': gmtime(),
        'memory': dict(memory_stats()),
        'contexts': dict(context_stats()),
        'thread': thread_seq_id(),
    }).encode('utf-8')

t = timer(datetime.timedelta(milliseconds=1), 1000, update_stats)
t.args = (t,)
#t.start()

class Foo(HttpServer):
    http11 = True
    dbname = 'geo.db'
    def plaintext(self, transport, data):
        return 'Hello, World!\n'

    def json(self, transport, data):
        return { 'message': 'Hello, world!\n' }

    def stats(self, transport, data):
        return t.data or b''

    def countries(self, transport, data):
        db = sqlite3.connect(self.dbname)
        cur = db.cursor()
        cur.execute("select name from country limit 10");
        results = cur.fetchall()
        return results
        #return json_serialization(request, results)

    def cities(self, transport, data):
        db = sqlite3.connect(self.dbname)
        cur = db.cursor()
        cur.execute("select * from city");
        results = cur.fetchall()
        return results

    def country(self, transport, data):
        db = sqlite3.connect(self.dbname)
        cur = db.cursor()
        cur.execute("select * from country");
        results = cur.fetchall()
        return results

class Bar(HttpServer):
    http11 = True
    #json_dumps = json.dumps
    #json_loads = json.loads
    dbname = 'geo.db'
    db = None
    _dict = None
    _bytes = None

    def client_created2(self, transport):
        parallel.debug("client_created\n")
        self._bytes = b'foo'
        #db = sqlite3.connect(':memory:')
        parallel.debug('copied\n')
        #self.d = { 'foo': 'bar' }
        #self.db = db
        #parallel.debug('bar')

    def client_created(self, transport):
        parallel.debug("client_created\n")
        #db = sqlite3.connect(':memory:')
        db = sqlite3.connect(self.dbname)
        parallel.debug('foo\n')
        self.db = db
        parallel.debug('bar\n')

    def _connection_made(self, transport):
        parallel.debug("connection_made!\n");
        if self._bytes:
            parallel.debug("bytes: %s\n" % self._bytes or '')
        else:
            parallel.debug("no bytes\n")

    def hello(self, transport, data):
        if self._bytes:
            parallel.debug("bytes: %s\n" % self._bytes or '')
        else:
            parallel.debug("no bytes\n")
        return 'Hello, World!\n'

    def json(self, transport, data):
        return { 'message': 'Hello, world!\n' }

    def countries(self, transport, data):
        #db = sqlite3.connect(self.dbname)
        parallel.debug("loading db...\n")
        cur = self.db.cursor()
        cur.execute("select name from country limit 10");
        results = cur.fetchall()
        return results
        #return json_serialization(request, results)

    def cities(self, transport, data):
        db = sqlite3.connect(self.dbname)
        cur = db.cursor()
        cur.execute("select * from city");
        results = cur.fetchall()
        return results

    def country(self, transport, data):
        db = sqlite3.connect(self.dbname)
        cur = db.cursor()
        cur.execute("select * from country");
        results = cur.fetchall()
        return results

class Bar2(HttpServer):
    http11 = True
    #json_dumps = json.dumps
    #json_loads = json.loads
    dbname = 'geo.db'
    db = None
    _dict = None
    _bytes = None

    def client_created(self, transport):
        parallel.debug("client_created\n")
        self._dict = { 'foo': 'bar' }
        #db = sqlite3.connect(':memory:')
        parallel.debug('copied\n')
        #self.d = { 'foo': 'bar' }
        #self.db = db
        #parallel.debug('bar')

    def client_created2(self, transport):
        parallel.debug("client_created")
        db = sqlite3.connect(':memory:')
        parallel.debug('foo')
        self.db = db
        parallel.debug('bar')

    def connection_made(self, transport):
        parallel.debug("connection_made!\n");
        if self._dict:
            parallel.debug("dict: %s\n" % str(self._dict or ''))
        else:
            parallel.debug("no dict\n")

    def get_hello(self, request):
        parallel.debug("get hello!\n")
        return text_response(request, 'hello\n')

    def hello(self, transport, data):
        if self._dict:
            parallel.debug("dict: %s\n" % str(self._dict or ''))
        else:
            parallel.debug("no dict\n")
        return 'Hello, World!\n'

    def json(self, transport, data):
        return { 'message': 'Hello, world!\n' }

    def countries(self, transport, data):
        db = sqlite3.connect(self.dbname)
        cur = db.cursor()
        cur.execute("select name from country limit 10");
        results = cur.fetchall()
        return results
        #return json_serialization(request, results)

    def cities(self, transport, data):
        db = sqlite3.connect(self.dbname)
        cur = db.cursor()
        cur.execute("select * from city");
        results = cur.fetchall()
        return results

    def country(self, transport, data):
        db = sqlite3.connect(self.dbname)
        cur = db.cursor()
        cur.execute("select * from country");
        results = cur.fetchall()
        return results

class Bar3(HttpServer):
    http11 = True
    #json_dumps = json.dumps
    #json_loads = json.loads
    dbname = 'geo.db'
    db = None
    _dict = None
    _bytes = None

    def client_created(self, transport):
        parallel.debug("client_created\n")
        self._dict = { 'foo': 'bar' }
        #db = sqlite3.connect(':memory:')
        parallel.debug('copied\n')
        #self.d = { 'foo': 'bar' }
        #self.db = db
        #parallel.debug('bar')

    def client_created2(self, transport):
        parallel.debug("client_created")
        db = sqlite3.connect(':memory:')
        parallel.debug('foo')
        self.db = db
        parallel.debug('bar')

    def connection_made(self, transport):
        parallel.debug("connection_made!\n");
        if self._dict:
            parallel.debug("dict: %s\n" % str(self._dict or ''))
        else:
            parallel.debug("no dict\n")

    def get_hello(self, request):
        parallel.debug("get hello!\n")
        return text_response(request, 'hello\n')

    def hello(self, transport, data):
        if self._dict:
            parallel.debug("dict: %s\n" % str(self._dict or ''))
        else:
            parallel.debug("no dict\n")
        return 'Hello, World!\n'

    def json(self, transport, data):
        return { 'message': 'Hello, world!\n' }

    def countries(self, transport, data):
        db = sqlite3.connect(self.dbname)
        cur = db.cursor()
        cur.execute("select name from country limit 10");
        results = cur.fetchall()
        return results
        #return json_serialization(request, results)

    def cities(self, transport, data):
        db = sqlite3.connect(self.dbname)
        cur = db.cursor()
        cur.execute("select * from city");
        results = cur.fetchall()
        return results

    def country(self, transport, data):
        db = sqlite3.connect(self.dbname)
        cur = db.cursor()
        cur.execute("select * from country");
        results = cur.fetchall()
        return results


class SqliteGeoHttpServer(HttpServer):
    #http11 = True
    dbname = 'geo.db'

    def get_countries(self, request):
        db = sqlite3.connect(self.dbname)
        cur = db.cursor()
        cur.execute("select * from country");
        results = cur.fetchall()
        return json_serialization(request, results)

    def get_cities_limit(self, request):
        db = sqlite3.connect(self.dbname)
        cur = db.cursor()
        cur.execute("select name from city limit 10");
        results = cur.fetchall()
        return json_serialization(request, results)

    def get_cities(self, request):
        db = sqlite3.connect(self.dbname)
        cur = db.cursor()
        cur.execute("select * from city");
        results = cur.fetchall()
        return json_serialization(request, results)

class SqliteWorldHttpServer(HttpServer):
    dbname = 'world.sqlite'

    def get_countries(self, request):
        db = sqlite3.connect(self.dbname)
        cur = db.cursor()
        cur.execute("select * from Countries");
        results = cur.fetchall()
        return json_serialization(request, results)

    def get_cities(self, request):
        db = sqlite3.connect(self.dbname)
        cur = db.cursor()
        cur.execute("select * from city");
        results = cur.fetchall()
        return json_serialization(request, results)

    def get_city(self, request, name=None):
        db = sqlite3.connect(self.dbname)
        cur = db.cursor()
        cur.execute("select * from city ");
        results = cur.fetchall()
        return json_serialization(request, results)

# vim:set ts=8 sw=4 sts=4 tw=80 et                                             :
