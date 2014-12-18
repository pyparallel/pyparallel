#===============================================================================
# Imports
#===============================================================================
import re
import sys

from abc import (
    ABCMeta,
    abstractmethod,
)

import ctk.util

from ctk.util import (
    iterable,
    add_linesep_if_missing,
    prepend_error_if_missing,
    prepend_warning_if_missing,
    requires_context,
    implicit_context,
    Dict,
    ImplicitContextSensitiveObject,
)

#===============================================================================
# Globals
#===============================================================================
COMMAND_CLASS_REGEX = re.compile('[A-Z][^A-Z]*')

#===============================================================================
# Helpers
#===============================================================================
def try_close_file(f):
    try:
        f.close()
    except IOError:
        pass

def get_io_streams(add_linesep_if_missing=True):
    if add_linesep_if_missing:
        add = ctk.util.add_linesep_if_missing
    else:
        add = lambda s: s

    cmd = Command.get_active_command()
    if not cmd:
        (o, e, i) = (sys.stdout, sys.stderr, sys.stdin)
        return (
            lambda s: o.write(add(s)),
            lambda s: e.write(add(s)),
            lambda: i.read(),
        )
    else:
        return (
            cmd._out,
            cmd._err,
            lambda: cmd.istream.read(),
        )

#===============================================================================
# Commands
#===============================================================================
class CommandError(BaseException):
    pass

class ClashingCommandNames(CommandError):
    def __init__(self, name, previous, this):
        msg = (
            "clashing command name: module '%s' defines a Command "
            "named '%s', but this was already defined in module '%s'"
        ) % (this, name, previous)
        BaseException.__init__(self, msg)


class _DummyStream(object):
    def write(self, msg):
        pass

    def read(self):
        return None

    def flush(self):
        pass

    def close(self):
        pass

DummyStream = _DummyStream()

class CommandFormatter:
    """
    Helper class used to format output from ``Command`` objects.

    Command subclasses can specify a custom instance of this class to alter
    the way messages are printed (for example, surrounding messages in html
    tags).

    Each method corresponds to the ``Command._<name>()`` method of the
    ``Command`` class (i.e. ``Command._out()`` calls the ``out(msg)``
    method of this class.

    The formatter is set via the ``Command.formatter`` attribute.

    This class is the default formatter.
    """

    def verbose(self, msg):
        """
        Called by ``Command._verbose()``.

        Does not prepend anything to `msg`.
        Adds trailing linesep to `msg` if there's not one already.
        """
        return add_linesep_if_missing(msg)

    def out(self, msg):
        """
        Called by ``Command._out()``.

        Does not prepend anything to `msg`.
        Adds trailing linesep to `msg` if there's not one already.
        """
        return add_linesep_if_missing(msg)

    def warn(self, msg):
        """
        Called by ``Command._warn()``.

        Prepends 'warning: ' to `msg` if it's not already present.
        Adds trailing linesep to `msg` if there's not one already.
        """
        return prepend_warning_if_missing(msg)

    def err(self, msg):
        """
        Called by ``Command._err()``.

        Prepends 'error: ' to `msg` if it's not already present.
        Adds trailing linesep to `msg` if there's not one already.
        """
        return prepend_error_if_missing(msg)

    def start(self, command):
        """
        Called by ``Command.__enter__``, after ``Command._allocate()``.

        ``command`` will be the parent ``Command`` instance this formatter
        instance has been attached to (via ``Command.formatter``).
        """
        pass

    def end(self, suppress, *exc_info):
        """
        Called by ``Command.__exit__``, after ``Command._deallocate()``.

        ``suppress`` is the value returned by ``Command._deallocate()``.
        It will be a boolean True/False.  A True value indicates that the
        command will be suppressing the exception and continuing with
        processing; False indicates the opposite.

        ``*exc_info`` will hold the value of whatever was picked up by
        ``Command.__exit__`` (either a 3-element tuple of Nones, or
        exception info).
        """
        pass


class Command(ImplicitContextSensitiveObject):
    __metaclass__ = ABCMeta

    __first_command__ = None
    __active_command__ = None

    # Protected members that a subclass can override.
    _conf_ = True
    _usage_ = None
    _quiet_ = None
    _verbose_ = None
    _shortname_ = None
    _description_ = None

    def __init__(self, istream=None, ostream=None, estream=None):

        self.istream = istream or DummyStream
        self.ostream = ostream or DummyStream
        self.estream = estream or DummyStream

        self.conf = None
        self.args = None
        self.result = None
        self.options = None

        self.load_order = None

        self.cprofile = False

        self.entered = False
        self.exited = False

        self.formatter = CommandFormatter()

        self.is_first = False
        self.first = None
        self.prev = None
        self.next = None

        self._next_commands = []

        self._exit_functions = []

        self._stash = None
        self._quiet = None

        regex = COMMAND_CLASS_REGEX
        tokens = [ t for t in regex.findall(self.__class__.__name__) ]
        if tokens[-1] == 'Command':
            tokens = tokens[:-1]

        self.name = '-'.join(t for t in tokens).lower()
        if self._shortname_ is not None:
            self.shortname = self._shortname_
        elif len(tokens) > 1:
            self.shortname = ''.join(t[0] for t in tokens).lower()
        else:
            self.shortname = self.name

    def __enter__(self):
        if not Command.__active_command__:
            Command.__first_command__ = self
            self.is_first = True
            self.first = self
            self._stash = Dict()
            self._first_enter()
        else:
            active = Command.__active_command__
            self.first = Command.__first_command__
            self.prev = active
            active.next = self

        Command.__active_command__ = self
        self.entered = True
        self._allocate()
        self.formatter.start(self)
        return self

    def __exit__(self, *exc_info):
        self.exited = True
        suppress = self._deallocate(*exc_info)
        self.formatter.end(suppress, *exc_info)
        Command.__active_command__ = self.prev
        if not self.prev:
            assert self.is_first
            self._first_exit()
            assert self.first == Command.get_first_command()
            Command.__first_command__ = None

        for (fn, args, kwds) in self._exit_functions:
            fn(*args, **kwds)

    @property
    def is_quiet(self):
        if self._quiet is not None:
            return self._quiet
        else:
            return self.options.quiet

    @property
    def stash(self):
        return (self._stash if self.is_first else self.first._stash)

    def _flush(self):
        self.ostream.flush()
        self.estream.flush()

    def _first_enter(self):
        pass

    def _first_exit(self):
        pass

    def _allocate(self):
        """
        Called by `__enter__`.  Subclasses should implement this method if
        they need to do any context-sensitive allocations.  (`_deallocate`
        should also be implemented if this method is implemented.)
        """
        pass

    def _deallocate(self, *exc_info):
        """
        Called by `__exit__`.  Subclasses should implement this method if
        they've implemented `_allocate` in order to clean up any resources
        they've allocated once the context has been left.
        """
        pass

    def _verbose(self, msg):
        """
        Writes `msg` to output stream if '--verbose'.
        """
        if self.options.verbose:
            self.ostream.write(self.formatter.verbose(msg))

    def _out(self, msg):
        """
        Write `msg` to output stream if not '--quiet'.
        """
        if not self.is_quiet:
            self.ostream.write(self.formatter.out(msg))

    def _warn(self, msg):
        """
        Write `msg` to output stream regardless of '--quiet'.
        """
        self.ostream.write(self.formatter.warn(msg))

    def _err(self, msg):
        """
        Write `msg` to error stream regardless of '--quiet'.
        """
        self.estream.write(self.formatter.err(msg))

    def _pre_load_options(self):
        """
        This is the first method called after entering the command's context,
        and it gives a subclass the opportunity to tweak/alter self.options
        before loading them (which triggers the invariant tests if applicable).
        """
        pass

    def _load_early_options(self):
        self._load_options(early=True)

    def _load_options(self, early=False):
        """
        Called within a command's active context after raw data values have
        been obtained from the calling environment (i.e. command line args
        being mapped to relevant command.<attr>s).  These will be accessible
        via self.options.

        For invariant-aware objects, the validation is kicked off by the
        setattr() call at the end.

        Special attention is paid to apply values in the same order as the
        class defines them in.  i.e.::

            class Foo(Command):
                id = None
                name = None
                parent = None

        The setattr() calls will be made in the same order: 'id', 'name' and
        'parent'.  This allows earlier invariants to affect the object state,
        which is then used by subsequent invariants.

        This is useful when dealing with anything that requires an identity
        (database record, for example); the id can be verified and the backing
        entity can be loaded before the subsequent invariants are tested,
        which means they can check the value provided against an existing
        database value, if need be.
        """
        cls = self.__class__
        opts = self.options
        keys = set(opts.keys())
        if hasattr(self, '_invariant_order'):
            attrs = [ a[1] for a in self._invariant_order if a[1] in keys ]
        else:
            attrs = [
                n for n in dir(cls) if n in keys and (
                    n[0] != '_' and
                    n[0].islower() and
                    getattr(cls, n)  is None and
                    getattr(self, n) is None
                )
            ]

        order = list()
        for attr in attrs:
            value = getattr(opts, attr)
            if value is not None:
                if early:
                    if hasattr(attr, '_early') and not attr._early:
                        continue
                setattr(self, attr, value)
                order.append(attr)

        self.load_order = order

    def _pre_enter(self):
        """
        Called before a command's context is entered.  You'd typically only
        need this if you wanted to run something before _first_enter() is
        called (like changing the process name before logging initializes).
        """
        pass

    def _pre_run(self):
        """
        Called within a command's active context, just prior to run().  If any
        last minute data/option/argument tweaking needs to be done, you should
        do it here.  The command object will have all its attributes primed
        with invariant-processed values by this stage.
        """
        pass

    @abstractmethod
    def run(self):
        """
        Called by start(), must be implemented by subclasses.
        """
        pass

    def _post_run(self):
        """
        Called with the command's context still active straight after a
        successful run() invocation.  If an error occurred during run(),
        this won't be called.  If you need to do some cleanup/handling
        regardless of whether or not run() completed successfully, put it
        in _deallocate(), which will always be called when the context is
        exited.
        """
        pass

    def _run_next(self):
        seen = set()
        for cls in iterable(self._next_commands):
            if cls in seen:
                continue
            command = self.prime(cls)
            command.start()
            seen.add(cls)

    def start(self):
        """
        Entry point for a command.  Responsible for setting up a context and
        calling the relevant pre/run/post methods in the right order.  Don't
        override unless you know what you're doing.
        """
        self._load_early_options()
        self._pre_enter()
        with self:
            self._pre_load_options()
            self._load_options()
            self._pre_run()
            if self.cprofile:
                try:
                    fn = self.cprofile_filename
                except AttributeError:
                    fn = None
                import cProfile
                cProfile.runctx('self.run()', globals(), locals(), filename=fn)
                if fn:
                    self._out("wrote profile stats to %s" % fn)
            else:
                self.run()
            self._post_run()
            self._run_next()
        self._end()

    def _end(self):
        pass

    def prime(self, cls):
        """
        Create a new instance of ``cls``, primed with the same values as this
        command instance.
        """
        c = cls(self.istream, self.ostream, self.estream)
        c.conf = self.conf
        c.options = self.options

        self_cls = self.__class__
        attrs = set([
            d for d in dir(cls) if (
                d[0] != '_' and
                d[0].islower() and (
                    hasattr(self, d) and
                    bool(getattr(self, d)) and
                    getattr(cls, d) is None
                )
            )
        ])

        # Mimic load order.
        for attr in [ a for a in self.load_order if a in attrs ]:
            setattr(c, attr, getattr(self, attr))

        return c

    @classmethod
    def get_active_command(cls):
        return Command.__active_command__

    @classmethod
    def get_first_command(cls):
        return Command.__first_command__

    def on_exit(self, fn, *args, **kwds):
        """
        Adds ``fn`` to a list of functions that are called during ``__exit``.
        """
        self._exit_functions.append((fn, args, kwds))

    @classmethod
    def close_file_on_exit(cls, f):
        command = cls.get_active_command()
        if not command:
            return
        command.on_exit(try_close_file, f)


# vim:set ts=8 sw=4 sts=4 tw=78 et:
