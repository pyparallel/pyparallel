# Helper module stub to assist with the `async` -> `parallel` module rename in
# lieu of Python 3.5's introduction of a keyword called `async`.

import async as _xasync
import _async as __xasync

from async import *
