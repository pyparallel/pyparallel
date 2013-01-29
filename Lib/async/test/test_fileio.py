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

    def _write(self, size, callback=None, errback=None):
        buf = b'0' * size
        n = tempfilename()
        f = async.open(n, 'wb', size=size)
        async.write(f, buf, callback=callback, errback=errback)
        async.run()
        f.close()

        with open(n, 'rb') as f:
            self.assertEqual(f.read(), buf)

    def _writefile(self, size):
        buf = b'0' * size
        n = tempfilename()
        async.writefile(n, buf)
        async.run()

        with open(n, 'rb') as f:
            self.assertEqual(f.read(), buf)

    def test_write_using_page_size_multiple_4096(self):
        self._write(4096)

    def test_write_using_page_size_multiple_8192(self):
        self._write(8192)

    def test_write_with_callback(self):
        buf = b'0' * 4096
        n = tempfilename()
        d = {}

        def cb(f, nbytes):
            _async.call_from_main_thread_and_wait(
                d.__setitem__, (1, 1))

        fileobj = async.open(n, 'wb', size=4096)
        async.write(fileobj, buf, callback=cb)
        async.run()
        fileobj.close()
        self.assertEqual(d[1], 1)

        with open(n, 'rb') as f2:
            self.assertEqual(f2.read(), buf)

    def test_write_with_callback2(self):
        buf = b'0' * 4096
        n = tempfilename()
        d = {}

        @async.call_from_main_thread
        def cb(f, nbytes):
            d[f.name] = nbytes

        fileobj = async.open(n, 'wb', size=4096)
        async.write(fileobj, buf, callback=cb)
        async.run()
        fileobj.close()
        self.assertEqual(d[n], 4096)

        with open(n, 'rb') as f2:
            self.assertEqual(f2.read(), buf)

    def test_write_with_callback3(self):
        buf = b'0' * 4096
        n = tempfilename()
        d = {}

        def cb(f, nbytes):
            _async.call_from_main_thread_and_wait(
                d.__setitem__, (1, nbytes))

        fileobj = async.open(n, 'wb', size=4096)
        print(fileobj.name)
        async.write(fileobj, buf, callback=cb)
        async.run()
        fileobj.close()
        print(d)
        self.assertEqual(d[1], 4096)

        with open(n, 'rb') as f2:
            self.assertEqual(f2.read(), buf)

    def test_write_with_callback4(self):
        buf = b'0' * 4096
        n = tempfilename()
        o = async.object(name=None, nbytes=None)

        def cb(f, nbytes):
            o.name = n
            o.nbytes = 4096

        fileobj = async.open(n, 'wb', size=4096)
        async.write(fileobj, buf, callback=cb)
        async.run()
        fileobj.close()
        self.assertEqual(o.name, n)
        self.assertEqual(o.nbytes, 4096)

        with open(n, 'rb') as f2:
            self.assertEqual(f2.read(), buf)

    def _test_write_with_callback2(self):
        buf = b'0' * 4096
        n = tempfilename()
        o = async.object(nbytes=None)

        def cb(f, nbytes):
            o.nbytes = nbytes
            async.signal(o)

        fileobj = async.open(n, 'wb', size=4096)
        async.write(fileobj, buf, callback=cb)
        async.run()
        fileobj.close()
        self.assertEqual(d[1], 1)

        with open(n, 'rb') as f2:
            self.assertEqual(f2.read(), buf)

    def _test_write__fileobj_signalled_when_no_callback(self):
        o = async.object()
        o = async.protect(object())

        @async.call_from_main_thread_and_wait
        def cb(f, nbytes):
            f.close()
            self.assertEqual(nbytes, 4096)
            async.signal(o)

        buf = b'0' * 4096
        n = tempfilename()
        f = async.open(n, 'wb', size=size)
        async.write(f, buf, callback=callback, errback=errback)
        async.run()
        f.close()

        with open(n, 'rb') as f:
            self.assertEqual(f.read(), buf)

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

def main():
    unittest.main()

if __name__ == '__main__':
    main()

# vim:set ts=8 sw=4 sts=4 tw=78 et:
