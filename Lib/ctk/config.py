#===============================================================================
# Imports
#===============================================================================
import os
import re
import sys
import stat
import base64
import inspect
import subprocess

from abc import (
    abstractproperty,
)

from os.path import (
    isdir,
    isfile,
    abspath,
    dirname,
    basename,
    expanduser,
    expandvars,
)

from textwrap import dedent

try:
    from ConfigParser import (
        NoOptionError,
        NoSectionError,
        RawConfigParser,
    )
except ImportError:
    from configparser import (
        NoOptionError,
        NoSectionError,
        RawConfigParser,
    )

from ctk.path import (
    join_path,
)

from ctk.util import (
    memoize,
    iterable,
    classproperty,

    Dict,
    Options,
)

#===============================================================================
# Globals
#===============================================================================

CONFIG = None
CONFIG_CLASS = None

PROGRAM_NAME = None
COMMAND_NAME = None
COMMAND_MODULES = None

PATH = dirname(abspath(__file__))

NAMESPACE = basename(PATH)

LIB_DIR  = join_path(PATH, '..')
BIN_DIR  = join_path(LIB_DIR, '../bin')
CONF_DIR = join_path(LIB_DIR, '../conf')
LOGS_DIR = join_path(LIB_DIR, '../logs')
DATA_DIR = join_path(LIB_DIR, '../data')

#fixme: revisit these assertions
#assert LIB_DIR.endswith('lib'), LIB_DIR
#
#for d in (LIB_DIR, BIN_DIR, CONF_DIR, LOGS_DIR):
#    assert isdir(d), d

# HOSTFQDN may have the FQDN or it may not; HOSTNAME will always be the
# shortest representation of the hostname.
HOSTFQDN = subprocess.check_output('hostname')[:-len(os.linesep)].lower()
try:
    HOSTNAME = HOSTFQDN.split('.')[0]
except TypeError:
    HOSTFQDN = HOSTFQDN.decode('utf-8')
    HOSTNAME = HOSTFQDN.split('.')[0]

#===============================================================================
# Exceptions
#===============================================================================
class ConfigError(BaseException):
    pass

class NoConfigObjectCreated(BaseException):
    pass

class ConfigObjectAlreadyCreated(BaseException):
    pass

class ConfigClassAlreadySet(BaseException):
    pass

#===============================================================================
# Helpers
#===============================================================================
def get_config():
    global CONFIG
    if not CONFIG:
        raise NoConfigObjectCreated()
    return CONFIG

def _clear_config_if_already_created():
    global CONFIG
    if CONFIG:
        CONFIG = None

def set_config_class(cls):
    assert cls
    global CONFIG_CLASS
    if CONFIG_CLASS:
        raise ConfigClassAlreadySet()
    CONFIG_CLASS = cls

def _clear_config_class_if_already_set():
    global CONFIG_CLASS
    if CONFIG_CLASS:
        CONFIG_CLASS = None

#===============================================================================
# Classes
#===============================================================================
class Config(RawConfigParser):

    def __init__(self, options=None):
        RawConfigParser.__init__(self)

        self.optionxform = str

        self.options = options if options else Options()
        self.hostname = HOSTFQDN
        self.shortname = HOSTNAME

        self.files = None
        self.filename = None
        self._is_production = None
        self._multiline_pattern = re.compile(r'([^\s].*?)([\s]+\\)?')

        self.__gnuwin32_dir = None

        global CONFIG
        if CONFIG is not None:
            raise ConfigObjectAlreadyCreated()
        CONFIG = self

    @classproperty
    @classmethod
    def namespace(cls):
        return basename(dirname(inspect.getsourcefile(cls)))

    @classmethod
    def _resolve_dir(cls, name):
        path = inspect.getsourcefile(cls)
        base = dirname(join_path(path, '../..'))
        return join_path(base, name)

    @classproperty
    @classmethod
    def lib_dir(cls):
        return cls._resolve_dir('lib')

    @classproperty
    @classmethod
    def bin_dir(cls):
        return cls._resolve_dir('bin')

    @classproperty
    @classmethod
    def conf_dir(cls):
        return cls._resolve_dir('conf')

    @classproperty
    @classmethod
    def logs_dir(cls):
        return cls._resolve_dir('logs')

    @classproperty
    @classmethod
    def data_dir(cls):
        d = cls._resolve_dir('data')
        if not isdir(d):
            os.makedirs(d)
        return d

    @classproperty
    @classmethod
    def parent(cls):
        try:
            return cls.__base__
        except AttributeError:
            return None

    def _absdir(self, name, section='main'):
        count = 0
        total = 0
        max_total = 10
        p = self.get(section, name)
        while count != 2 and total < max_total:
            count = 0
            total += 1
            if '~' in p:
                p = expanduser(p)
                count -= 1
            else:
                count += 1

            if '%' in p or '$' in p:
                p = expandvars(p)
                count -= 1
            else:
                count += 1

        if total == max_total:
            args = (section, name)
            msg = "Exceeded user/var path recursion depth for %s.%s." % args
            raise RuntimeError(msg)

        return abspath(p)

    @property
    def gnuwin32_dir(self):
        if not self.__gnuwin32_dir:
            d = self._absdir('gnuwin32_dir')
            assert isdir(d), d
            self.__gnuwin32_dir = d
        return self.__gnuwin32_dir

    @property
    def gnuwin32_bin(self):
        return join_path(self.gnuwin32_dir, 'bin')

    def get_gnuwin32_exe(self, name):
        if not name.endswith('.exe'):
            name = name + '.exe'
        return join_path(self.gnuwin32_bin, name)

    def get_gnu_exe(self, name):
        if os.name != 'nt':
            return name
        else:
            return self.get_gnuwin32_exe(name)

    @classmethod
    def discover_config_files(cls, files=None):
        if files is None:
            assert cls.parent
            files = list()

        files.append((cls.conf_dir, cls.namespace))

        try:
            if cls.parent:
                cls.parent.discover_config_files(files)
        except AttributeError:
            pass

    @property
    @memoize
    def sqlalchemy_ideal_chunk_size(self):
        return self.getint('sqlalchemy', 'ideal_chunk_size')

    @property
    @memoize
    def sqlalchemy_min_chunk_size(self):
        return self.getint('sqlalchemy', 'min_chunk_size')

    def post_load(self):
        """
        Called after load() has run.  Implement in sublcass to prime
        additional attributes/settings once config is available.
        """
        pass

    def load(self, filename=None):
        info = []
        self.discover_config_files(info)

        files = []
        for (conf_dir, namespace) in info:
            prefix = join_path(conf_dir, '%s%s.conf' % (namespace, '%s'))

            short = (self.hostname != self.shortname)
            upper = namespace.upper()

            files += [
                f for f in [
                    prefix % '',
                    prefix % ('-' + self.hostname),
                    prefix % ('-' + self.shortname) if short else None,
                    os.environ.get('%s_CONF' % upper) or None,
                ] if f
            ]

        if filename:
            files.append(filename)

        with open(files[0], 'r') as f:
            self.readfp(f, files[0])

        self.read(files[1:])

        self.files = files
        self.filename = filename

        self.post_load()

# vim:set ts=8 sw=4 sts=4 tw=78 et:
