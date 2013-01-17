import _async

from _async import (
    run,
    client,
    server,
    run_once,
    protect,
    unprotect,
    protected,
)

def call_from_main_thread_and_wait(f):
    def decorator(*_args, **_kwds):
        return _async.call_from_main_thread_and_wait(f, _args, _kwds)
    return decorator

def call_from_main_thread(f):
    def decorator(*_args, **_kwds):
        _async.call_from_main_thread(f, _args, _kwds)
    return decorator

RECEIVE_MODE_DATA = 1
RECEIVE_MODE_LINE = 2

# vim:set ts=8 sw=4 sts=4 tw=78 et:
