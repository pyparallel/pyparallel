import sys
import unittest

import async
import _async

class TestBasic(unittest.TestCase):
    def test_calling_run_with_no_events_fails(self):
        self.assertRaises(AsyncRunCalledWithoutEventsError, _async.run)

class TestSubmitWork(unittest.TestCase):
    def test_submit_simple_work(self):
        def f(i):
            return i * 2
        def cb(r):
            self.assertEqual(r, 4)
        _async.submit_work(f, 2, None, cb, None)

        self.assertRaises(AsyncRunCalledWithoutEventsError, _async.run)

    def test_calling_run_with_no_events_fails(self):
        self.assertRaises(AsyncRunCalledWithoutEventsError, _async.run)

def t1():
    def f(i):
        r = i * 2
        s = "result: %d" % r
        _async.call_from_main_thread(print, s)
    _async.submit_work(f, 2)
    async.run()

def t2():
    def f(i):
        return i * 2
    def cb(r):
        s = "result x 2 = %d" % (r * 2)
        _async.call_from_main_thread(print, s)

    _async.submit_work(f, 2, None, cb, None)
    async.run()

def t3():
    d = {}
    def f(i):
        return i * 2
    def cb(r):
        _async.call_from_main_thread(d.__setitem__, ('result', r*2))

    _async.submit_work(f, 2, None, cb, None)
    async.run()
    print(d)

def t4():
    d = {}
    def f(i):
        return i * 12
    def cb(r):
        _async.call_from_main_thread_and_wait(d.__setitem__, ('result', r*2))
        v = _async.call_from_main_thread_and_wait(
            d.__getitem__, 'result'
        )
        _async.call_from_main_thread_and_wait(print, "v: %d" % v)

    _async.submit_work(f, 2, None, cb, None)
    async.run()
    #print(d)

def t5():
    d = {}
    def f(i):
        return i * 12
    def cb(r):
        _async.call_from_main_thread_and_wait(d.__setitem__, ('result', r*2))
        v = _async.call_from_main_thread_and_wait(
            d.__getitem__, 'result'
        )
        _async.call_from_main_thread_and_wait(print, "v: %d" % v)

    _async.submit_work(f, 2, None, cb, None)
    async.run()
    #print(d)

def t6():
    d = {}
    def f(i):
        r = i * 2
        s = "result: %d" % r
        return s

    def cb(s):
        d['foo'] = reversed(s)

    _async.submit_work(f, 2, None, cb, None)
    async.run()
    print(d)

def f7():
    return result

def t7():
    _async.submit_work(f7, None, None, None, None)
    async.run()

def t8():
    _async.submit_work(f7, None, None, None, None)
    while _async.is_active():
        _async.run_once()

def t9():
    d = {'foo' : 'bar'}
    def m(x):
        return x.upper()

    def f(i):
        v = _async.call_from_main_thread_and_wait(m, 'foo')
        _async.call_from_main_thread(print, "v: %d" % v)

    _async.submit_work(f, 'foo', None, None, None)
    async.run()

if __name__ == '__main__':
    if len(sys.argv) == 2:
        fn = "%s()" % sys.argv[1]
        if not fn.startswith('t'):
            fn = 't' + fn
        print("fn: %s" % fn)
        eval(fn)
    else:
        unittest.main()

# vim:set ts=8 sw=4 sts=4 tw=78 et:
