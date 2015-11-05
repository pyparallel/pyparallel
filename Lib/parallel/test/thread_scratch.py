import ujson
import parallel
import datetime
from parallel import (
    timer,
    thread,
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

thr = thread(interval=8, thread_characteristics="Low Latency")

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
    def res(self, transport, data):
        return thr.system_responsiveness


server1 = parallel.server('0.0.0.0', 8081)
parallel.register(server1, RateLimitedServer)



import parallel
def gmtime():
    return parallel.gmtime()

t = parallel.thread(func=gmtime, interval=1000, thread_characteristics="Low Latency")

import parallel
import datetime
def gmtime():
    return parallel.gmtime()

t = parallel.timer(func=gmtime, duetime=datetime.timedelta(milliseconds=1000), period=1000)
