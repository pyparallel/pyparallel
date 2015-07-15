#===============================================================================
# Imports
#===============================================================================
import json
import random
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
    Request,
    HttpServer,
)

#===============================================================================
# Aliases
#===============================================================================
randint = random.randint

#===============================================================================
# Globals/Templates
#===============================================================================
localhost_connection_string = (
    'Driver={SQL Server};'
    'Server=localhost;'
    'Database=hello_world;'
    'Uid=benchmarkdbuser;'
    'Pwd=B3nchmarkDBPass;'
)

#===============================================================================
# Helpers
#===============================================================================
def json_serialization(request=None, obj=None):
    transport = None
    if not request:
        request = Request(transport=None, data=None)
    else:
        transport = request.transport
    if not obj:
        obj = {'message': 'Hello, World!'}
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

def html_response(request, text):
    response = request.response
    response.code = 200
    response.message = 'OK'
    response.content_type = 'text/html; charset=UTF-8'
    response.body = text

    return request

def quote_html(html):
    return html.replace("&", "&amp;").replace("<", "&lt;").replace(">", "&gt;")

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
    def render_db(cls, connection_string=None):
        fortunes = cls.load_from_db(connection_string)
        return cls.render(fortunes)

    @classmethod
    def load_from_db(cls, connection_string=None):
        cs = connection_string or localhost_connection_string
        con = pyodbc.connect(cs)
        cur = con.cursor()
        cur.execute(cls.sql)
        return cur.fetchall()

    @classmethod
    def json_from_db(cls, connection_string=None):
        results = Fortune.load_from_db(connection_string)
        results = Fortune.prepare_fortunes(results)
        fortunes = { r[1]: r[0] for r in results }
        return json.dumps(fortunes)

class BaseHttpServer(HttpServer):
    #http11 = True

    connection = None
    #connection_string = None
    #odbc = None

    _id = None
    _randomNumber = None

    db_sql = 'select * from world where id = ?'
    db_sql2 = 'select * from world where id = 1'
    update_sql = 'update world set randomNumber = ? where id = ?'
    fortune_sql = 'select * from fortune'

    def get_json(self, request):
        json_serialization(request)

    def get_plaintext(self, request):
        response = request.response
        response.code = 200
        response.message = 'OK'
        response.body = b'Hello, World!'
        #response.content_length = len(response.body) + 2

    def get_stats(self, request):
        #request.keep_alive = True
        #request.response.other_headers.append('Refresh: 1')
        stats = {
            'system': dict(sys_stats()),
            'server': dict(socket_stats(request.transport.parent)),
            'memory': dict(memory_stats()),
            'contexts': dict(context_stats()),
            'elapsed': request.transport.elapsed(),
            'thread': async.thread_seq_id(),
        }
        json_serialization(request, stats)

    def get_shutdown(self, request):
        request.transport.shutdown_server()
        json_serialization(request, obj={'message': 'Shutdown'})

    def get_elapsed(self, request):
        obj = { 'elapsed': request.transport.elapsed() }
        json_serialization(request, obj)

    def get_rdtsc(self, request):
        @call_from_main_thread_and_wait
        def _timestamp():
            return rdtsc()
        obj = { 'rdtsc': _timestamp() }
        json_serialization(request, obj)

    def get_test(self, request):
        con = pyodbc.connect(self.connection_string)
        cur = con.cursor()
        #cur.execute(self.db_sql, (1,))
        #results = cur.fetchall()
        #cur.close()
        #cur.execute(self.db_sql2)
        #cur.close()
        return json_serialization(request, {'message': 'Test'})

    def get_test2(self, request):
        con = pyodbc.connect(self.connection_string)
        cur = con.cursor()
        cur.execute(self.db_sql, (1,))
        #results = cur.fetchall()
        #cur.close()
        #cur.execute(self.db_sql2)
        #cur.close()
        return json_serialization(request, {'message': 'Test'})

    def get_db(self, request):
        #async.debug(self.connection_string)
        con = pyodbc.connect(self.connection_string)
        cur = con.cursor()
        cur.execute(self.db_sql, (randint(1, 10000)))
        results = cur.fetchall()
        db = {
            'id': results[0][0],
            'randomNumber': results[0][1],
        }
        return json_serialization(request, db)

    def get_queries(self, request):
        # This test is ridiculous.
        count = try_int(request.query.get('queries')) or 1
        if count < 1:
            count = 1
        elif count > 500:
            count = 500

        con = pyodbc.connect(self.connection_string)
        cur = con.cursor()
        ids = [ randint(1, 10000) for _ in range(0, count) ]
        results = []
        for i in ids:
            cur.execute(self.db_sql, i)
            r = cur.fetchall()
            results.append({'id': r[0][0], 'randomNumber': r[0][1]})
        return json_serialization(request, results)

    def get_updates(self, request):
        # Also ridiculous.
        count = try_int(request.query.get('queries')) or 1
        if count < 1:
            count = 1
        elif count > 500:
            count = 500

        con = pyodbc.connect(self.connection_string)
        cur = con.cursor()
        ids = [ randint(1, 10000) for _ in range(0, count) ]
        results = []
        updates = []
        for i in ids:
            cur.execute(self.db_sql, i)
            o = cur.fetchall()
            t = (o[0][0], i)
            results.append(t)
            updates.append({'id': t[0], 'randomNumber': t[1]})

        cur.executemany(self.update_sql, results)
        cur.commit()
        return json_serialization(request, updates)

    def get_fortunejson(self):
        #async.debug(self.connection_string)
        results = Fortune.load_from_db(self.connection_string)
        fortune = { r[1]: r[0] for r in results }
        fortune[0] = 'Begin.  The rest is easy.'
        return json_serialization(request, fortune)

    def get_fortunes(self, request):
        return html_response(request, Fortune.render_db(self.connection_string))

    def get_fortunesraw(self, request):
        return html_response(request, Fortune.render_raw())

    def get_fortuneslocal(self, request):
        return html_response(request, Fortune.render_db())

    def get_fortunesjson(self, request):
        results = Fortune.load_from_db(self.connection_string)
        results = Fortune.prepare_fortunes(results)
        fortunes = { r[1]: r[0] for r in results }
        return json_serialization(request, fortunes)

    def _get_db2(self, request):
        #async.debug(self.connection_string)
        con = pyodbc.connect(self.connection_string)
        return json_serialization(request, {'tmp': 'foo'})

        con = self.odbc.connect(self.connection_string)
        cur = con.cursor()
        cur.execute(self.db_sql, (randint(1, 10000)))
        results = cur.fetchall()
        db = {
            'id': results[0][0],
            'randomNumber': results[0][1],
        }
        return json_serialization(request, db)


    def _get_db(self, request):
        #async.debug(self.connection_string)
        #cur = con.cursor()
        @call_from_main_thread_and_wait
        def _fetch(obj):
            cur = obj.connection.cursor()
            cur.execute(obj.db_sql, (randint(1, 10000),))
            results = cur.fetchall()
            #enable_heap_override()
            obj._id = results[0][0]
            obj._randomNumber = results[0][1]
            #disable_heap_override()
            del results
            cur.close()

        _fetch(self)
        db = { 'id': self._id, 'randomNumber': self._randomNumber }
        return json_serialization(request, db)

    def _get_db3(self, request):
        import pypyodbc
        con = pypyodbc.connect(self.connection_string)
        cur = con.cursor()
        cur.execute(self.db_sql, (randint(1, 10000),))
        results = cur.fetchall()
        cur.close()
        con.close()
        db = {
            'id': results[0][0],
            'randomNumber': results[0][1],
        }
        return json_serialization(request, db)

class DbHttpServer(BaseHttpServer):
    odbc = True
    connection_string = None

class HttpServerConcurrency(BaseHttpServer):
    concurrency = True

class HttpServerLowLatency(BaseHttpServer):
    low_latency = True
    max_sync_send_attempts = 0
    max_sync_recv_attempts = 0

class HttpServerThroughput(BaseHttpServer):
    throughput = True
    max_sync_send_attempts = 1
    max_sync_recv_attempts = 1

plaintext_http11_response = (
    'HTTP/1.1 200 OK\r\n'
    'Server: PyParallel Web Server v0.1\r\n'
    'Date: Sat, 16 May 2015 15:21:34 GMT\r\n'
    'Content-Type: text/plain;charset=utf-8\r\n'
    'Content-Length: 15\r\n'
    '\r\n'
    'Hello, World!\r\n'
)

class BaseCheatingPlaintextHttpServer:
    #concurrency = True
    initial_bytes_to_send = plaintext_http11_response
    next_bytes_to_send = plaintext_http11_response

class LowLatencyCheatingHttpServer:
    low_latency = True
    initial_bytes_to_send = plaintext_http11_response
    next_bytes_to_send = plaintext_http11_response

class ConcurrencyCheatingHttpServer:
    concurrency = True
    initial_bytes_to_send = plaintext_http11_response
    next_bytes_to_send = plaintext_http11_response

class ThroughputCheatingHttpServer:
    throughput = True
    initial_bytes_to_send = plaintext_http11_response
    next_bytes_to_send = plaintext_http11_response

class FastHttpServer:
    http11 = True

    def data_received(self, transport, data):
        header = transport.http_header
        async.debug(str(header))
        return plaintext_http11_response




