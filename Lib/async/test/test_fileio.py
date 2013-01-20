import os
import sys
import atexit
import unittest
import tempfile

import async
import _async

TEMPDIR = None

def rmtempdir():
    global TEMPDIR
    if TEMPDIR:
        TEMPDIR.cleanup()

def tmpfile():
    global TEMPDIR
    if not TEMPDIR:
        TEMPDIR = tempfile.TemporaryDirectory()
        assert os.path.isdir(TEMPDIR.name)
        atexit.register(rmtempdir)
    assert os.path.isdir(TEMPDIR.name)
    f = tempfile.NamedTemporaryFile(dir=TEMPDIR.name, delete=False)
    assert os.path.isfile(f.name)
    return f

def tempfilename():
    n = None
    f = tmpfile()
    n = f.name
    f.close()
    return n

class TestFileIO(unittest.TestCase):
    def test_open(self):
        n = tempfilename()
        f = async.open(n, 'wb')
        f.close()

    def _test_write(self):
        n = tempfilename()
        f = async.open(n, 'w')
        async.write(f, b'foo')
        async.run()
        f.close()

        with open(n, 'r') as f:
            self.assertEqual(f.read(), b'foo')

    def _test_read(self):
        @async.call_from_main_thread_and_wait
        def callback(f, d):
            self.assertEqual(d, b'foo')

        n = tempfilename()
        with open(n, 'w') as f:
            f.write(b'foo')

        f = async.open(n, 'r')
        async.read(f, callback)
        async.run()
        f.close()

if __name__ == '__main__':
    unittest.main()
