import _async

from _async import *

def call_from_main_thread_and_wait(f):
    def decorator(*_args, **_kwds):
        return _async.call_from_main_thread_and_wait(f, _args, _kwds)
    return decorator

def call_from_main_thread(f):
    def decorator(*_args, **_kwds):
        _async.call_from_main_thread(f, _args, _kwds)
    return decorator

def synchronized(f):
    cs = _async.critical_section()
    def decorator(*_args, **_kwds):
        cs.enter()
        f(*_args, **_kwds)
        cs.leave()
    return decorator

def submit_work(func, args=None, kwds=None, callback=None, errback=None):
    _async.submit_work(func, args, kwds, callback, errback)

def submit_wait(obj, func, args=None, kwds=None, callback=None, errback=None):
    raise NotImplementedError
    _async.submit_wait(obj, func, args, kwds, callback, errback)

def wait_any(waits):
    raise NotImplementedError

def wait_all(waits):
    raise NotImplementedError

# vim:set ts=8 sw=4 sts=4 tw=78 et:
