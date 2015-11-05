#===============================================================================
# Imports
#===============================================================================
import sys
import json
import async
import parallel

import numpy as np
import pandas as pd

from async import (
    rdtsc,
    sys_stats,
    socket_stats,
    memory_stats,
    context_stats,
    enable_heap_override,
    disable_heap_override,
    call_from_main_thread,
    call_from_main_thread_and_wait,
)

from async.http.server import (
    quote_html,
    router,
    make_routes,
    text_response,
    html_response,
    json_serialization,

    Request,
    HttpServer,
)

#===============================================================================
# Aliases
#===============================================================================

#===============================================================================
# Globals/Templates
#===============================================================================
df2 = pd.DataFrame({
    'A' : 1.,
    'B' : pd.Timestamp('20130102'),
    'C' : pd.Series(1,index=list(range(4)),dtype='float32'),
    'D' : np.array([3] * 4, dtype='int32'),
    'E' : pd.Categorical(["test","train","test","train"]),
    'F' : 'foo',
})

df = pd.DataFrame({'A': [1., 2.], 'B': [3., 4.]})

#===============================================================================
# Helpers
#===============================================================================

#===============================================================================
# Classes
#===============================================================================
routes = make_routes()
route = router(routes)

class PandasHttpServer(HttpServer):
    routes = routes

    @route
    def json(self, request):
        # df.to_json() crashes
        json_serialization(request, df.to_dict())

    @route
    def csv(self, request):
        text_response(request, text=df.to_csv())

    @route
    def html(self, request):
        html_response(request, df.to_html())

    @route
    def repr(self, request):
        text_response(request, str(df))

class PandasHttpServer2(HttpServer):
    http11 = True

    def json(self, transport, data):
        # df.to_json() crashes
        return df.to_dict()

    def csv(self, transport, data):
        parallel.debug(repr(df))
        return bytes(df.to_csv())

    def html(self, transport, data):
        parallel.debug(repr(df))
        return str(df.to_html())

# vim:set ts=8 sw=4 sts=4 tw=80 et                                             :
