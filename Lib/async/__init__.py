import _async

def call_from_main_thread_and_wait(f):
    def decorator(*_args, **_kwds):
        return _async.call_from_main_thread_and_wait(f, _args, _kwds)
    return decorator

def call_from_main_thread(f):
    def decorator(*_args, **_kwds):
        _async.call_from_main_thread(f, _args, _kwds)
    return decorator

#class Transport:
#    def __init__(self, protocol):
#        self.protocol = protocol
#
#    def write(self, data):
#        pass
#
#    def writelines(self, iterable):
#        pass
#
#class BaseHTTPRequestHandler:
#
#    @call_from_main_thread
#    def log_error(fmt, *args)
#        sys.stderr.write(fmt % args)
#
#    def send_response(self, code, message=None):
#        """Add the response header to the headers buffer and log the
#        response code.
#
#        Also send two standard headers with the server software
#        version and the current date.
#
#        """
#        self.log_request(code)
#        self.send_response_only(code, message)
#        self.send_header('Server', self.version_string())
#        self.send_header('Date', self.date_time_string())
#
#
#    def send_error(self, code, message=None):
#        """Send and log an error reply.
#
#        Arguments are the error code, and a detailed message.
#        The detailed message defaults to the short entry matching the
#        response code.
#
#        This sends an error response (so it must be called before any
#        output has been generated), logs the error, and finally sends
#        a piece of HTML explaining the error to the user.
#
#        """
#
#        try:
#            shortmsg, longmsg = self.responses[code]
#        except KeyError:
#            shortmsg, longmsg = '???', '???'
#        if message is None:
#            message = shortmsg
#        explain = longmsg
#
#        self.log_error("code %d, message %s", code, message)
#
#        # using _quote_html to prevent Cross Site Scripting attacks
#        # (see bug #1100201)
#        content = (
#            self.error_message_format % {
#                'code': code,
#                'message': _quote_html(message),
#                'explain': explain
#            }
#        )
#        body = content.encode('UTF-8', 'replace')
#        self.send_response(code, message)
#        self.send_header("Content-Type", self.error_content_type)
#        self.send_header('Connection', 'close')
#        self.send_header('Content-Length', int(len(body)))
#        self.end_headers()
#        if self.command != 'HEAD' and code >= 200 and code not in (204, 304):
#            self.wfile.write(body)


# vim:set ts=8 sw=4 sts=4 tw=78 et:
