#===============================================================================
# Imports
#===============================================================================
from __future__ import print_function

import os
import re
import sys
import optparse
import textwrap
import importlib

from collections import (
    OrderedDict,
)

from textwrap import (
    dedent,
)

import ctk

from ctk.config import (
    Config,
    ConfigObjectAlreadyCreated,
    get_config,
    _clear_config_if_already_created,
)

from ctk.command import (
    Command,
    CommandError,
    ClashingCommandNames,
)

from ctk.util import (
    iterable,
    ensure_unique,
    add_linesep_if_missing,
    prepend_error_if_missing,
    Dict,
    Options,
    Constant,
    DecayDict,
)

from ctk.invariant import (
    Invariant,
)

#===============================================================================
# Constants
#===============================================================================
class _ArgumentType(Constant):
    Optional  = 1
    Mandatory = 2
ArgumentType = _ArgumentType()

#===============================================================================
# CommandLine Class
#===============================================================================
class CommandLine:
    _conf_ = True
    _argc_ = 0
    _optc_ = 0
    _vargc_ = None
    _usage_ = None
    _quiet_ = None
    _verbose_ = None
    _command_ = None
    _shortname_ = None
    _description_ = None

    def __init__(self, program_name, command_class, config_class):
        self.mandatory_opts = OrderedDict()
        self.config_class = config_class
        self.program_name = program_name
        self.command_class = command_class
        self.command_classname = command_class.__name__

        self.command = self.command_class(sys.stdin, sys.stdout, sys.stderr)
        self.name = self.command.name
        self.shortname = self.command.shortname

        self.prog = '%s %s' % (self.program_name, self.name)

        self.parser = None
        self.conf = None

    def add_option(self, *args, **kwds):
        if kwds.get('mandatory'):
            self.mandatory_opts[args] = kwds['dest']
            del kwds['mandatory']
        self.parser.add_option(*args, **kwds)

    def remove_option(self, *args):
        self.parser.remove_option(args[0])
        if args in self._mandatory_opts:
            del self._mandatory_opts[args]

    def usage_error(self, msg):
        self.parser.print_help()
        sys.stderr.write("\nerror: %s\n" % msg)
        self.parser.exit(status=1)

    def _add_parser_options(self):
        cmd = self.command
        if not hasattr(cmd, '_invariants'):
            return

        invariants = cmd._invariants
        for (_, name) in cmd._invariant_order:
            i = invariants[name]
            args = []
            if i._opt_short:
                args.append('-' + i._opt_short)
            if i._opt_long:
                args.append('--' + i._opt_long)


            fields = (
                'help',
                'action',
                'default',
                'metavar',
                'mandatory',
            )
            k = Dict()
            k.dest = name
            for f in fields:
                v = getattr(i, '_' + f)
                if v:
                    k[f] = v if not callable(v) else v()

            self.add_option(*args, **k)

    def run(self, args):
        k = Dict()
        k.prog = self.prog
        if self._usage_:
            k.usage = self._usage_
        if self._description_:
            k.description = self._description_
        else:
            docstring = self.command.__doc__
            if docstring:
                k.description = textwrap.dedent(docstring)

        self.parser = optparse.OptionParser(**k)

        if self.command._verbose_:
            assert self.command._quiet_ is None
            self.parser.add_option(
                '-v', '--verbose',
                dest='verbose',
                action='store_true',
                default=False,
                help="run in verbose mode [default: %default]"
            )

        if self.command._quiet_:
            assert self.command._verbose_ is None
            self.parser.add_option(
                '-q', '--quiet',
                dest='quiet',
                action='store_true',
                default=False,
                help="run in quiet mode [default: %default]"
            )


        if self.command._conf_:
            self.parser.add_option(
                '-c', '--conf',
                metavar='FILE',
                help="use alternate configuration file FILE"
            )

        self._add_parser_options()
        (opts, self.args) = self.parser.parse_args(args)

        # Ignore variable argument commands altogether.
        # xxx: todo
        if 0 and self._vargc_ is not True:
            arglen = len(self.args)
            if arglen == 0 and self._argc_ != 0:
                self.parser.print_help()
                self.parser.exit(status=1)
            if len(self.args) != self._argc_ and self._argc_ != 0:
                self.usage_error("invalid number of arguments")

        self.options = Options(opts.__dict__)

        if self.mandatory_opts:
            d = opts.__dict__
            for (opt, name) in self.mandatory_opts.items():
                if d.get(name) is None:
                    self.usage_error("%s is mandatory" % '/'.join(opt))

        #self._pre_process_parser_results()

        f = None
        if self._conf_:
            f = self.options.conf
            if f and not os.path.exists(f):
                self.usage_error("configuration file '%s' does not exist" % f)

        try:
            self.conf = self.config_class(options=self.options)
            self.conf.load(filename=f)
        except ConfigObjectAlreadyCreated:
            self.conf = get_config()

        self.command.conf = self.conf
        self.command.args = self.args
        self.command.options = self.options
        self.command.start()

#===============================================================================
# CLI Class
#===============================================================================

class CLI(object):
    """
    The CLI class glues together Command and CommandLine instances.
    """

    __unknown_subcommand__ = "Unknown subcommand '%s'"
    __usage__ = "Type '%prog help' for usage."
    __help__ = """\
        Type '%prog <subcommand> help' for help on a specific subcommand.

        Available subcommands:"""

    def __init__(self, *args, **kwds):
        k = DecayDict(**kwds)
        self.args = list(args) if args else []
        self.program_name = k.program_name
        self.module_names = k.module_names or []
        self.args_queue = k.get('args_queue', None)
        self.feedback_queue = k.get('feedback_queue', None)
        k.assert_empty(self)
        self.returncode = 0
        self.commandline = None

        include_ctk = True
        for name in self.module_names:
            if name == '-ctk':
                include_ctk = False
                self.module_names.remove('-ctk')
            elif name == 'ctk':
                include_ctk = False

        if include_ctk:
            self.module_names.insert(0, 'ctk')

        ensure_unique(self.module_names)

        self.modules = Dict()
        self.modules.config = OrderedDict()
        self.modules.commands = OrderedDict()

        self._help = self.__help__
        self._commands_by_name = dict()
        self._commands_by_shortname = dict()

        self._import_command_and_config_modules()
        self._load_commands()

        if not self.args_queue:
            if self.args:
                self.run()
            else:
                self.help()

    def run(self):
        if not self.args_queue:
            self._process_commandline()
            return

        from Queue import Empty
        cmdlines = {}
        while True:
            try:
                args = self.args_queue.get_nowait()
            except Empty:
                break

            cmdline = args.pop(0).lower()
            if cmdline not in cmdlines:
                cmdlines[cmdline] = self._find_commandline(cmdline)
            cl = cmdlines[cmdline]
            cl.run(args)
            self.args_queue.task_done()

    def _import_command_and_config_modules(self):
        for namespace in self.module_names:
            for suffix in ('commands', 'config'):
                name = '.'.join((namespace, suffix))
                store = getattr(self.modules, suffix)
                store[namespace] = importlib.import_module(name)

    def _find_command_subclasses(self):
        seen = dict()
        pattern = re.compile('^class ([^\s]+)\(.*', re.M)
        subclasses = list()
        for (namespace, module) in self.modules.commands.items():
            path = module.__file__
            if path[-1] == 'c':
                path = path[:-1]

            with open(path, 'r') as f:
                matches = pattern.findall(f.read())

            for name in [ n for n in matches if n[0] != '_' ]:
                attr = getattr(module, name)
                if attr == Command or not issubclass(attr, Command):
                    continue

                if name in seen:
                    args = (name, seen[name], namespace)
                    raise ClashingCommandNames(*args)

                seen[name] = namespace
                subclasses.append((namespace, name, attr))

        return subclasses

    def _load_commands(self):
        subclasses = [
            sc for sc in sorted(self._find_command_subclasses())
        ]

        for (namespace, command_name, command_class) in subclasses:
            if command_name in self._commands_by_name:
                continue

            config_module = self.modules.config[namespace]
            config_class = getattr(config_module, 'Config')
            cl = CommandLine(self.program_name, command_class, config_class)

            helpstr = self._helpstr(cl.name)

            if cl.shortname:
                if cl.shortname in self._commands_by_shortname:
                    continue
                self._commands_by_shortname[cl.shortname] = cl
                if '[n]@' in helpstr:
                    prefix = '[n]@'
                else:
                    prefix = ''
                helpstr += ' (%s%s)' % (prefix, cl.shortname)

            self._help += helpstr
            self._commands_by_name[cl.name] = cl

        # Add a fake version command so that it'll appear in the list of
        # available commands.  (We intercept version requests during
        # _process_command(); there's no actual command for it.)
        self._help += self._helpstr('version')
        self._commands_by_name['version'] = None

    def _helpstr(self, name):
        i = 12
        if name.startswith('multiprocess'):
            prefix = '[n]@'
            name = prefix + name
            i -= len(prefix)
        return os.linesep + (' ' * i) + name

    def _load_commandlines(self):
        subclasses = [
            sc for sc in sorted(self._find_commandline_subclasses())
        ]

        for (scname, subclass) in subclasses:
            if scname in self._commands_by_name:
                continue
            try:
                cl = subclass(self.program_name)
            except TypeError as e:
                # Skip abstract base classes (e.g. 'AdminCommandLine').
                if e.args[0].startswith("Can't instantiate abstract class"):
                    continue
                raise

            helpstr = self._helpstr(cl.name)

            if cl.shortname:
                if cl.shortname in self._commands_by_shortname:
                    continue
                self._commands_by_shortname[cl.shortname] = cl
                helpstr += ' (%s)' % cl.shortname

            self._help += helpstr
            self._commands_by_name[cl.name] = cl

        # Add a fake version command so that it'll appear in the list of
        # available subcommands.  It doesn't matter if it's None as we
        # intercept 'version', '-v' and '--version' in the
        # _process_commandline method before doing the normal command
        # lookup.
        self._help += self._helpstr('version')
        self._commands_by_name['version'] = None

    def _find_commandline(self, cmdline):
        return self._commands_by_name.get(cmdline,
               self._commands_by_shortname.get(cmdline))

    def _process_commandline(self):
        args = self.args
        cmdline = args.pop(0).lower()

        if cmdline and cmdline[0] != '_':
            if '-' not in cmdline and hasattr(self, cmdline):
                getattr(self, cmdline)(args)
                return self._exit(0)
            elif cmdline in ('-v', '-V', '--version'):
                self.version()
            else:
                cl = self.commandline = self._find_commandline(cmdline)
                if cl:
                    try:
                        cl.run(args)
                        return self._exit(0)
                    except (CommandError, Invariant) as err:
                        self._commandline_error(cl, str(err))

        if not self.returncode:
            self._error(
                os.linesep.join((
                    self.__unknown_subcommand__ % cmdline,
                    self.__usage__,
                ))
            )

    def _exit(self, code):
        self.returncode = code

    def _commandline_error(self, cl, msg):
        args = (self.program_name, cl.name, msg)
        msg = '%s %s failed: %s' % args
        sys.stderr.write(prepend_error_if_missing(msg))
        return self._exit(1)

    def _error(self, msg):
        sys.stderr.write(
            add_linesep_if_missing(
                dedent(msg).replace(
                    '%prog', self.program_name
                )
            )
        )
        return self._exit(1)

    def usage(self, args=None):
        self._error(self.__usage__)

    def version(self, args=None):
        sys.stdout.write(add_linesep_if_missing(ctk.__version__))
        return self._exit(0)

    def help(self, args=None):
        if args:
            l = [ args.pop(0), '-h' ]
            if args:
                l += args
            self._process_commandline(l)
        else:
            self._error(self._help + os.linesep)



#===============================================================================
# Main
#===============================================================================
def extract_command_args_and_kwds(*args_):
    args = [ a for a in args_ ]
    kwds = {
        'program_name': args.pop(0),
        'module_names': [ m for m in args.pop(0).split(',') ] if args else None
    }
    return (args, kwds)

def run(*args_):
    (args, kwds) = extract_command_args_and_kwds(*args_)
    ctk.config._clear_config_if_already_created()
    return CLI(*args, **kwds)

def run_mp(**kwds):
    cli = CLI(**kwds)
    cli.run()

if __name__ == '__main__':
    # Intended invocation:
    #   python -m ctk.cli <program_name> <library_name> \
    #                           <command_name> [arg1 arg2 argN]
    # i.e. python -m ctk.cli dstk dstoolkit gdcf -f ...

    # Multiprocessor support: prefix command_name with @.  The @ will be
    # removed, the command will be run, and then the command.result field
    # will be expected to be populated with a list of argument lists that
    # will be pushed onto a multiprocessing joinable queue.

    is_mp = False
    args = sys.argv[1:]
    if len(args) <= 2:
        cli = run(*args)
        sys.exit(cli.returncode)

    command = args[2]
    if '@' in command:
        is_mp = True
        ix = command.find('@')
        parallelism_hint = int(command[:ix] or 0)
        args[2] = command[ix+1:]

    cli = run(*args)
    if not is_mp or cli.returncode:
        sys.exit(cli.returncode)

    command = cli.commandline.command
    results = command.results
    if not results:
        err("parallel command did not produce any results\n")
        sys.exit(1)

    from multiprocessing import (
        cpu_count,
        Process,
        JoinableQueue,
    )

    args_queue = JoinableQueue(len(results))

    for args in results:
        args_queue.put(args[2:])

    # Grab the program_name and module_names from the first result args.
    (_, kwds) = extract_command_args_and_kwds(*results[0])
    kwds['args_queue'] = args_queue

    nprocs = cpu_count()
    if parallelism_hint:
        if parallelism_hint > nprocs:
            fmt = "warning: parallelism hint exceeds ncpus (%d vs %d)\n"
            msg = fmt % (parallelism_hint, nprocs)
            sys.stderr.write(msg)
        nprocs = parallelism_hint

    procs = []
    for i in range(0, nprocs):
        p = Process(target=run_mp, kwargs=kwds)
        procs.append(p)
        p.start()

    sys.stdout.write("started %d processes\n" % len(procs))

    args_queue.join()

# vim:set ts=8 sw=4 sts=4 tw=78 et:
