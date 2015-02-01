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
# Main Commands
#===============================================================================
class JsonSerialization(TCPServerCommand):
    port = None
    class PortArg(NonEphemeralPortInvariant):
        _help = 'port to listen on [default: %default]'
        _default = 8080

    ip = None
    class IpArg(StringInvariant):
        _help = 'IP address to listen on [default: %default]'
        _default = IPADDR

    def run(self):
        ip = self.options.ip
        port = int(self.options.port)
        root = self.options.root or os.getcwd()

        self._out("Serving HTTP on %s port %d ..." % (ip, port))

        import async
        from . import JSONSerializationHttpServer

        with chdir(root):
            server = async.server(ip, port)
            protocol = JSONSerializationHttpServer
            async.register(transport=server, protocol=protocol)
            async.run()

class PlainText(TCPServerCommand):
    port = None
    class PortArg(NonEphemeralPortInvariant):
        _help = 'port to listen on [default: %default]'
        _default = 8080

    ip = None
    class IpArg(StringInvariant):
        _help = 'IP address to listen on [default: %default]'
        _default = IPADDR

    def run(self):
        ip = self.options.ip
        port = int(self.options.port)
        root = self.options.root or os.getcwd()

        self._out("Serving plain text on %s port %d ..." % (ip, port))

        import async
        from . import PlainTextDummyServer

        with chdir(root):
            server = async.server(ip, port)
            protocol = PlainTextDummyServer
            async.register(transport=server, protocol=protocol)
            async.run()

# vim:set ts=8 sw=4 sts=4 tw=78 et:
