import unittest
import _async
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


class TestUnprotectedDetection(unittest.TestCase):
    def test_dict(self):
        d = dict()

        def foo():
            tsc = async.rdtsc()
            d['foo'] = tsc

        def callback():
            self.assertRaises(async.UnprotectedError, foo)

        async.submit_work(callback)
        async.run()

    def test_dict_delitem(self):
        d = { 'foo' : None }

        def foo():
            del d['foo']

        def callback():
            self.assertRaises(async.UnprotectedError, foo)

        async.submit_work(callback)
        async.run()

    def test_object(self):
        o = _object(foo=None)

        def foo():
            o.foo = async.rdtsc()

        def callback():
            self.assertRaises(async.UnprotectedError, foo)

        async.submit_work(callback)
        async.run()

    def test_object_delattr(self):
        o = _object(foo=None)

        def foo():
            del o.foo

        def callback():
            self.assertRaises(async.UnprotectedError, foo)

        async.submit_work(callback)
        async.run()

    def test_list_append(self):
        l = list()

        def foo():
            l.append(async.rdtsc())

        def callback():
            self.assertRaises(async.AssignmentError, foo)

        async.submit_work(callback)
        async.run()

    def test_list_assign(self):
        l = [ None, ]

        def foo():
            l[0] = async.rdtsc()

        def callback():
            self.assertRaises(async.UnprotectedError, foo)

        async.submit_work(callback)
        async.run()

    def test_list_sort(self):
        l = [ 3, 2, 1 ]

        def foo():
            l.sort()

        def callback():
            self.assertRaises(async.UnprotectedError, foo)

        async.submit_work(callback)
        async.run()

class TestDictAssignmentDetectionAndProtection(unittest.TestCase):
    def test_assign_new_subscript(self):
        d = async.dict()

        def callback():
            d['foo'] = async.rdtsc()

        async.submit_work(callback)
        async.run()
        del d
        self.assertEqual(async.persisted_contexts(), 0)

    def test_assign_subscript_over_none(self):
        d = async.dict()
        d['foo'] = None

        def callback():
            d['foo'] = async.rdtsc()

        async.submit_work(callback)
        async.run()
        del d
        self.assertEqual(async.persisted_contexts(), 0)

    def test_assign_subscript_over_non_none(self):
        d = async.dict()
        d['foo'] = 'alsdkjf'

        def foo():
            d['foo'] = async.rdtsc()

        def callback():
            self.assertRaises(async.AssignmentError, foo)

        async.submit_work(callback)
        async.run()
        del d
        self.assertEqual(async.persisted_contexts(), 0)

    def test_assign_subscript_over_previous_context(self):
        d = async.dict()
        o = async.object()
        t = async.object()

        async.prewait(o)
        async.prewait(t)

        def one():
            self.assertTrue('foo' not in d)
            d['foo'] = async.rdtsc()
            async.signal(t)

        def two():
            def foo():
                d['foo'] = async.rdtsc()
            async.wait(t)
            self.assertTrue('foo' in d)
            self.assertRaises(async.AssignmentError, foo)

        async.submit_wait(o, one)
        async.submit_work(two)
        async.signal(o)
        async.run()
        del d
        self.assertEqual(async.persisted_contexts(), 0)

    def test_del_none_subscript(self):
        d = async.dict()
        d['foo'] = None

        def callback():
            del d['foo']

        async.submit_work(callback)
        async.run()
        del d
        self.assertEqual(async.persisted_contexts(), 0)

    def test_del_subscript_from_previous_context(self):
        d = async.dict()
        o = async.object()
        t = async.object()

        async.prewait(o)
        async.prewait(t)

        def one():
            self.assertTrue('foo' not in d)
            d['foo'] = async.rdtsc()
            async.signal(t)

        def two():
            def foo():
                del d['foo']
            async.wait(t)
            self.assertTrue('foo' in d)
            self.assertRaises(async.AssignmentError, foo)

        async.submit_wait(o, one)
        async.submit_work(two)
        async.signal(o)
        async.run()
        del d
        self.assertEqual(async.persisted_contexts(), 0)


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

        self.assertEqual(async.active_contexts(), 0)

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

class TestPersistence(unittest.TestCase):
    def test_persistence_basic(self):
        self.assertEqual(async.persisted_contexts(), 0)

        def cb(w, o):
            async.wait(w)
            o.foo = async.rdtsc()

        w = async.prewait(async.object())
        o = async.object(foo=None)
        self.assertTrue(async.protected(o))
        async.submit_work(cb, (w, o))
        self.assertEqual(async.active_contexts(), 1)
        self.assertEqual(async.persisted_contexts(), 0)
        async.signal(w)
        async.run()
        self.assertEqual(async.active_contexts(), 0)
        self.assertEqual(async.persisted_contexts(), 1)
        del o.foo
        self.assertEqual(async.persisted_contexts(), 0)

    def _test_persistence_via_setattr(self):
        self.assertEqual(async.persisted_contexts(), 0)

        def cb(w, o):
            async.wait(w)
            setattr(o, 'foo', async.rdtsc())

        w = async.prewait(async.object())
        o = async.object(foo=None)
        self.assertTrue(async.protected(o))
        self.assertEqual(async.active_contexts(), 0)
        async.submit_work(cb, (w, o))
        self.assertEqual(async.active_contexts(), 1)
        self.assertEqual(async.persisted_contexts(), 0)
        async.signal(w)
        async.run()
        self.assertEqual(async.active_contexts(), 0)
        self.assertEqual(async.persisted_contexts(), 1)
        delattr(o, 'foo')
        self.assertEqual(async.persisted_contexts(), 0)

    def _test_protect_against_object(self):
        self.assertEqual(async.persisted_contexts(), 0)

        o = async.object()
        r = async.object()
        w = async.object()

        d = async.object(reader=None, writer=None)

        def reader():
            async.read_lock(o)
            async.signal(w)         # start writer callback
            async.wait(r)           # wait for writer callback
            d.reader = async.rdtsc()
            async.read_unlock(o)

        def writer():
            async.signal(r)         # tell the reader we've entered
            async.write_lock(o)     # will be blocked until reader unlocks
            d.writer = async.rdtsc()
            async.write_unlock(o)

        async.submit_wait(r, reader)
        async.submit_wait(w, writer)
        async.signal(r)
        async.run()
        self.assertEqual(async.persisted_contexts(), 2)
        delattr(d, 'writer')
        self.assertEqual(async.persisted_contexts(), 1)
        delattr(d, 'reader')
        self.assertEqual(async.persisted_contexts(), 0)

    def test_protect_against_dict(self):
        self.assertEqual(async.persisted_contexts(), 0)

        o = async.object()
        r = async.object()
        w = async.object()

        d = async.dict()

        def reader(name):
            async.read_lock(o)
            async.signal(w)         # start writer callback
            async.wait(r)           # wait for writer callback
            d[name] = async.rdtsc()
            async.read_unlock(o)

        def writer(name):
            async.signal(r)         # tell the reader we've entered
            async.write_lock(o)     # will be blocked until reader unlocks
            d[name] = async.rdtsc()
            async.write_unlock(o)

        async.submit_wait(r, reader, 'r')
        async.submit_wait(w, writer, 'w')
        async.signal(r)
        async.run()
        self.assertEqual(async.persisted_contexts(), 2)
        del d
        self.assertEqual(async.persisted_contexts(), 0)

    def test_protect_against_dict2(self):
        self.assertEqual(async.persisted_contexts(), 0)

        o = async.object()
        r = async.object()
        w = async.object()

        d = async.dict()

        def reader(name):
            async.read_lock(o)
            async.signal(w)         # start writer callback
            async.wait(r)           # wait for writer callback
            d[name] = async.rdtsc()
            async.read_unlock(o)

        def writer(name):
            async.signal(r)         # tell the reader we've entered
            async.write_lock(o)     # will be blocked until reader unlocks
            d[name] = async.rdtsc()
            async.write_unlock(o)

        async.submit_wait(r, reader, 'r')
        async.submit_wait(w, writer, 'w')
        async.signal(r)
        async.run()
        self.assertEqual(async.persisted_contexts(), 2)
        del d['w']
        del d['r']
        del d
        self.assertEqual(async.persisted_contexts(), 0)

    def test_protect_against_dict3(self):
        self.assertEqual(async.persisted_contexts(), 0)

        o = async.protect(object())
        r = async.protect(object())
        w = async.protect(object())

        d = async.dict()

        def reader(name):
            async.read_lock(o)
            async.signal(w)         # start writer callback
            async.wait(r)           # wait for writer callback
            d[name] = async.rdtsc()
            async.read_unlock(o)

        def writer(name):
            async.signal(r)         # tell the reader we've entered
            async.write_lock(o)     # will be blocked until reader unlocks
            d[name] = async.rdtsc()
            async.write_unlock(o)

        async.submit_wait(r, reader, 'r')
        async.submit_wait(w, writer, 'w')
        async.signal(r)
        async.run()
        #self.assertGreater(d['w'], d['r'])
        self.assertEqual(async.persisted_contexts(), 2)
        del d['w']
        del d['r']
        #del d
        self.assertEqual(async.persisted_contexts(), 0)

    def test_persist_dict_with_del(self):
        self.assertEqual(async.persisted_contexts(), 0)

        d = async.dict()

        def cb():
            d['foo'] = async.rdtsc()

        async.submit_work(cb)
        async.run()
        self.assertEqual(async.active_contexts(), 0)
        self.assertEqual(async.persisted_contexts(), 1)
        del d
        self.assertEqual(async.active_contexts(), 0)
        self.assertEqual(async.persisted_contexts(), 0)

    def test_persist_dict_with_del_submit_wait(self):
        self.assertEqual(async.persisted_contexts(), 0)

        o = async.object()
        d = async.dict()

        def cb():
            d['foo'] = async.rdtsc()

        async.submit_wait(o, cb)
        async.signal(o)
        async.run()
        self.assertEqual(async.active_contexts(), 0)
        self.assertEqual(async.persisted_contexts(), 1)
        del d
        self.assertEqual(async.persisted_contexts(), 0)

    def test_persist_dict_with_delitem(self):
        self.assertEqual(async.persisted_contexts(), 0)

        o = async.object()
        d = async.dict()

        def cb():
            d['foo'] = async.rdtsc()

        async.submit_wait(o, cb)
        async.signal(o)
        async.run()
        self.assertEqual(async.active_contexts(), 0)
        self.assertEqual(async.persisted_contexts(), 1)
        del d['foo']
        self.assertEqual(async.persisted_contexts(), 0)

    def test_persist_dict_with_pxobj_as_key(self):
        self.assertEqual(async.persisted_contexts(), 0)

        o = async.object()
        d = async.dict()

        def cb():
            d[async.rdtsc()] = None

        async.submit_wait(o, cb)
        async.signal(o)
        async.run()
        self.assertEqual(async.active_contexts(), 0)
        self.assertEqual(async.persisted_contexts(), 1)
        del d
        self.assertEqual(async.persisted_contexts(), 0)

    def test_persist_dict_with_multiple_callbacks(self):
        self.assertEqual(async.persisted_contexts(), 0)

        o = async.object()
        d = async.dict()

        def cb():
            d[async.rdtsc()] = None

        async.submit_work(cb)
        async.submit_work(cb)
        async.submit_work(cb)
        async.submit_work(cb)
        async.run()
        self.assertEqual(async.active_contexts(), 0)
        self.assertEqual(async.persisted_contexts(), 4)
        self.assertEqual(len(d), 4)
        del d
        self.assertEqual(async.persisted_contexts(), 0)

class TestWraps(unittest.TestCase):
    def _test(self, init, cls):
        o = init()
        self.assertTrue(async.protected(o))
        self.assertIsInstance(o, cls)
        return o

    def test_object(self):
        o = self._test(async.object, object)
        o.foo = 'bar'
        o.bar = 'foo'
        self.assertEqual(o.foo, 'bar')
        self.assertEqual(o.bar, 'foo')
        self.assertRaises(AttributeError, lambda s: getattr(o, s), 'moo')

    def test_dict(self):
        o = self._test(async.dict, dict)
        o['foo'] = 'bar'
        self.assertTrue('foo' in o)
        self.assertEqual(o['foo'], 'bar')

def main():
    unittest.main()

if __name__ == '__main__':
    main()

# vim:set ts=8 sw=4 sts=4 tw=78 et:
