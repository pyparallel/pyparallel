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

def tcpsock():
    return socket.socket(AF_INET, SOCK_STREAM)

CHARGEN = [
r""" !"#$%&'()*+,-./0123456789:;<=>?@ABCDEFGHIJKLMNOPQRSTUVWXYZ[\]^_`abcdefg""",
r"""!"#$%&'()*+,-./0123456789:;<=>?@ABCDEFGHIJKLMNOPQRSTUVWXYZ[\]^_`abcdefgh""",
r""""#$%&'()*+,-./0123456789:;<=>?@ABCDEFGHIJKLMNOPQRSTUVWXYZ[\]^_`abcdefghi""",
r"""#$%&'()*+,-./0123456789:;<=>?@ABCDEFGHIJKLMNOPQRSTUVWXYZ[\]^_`abcdefghij""",
r"""$%&'()*+,-./0123456789:;<=>?@ABCDEFGHIJKLMNOPQRSTUVWXYZ[\]^_`abcdefghijk""",
]

QOTD = 'An apple a day keeps the doctor away.\r\n'

ECHO_HOST    = ('echo.snakebite.net',     7)
QOTD_HOST    = ('qotd.snakebite.net',    17)
DISCARD_HOST = ('discard.snakebite.net',  9)
DAYTIME_HOST = ('daytime.snakebite.net', 13)
CHARGEN_HOST = ('chargen.snakebite.net', 19)

SERVICES_IP = socket.getaddrinfo(*ECHO_HOST)[0][4][0]

ECHO_IP     = (SERVICES_IP,  7)
DISCARD_IP  = (SERVICES_IP,  9)
DAYTIME_IP  = (SERVICES_IP, 13)
CHARGEN_IP  = (SERVICES_IP, 19)

NO_CB = None
NO_EB = None

HOST = '127.0.0.1'
ADDR = (HOST, 0)

TEMPDIR = None

def rmtempdir():
    if TEMPDIR:
        TEMPDIR.cleanup()

def tempfile():
    if not TEMPDIR:
        TEMPDIR = tempfile.TemporaryDirectory()
        assert os.path.isdir(TEMPDIR)
        atexit.register(rmtempdir)
    assert os.path.isdir(TEMPDIR)
    f = tempfile.NamedTemporaryFile(dir=TEMPDIR, delete=False)
    assert os.path.isfile(f)
    return f

def tempfilename():
    f = tempfile()
    f.close()
    return f.name

class TestBasic(unittest.TestCase):
    def test_calling_run_with_no_events_fails(self):
        self.assertRaises(AsyncRunCalledWithoutEventsError, _async.run_once)

class TestSubmitWork(unittest.TestCase):

    def test_submit_simple_work(self):
        def f(i):
            return i * 2
        def cb(r):
            _async.call_from_main_thread(
                self.assertEqual,
                (r, 4),
            )
        _async.submit_work(f, 2, None, cb, None)
        _async.run()

    def test_value_error_in_callback(self):
        def f():
            return laksjdflaskjdflsakjdfsalkjdf
        _async.submit_work(f, None, None, None, None)
        self.assertRaises(NameError, _async.run)

    def test_value_error_in_callback_then_run(self):
        def f():
            return laksjdflaskjdflsakjdfsalkjdf
        _async.submit_work(f, None, None, None, None)
        self.assertRaises(NameError, _async.run)
        _async.run()

    def test_multiple_value_errors_in_callback_then_run(self):
        def f():
            return laksjdflaskjdflsakjdfsalkjdf
        _async.submit_work(f, None, None, None, None)
        _async.submit_work(f, None, None, None, None)
        self.assertRaises(NameError, _async.run)
        self.assertRaises(NameError, _async.run)
        _async.run()

    def test_call_from_main_thread(self):
        d = {}
        def f(i):
            _async.call_from_main_thread_and_wait(
                d.__setitem__,
                ('foo', i*2),
            )
            return _async.call_from_main_thread_and_wait(
                d.__getitem__, 'foo'
            )
        def cb(r):
            _async.call_from_main_thread(
                self.assertEqual,
                (r, 4),
            )
        _async.submit_work(f, 2, None, cb, None)
        _async.run()

    def test_call_from_main_thread_decorator(self):
        @async.call_from_main_thread
        def f():
            self.assertFalse(_async.is_parallel_thread)
        _async.submit_work(f, None, None, None, None)
        _async.run()

    def test_submit_simple_work_errback_invoked(self):
        def f():
            return laksjdflaskjdflsakjdfsalkjdf
        def test_e(et, ev, eb):
            try:
                f()
            except NameError as e2:
                self.assertEqual(et, e2.__class__)
                self.assertEqual(ev, e2.args[0])
                self.assertEqual(eb.__class__, e2.__traceback__.__class__)
            else:
                self.assertEqual(0, 1)
        def cb(r):
            _async.call_from_main_thread(self.assertEqual, (0, 1))
        def eb(e):
            _async.call_from_main_thread_and_wait(test_e, e)

        _async.submit_work(f, None, None, cb, eb)
        _async.run()

class TestSubmitFileIO(unittest.TestCase):
    def test_write(self):
        n = tempfilename()
        f = open(n, 'w')
        _async.submit_io(f.write, b'foo', None, None, None)
        _async.run()
        f.close()
        with open(n, 'w') as f:
            self.assertEqual(f.read(), b'foo')

    def test_read(self):
        @async.call_from_main_thread
        def cb(d):
            self.assertEqual(d, b'foo')

        n = tempfilename()
        with open(n, 'w') as f:
            f.write(b'foo')

        f = open(n, 'r')
        _async.submit_io(f.read, None, None, cb, None)
        _async.run()

class TestConnectSocketIO(unittest.TestCase):
    def test_backlog(self):
        sock = tcpsock()
        port = sock.bind(ADDR)
        sock.listen(100)
        self.assertEqual(sock.backlog, 100)
        sock.close()

    def test_connect(self):
        @async.call_from_main_thread
        def cb():
            self.assertEqual(1, 1)

        sock = tcpsock()
        _async.connect(sock, DISCARD_IP, 1, None, cb, NO_EB)
        _async.run()

    def test_connect_with_data(self):
        @async.call_from_main_thread
        def cb(sock):
            self.assertEqual(1, 1)

        sock = tcpsock()
        _async.connect(sock, DISCARD_IP, 1, b'buf', cb, NO_EB)
        _async.run()

    def test_connect_with_data(self):
        @async.call_from_main_thread
        def cb(sock):
            self.assertEqual(1, 1)

        sock = tcpsock()
        _async.connect(sock, DISCARD_IP, 1, b'buf', cb, NO_EB)
        _async.run()

    def test_connect_then_recv(self):
        @async.call_from_main_thread
        def _check(data):
            self.assertEqual(data, QOTD)

        def read_cb(sock, data):
            _check(data)

        def connect_cb(sock):
            _async.recv(sock, read_cb, NO_EB)

        sock = tcpsock()
        _async.connect(sock, QOTD_IP, 1, None, connect_cb, NO_EB)
        _async.run()

    def test_connect_with_data_then_recv(self):
        @async.call_from_main_thread
        def _check(data):
            self.assertEqual(data, b'hello')

        def read_cb(sock, data):
            _check(data)

        def connect_cb(sock):
            _async.recv(sock, read_cb, NO_EB)

        sock = tcpsock()
        _async.connect(sock, ECHO_IP, 1, b'hello', connect_cb, NO_EB)
        _async.run()

    def test_connect_then_send_then_recv(self):
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

    def test_recv_before_connect_with_data_then_recv(self):
        @async.call_from_main_thread
        def _check(data):
            self.assertEqual(data, b'hello')

        def read_cb(sock, data):
            _check(data)

        sock = tcpsock()
        _async.recv(sock, read_cb, NO_EB)
        _async.connect(sock, ECHO_IP, 1, b'hello', NO_CB, NO_EB)
        _async.run()

    def test_recv_before_connect_then_send_then_recv(self):
        @async.call_from_main_thread
        def _check(data):
            self.assertEqual(data, b'hello')

        def read_cb(sock, data):
            _check(data)

        def connect_cb(sock):
            _async.send(sock, b'hello', NO_CB, NO_EB)

        sock = tcpsock()
        _async.recv(sock, read_cb, NO_EB)
        _async.connect(sock, ECHO_IP, 1, None, connect_cb, NO_EB)
        _async.run()

class TestAcceptSocketIO(unittest.TestCase):
    def test_accept(self):
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

    def test_accept_backlog2(self):
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
