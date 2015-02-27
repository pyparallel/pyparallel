#===============================================================================
# Imports
#===============================================================================
import json

from async import (
    socket_stats,
    memory_stats,
    context_stats,
)

from async.http.server import (
    Request,
    HttpServer,
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

class JSONSerializationHttpServer(HttpServer):
    concurrency = True

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
            'server': dict(socket_stats(request.transport.parent)),
            'memory': dict(memory_stats()),
            'contexts': dict(context_stats()),
            'elapsed': request.transport.elapsed(),
        }
        json_serialization(request, stats)

    def get_shutdown(self, request):
        request.transport.shutdown_server()
        json_serialization(request, obj={'message': 'Shutdown'})

    def get_elapsed(self, request):
        obj = { 'elapsed': request.transport.elapsed() }
        json_serialization(request, obj)

