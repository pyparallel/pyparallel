#===============================================================================
# Imports
#===============================================================================
import json

from async.http.server import (
    Request,
    HttpServer,
)
#===============================================================================
# Helpers
#===============================================================================
def json_serialization(request=None, obj=None):
    if not request:
        request = Request(transport=None, data=None)
    if not obj:
        obj = {'message': 'Hello, World!'}
    response = request.response
    response.code = 200
    response.message = 'OK'
    response.content_type = 'application/json; charset=UTF-8'
    response.body = json.dumps(obj)
    return request

class JSONSerializationHttpServer(HttpServer):
    def get_json(self, request):
        json_serialization(request)

plain_text = b'''HTTP/1.1 200 OK\r
Content-Length: 15\r
Content-Type: text/plain; charset=UTF-8\r
Server: Example\r
Date: Wed, 17 Apr 2013 12:00:00 GMT\r
\r
\r
Hello, World!'''

class PlainTextDummyServer:
    def data_received(self, transport, data):
        return plain_text
