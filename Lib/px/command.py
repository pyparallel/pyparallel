#===============================================================================
# Imports
#===============================================================================
from ctk.commandinvariant import (
    InvariantAwareCommand,
)

#===============================================================================
# Helpers
#===============================================================================

#===============================================================================
# Base Ronda Command
#===============================================================================
class PxCommand(InvariantAwareCommand):
    pass

class TCPServerCommand(PxCommand):
    server = None
    protocol = None

class TCPClientCommand(PxCommand):
    pass

# vim:set ts=8 sw=4 sts=4 tw=78 et:
