import os
import sys
import atexit
import unittest
import tempfile

import async
import itertools

#from async.test import (
#    ECHO_IP,
#    QOTD_DATA,
#    QOTD_HOST,
#    QOTD_PORT,
#    QOTD_IP,
#)

QOTD_IP = '10.211.55.2'
QOTD_PORT = 20017

class TestClient(unittest.TestCase):
    def test_nohost_error(self):
        self.assertRaises(TypeError, async.client)

    def test_noport_error(self):
        self.assertRaises(TypeError, async.client, ECHO_IP)

    def test_host_and_port(self):
        c = async.client(QOTD_IP, QOTD_PORT)
        self.assertEqual(c.host, QOTD_IP)
        self.assertEqual(c.ip, QOTD_IP)
        self.assertEqual(c.port, QOTD_PORT)
        c.close()

    def test_connect(self):
        c = async.client(QOTD_IP, QOTD_PORT)
        c.connect((QOTD_IP, QOTD_PORT))
        c.close()

    def _test_connection_made(self):
        def cb1(*args, **kwds):
            pass

        def cb2(*args, **kwds):
            pass

        s = async.socket()
        self.assertNone(s.connection_made)

        s = async.socket(connection_made=cb1)
        self.assertEqual(s.connection_made, cb1)
        s.connection_made = cb2
        self.assertEqual(s.connection_made, cb2)

    def _test_connect(self):
        d = async.prewait(async.dict())
        def cb(sock, *args, **kwds):
            d[1] = None
            async.signal(d)

        s = async.socket(connection_made=cb)
        s.connect(QOTD_IP)
        for i in range(0, 5):
            async.run_once()
            if 1 in d:
                break

        self.assertTrue(1 in d)
        s.close()

    def _test_async_client_data_received_kwd(self):
        @async.call_from_main_thread_and_wait
        def _check(buf):
            self.assertEqual(buf, QOTD)

        def data_received(sock, buf):
            _check(buf)

        async.client(QOTD_IP, data_received=data_received)
        async.run()
        self.assertEqual(async.active_clients, 0)

    def _test_async_client_data_received_cls(self):
        class client:
            def data_received(self, sock, buf):
                self.assertEqual(async.client_count, 1)
                self.assertEqual(buf, QOTD)

        async.client(QOTD_IP, client)
        async.run()
        self.assertEqual(async.client_count, 0)


if __name__ == '__main__':
    unittest.main()

# vim:set ts=8 sw=4 sts=4 tw=78 et:
