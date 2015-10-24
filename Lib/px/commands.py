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
        _default = '.'

    ip = None
    class IpArg(StringInvariant):
        _help = 'IP address to listen on [default: %default]'
        _default = IPADDR

    def run(self):
        ip = self.options.ip
        port = int(self.options.port)
        root = self.options.root or os.getcwd()

        self._out("Serving HTTP on %s port %d ..." % (ip, port))

        import parallel.http.server
        with chdir(root):
            server = parallel.server(ip, port)
            protocol = parallel.http.server.HttpServer
            parallel.register(transport=server, protocol=protocol)
            parallel.run()


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

        import parallel.services
        server = parallel.server(ip, port)
        protocol = parallel.services.Chargen
        parallel.register(transport=server, protocol=protocol)
        parallel.run()

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

        import parallel.services
        server = parallel.server(ip, port)
        protocol = parallel.services.Disconnect
        parallel.register(transport=server, protocol=protocol)
        parallel.run()

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

        import parallel.services
        server = parallel.server(ip, port)
        protocol = parallel.services.StaticQotd
        parallel.register(transport=server, protocol=protocol)
        parallel.run()

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

        import parallel.services
        server = parallel.server(ip, port)
        protocol = parallel.services.DynamicQotd
        parallel.register(transport=server, protocol=protocol)
        parallel.run()


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

        import parallel.services
        server = parallel.server(ip, port)
        protocol = parallel.services.Daytime
        parallel.register(transport=server, protocol=protocol)
        parallel.run()

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

        import parallel.services
        server = parallel.server(ip, port)
        protocol = parallel.services.Time
        parallel.register(transport=server, protocol=protocol)
        parallel.run()

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

        import parallel.services
        server = parallel.server(ip, port)
        protocol = parallel.services.Discard
        parallel.register(transport=server, protocol=protocol)
        parallel.run()


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

        import parallel.services
        server = parallel.server(ip, port)
        protocol = Disconnect
        parallel.register(transport=server, protocol=protocol)
        parallel.run()

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
                #parallel.print(data)
                parallel.debug(data)

        import parallel
        client = parallel.client(ip, port)
        protocol = HttpGet
        parallel.register(transport=client, protocol=protocol)
        parallel.run()

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
                #parallel.print(data)
                parallel.debug(data)

        import parallel
        client = parallel.client(hostname, port)
        protocol = HttpGet
        parallel.register(transport=client, protocol=protocol)
        parallel.run()

#===============================================================================
# Examples Server
#===============================================================================
class WikiServer(TCPServerCommand):
    _shortname_ = 'wiki'

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

        wikidir = join_path(dirname(__file__), '../../examples/wiki')
        with chdir(wikidir):
            import wiki
            self._out("Running wiki server on %s port %d ..." % (ip, port))
            import parallel
            server = parallel.server(ip, port)
            parallel.register(transport=server, protocol=wiki.WikiServer)
            parallel.run()

class TechempowerFrameworkBenchmarkServer(TCPServerCommand):
    _shortname_ = 'tefb'

    port = None
    _port = None
    class PortArg(NonEphemeralPortInvariant):
        _help = 'port to listen on [default: %default]'
        _default = 8080

    ip = None
    class IpArg(StringInvariant):
        _help = 'IP address to listen on [default: %default]'
        _default = IPADDR

    database_server = None
    class DatabaseServerArg(StringInvariant):
        _help = (
            'hostname or IP address of SQL Server instance to '
            'connect to [default: %default]'
        )
        _default = 'localhost'

    database_name = None
    class DatabaseNameArg(StringInvariant):
        _help = 'name of database [default: %default]'
        _default = 'hello_world'

    database_username = None
    class DatabaseUsernameArg(StringInvariant):
        _help = 'username of database user [default: %default]'
        _default = 'benchmarkdbuser'

    database_password = None
    class DatabasePasswordArg(StringInvariant):
        _help = 'password for database user [default: %default]'
        _default = 'B3nchmarkDBPass'

    database_driver = None
    class DatabaseDriverArg(StringInvariant):
        _help = 'driver name to use in connect string [default: %default]'
        _default = 'SQL Server'

    additional_connect_string_args = None
    class AdditionalConnectStringArgsArg(StringInvariant):
        _minlen = 0
        _help = 'any additional args to append to connect string'
        _default = ''

    database_connect_string = None
    class DatabaseConnectStringArg(StringInvariant):
        _help = 'connect string to use [default: %default]'
        _default = (
            'Driver={%(database_driver)s};'
            'Server=%(database_server)s;'
            'Database=%(database_name)s;'
            'Uid=%(database_username)s;'
            'Pwd=%(database_password)s;'
            '%(additional_connect_string_args)s'
        )

    test_only = None
    class TestOnlyArg(BoolInvariant):
        _help = 'verify the database can be connected to, then exit'
        _default = False

    wiki = None
    class WikiArg(BoolInvariant):
        _help = 'merges the wiki server into the http server protocol'
        _default = False

    def __getitem__(self, name):
        return getattr(self, name)

    def run(self):
        ip = self.options.ip
        port = self._port

        connect_string = self.database_connect_string % self
        self._out("Using connect string: %s" % connect_string)

        tefbdir = join_path(dirname(__file__), '../../examples/tefb')
        with chdir(tefbdir):
            import tefb
            import parallel
            import pyodbc

            parallel.register_dealloc(pyodbc.Connection)
            parallel.register_dealloc(pyodbc.Cursor)
            parallel.register_dealloc(pyodbc.Row)

            protocol = tefb.TefbHttpServer
            if self.options.wiki:
                wikidir = tefbdir.replace('tefb', 'wiki')
                with chdir(wikidir):
                    import wiki
                protocol.merge(wiki.WikiServer)
            protocol.connect_string = connect_string
            dummy = hash(connect_string)
            self._out("Testing database connectivity...")
            con = pyodbc.connect(connect_string)
            cur = con.cursor()
            cur.execute(protocol.db_sql, (1,))
            cur.fetchall()
            cur.close()
            con.close()
            self._out("Database works.")
            if self.options.test_only:
                return

            self._out("Running server on %s port %d ..." % (ip, port))
            server = parallel.server(ip, port)
            parallel.register(transport=server, protocol=protocol)
            try:
                parallel.run()
            except KeyboardInterrupt:
                server.shutdown()

class PandasServer(TCPServerCommand):
    _shortname_ = 'pd'

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

        pydatadir = join_path(dirname(__file__), '../../examples/pydata')
        with chdir(pydatadir):
            import pandas_test as pdt
            # Prime all our methods.
            pdt.df.to_html()
            pdt.df.to_csv()
            pdt.df.to_dict()
            self._out("Running pandas server on %s port %d ..." % (ip, port))
            import parallel
            server = parallel.server(ip, port)
            parallel.register(transport=server, protocol=pdt.PandasHttpServer)
            parallel.run()

class SqliteServer(TCPServerCommand):
    _shortname_ = 'sq3'

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

        pydatadir = join_path(dirname(__file__), '../../examples/pydata')
        with chdir(pydatadir):
            import parallel
            import sqlite_test as sqt
            import sqlite3

            server1 = parallel.server(ip, port)
            self._out("Running geo server on %s port %d ..." % (ip, port))
            protocol = sqt.SqliteGeoHttpServer
            parallel.register(transport=server1, protocol=protocol)

            server2 = parallel.server(ip, port+1)
            self._out("Running world server on %s port %d ..." % (ip, port+1))
            protocol = sqt.SqliteWorldHttpServer
            parallel.register(transport=server2, protocol=protocol)

            server3 = parallel.server(ip, port+2)
            self._out("Running fast server on %s port %d ..." % (ip, port+2))
            protocol = sqt.Foo
            parallel.register(transport=server3, protocol=protocol)

            server4 = parallel.server(ip, port+3)
            self._out("Running fast server2 on %s port %d ..." % (ip, port+3))
            protocol = sqt.Bar
            parallel.register(transport=server4, protocol=protocol)

            try:
                parallel.run()
            except KeyboardInterrupt:
                server1.shutdown()
                server2.shutdown()
                server3.shutdown()
                server4.shutdown()

class SqliteServerTest(TCPServerCommand):
    _shortname_ = 'sqt'

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

        pydatadir = join_path(dirname(__file__), '../../examples/pydata')
        with chdir(pydatadir):
            import parallel
            import sqlite_test as sqt
            import sqlite3

            server1 = parallel.server(ip, port)
            self._out("Running geo server on %s port %d ..." % (ip, port))
            protocol = sqt.Bar
            parallel.register(transport=server1, protocol=protocol)

            server2 = parallel.server(ip, port+1)
            self._out("Running geo server on %s port %d ..." % (ip, port+1))
            protocol = sqt.Bar2
            parallel.register(transport=server2, protocol=protocol)

            try:
                parallel.run()
            except KeyboardInterrupt:
                server1.shutdown()
                server2.shutdown()

class DbServerTest(TCPServerCommand):
    _shortname_ = 'dbt'

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

        pydatadir = join_path(dirname(__file__), '../../examples/pydata')
        with chdir(pydatadir):
            import parallel
            import db_test as dbt

            server1 = parallel.server(ip, port)
            self._out("Running server on %s port %d ..." % (ip, port))
            protocol = dbt.Foo
            parallel.register(transport=server1, protocol=protocol)

            try:
                parallel.run()
            except KeyboardInterrupt:
                server1.shutdown()

class RateLimitServerTest(TCPServerCommand):
    _shortname_ = 'rl'

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

        miscdir = join_path(dirname(__file__), '../../examples/misc')
        with chdir(miscdir):
            import parallel
            from parallel.test import ratelimit_test as rlt

            server1 = parallel.server(ip, port)
            self._out("Running server on %s port %d ..." % (ip, port))
            protocol = rlt.RateLimitedServer
            parallel.register(transport=server1, protocol=protocol)

            try:
                parallel.run()
            except KeyboardInterrupt:
                server1.shutdown()


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

        import parallel
        class GeneratorTest3:
            def yield_c(self):
                for c in ('A', 'B', 'C'):
                    parallel.debug(c)
                    yield c

            def data_received(self, transport, data):
                return (', '.join(c for c in self.yield_c())).encode('utf-8')

        server = parallel.server(ip, port)
        protocol = GeneratorTest3
        parallel.register(transport=server, protocol=protocol)
        parallel.run()

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

        import parallel.http.server
        with chdir(root):
            server = parallel.server(ip, port)
            protocol = parallel.http.server.HttpServer
            parallel.register(transport=server, protocol=protocol)
            parallel.run()

        class Protocol(parallel.http.server.HttpServer):
            @parallel.call_from_main_thread_and_wait
            def _timestamp(self):
                return parallel.rdtsc()

            def data_received(self, transport, data):
                return b', '.join([chr(i) for i in (1, 2, 3)])

        import parallel
        server = parallel.server(ip, port)
        protocol = GeneratorTest
        parallel.register(transport=server, protocol=protocol)
        parallel.run()

#===============================================================================
# Dev Helpers
#===============================================================================
class UpdateDiffs(PxCommand):
    def run(self):
        raise CommandError(
            'deprecated: use update-hg-diffs or update-git-diffs instead'
        )

class UpdateHgDiffs(PxCommand):
    """
    Diffs PyParallel against original v3.3.5 tag it was based upon and,
    for all modified files (i.e. we exclude new files), create a diff
    and store it in diffs/<dirname>/<filename>.diff.
    """
    base_rev = None
    class BaseRevArg(StringInvariant):
        _help = 'base rev/tag to diff against [default: %default]'
        _default = 'v3.3.5'

    target_rev = None
    class TargetRevArg(StringInvariant):
        _help = 'target rev/tag to diff against [default: %default]'
        _default = '3.3-px'

    root = None
    class RootArg(DirectoryInvariant):
        _help = 'hg repository root'

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

class UpdateGitDiffs(PxCommand):
    """
    Diffs PyParallel against original v3.3.5 tag it was based upon and,
    for all modified files (i.e. we exclude new files), create a diff
    and store it in diffs/<dirname>/<filename>.diff.
    """
    base_rev = None
    class BaseRevArg(StringInvariant):
        _help = 'base rev/tag to diff against [default: %default]'
        _default = 'v3.3.5'

    target_rev = None
    class TargetRevArg(StringInvariant):
        _help = 'target rev/tag to diff against [default: %default]'
        _default = 'branches/3.3-px'

    root = None
    class RootArg(DirectoryInvariant):
        _help = 'git repository root'

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

        # This is super hacky.  Mimic c:\msysgit\git-cmd.bat.
        git_root = 'c:\\msysgit'
        git_bin = join_path(git_root, 'bin')
        mingw_bin = join_path(git_root, 'mingw/bin')
        git_cmd = join_path(git_root, 'cmd')

        prepend_path = ';'.join((
            git_bin,
            mingw_bin,
            git_cmd,
        ))

        existing_path = os.environ['PATH']
        new_path = ';'.join((prepend_path, existing_path))
        os.environ['PATH'] = new_path
        os.environ['PLINK_PROTOCOL'] = 'ssh'
        os.environ['TERM'] = 'msys'

        cmd = (
            'git diff --name-status %s %s > git-st.txt' % (
                base_rev,
                target_rev,
            )
        )
        os.system(cmd)

        with open('git-st.txt', 'r') as f:
            data = f.read()

        lines = data.splitlines()
        d = defaultdict(list)
        for line in lines:
            (action, path) = line.split('\t', 1)
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
                'git diff '
                #' --ignore-all-space '
                ' --ignore-blank-lines '
                ' --ignore-space-change '
                ' %s "%s" > "%s"' % (
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

        os.unlink('git-st.txt')

class PatchCythonGeneratedFile(PxCommand):
    """
    Cython caches tracebacks.  Don't do this for PyParallel.
    """

    src = b"""\
static void __Pyx_AddTraceback(const char *funcname, int c_line,
                               int py_line, const char *filename) {
    PyCodeObject *py_code = 0;
    PyFrameObject *py_frame = 0;
    py_code = __pyx_find_code_object(c_line ? c_line : py_line);
    if (!py_code) {
        py_code = __Pyx_CreateCodeObjectForTraceback(
            funcname, c_line, py_line, filename);
        if (!py_code) goto bad;
        __pyx_insert_code_object(c_line ? c_line : py_line, py_code);
    }"""

    dst = b"""\
static void __Pyx_AddTraceback(const char *funcname, int c_line,
                               int py_line, const char *filename) {
    PyCodeObject *py_code = 0;
    PyFrameObject *py_frame = 0;
#ifdef WITH_PARALLEL
    if (!Py_PXCTX())
        py_code = __pyx_find_code_object(c_line ? c_line : py_line);
#else
    py_code = __pyx_find_code_object(c_line ? c_line : py_line);
#endif
    if (!py_code) {
        py_code = __Pyx_CreateCodeObjectForTraceback(
            funcname, c_line, py_line, filename);
        if (!py_code) goto bad;
#ifdef WITH_PARALLEL
        if (!Py_PXCTX())
            __pyx_insert_code_object(c_line ? c_line : py_line, py_code);
#else
        __pyx_insert_code_object(c_line ? c_line : py_line, py_code);
#endif
    }"""

    path = None
    class PathArg(PathInvariant):
        _help = 'path of the generated Cython file (e.g. datrie.c)'

    def run(self):
        import pdb
        dbg = pdb.Pdb()

        path = self.path
        assert os.path.exists(path), path

        backup = path + '.orig'
        with open(path, 'rb') as f:
            data = f.read()

        #dbg.set_trace()
        data = data.replace(b'++Py_REFCNT(', b'Py_INCREF(') \
                   .replace(b'--Py_REFCNT(', b'Py_DECREF(')

        #data = data.replace(b' Py_INCREF(', b' Py_IncRef(') \
        #           .replace(b' Py_DECREF(', b' Py_DecRef(')

        #dbg.set_trace()
        ix = data.find(self.src)
        if ix == -1:
            ix = data.find(self.dst)
            if ix == -1:
                msg = (
                    "Failed to find target string in path %s "
                    "(try copying the backup %s back to %s)" % (
                        path,
                        backup,
                        path,
                    )
                )
                raise CommandError(msg)

        with open(backup, 'wb') as f:
            f.write(data)

        #dbg.set_trace()
        new_data = data.replace(self.src, self.dst)

        with open(path, 'wb') as f:
            f.write(new_data)

        self._out("Converted %s" % path)

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
