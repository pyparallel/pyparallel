import os
import sys
import atexit
import unittest
import tempfile

import async
import _async

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

