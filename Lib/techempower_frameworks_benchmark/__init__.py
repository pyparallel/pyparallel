#===============================================================================
# Imports
#===============================================================================
import json

import async

from async import (
    rdtsc,
    sys_stats,
    socket_stats,
    memory_stats,
    context_stats,
    call_from_main_thread,
    call_from_main_thread_and_wait,
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

class BaseHttpServer(HttpServer):

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
