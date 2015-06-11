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

    connect_string = None
    class ConnectStringArg(StringInvariant):
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
        BaseHttpServer.connect_string = self.options.connect_string
        import pypyodbc
        BaseHttpServer.odbc = pypyodbc

        con = pypyodbc.connect(self.options.connect_string)
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

    connect_string = None
    class ConnectStringArg(StringInvariant):
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
        BaseHttpServer.connect_string = self.options.connect_string
        #import pypyodbc
        #BaseHttpServer.odbc = pypyodbc

        #con = pypyodbc.connect(self.options.connect_string)
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

    connect_string = None
    class ConnectStringArg(StringInvariant):
        _help = 'connect string to use for database [default: %default]'
        _default = (
            'Driver={ODBC Driver 11 for SQL Server};'
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

        cs = self.options.connect_string % self.options.server
        import async
        from . import BaseHttpServer
        BaseHttpServer.connect_string = cs
        import pyodbc
        async.register_dealloc(pyodbc.Connection)
        async.register_dealloc(pyodbc.Cursor)
        async.register_dealloc(pyodbc.Row)
        #async.register_dealloc(pyodbc.CnxnInfo)

        BaseHttpServer.odbc = pyodbc
        # Force a hash.
        dummy = id(BaseHttpServer.connect_string)

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

        self._out("Base Cheating:        Serving HTTP on %s port %d ..." % (ip, port))
        self._out("Low Latency Cheating: Serving HTTP on %s port %d ..." % (ip, port+1))
        self._out("Throughput Cheating:  Serving HTTP on %s port %d ..." % (ip, port+2))
        self._out("Concurrency Cheating: Serving HTTP on %s port %d ..." % (ip, port+3))

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
