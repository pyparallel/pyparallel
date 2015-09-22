#===============================================================================
# Imports
#===============================================================================
import sys
import json
import pyodbc

import async

from ctk.util import (
    try_int,
    Dict,
)

from async import (
    rdtsc,
    sys_stats,
    socket_stats,
    memory_stats,
    context_stats,
    enable_heap_override,
    disable_heap_override,
    call_from_main_thread,
    call_from_main_thread_and_wait,
)

from async.http.server import (
    quote_html,
    router,
    make_routes,
    text_response,
    html_response,
    json_serialization,

    Request,
    HttpServer,
)

# It can be a pain setting up the environment to load the debug version of
# numpy, so, if we can't import it, just default to normal Python random.
np = None
if not sys.executable.endswith('_d.exe'):
    try:
        import numpy as np
        randint = np.random.randint
        randints = lambda size: randint(0, high=10000, size=size)
        randints2d = lambda size: randint(0, high=10000, size=(2, size))
        print("Using NumPy random facilities.")
    except ImportError:
        pass

if not np:
    import random
    randint = random.randint
    randints = lambda size: [ randint(1, 10000) for _ in range(0, size) ]
    randints2d = lambda size: [
        (x, y) for (x, y) in zip(randints(size), randints(size))
    ]
    print("Couldn't load NumPy; reverting to normal CPython random facilities.")

#===============================================================================
# Aliases
#===============================================================================

#===============================================================================
# Globals/Templates
#===============================================================================
localhost_connect_string = (
    'Driver={SQL Server};'
    'Server=localhost;'
    'Database=hello_world;'
    'Uid=benchmarkdbuser;'
    'Pwd=B3nchmarkDBPass;'
)

#===============================================================================
# Helpers
#===============================================================================

#===============================================================================
# Classes
#===============================================================================
class Fortune:
    _bytes = [
        (b'fortune: No such file or directory',                                1),
        (b"A computer scientist is someone who "
         b"fixes things that aren't broken.",                                  2),
        (b'After enough decimal places, nobody gives a damn.',                 3),
        (b'A bad random number generator: 1, 1, 1, 1, '
         b'1, 4.33e+67, 1, 1, 1',                                              4),
        (b'A computer program does what you tell it to do, '
         b'not what you want it to do.',                                       5),
        (b'Emacs is a nice operating system, but I prefer UNIX. '
         b'\xe2\x80\x94 Tom Christaensen',                                     6),
        (b'Any program that runs right is obsolete.',                          7),
        (b'A list is only as strong as its weakest link. '
         b'\xe2\x80\x94 Donald Knuth',                                         8),
        (b'Feature: A bug with seniority.',                                    9),
        (b'Computers make very fast, very accurate mistakes.',                10),
        (b'<script>alert("This should not be '
         b'displayed in a browser alert box.");</script>',                    11),
        (b'\xe3\x83\x95\xe3\x83\xac\xe3\x83\xbc\xe3\x83\xa0'
         b'\xe3\x83\xaf\xe3\x83\xbc\xe3\x82\xaf\xe3\x81\xae'
         b'\xe3\x83\x99\xe3\x83\xb3\xe3\x83\x81\xe3\x83\x9e'
         b'\xe3\x83\xbc\xe3\x82\xaf',                                         12),
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

routes = make_routes()
route = router(routes)

class TefbHttpServer(HttpServer):
    routes = routes

    db_sql = 'select id, randomNumber from world where id = ?'
    update_sql = 'update world set randomNumber = ? where id = ?'
    fortune_sql = 'select * from fortune'

    connect_string = localhost_connect_string

    @route
    def json(self, request):
        json_serialization(request, obj={'message': 'Hello, World'})

    @route
    def plaintext(self, request):
        text_response(request, text='Hello, World!')

    @route
    def fortunes(self, request):
        return html_response(request, Fortune.render_db(self.connect_string))

    @route
    def fortunes_raw(self, request):
        return html_response(request, Fortune.render_raw())

    @route
    def db(self, request):
        con = pyodbc.connect(self.connect_string)
        cur = con.cursor()
        cur.execute(self.db_sql, (randint(1, 10000)))
        results = cur.fetchall()
        db = {
            'id': results[0][0],
            'randomNumber': results[0][1],
        }
        return json_serialization(request, db)

    @route
    def queries(self, request):
        count = try_int(request.query.get('queries')) or 1
        if count < 1:
            count = 1
        elif count > 500:
            count = 500

        con = pyodbc.connect(self.connect_string)
        cur = con.cursor()
        ids = randints(count)
        results = []
        for npi in ids:
            i = int(npi)
            cur.execute(self.db_sql, i)
            r = cur.fetchall()
            results.append({'id': r[0][0], 'randomNumber': r[0][1]})
        return json_serialization(request, results)

    @route
    def updates(self, request):
        count = try_int(request.query.get('queries')) or 1
        if count < 1:
            count = 1
        elif count > 500:
            count = 500

        con = pyodbc.connect(self.connect_string)
        cur = con.cursor()
        ints2d = randints2d(count)
        results = []
        updates = []
        for npi in ints2d:
            (i, rn) = (int(npi[0]), int(npi[1]))
            cur.execute(self.db_sql, i)
            o = cur.fetchall()
            results.append((rn, i))
            updates.append({'id': i, 'randomNumber': rn})

        cur.executemany(self.update_sql, results)
        cur.commit()
        return json_serialization(request, updates)

    @route
    def stats(self, request):
        stats = {
            'system': dict(sys_stats()),
            'server': dict(socket_stats(request.transport.parent)),
            'memory': dict(memory_stats()),
            'contexts': dict(context_stats()),
            'elapsed': request.transport.elapsed(),
            'thread': async.thread_seq_id(),
        }
        json_serialization(request, stats)

    @route
    def hello(self, request, *args, **kwds):
        j = { 'args': args, 'kwds': kwds }
        return json_serialization(request, j)

    @route
    def shutdown(self, request):
        request.transport.shutdown_server()
        json_serialization(request, obj={'message': 'Shutdown'})

    @route
    def elapsed(self, request):
        obj = { 'elapsed': request.transport.elapsed() }
        json_serialization(request, obj)

plaintext_http11_response = (
    'HTTP/1.1 200 OK\r\n'
    'Server: PyParallel Web Server v0.1\r\n'
    'Date: Sat, 16 May 2015 15:21:34 GMT\r\n'
    'Content-Type: text/plain;charset=utf-8\r\n'
    'Content-Length: 15\r\n'
    '\r\n'
    'Hello, World!\r\n'
)
class CheatingPlaintextHttpServer:
    initial_bytes_to_send = plaintext_http11_response
    next_bytes_to_send = plaintext_http11_response

# vim:set ts=8 sw=4 sts=4 tw=80 et                                             :
