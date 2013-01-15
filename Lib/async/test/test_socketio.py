import os
import sys
import atexit
import unittest
import tempfile

import async
import _async

import socket

from socket import (
    AF_INET,
    SOCK_STREAM,
)

CHARGEN = [
r""" !"#$%&'()*+,-./0123456789:;<=>?@ABCDEFGHIJKLMNOPQRSTUVWXYZ[\]^_`abcdefg""",
r"""!"#$%&'()*+,-./0123456789:;<=>?@ABCDEFGHIJKLMNOPQRSTUVWXYZ[\]^_`abcdefgh""",
r""""#$%&'()*+,-./0123456789:;<=>?@ABCDEFGHIJKLMNOPQRSTUVWXYZ[\]^_`abcdefghi""",
r"""#$%&'()*+,-./0123456789:;<=>?@ABCDEFGHIJKLMNOPQRSTUVWXYZ[\]^_`abcdefghij""",
r"""$%&'()*+,-./0123456789:;<=>?@ABCDEFGHIJKLMNOPQRSTUVWXYZ[\]^_`abcdefghijk""",
]

QOTD = b'An apple a day keeps the doctor away.\r\n'

ECHO_HOST    = ('echo.snakebite.net',     7)
QOTD_HOST    = ('qotd.snakebite.net',    17)
DISCARD_HOST = ('discard.snakebite.net',  9)
DAYTIME_HOST = ('daytime.snakebite.net', 13)
CHARGEN_HOST = ('chargen.snakebite.net', 19)

SERVICES_IP = socket.getaddrinfo(*ECHO_HOST)[0][4][0]

ECHO_IP     = (SERVICES_IP,  7)
QOTD_IP     = (SERVICES_IP, 17)
DISCARD_IP  = (SERVICES_IP,  9)
DAYTIME_IP  = (SERVICES_IP, 13)
CHARGEN_IP  = (SERVICES_IP, 19)

NO_CB = None
NO_EB = None

HOST = '127.0.0.1'
ADDR = (HOST, 0)

TEMPDIR = None

class TestClient(unittest.TestCase):
    def test_async_client_data_received(self):
        @async.call_from_main_thread_and_wait
        def _check(buf)
            self.assertEqual(buf, QOTD)

        def data_received(sock, buf):
            _check(buf)

        async.client(QOTD_IP, data_received=data_received)
        async.run()
        self.assertEqual(async.client_count, 0)

class TestConnectSocketIO(unittest.TestCase):

    def _test_connect(self):
        sock = async.socket(tcpsock())

        @async.call_from_main_thread
        def cb(*args):
            self.assertEqual(sock, args[0])

        sock.connect_async(DISCARD_IP, None, cb, NO_EB)
        _async.run()

    def _test_connect_with_data(self):
        @async.call_from_main_thread
        def cb(sock):
            self.assertEqual(1, 1)

        sock = tcpsock()
        _async.connect(sock, DISCARD_IP, b'buf', cb, NO_EB)
        _async.run()

    def _test_connect_with_data(self):
        @async.call_from_main_thread
        def cb(sock):
            self.assertEqual(1, 1)

        sock = tcpsock()
        _async.connect(sock, DISCARD_IP, b'buf', cb, NO_EB)
        _async.run()

    def _test_connect_then_recv(self):
        @async.call_from_main_thread
        def _check(data):
            self.assertEqual(data, QOTD)

        def read_cb(sock, data):
            _check(data)

        def connect_cb(sock):
            _async.recv(sock, read_cb, NO_EB)

        sock = tcpsock()
        _async.connect(sock, QOTD_IP, None, connect_cb, NO_EB)
        _async.run()

    def _test_connect_with_data_then_recv(self):
        @async.call_from_main_thread
        def _check(data):
            self.assertEqual(data, b'hello')

        def read_cb(sock, data):
            _check(data)

        def connect_cb(sock):
            _async.recv(sock, read_cb, NO_EB)

        sock = tcpsock()
        _async.connect(sock, ECHO_IP, b'hello', connect_cb, NO_EB)
        _async.run()

    def _test_connect_then_send_then_recv(self):
        @async.call_from_main_thread
        def _check(data):
            self.assertEqual(data, b'hello')

        def read_cb(sock, data):
            _check(data)

        def connect_cb(sock):
            _async.recv(sock, read_cb, NO_EB)
            _async.send(sock, b'hello', NO_CB, NO_EB)

        sock = tcpsock()
        _async.connect(sock, ECHO_IP, 1, None, connect_cb, NO_EB)
        _async.run()

    def _test_recv_before_connect_with_data_then_recv(self):
        @async.call_from_main_thread
        def _check(data):
            self.assertEqual(data, b'hello')

        def read_cb(sock, data):
            _check(data)

        sock = tcpsock()
        _async.recv(sock, read_cb, NO_EB)
        _async.connect(sock, ECHO_IP, b'hello', NO_CB, NO_EB)
        _async.run()

    def _test_recv_before_connect_then_send_then_recv(self):
        @async.call_from_main_thread
        def _check(data):
            self.assertEqual(data, b'hello')

        def read_cb(sock, data):
            _check(data)

        def connect_cb(sock):
            _async.send(sock, b'hello', NO_CB, NO_EB)

        sock = tcpsock()
        _async.recv(sock, read_cb, NO_EB)
        _async.connect(sock, ECHO_IP, None, connect_cb, NO_EB)
        _async.run()

class TestAcceptSocketIO(unittest.TestCase):
    def _test_accept(self):
        @async.call_from_main_thread
        def new_connection(sock, data):
            self.assertEqual(data, b'hello')

        sock = tcpsock()
        port = sock.bind(ADDR)
        addr = sock.getsockname()
        sock.listen(1)
        _async.accept(sock, new_connection, NO_EB)

        client = tcpsock()
        _async.connect(client, addr, 1, b'hello', NO_CB, NO_EB)
        _async.run()
        sock.close()

    def _test_accept_backlog2(self):
        counter = 0
        @async.call_from_main_thread
        def new_connection(sock, data):
            self.assertEqual(data, b'hello')
            counter += 1

        sock = tcpsock()
        port = sock.bind(ADDR)
        addr = sock.getsockname()
        sock.listen(2)
        _async.accept(sock, new_connection, NO_EB)

        client = tcpsock()
        _async.connect(client, addr, 2, b'hello', NO_CB, NO_EB)
        _async.run()
        self.assertEqual(counter, 2)


if __name__ == '__main__':
    unittest.main()

# vim:set ts=8 sw=4 sts=4 tw=78 et:
