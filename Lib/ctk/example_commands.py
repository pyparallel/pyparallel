#===============================================================================
# Imports
#===============================================================================
import os

from ctk.command import (
    Command,
    CommandError,
)

#===============================================================================
# Miscellaneous/Generic Commands
#===============================================================================
class DumpConfigCommand(Command):
    def run(self):
        self.conf.write(self.ostream)

class ShowConfigFileLoadOrderCommand(Command):
    def run(self):
        if not self.conf.files:
            raise CommandError('no configuration files are being loaded')
        self._out(os.linesep.join(self.conf.files))


# vim:set ts=8 sw=4 sts=4 tw=78 et:
