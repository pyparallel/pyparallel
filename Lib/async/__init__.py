import _async

from _async import (
    run,
    run_once,
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

class _client:
    __slots__ = [
        'socket',

        'data_received',
        'line_received',
        'write_succeeded',
        'connection_lost',
        'connection_closed',

        'initial_data',
        'receive_mode',
        'auto_reconnect',
        'eol',
        'wait_for_eol',
    ]
    def __init__(self, sock, **kwds):
        self.socket = sock
        self.__dict__.update(**kwds)

def make_client(initial_data=None,
            connected=None,
            data_received=None,
            line_received=None,
            write_succeeded=None,
            connection_lost=None,
            connection_closed=None,

            receive_mode=RECEIVE_MODE_DATA,
            auto_reconnect=False,
            eol='\r\n',
            wait_for_eol=True):
    c = object()



def client(address,
           initial_data=None,
           connected=None,
           data_received=None,
           line_received=None,
           write_succeeded=None,
           connection_lost=None,
           connection_closed=None,

           receive_mode=RECEIVE_MODE_DATA,
           auto_reconnect=False,
           eol='\r\n',
           wait_for_eol=True):

    c = _async.client(address,
                      initial_data=initial_data,
                      connected=connected,
                      data_received=data_received,
                      line_received=line_received,
                      write_succeeded=write_succeeded,
                      connection_lost=connection_lost,
                      connection_closed=connection_closed,

                      auto_reconnect=auto_reconnect,
                      eol=eol,
                      wait_for_eol=wait_for_eol)






# vim:set ts=8 sw=4 sts=4 tw=78 et:
