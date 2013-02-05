class Constant(dict):
    def __init__(self):
        items = self.__class__.__dict__.items()
        for (key, value) in filter(lambda t: t[0][:2] != '__', items):
            try:
                self[value] = key
            except:
                pass
    def __getattr__(self, name):
        return self.__getitem__(name)
    def __setattr__(self, name, value):
        return self.__setitem__(name, value)

class _CachingBehavior(Constant):
    Default         = 0
    Buffered        = 1
    RandomAccess    = 2
    SequentialScan  = 3
    WriteThrough    = 4
    Temporary       = 5
CachingBehavior = _CachingBehavior()

_dict = dict
_list = list
_object = object

import _async
from _async import *

class object:
    def __init__(self, **kwds):
        self.__dict__.update(kwds)
        _async.protect(self)

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

def submit_wait(obj, func=None, args=None, kwds=None,
                callback=None, errback=None, timeout=None):
    _async.submit_wait(obj, timeout, func, args, kwds, callback, errback)

#def submit_write_io(writeiofunc, buf, callback=None, errback=None):
#    _async.submit_write_io(writeiofunc, buf, callback, errback)

#def submit_read_io(readiofunc, callback, nbytes=0, errback=None):
#    _async.submit_read_io(readiofunc, nbytes, callback, errback)

_open = open
def open(filename, mode, caching=0, size=0, template=None):
    def open_decorator(f):
        def decorator(name, mode, fileobj):
            return f(caching, size, template, name, mode, fileobj)
        return decorator

    @open_decorator
    def fileopener(caching, size, template, name, mode, fileobj):
        return _async.fileopener(caching, size, template, name, mode, fileobj)

    f = _open(
        filename,
        mode=mode,
        opener=fileopener,
        closer=_async.filecloser
    )
    r = _async._rawfile(f)
    p = _async.protect(r)
    return p
_async_open = open

def close(obj):
    _async._close(obj)
    obj.close()

def write(obj, buf, callback=None, errback=None):
    _async.submit_write_io(obj, buf, callback, errback)

def writefile(filename, buf, callback=None, errback=None):
    f = open(filename, 'wb', size=len(buf))
    _async.submit_write_io(f, buf, callback, errback)
    f.close()

def read(obj, callback, nbytes=0, errback=None):
    _async.submit_read_io(obj.write, buf, callback, errback)

def prewait(obj=None):
    if obj is None:
        obj = object()
    return _async.prewait(obj)

def pipe(reader, writer, bufsize=0, callback=None, errback=None):
    raise NotImplementedError

def wait_any(waits):
    raise NotImplementedError

def wait_all(waits):
    raise NotImplementedError

def chargen(lineno, nchars=72):
    start = ord(' ')
    end = ord('~')
    c = lineno + start
    if c > end:
        c = (c % end) + start
    b = bytearray(nchars)
    for i in range(0, nchars):
        if c > end:
            c = start
        b[i] = c
        c += 1

    return b

class EchoServer(_async.server):
    def data_received(self, data):
        return data

QOTD = b'An apple a day keeps the doctor away.\r\n'

class QOTD(_async.server):
    initial_bytes_to_send = QOTD

class BaseChargen:
    def __init__(self):
        self._lineno = -1

    @property
    def lineno(self):
        self._lineno += 1
        return self._lineno

    def chargen(self):
        return chargen(self.lineno)

class Chargen(BaseChargen):
    def initial_bytes_to_send(self):
        return self.chargen()

    def data_sent(self, data):
        return self.chargen()

class ChargenBrute(BaseChargen):
    def connection_made(self, sock):
        async.call_next(self.connection_made)
        return self.chargen()

class Disconnect:
    def connection_made(self, sock):
        sock.disconnect()

class Discard:
    def data_received(self, data):
        pass

class EchoData:
    def data_received(self, data):
        return data

class EchoLine:
    line_mode = True
    def line_received(self, line):
        return line

# vim:set ts=8 sw=4 sts=4 tw=78 et:
