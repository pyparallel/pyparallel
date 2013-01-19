import unittest
import async
import time

class _object(dict):
    def __init__(self, **kwds):
        self.__dict__.update(**kwds)

class TestProtect(unittest.TestCase):

    def test_protect_basic(self):
        o = _object()
        async.protect(o)
        self.assertEqual(async.protected(o), True)
        async.unprotect(o)
        self.assertEqual(async.protected(o), False)

    def test_protect_getattr(self):
        o = _object(foo='bar')
        async.protect(o)
        self.assertEqual(async.protected(o), True)
        self.assertEqual(o.foo, 'bar')
        async.unprotect(o)
        self.assertEqual(o.foo, 'bar')
        self.assertEqual(async.protected(o), False)

    def test_protect_setattr(self):
        o = _object(foo=None)
        async.protect(o)
        self.assertEqual(async.protected(o), True)
        o.foo = 'bar'
        self.assertEqual(o.foo, 'bar')
        async.unprotect(o)
        self.assertEqual(o.foo, 'bar')
        o.foo = 'foo'
        self.assertEqual(o.foo, 'foo')
        self.assertEqual(async.protected(o), False)

    def test_protect_both(self):
        o = _object(foo='bar', cat=None)
        async.protect(o)
        self.assertEqual(async.protected(o), True)
        o.cat = 'dog'
        self.assertEqual(o.foo, 'bar')
        self.assertEqual(o.cat, 'dog')
        async.unprotect(o)
        self.assertEqual(o.foo, 'bar')
        self.assertEqual(o.cat, 'dog')
        self.assertEqual(async.protected(o), False)

class TestProtectionError(unittest.TestCase):
    def test_protection_error_basic(self):
        o = _object()
        self.assertRaises(async.ProtectionError, async.read_lock, o)
        self.assertRaises(async.ProtectionError, async.read_unlock, o)
        self.assertRaises(async.ProtectionError, async.try_read_lock, o)

        self.assertRaises(async.ProtectionError, async.write_lock, o)
        self.assertRaises(async.ProtectionError, async.write_unlock, o)
        self.assertRaises(async.ProtectionError, async.try_write_lock, o)

    def test_protection_error_in_async_callback(self):
        def callback():
            o = _object()
            self.assertRaises(async.ProtectionError, async.protect, o)
            self.assertRaises(async.ProtectionError, async.unprotect, o)
            self.assertRaises(async.ProtectionError, async.wait, o)

        async.submit_work(callback)
        async.run()

class TestAsyncSignalAndWait(unittest.TestCase):
    def test_nowaiters_error(self):
        o = async.protect(object())
        self.assertRaises(async.NoWaitersError, async.signal, o)

    def test_nowaiters_error_from_callback(self):
        o = async.protect(object())

        def callback():
            self.assertRaises(async.NoWaitersError, async.signal, o)
        async.submit_work(callback)
        async.run()

    def test_wait_error_from_callback(self):
        o = async.protect(object())

        def callback():
            self.assertRaises(async.WaitError, async.wait, o)

        async.submit_work(callback)
        async.run()

class TestAsyncProtection(unittest.TestCase):
    def _test_basic(self):
        d = {}
        o = object()
        async.protect(o)

        @async.call_from_main_thread_and_wait
        def _timestamp(name):
            d[name] = async.rdtsc()

        def reader(name):
            async.read_lock(o)
            async.signal(o)         # start writer callback
            async.wait(o)           # wait for writer callback
            _timestamp(name)
            async.read_unlock(o)

        def writer(name):
            async.signal(o)         # tell the reader we've entered
            async.write_lock(o)     # will be blocked until reader unlocks
            _timestamp(name)
            async.write_unlock(o)
            async.signal(o)         # tell the main thread we're done

        async.submit_wait(o, writer, 'w')
        async.submit_work(reader, 'r')
        async.run()
        self.assertGreater(d['w'], d['r'])

if __name__ == '__main__':
    unittest.main()

# vim:set ts=8 sw=4 sts=4 tw=78 et:
