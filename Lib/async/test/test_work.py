import os
import sys
import unittest

import async
import _async

class TestBasic(unittest.TestCase):
    def test_calling_run_with_no_events_fails(self):
        self.assertRaises(AsyncRunCalledWithoutEventsError, _async.run_once)

class TestSubmitWork(unittest.TestCase):

    def test_submit_simple_work(self):
        def f():
            return
        _async.submit_work(f, None, None, None, None)
        _async.run()
        self.assertEqual(1, 1)

    def test_submit_simple_work_with_arg(self):
        def f(i):
            return i * 2
        _async.submit_work(f, 2, None, None, None)
        _async.run()
        self.assertEqual(1, 1)

    def test_submit_simple_work_with_callback(self):
        @async.call_from_main_thread
        def _check(r):
            self.assertEqual(r, 4)

        def f(i):
            return i * 2
        def cb(r):
            _check(r)

        _async.submit_work(f, 2, None, cb, None)
        _async.run()

    def test_value_error_in_callback(self):
        def f():
            return laksjdflaskjdflsakjdfsalkjdf
        _async.submit_work(f, None, None, None, None)
        self.assertRaises(NameError, _async.run)

    def test_value_error_in_callback_then_run(self):
        def f():
            return laksjdflaskjdflsakjdfsalkjdf
        _async.submit_work(f, None, None, None, None)
        self.assertRaises(NameError, _async.run)
        _async.run()

    def test_multiple_value_errors_in_callback_then_run(self):
        def f():
            return laksjdflaskjdflsakjdfsalkjdf
        _async.submit_work(f, None, None, None, None)
        _async.submit_work(f, None, None, None, None)
        self.assertRaises(NameError, _async.run)
        self.assertRaises(NameError, _async.run)
        _async.run()

    def test_call_from_main_thread(self):
        d = {}
        def f(i):
            _async.call_from_main_thread_and_wait(
                d.__setitem__,
                ('foo', i*2),
            )
            return _async.call_from_main_thread_and_wait(
                d.__getitem__, 'foo'
            )
        def cb(r):
            _async.call_from_main_thread(
                self.assertEqual,
                (r, 4),
            )
        _async.submit_work(f, 2, None, cb, None)
        _async.run()

    def test_call_from_main_thread_decorator(self):
        @async.call_from_main_thread
        def f():
            self.assertFalse(_async.is_parallel_thread())
        _async.submit_work(f, None, None, None, None)
        _async.run()

    def test_submit_simple_work_errback_invoked(self):
        def f():
            return laksjdflaskjdflsakjdfsalkjdf

        @async.call_from_main_thread_and_wait
        def test_e(e):
            print("\n-2\n")
            self.assertFalse(_async.is_parallel_thread())
            print(repr(e))
            (et, ev, eb) = e
            print("\n-1\n")
            print("*1\n")
            print(repr(et))
            print("*2\n")
            print(repr(ev))
            print("*3\n")
            print(repr(eb))
            try:
                print("\n0\n")
                f()
            except NameError as e2:
                print("\n1\n")
                print(repr(et))
                self.assertEqual(et, e2.__class__)
                print("2\n")
                print(repr(ev))
                self.assertEqual(ev, e2.args[0])
                print("3\n")
                print(repr(eb))
                self.assertEqual(eb.__class__, e2.__traceback__.__class__)
            else:
                self.assertEqual(0, 1)

        @async.call_from_main_thread_and_wait
        def cb(r):
            self.assertEqual(0, 1)

        def eb(e):
            test_e(e)

        _async.submit_work(f, None, None, cb, eb)
        while _async.is_active():
            _async.run_once()

if __name__ == '__main__':
    unittest.main()

# vim:set ts=8 sw=4 sts=4 tw=78 et:
