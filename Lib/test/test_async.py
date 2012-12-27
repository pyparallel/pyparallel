import sys
import unittest

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
        return i * 2
    def cb(r):
        print("result: %d" % r)
    _async.submit_work(f, 2, None, cb, None)
    _async.run_once()

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
