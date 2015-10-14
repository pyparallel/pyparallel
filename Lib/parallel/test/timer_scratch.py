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

def update_stats():
    return ujson.dumps({
        'timestamp': gmtime(),
        'system': dict(sys_stats()),
        'memory': dict(memory_stats()),
        'contexts': dict(context_stats()),
        'thread': thread_seq_id(),
    }).encode('utf-8')

t = timer(datetime.timedelta(seconds=2), 50, update_stats)