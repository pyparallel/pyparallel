#===============================================================================
# Imports
#===============================================================================
import json
import ujson
import sqlite3
import pyodbc

import parallel

from parallel.http.server import (
    quote_html,
    text_response,
    html_response,
    json_serialization,
    HttpServer,
)
#===============================================================================
# Globals
#===============================================================================
localhost_connect_string = (
    'Driver={SQL Server};'
    'Server=localhost;'
    'Database=hello_world;'
    'Uid=benchmarkdbuser;'
    'Pwd=B3nchmarkDBPass;'
)

#===============================================================================
# Classes
#===============================================================================
class Fortune:
    _bytes = [
        (b'fortune: No such file or directory',                              1),
        (b"A computer scientist is someone who "
         b"fixes things that aren't broken.",                                2),
        (b'After enough decimal places, nobody gives a damn.',               3),
        (b'A bad random number generator: 1, 1, 1, 1, '
         b'1, 4.33e+67, 1, 1, 1',                                            4),
        (b'A computer program does what you tell it to do, '
         b'not what you want it to do.',                                     5),
        (b'Emacs is a nice operating system, but I prefer UNIX. '
         b'\xe2\x80\x94 Tom Christaensen',                                   6),
        (b'Any program that runs right is obsolete.',                        7),
        (b'A list is only as strong as its weakest link. '
         b'\xe2\x80\x94 Donald Knuth',                                       8),
        (b'Feature: A bug with seniority.',                                  9),
        (b'Computers make very fast, very accurate mistakes.',              10),
        (b'<script>alert("This should not be '
         b'displayed in a browser alert box.");</script>',                  11),
        (b'\xe3\x83\x95\xe3\x83\xac\xe3\x83\xbc\xe3\x83\xa0'
         b'\xe3\x83\xaf\xe3\x83\xbc\xe3\x82\xaf\xe3\x81\xae'
         b'\xe3\x83\x99\xe3\x83\xb3\xe3\x83\x81\xe3\x83\x9e'
         b'\xe3\x83\xbc\xe3\x82\xaf',                                       12),
    ]

    fortunes = [ (r[0].decode('utf-8'), r[1]) for r in _bytes ]

    header = (
      '<!DOCTYPE html>'
      '<html>'
        '<head>'
          '<title>Fortunes</title>'
        '</head>'
        '<body>'
          '<table>'
            '<tr>'
              '<th>id</th>'
              '<th>message</th>'
            '</tr>'
    )
    row = '<tr><td>%d</td><td>%s</td></tr>'
    footer = (
          '</table>'
        '</body>'
      '</html>'
    )

    sql = 'select message, id from fortune'

    @classmethod
    def prepare_fortunes(cls, fortunes):
        fortunes = [ (f[0], f[1]) for f in fortunes ]
        fortunes.append(('Begin.  The rest is easy.', 0))
        fortunes.sort()
        return fortunes

    @classmethod
    def render(cls, fortunes):
        fortunes = cls.prepare_fortunes(fortunes)
        row = cls.row
        return ''.join((
            cls.header,
            ''.join([ row % (f[1], quote_html(f[0])) for f in fortunes ]),
            cls.footer,
        ))

    @classmethod
    def render_raw(cls):
        return cls.render(cls.fortunes)

    @classmethod
    def render_db(cls, connect_string=None):
        fortunes = cls.load_from_db(connect_string)
        return cls.render(fortunes)

    @classmethod
    def load_from_db(cls, connect_string=None):
        cs = connect_string or localhost_connect_string
        con = pyodbc.connect(cs)
        cur = con.cursor()
        cur.execute(cls.sql)
        return cur.fetchall()

    @classmethod
    def json_from_db(cls, connect_string=None):
        results = Fortune.load_from_db(connect_string)
        results = Fortune.prepare_fortunes(results)
        fortunes = { r[1]: r[0] for r in results }
        return json.dumps(fortunes)

class Foo(HttpServer):
    http11 = True
    dbname = 'geo.db'

    connect_string = localhost_connect_string

    def plaintext(self, transport, data):
        return 'Hello, World!\n'

    def json(self, transport, data):
        return { 'message': 'Hello, world!\n' }

    def get_fortunes(self, request):
        return html_response(request, Fortune.render_db(self.connect_string))

    def fortunes2(self, transport, data):
        return Fortune.render_db(self.connect_string)

    def fortunes_json(self, transport, data):
        cs = self.connect_string
        con = pyodbc.connect(cs)
        cur = con.cursor()
        cur.execute(Fortunes.sql)
        results = cur.fetchall()
        results = Fortune.prepare_fortunes(results)
        fortunes = { r[1]: r[0] for r in results }
        parallel.debug(fortunes)
        return fortunes

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

class Foo2(HttpServer):
    http11 = True
    dbname = 'geo.db'

    connect_string = localhost_connect_string

    def plaintext(self, transport, data):
        return 'Hello, World!\n'

    def json(self, transport, data):
        return { 'message': 'Hello, world!\n' }

    def fortunes(self, transport, data):

        return html_response(request, Fortune.render_db(self.connect_string))

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
