#===============================================================================
# Imports
#===============================================================================
import ujson
import parallel
import datetime

from parallel import (
    rdtsc,
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
#===============================================================================
# Globals
#===============================================================================

#===============================================================================
# Classes
#===============================================================================
class RateLimitedServer(HttpServer):
    http11 = True
    rate_limit = datetime.timedelta(milliseconds=16)

    def hello(self, transport, data):
        return b'Hello, World!'

    def stats(self, transport, data):
        return {
            'system': dict(sys_stats()),
            'server': dict(socket_stats(transport.parent)),
            'memory': dict(memory_stats()),
            'contexts': dict(context_stats()),
            'elapsed': transport.elapsed(),
            'thread': thread_seq_id(),
        }

# vim:set ts=8 sw=4 sts=4 tw=80 et                                             :
