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
    TCPClientCommand,
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
# Socket/Networking Commands
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
    port = None
    class PortArg(NonEphemeralPortInvariant):
        _help = 'port to listen on [default: %default]'
        _default = 10019

    ip = None
    class IpArg(StringInvariant):
        _help = 'IP address to listen on [default: %default]'
        _default = IPADDR

    def run(self):
        ip = self.options.ip
        port = int(self.options.port)

        self._out("Running chargen on %s port %d ..." % (ip, port))

        import async.services
        server = async.server(ip, port)
        protocol = async.services.Chargen
        async.register(transport=server, protocol=protocol)
        async.run()

class DisconnectServer(TCPServerCommand):
    port = None
    class PortArg(NonEphemeralPortInvariant):
        _help = 'port to listen on [default: %default]'
        _default = 10000

    ip = None
    class IpArg(StringInvariant):
        _help = 'IP address to listen on [default: %default]'
        _default = IPADDR

    def run(self):
        ip = self.options.ip
        port = int(self.options.port)

        self._out("Running disconnect on %s port %d ..." % (ip, port))

        import async.services
        server = async.server(ip, port)
        protocol = async.services.Disconnect
        async.register(transport=server, protocol=protocol)
        async.run()

class StaticQotdServer(TCPServerCommand):
    port = None
    class PortArg(NonEphemeralPortInvariant):
        _help = 'port to listen on [default: %default]'
        _default = 10017

    ip = None
    class IpArg(StringInvariant):
        _help = 'IP address to listen on [default: %default]'
        _default = IPADDR

    def run(self):
        ip = self.options.ip
        port = int(self.options.port)

        self._out("Running static QOTD on %s port %d ..." % (ip, port))

        import async.services
        server = async.server(ip, port)
        protocol = async.services.StaticQotd
        async.register(transport=server, protocol=protocol)
        async.run()

class DynamicQotdServer(TCPServerCommand):
    port = None
    class PortArg(NonEphemeralPortInvariant):
        _help = 'port to listen on [default: %default]'
        _default = 10017

    ip = None
    class IpArg(StringInvariant):
        _help = 'IP address to listen on [default: %default]'
        _default = IPADDR

    def run(self):
        ip = self.options.ip
        port = int(self.options.port)

        self._out("Running dynamic QOTD on %s port %d ..." % (ip, port))

        import async.services
        server = async.server(ip, port)
        protocol = async.services.DynamicQotd
        async.register(transport=server, protocol=protocol)
        async.run()


class DaytimeServer(TCPServerCommand):
    _shortname_ = 'day'

    port = None
    class PortArg(NonEphemeralPortInvariant):
        _help = 'port to listen on [default: %default]'
        _default = 10013

    ip = None
    class IpArg(StringInvariant):
        _help = 'IP address to listen on [default: %default]'
        _default = IPADDR

    def run(self):
        ip = self.options.ip
        port = int(self.options.port)

        self._out("Running daytime server on %s port %d ..." % (ip, port))

        import async.services
        server = async.server(ip, port)
        protocol = async.services.Daytime
        async.register(transport=server, protocol=protocol)
        async.run()

class TimeServer(TCPServerCommand):
    port = None
    class PortArg(NonEphemeralPortInvariant):
        _help = 'port to listen on [default: %default]'
        _default = 10037

    ip = None
    class IpArg(StringInvariant):
        _help = 'IP address to listen on [default: %default]'
        _default = IPADDR

    def run(self):
        ip = self.options.ip
        port = int(self.options.port)

        self._out("Running time server on %s port %d ..." % (ip, port))

        import async.services
        server = async.server(ip, port)
        protocol = async.services.Time
        async.register(transport=server, protocol=protocol)
        async.run()

class DiscardServer(TCPServerCommand):
    _shortname_ = 'disc'

    port = None
    class PortArg(NonEphemeralPortInvariant):
        _help = 'port to listen on [default: %default]'
        _default = 10009

    ip = None
    class IpArg(StringInvariant):
        _help = 'IP address to listen on [default: %default]'
        _default = IPADDR

    def run(self):
        ip = self.options.ip
        port = int(self.options.port)

        self._out("Running discard server on %s port %d ..." % (ip, port))

        import async.services
        server = async.server(ip, port)
        protocol = async.services.Discard
        async.register(transport=server, protocol=protocol)
        async.run()


class EchoServer(TCPServerCommand):
    port = None
    class PortArg(NonEphemeralPortInvariant):
        _help = 'port to listen on [default: %default]'
        _default = 10007

    ip = None
    class IpArg(StringInvariant):
        _help = 'IP address to listen on [default: %default]'
        _default = IPADDR

    def run(self):
        ip = self.options.ip
        port = int(self.options.port)

        self._out("Running echo server on %s port %d ..." % (ip, port))

        import async.services
        server = async.server(ip, port)
        protocol = Disconnect
        async.register(transport=server, protocol=protocol)
        async.run()

#===============================================================================
# Clients
#===============================================================================
class SimpleHttpGetClient(TCPClientCommand):
    port = None
    class PortArg(NonEphemeralPortInvariant):
        _help = 'port to connect to [default: %default]'
        _default = 8080

    ip = None
    class IpArg(StringInvariant):
        _help = 'IP address to connect to [default: %default]'
        _default = IPADDR

    def run(self):
        ip = self.options.ip
        port = int(self.options.port)

        self._out("Connecting to %s:%d..." % (ip, port))

        class HttpGet:
            initial_bytes_to_send = b'GET /json HTTP/1.0\r\n\r\n\r\n'
            def data_received(self, transport, data):
                #async.print(data)
                async.debug(data)

        import async
        client = async.client(ip, port)
        protocol = HttpGet
        async.register(transport=client, protocol=protocol)
        async.run()


#===============================================================================
# Testing
#===============================================================================
class TestGenerator(TCPServerCommand):
    port = None
    class PortArg(NonEphemeralPortInvariant):
        _help = 'port to listen on [default: %default]'
        _default = 10008

    ip = None
    class IpArg(StringInvariant):
        _help = 'IP address to listen on [default: %default]'
        _default = IPADDR

    def run(self):

        ip = self.options.ip
        port = int(self.options.port)

        self._out("Running test server on %s port %d ..." % (ip, port))

        class GeneratorTest:
            def data_received(self, transport, data):
                return b', '.join([chr(i) for i in (1, 2, 3)])

        import async
        server = async.server(ip, port)
        protocol = GeneratorTest
        async.register(transport=server, protocol=protocol)
        async.run()


#===============================================================================
# System Info/Memory Commands
#===============================================================================
class SystemStructureSizes(PxCommand):
    def run(self):
        out = self._out

        import _async

        from itertools import (
            chain,
            repeat
        )

        from ctk.util import (
            render_text_table,
            Dict,
        )

        k = Dict()
        k.banner = ('System Structure Sizes', '(%d-bit)' % _async._bits)
        k.formats = lambda: chain((str.center,), repeat(str.center))
        k.output = self.ostream

        names = dir(_async)

        rows = [('Name', 'Size (bytes)')]

        def format_mem(n):
            name = n.replace('_mem_', '').replace('_', ' ').title()
            return (name, getattr(_async, n))

        def format_sizeof(n):
            name = 'sizeof(%s)' % n.replace('_sizeof_', '')
            return (name, getattr(_async, n))

        rows += [ format_mem(n) for n in names if n.startswith('_mem_') ]
        rows += [ format_sizeof(n) for n in names if n.startswith('_sizeof_') ]

        render_text_table(rows, **k)

class MemoryStats(PxCommand):
    def run(self):
        out = self._out

        import _async

        from itertools import (
            chain,
            repeat
        )

        from ctk.util import (
            bytes_to_tb,
            bytes_to_gb,
            bytes_to_mb,
            bytes_to_kb,
            render_text_table,
            Dict,
        )

        k = Dict()
        k.banner = (
            'System Memory Stats',
            '(Current Load: %d%%)' % _async._memory_load,
        )
        k.formats = lambda: chain((str.rjust,), repeat(str.rjust))
        k.output = self.ostream

        names = [
            n for n in dir(_async) if (
                n.startswith('_memory_') and
                'load' not in n
            )
        ]

        rows = [('Name', 'KB', 'MB', 'GB', 'TB')]

        def format_mem(n):
            name = n.replace('_memory_', '').replace('_', ' ').title()
            b = getattr(_async, n)
            return (
                name,
                bytes_to_kb(b),
                bytes_to_mb(b),
                bytes_to_gb(b),
                bytes_to_tb(b),
            )

        rows += [ format_mem(n) for n in names if n.startswith('_memory_') ]

        render_text_table(rows, **k)

# vim:set ts=8 sw=4 sts=4 tw=78 et:
