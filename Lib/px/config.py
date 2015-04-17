#===============================================================================
# Imports
#===============================================================================
import os
import re

from collections import OrderedDict

import ctk.config

from ctk.path import (
    join_path,
    find_all_files_ending_with,
)

from ctk.util import (
    chdir,
    memoize,
    isiterable,
    strip_crlf,
    classproperty,

    Dict,
    ProcessWrapper,
)

from os.path import (
    isdir,
    exists,
    abspath,
    dirname,
    normpath,
    expanduser,
)

#===============================================================================
# Aliases
#===============================================================================
NoOptionError = ctk.config.NoOptionError
NoSectionError = ctk.config.NoSectionError

#===============================================================================
# Classes
#===============================================================================
class Config(ctk.config.Config):
    @property
    def demo_data_dir(self):
        try:
            return ctk.config.Config.get(self, 'demo', 'data_dir')
        except (NoSectionError, NoOptionError):
            return self.data_dir

#===============================================================================
# Helpers
#===============================================================================
def get_or_create_config():
    import ctk.config
    try:
        conf = ctk.config.get_config()
    except ctk.config.NoConfigObjectCreated:
        conf = Config()
        conf.load()
    return conf

# vim:set ts=8 sw=4 sts=4 tw=78 et:
