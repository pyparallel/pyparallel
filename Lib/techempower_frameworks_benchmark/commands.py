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
    PositiveIntegerInvariant,
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
#IPADDR = socket.gethostbyname(HOSTNAME)
IPADDR = '0.0.0.0'

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
        from . import BaseHttpServer

        with chdir(root):
            server = async.server(ip, port)
            async.register(transport=server, protocol=BaseHttpServer)
            try:
                async.run()
            except KeyboardInterrupt:
                server.shutdown()

class DbHttpServer(TCPServerCommand):
    port = None
    class PortArg(NonEphemeralPortInvariant):
        _help = 'port to listen on [default: %default]'
        _default = 8080

    ip = None
    class IpArg(StringInvariant):
        _help = 'IP address to listen on [default: %default]'
        _default = IPADDR

    connection_string = None
    class ConnectionStringArg(StringInvariant):
        _help = 'connect string to use for database [default: %default]'
        _default = (
            'Driver={SQL Server};'
            'Server=cougar;'
            'Database=hello_world;'
            'Uid=benchmarkdbuser;'
            'Pwd=B3nchmarkDBPass;'
        )

    def run(self):
        ip = self.options.ip
        port = int(self.options.port)
        root = self.options.root or os.getcwd()

        self._out("Serving HTTP on %s port %d ..." % (ip, port))

        import async
        from . import BaseHttpServer
        BaseHttpServer.connection_string = self.options.connection_string
        import pypyodbc
        BaseHttpServer.odbc = pypyodbc

        con = pypyodbc.connect(self.options.connection_string)
        BaseHttpServer.connection = con
        cur = con.cursor()
        cur.execute(BaseHttpServer.db_sql, (1,))
        cur.fetchall()
        cur.close()
        #con.close()

        with chdir(root):
            server = async.server(ip, port)
            async.register(transport=server, protocol=BaseHttpServer)
            try:
                async.run()
            except KeyboardInterrupt:
                server.shutdown()

class DbOtherHttpServer(TCPServerCommand):
    port = None
    class PortArg(NonEphemeralPortInvariant):
        _help = 'port to listen on [default: %default]'
        _default = 8080

    ip = None
    class IpArg(StringInvariant):
        _help = 'IP address to listen on [default: %default]'
        _default = IPADDR

    connection_string = None
    class ConnectionStringArg(StringInvariant):
        _help = 'connect string to use for database [default: %default]'
        _default = (
            'Driver={SQL Server};'
            'Server=cougar;'
            'Database=hello_world;'
            'Uid=benchmarkdbuser;'
            'Pwd=B3nchmarkDBPass;'
        )

    def run(self):
        ip = self.options.ip
        port = int(self.options.port)
        root = self.options.root or os.getcwd()

        self._out("Serving HTTP on %s port %d ..." % (ip, port))

        import async
        from . import BaseHttpServer
        BaseHttpServer.connection_string = self.options.connection_string
        #import pypyodbc
        #BaseHttpServer.odbc = pypyodbc

        #con = pypyodbc.connect(self.options.connection_string)
        #BaseHttpServer.connection = con
        #cur = con.cursor()
        #cur.execute(BaseHttpServer.db_sql, (1,))
        #cur.fetchall()
        #cur.close()
        #con.close()

        with chdir(root):
            server = async.server(ip, port)
            async.register(transport=server, protocol=BaseHttpServer)
            try:
                async.run()
            except KeyboardInterrupt:
                server.shutdown()

class DbPyodbcHttpServer(TCPServerCommand):
    port = None
    class PortArg(NonEphemeralPortInvariant):
        _help = 'port to listen on [default: %default]'
        _default = 8080

    ip = None
    class IpArg(StringInvariant):
        _help = 'IP address to listen on [default: %default]'
        _default = IPADDR

    connection_string = None
    class ConnectionStringArg(StringInvariant):
        _help = 'connect string to use for database [default: %default]'
        _default = (
            'Driver={SQL Server};'
            'Server=%s;'
            'Database=hello_world;'
            'Uid=benchmarkdbuser;'
            'Pwd=B3nchmarkDBPass;'
        )

    server = None
    class ServerArg(StringInvariant):
        _help = 'address of SQL Server instance'
        _default = 'localhost'

    def run(self):
        ip = self.options.ip
        port = int(self.options.port)
        root = self.options.root or os.getcwd()

        cs = self.options.connection_string % self.options.server
        import async
        from . import BaseHttpServer
        BaseHttpServer.connection_string = cs
        import pyodbc
        async.register_dealloc(pyodbc.Connection)
        async.register_dealloc(pyodbc.Cursor)
        async.register_dealloc(pyodbc.Row)
        #async.register_dealloc(pyodbc.CnxnInfo)

        #BaseHttpServer.odbc = pyodbc
        # Force a hash.
        dummy = hash(BaseHttpServer.connection_string)

        self._out("Attempting to connect to %s..." % cs)

        con = pyodbc.connect(cs)
        #BaseHttpServer.connection = con
        cur = con.cursor()
        cur.execute(BaseHttpServer.db_sql, (1,))
        cur.fetchall()
        cur.close()
        con.close()

        self._out("Serving HTTP on %s port %d ..." % (ip, port))

        with chdir(root):
            server = async.server(ip, port)
            async.register(transport=server, protocol=BaseHttpServer)
            try:
                async.run()
            except KeyboardInterrupt:
                server.shutdown()

class PyodbcMemLeakHelp(TCPServerCommand):
    count = None
    _count = None
    class CountArg(PositiveIntegerInvariant):
        _help = 'number of loops to perform'
        _default = 50000

    connection_string = None
    class ConnectionStringArg(StringInvariant):
        _help = 'connect string to use for database [default: %default]'
        _default = (
            'Driver={SQL Server};'
            'Server=%s;'
            'Database=hello_world;'
            'Uid=benchmarkdbuser;'
            'Pwd=B3nchmarkDBPass;'
        )

    server = None
    class ServerArg(StringInvariant):
        _help = 'address of SQL Server instance'
        _default = 'localhost'

    def run(self):
        count = self._count
        out = self._out
        cs = self.options.connection_string % self.options.server
        import os
        import sys
        import pyodbc
        from ctk.util import progressbar
        from . import BaseHttpServer
        sql = BaseHttpServer.db_sql2
        def wait_for_enter():
            sys.stdin.read(1)
        out("Press Enter when ready...")
        wait_for_enter()
        for i in progressbar(range(count), total=count, leave=True):
            con = pyodbc.connect(cs)
            cur = con.cursor()
            cur.execute(sql)
            cur.close()
            con.close()

        out("About to call gc.collect(), press Enter to continue.")
        wait_for_enter()
        import gc
        gc.collect()
        out("gc.collect() called, press Enter to exit.")
        wait_for_enter()


class LowLatencyHttpServer(TCPServerCommand):
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

        self._out("Low Latency: Serving HTTP on %s port %d ..." % (ip, port))

        import async
        from . import HttpServerLowLatency

        with chdir(root):
            server = async.server(ip, port)
            async.register(transport=server, protocol=HttpServerLowLatency)
            try:
                async.run()
            except KeyboardInterrupt:
                server.shutdown()

class ConcurrencyHttpServer(TCPServerCommand):
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

        self._out("Concurrency: Serving HTTP on %s port %d ..." % (ip, port))

        import async
        from . import HttpServerConcurrency

        with chdir(root):
            server = async.server(ip, port)
            async.register(transport=server, protocol=HttpServerConcurrency)
            try:
                async.run()
            except KeyboardInterrupt:
                server.shutdown()

class ThroughputHttpServer(TCPServerCommand):
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

        self._out("Throughput: Serving HTTP on %s port %d ..." % (ip, port))

        import async
        from . import HttpServerThroughput

        with chdir(root):
            server = async.server(ip, port)
            async.register(transport=server, protocol=HttpServerThroughput)
            try:
                async.run()
            except KeyboardInterrupt:
                server.shutdown()

class CheatingPlaintextHttpServer(TCPServerCommand):
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

        self._out("Cheating: Serving HTTP on %s port %d ..." % (ip, port))

        import async
        from . import BaseCheatingPlaintextHttpServer
        protocol = BaseCheatingPlaintextHttpServer

        with chdir(root):
            server = async.server(ip, port)
            async.register(transport=server, protocol=protocol)
            try:
                async.run()
            except KeyboardInterrupt:
                server.shutdown()

class FastHttpServer(TCPServerCommand):
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

        self._out("Serving fast HTTP on %s port %d ..." % (ip, port))

        import async
        class HttpServer:
            http11 = True
            def json(self, transport, data):
                return { 'message': 'Hello, World!' }

            def plaintext(self, transport, data):
                return b'Hello, World!'

        with chdir(root):
            server = async.server(ip, port)
            async.register(transport=server, protocol=HttpServer)
            try:
                async.run()
            except KeyboardInterrupt:
                server.shutdown()


class MultipleHttpServers(TCPServerCommand):
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

        self._out("Base:            Serving HTTP on %s port %d ..." % (ip, port))
        self._out("Concurrency:     Serving HTTP on %s port %d ..." % (ip, port+1))
        self._out("Low Latency:     Serving HTTP on %s port %d ..." % (ip, port+2))
        self._out("Throughput:      Serving HTTP on %s port %d ..." % (ip, port+3))
        self._out("Base Cheating:   Serving HTTP on %s port %d ..." % (ip, port+4))

        import async
        from . import (
            BaseHttpServer,
            HttpServerConcurrency,
            HttpServerLowLatency,
            HttpServerThroughput,
            BaseCheatingPlaintextHttpServer,
        )

        protocols = (
            BaseHttpServer,
            HttpServerConcurrency,
            HttpServerLowLatency,
            HttpServerThroughput,
            BaseCheatingPlaintextHttpServer,
        )

        ports = (port, port+1, port+2)
        with chdir(root):
            servers = []
            for (port, protocol) in zip(ports, protocols):
                server = async.server(ip, port)
                async.register(transport=server, protocol=protocol)
                servers.append(server)
            try:
                async.run()
            except KeyboardInterrupt:
                for server in servers:
                    server.shutdown()
                raise

class MutipleCheatingHttpServers(TCPServerCommand):
    port = None
    class PortArg(NonEphemeralPortInvariant):
        _help = 'port to listen on [default: %default]'
        _default = 7080

    ip = None
    class IpArg(StringInvariant):
        _help = 'IP address to listen on [default: %default]'
        _default = IPADDR

    def run(self):
        ip = self.options.ip
        port = int(self.options.port)
        root = self.options.root or os.getcwd()

        o = self._out
        o("Base Cheating:        Serving HTTP on %s port %d ..." % (ip, port))
        o("Low Latency Cheating: Serving HTTP on %s port %d ..." % (ip, port+1))
        o("Throughput Cheating:  Serving HTTP on %s port %d ..." % (ip, port+2))
        o("Concurrency Cheating: Serving HTTP on %s port %d ..." % (ip, port+3))

        import async
        from . import (
            BaseCheatingPlaintextHttpServer,
            LowLatencyCheatingHttpServer,
            ThroughputCheatingHttpServer,
            ConcurrencyCheatingHttpServer,
        )

        protocols = (
            BaseCheatingPlaintextHttpServer,
            LowLatencyCheatingHttpServer,
            ThroughputCheatingHttpServer,
            ConcurrencyCheatingHttpServer,
        )

        ports = (port, port+1, port+2, port+3)
        with chdir(root):
            servers = []
            for (port, protocol) in zip(ports, protocols):
                server = async.server(ip, port)
                async.register(transport=server, protocol=protocol)
                servers.append(server)
            try:
                async.run()
            except KeyboardInterrupt:
                for server in servers:
                    server.shutdown()

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
