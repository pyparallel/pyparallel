#===============================================================================
# Imports
#===============================================================================
import os

import socket

from os.path import (
    exists,
    abspath,
    dirname,
    basename,
)

from ctk.util import (
    chdir,
    implicit_context,
)

from ctk.path import join_path

from ctk.invariant import (
    BoolInvariant,
    PathInvariant,
    StringInvariant,
    DirectoryInvariant,
    NonEphemeralPortInvariant,
    ExistingDirectoryInvariant,
)

from ctk.command import (
    CommandError,
)

from px.command import (
    PxCommand,
    TCPServerCommand,
)

#===============================================================================
# Globals
#===============================================================================
HOSTNAME = socket.gethostname()
IPADDR = socket.gethostbyname(HOSTNAME)

#===============================================================================
# Miscellaneous/Generic Commands
#===============================================================================
class DumpConfigCommand(PxCommand):
    def run(self):
        self.conf.write(self.ostream)

class ShowConfigFileLoadOrderCommand(PxCommand):
    def run(self):
        if not self.conf.files:
            raise CommandError('no configuration files are being loaded')
        self._out(os.linesep.join(self.conf.files))

#===============================================================================
# Main Commands
#===============================================================================
class HttpServer(TCPServerCommand):
    port = None
    class PortArg(NonEphemeralPortInvariant):
        _help = 'port to listen on [default: %default]'
        _default = 8080

    root = None
    class RootArg(ExistingDirectoryInvariant):
        _help = 'root path to serve documents from (defaults to current dir)'
        _mandatory = False

    ip = None
    class IpArg(StringInvariant):
        _help = 'IP address to listen on [default: %default]'
        _default = IPADDR

    def run(self):
        ip = self.options.ip
        port = int(self.options.port)
        root = self.options.root or os.getcwd()

        self._out("Serving HTTP on %s port %d ..." % (ip, port))

        import async.http.server
        with chdir(root):
            server = async.server(ip, port)
            protocol = async.http.server.HttpServer
            async.register(transport=server, protocol=protocol)
            async.run()


class ProductionHttpServer(TCPServerCommand):
    pass

class ChargenServer(TCPServerCommand):
    pass

class DisconnectServer(TCPServerCommand):
    pass

class QotdServer(TCPServerCommand):
    pass

class TimeServer(TCPServerCommand):
    pass

class NullSinkServer(TCPServerCommand):
    pass

class EchoServer(TCPServerCommand):
    pass

# vim:set ts=8 sw=4 sts=4 tw=78 et:
