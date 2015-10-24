import ujson
import parallel
import datetime
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
    quote_html,
    text_response,
    html_response,
    json_serialization,
    HttpServer,
)

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
t.start()

class RateLimitedServer(HttpServer):
    http11 = True
    rate_limit = datetime.timedelta(milliseconds=16)
    def hello(self, transport, data):
        return b'Hello, World!'
    def stats(self, transport, data):
        return t.data or b''
    def uni(self, transport, data):
        return '<html><body>Works!</body></html>'
    def bytearr(self, transport, data):
        return bytearray(b'abcd')

class NormalServer(HttpServer):
    http11 = True
    def hello(self, transport, data):
        return b'Hello, World!'
    def stats(self, transport, data):
        return t.data or b''
    def uni(self, transport, data):
        return '<html><body>Works!</body></html>'
    def bytearr(self, transport, data):
        return bytearray(b'abcd')

server1 = parallel.server('0.0.0.0', 8080)
server2 = parallel.server('0.0.0.0', 8081)
parallel.register(server1, RateLimitedServer)
parallel.register(server2, NormalServer)
