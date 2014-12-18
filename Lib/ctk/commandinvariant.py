#===============================================================================
# Imports
#===============================================================================
from ctk.command import (
    Command,
)

from ctk.invariant import (
    InvariantAwareObject,
)

#===============================================================================
# Commands
#===============================================================================

class InvariantAwareCommand(InvariantAwareObject, Command):
    def __init__(self, *args, **kwds):
        long_opts = kwds.get('long_opts', {})
        short_opts = kwds.get('short_opts', {})
        for (s, l) in zip(('v', 'q', 'c'), ('verbose', 'quiet', 'conf')):
            attr = '_%s_' % l
            if getattr(self, attr):
                assert l not in long_opts,  (l, long_opts)
                assert s not in short_opts, (s, short_opts)
                long_opts[l] = None
                short_opts[s] = None

        kwds['long_opts'] = long_opts
        kwds['short_opts'] = short_opts
        InvariantAwareObject.__init__(self, *args, **kwds)

        del kwds['long_opts']
        del kwds['short_opts']
        Command.__init__(self, *args, **kwds)

# vim:set ts=8 sw=4 sts=4 tw=78 et:
