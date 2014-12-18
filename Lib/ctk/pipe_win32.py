#===============================================================================
# Imports
#===============================================================================
import os

from win32pipe import CreatePipe
from pywintypes import SECURITY_ATTRIBUTES
from msvcrt import open_osfhandle

#===============================================================================
# Classes
#===============================================================================
class Pipe(object):
    def __init__(self, fd, mode):
        assert mode in ('r', 'w')
        self._mode = mode

        flags = os.O_APPEND | os.O_BINARY
        flags |= os.O_RDONLY if mode == 'r' else os.O_WRONLY
        handle = open_osfhandle(fd, flags)
        self._f = os.fdopen(handle, mode + 'b')
        self._h = handle
        self._fd = fd

    def __getattr__(self, attr):
        if attr[0] == '_':
            return object.__getattr__(self, attr)
        else:
            return getattr(self._f, attr)
    def seekable(self):
        return False
    def readable(self):
        return 'r' in self._mode
    def writable(self):
        return 'w' in self._mode

def pipe():
    sa = SECURITY_ATTRIBUTES()
    sa.bInheritHandle = 1
    (r, w) = CreatePipe(sa, 0)
    return (Pipe(r, 'r'), Pipe(w, 'w'))

# vim:set ts=8 sw=4 sts=4 tw=78 et:
