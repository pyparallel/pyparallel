import unittest
import async

from async.test import (
    QOTD,
    QOTD_IP,
    HOST,
    ADDR,
)

class TestSocket(unittest.TestCase):
    def test_basic(self):
        s = async.socket()

    def test_connection_made(self):
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

    def test_connect(self):
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


def test():
    unittest.main()

if __name__ == '__main__':
    test()

# vim:set ts=8 sw=4 sts=4 tw=78 et:
