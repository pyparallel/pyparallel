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
    PortInvariant,
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
#IPADDR = socket.gethostbyname(HOSTNAME)
IPADDR = '0.0.0.0'

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

class SimpleHttpGetClientWithDns(TCPClientCommand):
    port = None
    class PortArg(PortInvariant):
        _help = 'port to connect to [default: %default]'
        _default = 80

    addr = None
    class AddrArg(StringInvariant):
        _help = 'host address to connect to [default: %default]'
        _default = 'www.google.com'

    def run(self):
        hostname = self.options.addr
        port = int(self.options.port)

        self._out("Connecting to %s:%d..." % (hostname, port))

        class HttpGet:
            initial_bytes_to_send = b'GET / HTTP/1.0\r\n\r\n\r\n'
            def data_received(self, transport, data):
                #async.print(data)
                async.debug(data)

        import async
        client = async.client(hostname, port)
        protocol = HttpGet
        async.register(transport=client, protocol=protocol)
        async.run()

#===============================================================================
# Demo Server
#===============================================================================
class DemoServer(TCPServerCommand):
    _shortname_ = 'demo'

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

        self._out("Running demo server on %s port %d ..." % (ip, port))

        import async.services
        server = async.server(ip, port)
        protocol = async.services.Time
        async.register(transport=server, protocol=protocol)
        async.run()



#===============================================================================
# Testing
#===============================================================================
def _test_generator():
    yield '1'

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

        class GeneratorTest1:
            def initial_bytes_to_send(self):
                return (', '.join([chr(i) for i in (1, 2, 3)])).encode('utf-8')

        class GeneratorTest2:
            def initial_bytes_to_send(self):
                return (', '.join(chr(i) for i in (1, 2, 3))).encode('utf-8')

        import async
        class GeneratorTest3:
            def yield_c(self):
                for c in ('A', 'B', 'C'):
                    async.debug(c)
                    yield c

            def data_received(self, transport, data):
                return (', '.join(c for c in self.yield_c())).encode('utf-8')

        server = async.server(ip, port)
        protocol = GeneratorTest3
        async.register(transport=server, protocol=protocol)
        async.run()

#===============================================================================
# Primitives
#===============================================================================
class TestCallFromMainThreadAndWait(TCPServerCommand):
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

        self._out("Running test server on %s port %d ..." % (ip, port))

        import async.http.server
        with chdir(root):
            server = async.server(ip, port)
            protocol = async.http.server.HttpServer
            async.register(transport=server, protocol=protocol)
            async.run()

        class Protocol(async.http.server.HttpServer):
            @async.call_from_main_thread_and_wait
            def _timestamp(self):
                return async.rdtsc()

            def data_received(self, transport, data):
                return b', '.join([chr(i) for i in (1, 2, 3)])

        import async
        server = async.server(ip, port)
        protocol = GeneratorTest
        async.register(transport=server, protocol=protocol)
        async.run()

#===============================================================================
# Dev Helpers
#===============================================================================
class UpdateDiffs(PxCommand):
    """
    Diffs PyParallel against original v3.3.5 tag it was based upon and,
    for all modified files (i.e. we exclude new files), create a diff
    and store it in diffs/<dirname>/<filename>.diff.
    """
    base_rev = None
    class BaseRevArg(StringInvariant):
        _help = 'base rev/tag to diff against [default: %default]'
        _default = 'v3.3.5'
        _mandatory = False

    target_rev = None
    class TargetRevArg(StringInvariant):
        _help = 'target rev/tag to diff against [default: %default]'
        _default = '3.3-px'
        _mandatory = False

    root = None
    class RootArg(DirectoryInvariant):
        _help = 'hg repository root'
        _mandatory = False

    def run(self):
        import os
        from collections import defaultdict
        from ctk.path import abspath, normpath, join_path, splitpath

        import pdb
        dbg = pdb.Pdb()
        #dbg.set_trace()

        root = self.options.root
        if not root:
            root = join_path(dirname(__file__), '../..')

        if os.getcwd() != root:
            os.chdir(root)

        basedir = join_path(root, 'diffs')

        base_rev = self.options.base_rev
        target_rev = self.options.target_rev

        #dbg.set_trace()
        cmd = (
            'hg st --rev %s --rev %s > hg-st.txt' % (
                base_rev,
                target_rev,
        ))
        os.system(cmd)

        with open('hg-st.txt', 'r') as f:
            data = f.read()

        lines = data.splitlines()
        d = defaultdict(list)
        for line in lines:
            (action, path) = line.split(' ', 1)
            d[action].append(path)

        isdir = os.path.isdir
        for path in d['M']:
            (base, filename) = splitpath(path)
            if base:
                diffdir = join_path(basedir, base)
            else:
                diffdir = basedir
            if not isdir(diffdir):
                os.makedirs(diffdir)

            patchname = filename + '.patch'
            patchpath = join_path(diffdir, patchname)
            cmd = (
                'hg diff --rev %s '
                ' --git '
                ' --show-function '
                #' --ignore-all-space '
                ' --ignore-blank-lines '
                ' --ignore-space-change '
                ' "%s" > "%s"' % (
                    base_rev,
                    path,
                    patchpath,
                )
            )
            #dbg.set_trace()
            os.system(cmd)
            st = os.stat(patchpath)
            if st.st_size == 0:
                os.unlink(patchpath)
            else:
                self._out("Updated %s." % patchpath.replace(root + '\\', ''))


#===============================================================================
# System Info/Memory Commands
#===============================================================================
class IsAsyncOdbcAvailable(PxCommand):
    def run(self):
        import _async
        if _async._async_odbc_available:
            self._out('yes')
        else:
            self._out('no')

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
