#===============================================================================
# Imports
#===============================================================================
import ujson
import parallel
import datetime

from parallel import (
    rdtsc,
    timer,
    thread,
    sys_stats,
    socket_stats,
    memory_stats,
    context_stats,
    thread_seq_id,
    register_dealloc,
)

import windows

from parallel.http.server import (
    quote_html,
    text_response,
    html_response,
    json_serialization,
    HttpServer,
)
#===============================================================================
# Aliases
#===============================================================================
Screenshot = windows.user.Screenshot
register_dealloc(Screenshot)
save_bitmap = windows.gdiplus.save_bitmap

#===============================================================================
# Globals
#===============================================================================
screenshot = Screenshot()
screenshot.save("screenshot.bmp")
save_bitmap(screenshot.handle, "screenshot.jpg", quality=100)
save_bitmap(screenshot.handle, "screenshot.png", quality=100)

def update_screenshot():
    screenshot = Screenshot()
    return bytes(screenshot)

t = timer(datetime.timedelta(milliseconds=15), 15, update_screenshot)
t.start()

#===============================================================================
# Scratch
#===============================================================================
from parallel import *
import datetime

class Test(timer):
    def __init__(self, *args, **kwds):
        debug("Entered Test.__init__().")
        timer.__init__(self, *args, **kwds)
    def run(self):
        debug("entered run()")
        
t = Test(datetime.timedelta(milliseconds=15), 1000)
t.func = t.run

from parallel import *
import datetime
import windows
Display = windows.user.Display
Screenshot = windows.user.Screenshot
d = Display()


def refresh_display():
    d.refresh()


def take_screenshot():
    s = Screenshot()
    return bytes(s)

dt = timer(datetime.timedelta(milliseconds=15), 15, refresh_display)
ds = timer(datetime.timedelta(milliseconds=15), 15, take_screenshot)

#===============================================================================
# Classes
#===============================================================================
class ScreenshotServer(HttpServer):
    http11 = True
    rate_limit = datetime.timedelta(milliseconds=10)

    def get_screenshot2(self, request):
        response = request.response
        response.code = 200
        response.message = 'OK'
        response.content_type = 'image/bmp'
        response.body = bytes(screenshot)

    def get_screenshot3(self, request):
        response = request.response
        response.code = 200
        response.message = 'OK'
        response.content_type = 'image/bmp'
        screenshot = Screenshot()
        response.body = bytes(screenshot)

    def get_screenshot4(self, request):
        response = request.response
        response.code = 200
        response.message = 'OK'
        response.content_type = 'image/bmp'
        response.body = t.data

    def hello(self, transport, data):
        return b'Hello, World!'

    def stats(self, transport, data):
        return {
            'system': dict(sys_stats()),
            'server': dict(socket_stats(transport.parent)),
            'memory': dict(memory_stats()),
            'contexts': dict(context_stats()),
            'elapsed': transport.elapsed(),
            'thread': thread_seq_id(),
        }

# vim:set ts=8 sw=4 sts=4 tw=80 et                                             :
