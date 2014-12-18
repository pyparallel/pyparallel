#===============================================================================
# Imports
#===============================================================================
import sys
import pdb

#===============================================================================
# Helpers
#===============================================================================
def iset_trace():
    from IPython.core.debugger import Pdb
    Pdb(color_scheme='Linux').set_trace(sys._getframe().f_back)

def idebug(f, *args, **kwds):
    from IPython.core.debugger import Pdb
    pdb = Pdb(color_scheme='Linux')
    return pdb.runcall(f, *args, **kwds)

#===============================================================================
# Classes
#===============================================================================

class RemoteDebugSession(pdb.Pdb):
    def __init__(self, host, port):
        import pdb
        import socket
        self.old_stdout = sys.stdout
        self.old_stdin = sys.stdin
        self.sock = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
        self.sock.bind((host, port))
        (self.host, self.port) = self.sock.getsockname()
        self.dst_host = ''
        self.dst_port = 0
        self.state = 'listening'
        #self._dump_state()

        self.sock.listen(1)
        (clientsocket, address) = self.sock.accept()
        (self.dst_host, self.dst_port) = address
        self.state = 'connected'
        #self._dump_state()

        handle = clientsocket.makefile('rw')
        pdb.Pdb.__init__(self, completekey='tab', stdin=handle, stdout=handle)
        sys.stdout = sys.stdin = handle

    def __repr__(self):
        return repr(dict(
            (k, getattr(self, k))
                for k in dir(RemoteDebugSessionStatus)
                    if k[0] != '_'
        ))

    def __str__(self):
        return repr(self)

    def _dump_state(self):
        with open(self.path, 'w') as f:
            f.write(repr(self))
            f.flush()
            f.close()

    def __do_continue(self, *arg):
        self.state = 'finished'
        #self._dump_state()
        sys.stdout = self.old_stdout
        sys.stdin = self.old_stdin
        self.sock.close()
        self.set_continue()
        return 1

# vim:set ts=8 sw=4 sts=4 tw=78 et:
