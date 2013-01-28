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

    def test_nowaiters_error_saw(self):
        s = async.protect(object())
        w = async.protect(object())
        self.assertRaises(async.NoWaitersError, async.signal_and_wait, s, w)

    def test_same_signal_and_wait(self):
        s = w = async.protect(object())
        self.assertRaises(async.WaitError, async.signal_and_wait, s, w)

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

    def test_prewait1(self):
        o = async.protect(object())
        async.prewait(o)
        async.signal(o)
        async.wait(o)
        self.assertTrue(True)

    def test_prewait2(self):
        o = async.prewait(async.protect(object()))
        async.signal(o)
        async.wait(o)
        self.assertTrue(True)

class TestPrewait(unittest.TestCase):
    def test_prewait(self):
        d = {}
        o = async.protect(object())
        r = async.protect(object())
        w = async.protect(object())
        m = async.protect(object())

        @async.call_from_main_thread_and_wait
        def _timestamp(name):
            d[name] = async.rdtsc()

        def reader(name):
            async.wait(r)
            async.signal(m)
            async.read_lock(o)
            async.signal(w)         # start writer callback
            async.wait(r)           # wait for writer callback
            _timestamp(name)
            async.read_unlock(o)

        def writer(name):
            async.wait(w)
            async.signal(r)         # tell the reader we've entered
            async.write_lock(o)     # will be blocked until reader unlocks
            _timestamp(name)
            async.write_unlock(o)

        async.prewait(w)
        async.prewait(r)
        async.prewait(m)

        async.submit_work(reader, 'r')
        async.submit_work(writer, 'w')

        async.signal_and_wait(r, m)
        async.run()
        self.assertGreater(d['w'], d['r'])

    def _test_multiple_prewaits(self, num):
        first = 0
        last = num
        indexes = range(first, last)
        objs = [ async.prewait(async.object()) for _ in indexes ]
        d = {}

        @async.call_from_main_thread_and_wait
        def _timestamp(index):
            d[index] = async.rdtsc()

        def cb(index, wait, signal):
            async.wait(wait)
            _timestamp(index)
            if signal:
                async.signal(signal)

        for i in indexes:
            wait = objs[i]
            signal = None if i == indexes[-1] else objs[i+1]
            async.submit_work(cb, (i, wait, signal))

        self.assertEqual(async.active_contexts(), last)
        async.signal(objs[first])
        async.run()
        self.assertEqual(async.active_contexts(), 0)

        for i in indexes[1:]:
            self.assertGreater(d[i], d[i-1])

    def test_multiple_prewaits_2(self):
        self._test_multiple_prewaits(2)

    def test_multiple_prewaits_4(self):
        self._test_multiple_prewaits(4)

    def test_multiple_prewaits_8(self):
        self._test_multiple_prewaits(4)

class TestPersistence(unittest.TestCase):
    def test_nopersistence_raises_error(self):
        success = False
        def cb(o):
            try:
                o.foo = 'bar'
            except async.PersistenceError:
                success = True

        o = async.object(foo=None)
        async.submit_work(cb, o)
        async.run()
        self.assertTrue(success)
        self.assertEqual(o.foo, None)

class TestAsyncProtection(unittest.TestCase):
    def test_basic(self):
        d = {}
        o = async.protect(object())
        r = async.protect(object())
        w = async.protect(object())

        @async.call_from_main_thread_and_wait
        def _timestamp(name):
            d[name] = async.rdtsc()

        def reader(name):
            async.read_lock(o)
            async.signal(w)         # start writer callback
            async.wait(r)           # wait for writer callback
            _timestamp(name)
            async.read_unlock(o)

        def writer(name):
            async.signal(r)         # tell the reader we've entered
            async.write_lock(o)     # will be blocked until reader unlocks
            _timestamp(name)
            async.write_unlock(o)

        async.submit_wait(r, reader, 'r')
        async.submit_wait(w, writer, 'w')
        async.signal(r)
        async.run()
        self.assertGreater(d['w'], d['r'])

    def test_signal_and_wait(self):
        d = {}
        o = async.protect(object())
        r = async.protect(object())
        w = async.protect(object())

        @async.call_from_main_thread_and_wait
        def _timestamp(name):
            d[name] = async.rdtsc()

        def reader(name):
            async.read_lock(o)
            async.signal_and_wait(w, r) # start writer callback, wait
            _timestamp(name)
            async.read_unlock(o)

        def writer(name):
            async.signal(r)         # tell the reader we've entered
            async.write_lock(o)     # will be blocked until reader unlocks
            _timestamp(name)
            async.write_unlock(o)

        async.submit_wait(r, reader, 'r')
        async.submit_wait(w, writer, 'w')
        async.signal(r)
        async.run()
        self.assertGreater(d['w'], d['r'])

if __name__ == '__main__':
    unittest.main()

# vim:set ts=8 sw=4 sts=4 tw=78 et:
