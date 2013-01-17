import os
import sys
import atexit
import unittest
import tempfile

import async

from async.test import (
    QOTD,
    QOTD_IP,
)

class TestClient(unittest.TestCase):
    def test_async_client_data_received_kwd(self):
        @async.call_from_main_thread_and_wait
        def _check(buf)
            self.assertEqual(buf, QOTD)

        def data_received(sock, buf):
            _check(buf)

        async.client(QOTD_IP, data_received=data_received)
        async.run()
        self.assertEqual(async.active_clients, 0)

    def test_async_client_data_received_cls(self):
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
