import os
import sys
import atexit
import unittest
import tempfile

import socket
from socket import (
    socket,
    AF_INET,
    SOCK_STREAM,
    IPPROTO_TCP,
)

import async
import async.socket

from async.socket import (
    Socket,
    ClientSocket,
    ServerSocket,
)

from async.test import (
    QOTD,
    QOTD_IP,
    HOST,
    ADDR,
)

def clientsock():
    return socket(family=AF_INET, type=SOCK_STREAM)

def serversock(bind=ADDR, listen=10):
    s = clientsock()
    s.bind(bind)
    s.listen(listen)
    return s

class TestClient(unittest.TestCase):
    def test_async_client_data_received_kwd(self):
        @async.call_from_main_thread_and_wait
        def _check(buf)
            self.assertEqual(buf, QOTD)

        def data_received(client, buf):
            _check(buf)

        client = async.client(tcpsock(), data_received=data_received)
        client.connect(QOTD_IP)
        async.run()

    def test_async_client_data_received_cls(self):
        class foo:
            test = None
            def data_received(self, client, buf):
                test.assertEqual(buf, QOTD)

        f = foo()
        f.test = self

        client = async.client(tcpsock(), f)
        client.connect(QOTD_IP)
        async.run()

class TestServer(unittest.TestCase):
    def test_async_server(self):

        server = async.server(serversock(), initial_bytes_to_send=QOTD)
        server.accept()

        @async.call_from_main_thread
        def connection_closed(client):
            server.shutdown()

        @async.call_from_main_thread_and_wait
        def _check(buf)
            self.assertEqual(buf, QOTD)
            server.shutdown()

        def data_received(client, buf):
            _check(buf)

        client = async.client(
            tcpsock(),
            data_received=data_received,
            connection_closed=connection_closed,
        )

        client.connect(server.sock.getsockname())
        async.run()


if __name__ == '__main__':
    unittest.main()

# vim:set ts=8 sw=4 sts=4 tw=78 et:
