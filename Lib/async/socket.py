
import _async

import _socket

from _socket import (
    socket,
    AF_INET,
    SOCK_STREAM,
)

_BUFSIZE = 8192
_MIN_BUFSIZE = 4096
_MAX_BUFSIZE = 65536

def validate_bufsize(size):
    valid = False
    try:
        valid = (
            isinstance(size, int) and
            size >= _MIN_BUFSIZE  and
            size <= _MAX_BUFSIZE
        )
    except:
        pass

    if not valid:
        raise ValueError("invalid bufsize")

def get_default_bufsize():
    return _BUFSIZE

def set_default_bufsize(size):
    validate_bufsize(size)
    _BUFSIZE = size


class Socket(socket):
    def __init__(self, sock=None, family=AF_INET, type=SOCK_STREAM, proto=0,
                 fileno=None):

        validate_bufsize(bufsize)

        connected = False
        if sock is not None:
            socket.__init__(self,
                            family=sock.family,
                            type=sock.type,
                            proto=sock.proto,
                            fileno=sock.fileno())
            self.settimeout(sock.gettimeout())
            try:
                sock.getpeername()
            except socket_error as e:
                if e.errno != errno.ENOTCONN:
                    raise
            else:
                connected = True
            sock.detach()
        elif fileno is not None:
            socket.__init__(self, fileno=fileno)
        else:
            socket.__init__(self, family=family, type=type, proto=proto)

        self._sock = _async.socket(self)


# vim:set ts=8 sw=4 sts=4 tw=78 et:
