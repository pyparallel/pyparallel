import os
import sys
import time
import html
import json
import async
import urllib
import mimetypes
import posixpath

is_pyparallel = False
try:
    import _async
    is_pyparallel = True
    InvalidFileRangeError = _async.InvalidFileRangeError
    FileTooLargeError = _async.FileTooLargeError
except ImportError:
    InvalidFileRangeError = RuntimeError
    FileTooLargeError = RuntimeError

from async.http import (
    DEFAULT_CONTENT_TYPE,
    DEFAULT_ERROR_CONTENT_TYPE,
    DEFAULT_ERROR_MESSAGE,
    DEFAULT_RESPONSE,
    DEFAULT_SERVER_RESPONSE,
    DIRECTORY_LISTING,
    RESPONSES,
)

if not mimetypes.inited:
    mimetypes.init()

extensions_map = mimetypes.types_map.copy()
extensions_map.update({
    '': 'application/octet-stream', # Default
    '.py': 'text/plain',
    '.c': 'text/plain',
    '.h': 'text/plain',
})

url_unquote = urllib.parse.unquote
html_escape = html.escape
normpath = posixpath.normpath

def keep_alive_check(f):
    def decorator(*args):
        result = f(*args)
        (obj, transport) = (args[0:2])
        if not obj.keep_alive:
            transport.close()
        return result


def _quote_html(html):
    return html.replace("&", "&amp;").replace("<", "&lt;").replace(">", "&gt;")

def translate_path(path, base=None):
    """Translate a /-separated PATH to the local filename syntax.

    Components that mean special things to the local file system
    (e.g. drive or directory names) are ignored.  (XXX They should
    probably be diagnosed.)

    """
    # abandon query parameters
    path = path.split('?',1)[0]
    path = path.split('#',1)[0]
    path = normpath(url_unquote(path))
    words = path.split('/')
    words = filter(None, words)
    if not base:
        base = os.getcwd()
    path = base
    for word in words:
        drive, word = os.path.splitdrive(word)
        head, word = os.path.split(word)
        if word in (os.curdir, os.pardir):
            continue
        path = os.path.join(path, word)
    return path

def guess_type(path):
    """Guess the type of a file.

    Argument is a PATH (a filename).

    Return value is a string of the form type/subtype,
    usable for a MIME Content-type header.

    The default implementation looks the file's extension
    up in the table self.extensions_map, using application/octet-stream
    as a default; however it would be permissible (if
    slow) to look inside the data to make a better guess.

    """

    (base, ext) = posixpath.splitext(path)
    if ext in extensions_map:
        return extensions_map[ext]
    ext = ext.lower()
    if ext in extensions_map:
        return extensions_map[ext]
    else:
        return extensions_map['']

weekdayname = ['Mon', 'Tue', 'Wed', 'Thu', 'Fri', 'Sat', 'Sun']

monthname = [None,
             'Jan', 'Feb', 'Mar', 'Apr', 'May', 'Jun',
             'Jul', 'Aug', 'Sep', 'Oct', 'Nov', 'Dec']


def date_time_string(timestamp=None):
    """Return the current date and time formatted for a message header."""
    if timestamp is None:
        timestamp = time.time()
    year, month, day, hh, mm, ss, wd, y, z = time.gmtime(timestamp)
    return "%s, %02d %3s %4d %02d:%02d:%02d GMT" % (
        weekdayname[wd],
        day,
        monthname[month],
        year,
        hh, mm, ss
    )

class Options(dict):
    def __init__(self, values=dict()):
        assert isinstance(values, dict)
        dict.__init__(self, **values)

    def __getattr__(self, name):
        try:
            return self[name]
        except KeyError:
            return None

class InvalidHeaderText(Exception):
    pass

class Headers(dict):
    def __init__(self, text):
        self._text = text
        if not text:
            return
        for line in text.split(b'\r\n'):
            ix = line.find(b':')
            if ix == -1:
                raise InvalidHeaderText()
            (key, value) = (line[:ix], line[ix+1:])
            key = key.lower().decode()
            value = value.lstrip().decode()
            self[key] = value
            self[key.replace('-', '_')] = value

    def __getattr__(self, name):
        try:
            return self[name]
        except KeyError:
            return None

class Response:
    __slots__ = (
        'body',
        'code',
        'etag',
        'date',
        'server',
        'version',
        'headers',
        'request',
        'command',
        'explain',
        'message',
        'sendfile',
        'timestamp',
        'transport',
        'content_type',
        'content_range',
        'last_modified',
        'other_headers',
        'content_length',
        '_response',
    )

    def __init__(self, request):
        self.body = ''
        self.code = 0
        self.etag = None
        self.date = None
        self.server = DEFAULT_SERVER_RESPONSE
        self.version = None
        self.headers = None
        self.request = request
        self.command = None
        self.explain = ''
        self.message = None
        self.sendfile = False
        self.timestamp = None
        self.transport = request.transport
        self.last_modified = None
        self.content_type = DEFAULT_CONTENT_TYPE
        self.content_range = None
        self.last_modified = None
        self.other_headers = []
        self.content_length = 0
        self._response = None

    def __bytes__(self):
        self.date = date_time_string()

        body = self.body
        code = self.code
        date = self.date
        server = self.server
        explain = self.explain
        message = self.message
        content_type = self.content_type

        connection = ''
        if not self.request.keep_alive:
            connection = 'Connection: close'

        if connection:
            self.other_headers.append(connection)

        if self.last_modified:
            lm = 'Last-Modified: %s' % self.last_modified
            self.other_headers.append(lm)

        if self.content_range:
            self.other_headers.append(self.content_range)

        if self.other_headers:
            other_headers = '\r\n'.join(self.other_headers)
            rn1 = '\r\n'
        else:
            rn1 = ''
            other_headers = ''

        bytes_body = None
        if body:
            if isinstance(body, bytes):
                bytes_body = body
                body = None

                if not self.content_length:
                    self.content_length = len(bytes_body) #+ len(rn2)
            elif not self.content_length:
                bytes_body = body.encode('UTF-8', 'replace')
                body = None
                self.content_length = len(bytes_body) #+ len(rn2)

        if self.content_length:
            content_length = 'Content-Length: %d' % self.content_length
            rn2 = '\r\n'
        else:
            content_length = 'Content-Length: 0'
            rn2 = ''

        kwds = {
            'code': code,
            'message': message,
            'server': server,
            'date': date,
            'content_type': content_type,
            'content_length': content_length,
            'other_headers': other_headers,
            'rn1': rn1,
            'rn2': rn2,
            'body': body if body else '',
        }
        response = (DEFAULT_RESPONSE % kwds).encode('UTF-8', 'replace')

        if bytes_body:
            response += bytes_body

        self._response = response
        return response

    def _to_dict(self):
        return {
            k: getattr(self, k)
                for k in self.__slots__
                    if k not in ('transport', 'request')
        }

    #def __repr__(self):
    #    return repr(self._to_dict())

    def _to_json(self):
        return json.dumps(self._to_dict())


class Request:
    __slots__ = (
        'data',
        'body',
        'path',
        'range',
        'query',
        'version',
        'headers',
        'command',
        'raw_path',
        'response',
        'fragment',
        'transport',
        'timestamp',
        'keep_alive',
    )

    def __init__(self, transport, data):
        self.transport = transport
        self.data = data

        self.body = None
        self.path = None
        self.range = None
        self.query = {}
        self.version = None
        self.headers = None
        self.command = None
        self.raw_path = None
        self.fragment = None
        self.timestamp = None
        self.keep_alive = False
        self.response = Response(self)

    def _to_dict(self):
        return {
            k: getattr(self, k)
                for k in self.__slots__
                    if k not in ('transport', 'response')
        }

    #def __repr__(self):
    #    return repr(self._to_dict())

    def _to_json(self):
        return json.dumps(self._to_dict())


class InvalidRangeRequest(BaseException):
    pass

class RangedRequest:
    __slots__ = (
        'first_byte',
        'last_byte',
        'suffix_length',

        # These are filled in when set_file_size() is called.
        'offset',
        'num_bytes_to_send',
        'file_size',
        'content_range',
    )

    def __init__(self, requested_range):
        self.first_byte = None
        self.last_byte = None
        self.suffix_length = None

        self.offset = None
        self.num_bytes_to_send = None
        self.file_size = None
        self.content_range = None

        try:
            r = requested_range.replace(' ', '')        \
                               .replace('bytes', '')    \
                               .replace('=', '')

            if r.startswith('-'):
                self.suffix_length = int(r[1:])
            elif r.endswith('-'):
                self.first_byte = int(r[:-1])
            else:
                pair = r.split('-')
                self.first_byte = int(pair[0])
                self.last_byte = int(pair[1])
        except Exception as e:
            raise InvalidRangeRequest

    def set_file_size(self, file_size):
        self.file_size = file_size

        if self.suffix_length is not None:
            if self.suffix_length > self.file_size:
                raise InvalidRangeRequest

            self.last_byte = file_size-1
            self.first_byte = file_size - self.suffix_length - 1

        else:
            if self.first_byte > file_size-1:
                raise InvalidRangeRequest

            if not self.last_byte or self.last_byte > file_size-1:
                self.last_byte = file_size-1

        self.num_bytes_to_send = (self.last_byte - self.first_byte) + 1

        self.content_range = 'Content-Range: %d-%d/%d' % (
            self.first_byte,
            self.last_byte,
            self.file_size,
        )

class HttpServer:

    use_sendfile = True
    #throughput = True
    #low_latency = True
    #max_sync_send_attempts = 100
    #max_sync_recv_attempts = 100

    def data_received(self, transport, data):
        #async.debug(data)
        request = Request(transport, data)
        try:
            self.process_new_request(request)
        except Exception as e:
            #msg = repr(e)
            #async.debug(msg)
            if e.args:
                msg = '\n'.join(e.args)
            elif e.message:
                msg = e.message
            self.error(request, 500, msg)

        if not request.keep_alive:
            request.transport.close()

        response = request.response
        if not response or response.sendfile:
            return None
        else:
            return bytes(response)

    def process_data_received(self, request):
        self.process_new_request(request)
        if not request.keep_alive:
            request.transport.close()

        response = request.response
        if not response or response.sendfile:
            return None
        else:
            return bytes(response)

    def process_new_request(self, request):
        raw = request.data
        ix = raw.find(b'\r\n')
        if ix == -1:
            return self.error(request, 400, "Line too long")
        (requestline, rest) = (raw[:ix], raw[ix+2:])
        words = requestline.split()
        num_words = len(words)
        if num_words == 3:
            (command, raw_path, version) = words
            if version[:5] != b'HTTP/':
                msg = "Bad request version (%s)" % version
                return self.error(request, 400, msg)
            try:
                base_version_number = version.split(b'/', 1)[1]
                version_number = base_version_number.split(b'.')
                # RFC 2145 section 3.1 says there can be only one "." and
                #   - major and minor numbers MUST be treated as
                #      separate integers;
                #   - HTTP/2.4 is a lower version than HTTP/2.13, which in
                #      turn is lower than HTTP/12.3;
                #   - Leading zeros MUST be ignored by recipients.
                if len(version_number) != 2:
                    raise ValueError
                version_number = int(version_number[0]), int(version_number[1])
            except (ValueError, IndexError):
                msg = "Bad request version (%s)" % version
                return self.error(request, 400, msg)
            if version_number >= (1, 1):
                request.keep_alive = True
            if version_number >= (2, 0):
                msg = "Invalid HTTP Version (%s)" % base_version_number
                return self.error(request, 505, msg)

        elif num_words == 2:
            (command, raw_path) = words
            if command != b'GET':
                msg = "Bad HTTP/0.9 request type (%s)" % command
                return self.error(request, 400, msg)

        elif not words:
            request.response = None
            request.keep_alive = False
            return
        else:
            msg = "Bad request syntax (%s)" % requestline
            return self.error(request, 400, msg)

        command = command.decode()
        funcname = 'do_%s' % command
        if not hasattr(self, funcname):
            msg = 'Unsupported method (%s)' % funcname
            return self.error(request, 501, msg)

        ix = rest.rfind(b'\r\n\r\n')
        if ix == -1:
            return self.error(request, 400, "Line too long")

        raw_headers = rest[:ix]
        try:
            headers = Headers(raw_headers)
        except Exception as e:
            return self.error(request, 400, "Malformed headers")

        h = request.headers = headers

        version = version.decode()
        raw_path = raw_path.decode()

        # Eh, urllib.parse.urlparse doesn't work here... I took a look at the
        # source and figure it's either because of all the global caching
        # going on, or it's using a generator somewhere.  So, let's just
        # manually unpack the url as needed.  (Update: probably should review
        # this assumption now that we've supposedly fixed generators.)
        url = raw_path
        if '#' in url:
            (url, request.fragment) = url.split('#', 1)
        if '?' in url:
            (url, qs) = url.split('?', 1)
            if '&' in qs:
                pairs = qs.split('&')
            else:
                pairs = [ qs, ]

            for pair in pairs:
                # Discard anything that isn't in key=value format.
                if '=' not in pair:
                    continue
                (key, value) = pair.split('=')
                if '%' in value:
                    value = url_unquote(value)
                request.query[key] = value

        request.path = url
        request.raw_path = raw_path
        request.version = version
        request.command = command

        # IE sends through 'Keep-Alive', not 'keep-alive' like everything
        # else.
        connection = (h.connection or '').lower()
        if connection == 'close':
            request.keep_alive = False
        elif connection == 'keep-alive' or version >= 'HTTP/1.1':
            request.keep_alive = True

        if not h.range:
            # See if there's a ?range=1234-5678 and use that (handy when you
            # want to test range handling via the browser, where typing
            # /foo?range=1234-4567 is easy).
            if 'range' in request.query:
                h.range = request.query['range']

        if h.range:
            if ',' in h.range:
                # Don't permit multiple ranges.
                return self.error(request, 400, "Multiple ranges not supported")

            # But for anything else, the HTTP spec says to fall through and
            # process as per normal, so we just blow away the h.range header
            # in that case.
            elif h.range.count('-') != 1:
                h.range = None
            else:
                try:
                    request.range = RangedRequest(h.range)
                except InvalidRangeRequest:
                    h.range = None

        # This routing/dispatching logic is quite possibly the most horrendous
        # thing I've ever written.  On the other hand, it gets the immediate
        # job done, so eh.
        self.pre_route(request)
        func = self.dispatch(request)
        if not func:
            return self.error(request, 400, 'Unsupported Method')

        return func(request)

    def pre_route(self, request):
        """Fiddle with request.path if necessary here."""
        return None

    def route(self, request):
        """Override in subclass if desired.  Return a callable."""
        return None

    def dispatch(self, request):
        func = self.route(request)
        if not func:
            func = self.simple_overload_dispatch(request)
        return func

    def simple_overload_dispatch(self, request):
        func = None
        path = request.path
        command = request.command
        funcname = 'do_%s' % command
        overload_suffix = path.replace('/', '_')
        if overload_suffix[-1] == '_':
            overload_suffix = overload_suffix[:-1]
        overload_funcname = ''.join((command.lower(), overload_suffix))
        try:
            func = getattr(self, overload_funcname)
        except AttributeError:
            try:
                # Take off the command bit.
                overload_funcname = overload_suffix
                func = getattr(self, overload_funcname)
            except AttributeError:
                try:
                    func = getattr(self, funcname)
                except AttributeError:
                    pass

        return func

    def do_HEAD(self, request):
        return self.do_GET(request)

    def do_GET(self, request):
        response = request.response
        path = translate_path(request.path)
        if os.path.isdir(path):
            if not request.path.endswith('/'):
                return self.redirect(request, request.path + '/')
            found = False
            for index in ("index.html", "index.htm"):
                index = os.path.join(path, index)
                if os.path.exists(index):
                    path = index
                    found = True
                    break
            if not found:
                return self.list_directory(request, path)

        if not os.path.exists(path):
            msg = 'File not found: %s' % path
            return self.error(request, 404, msg)

        return self.sendfile(request, path)

    def list_directory(self, request, path):
        #async.debug(repr(request))
        try:
            paths = os.listdir(path)
        except os.error:
            msg = 'No permission to list directory.'
            return self.error(request, 404, msg)

        paths.sort(key=lambda a: a.lower())

        displaypath = html_escape(url_unquote(request.path))
        #charset = sys.getfilesystemencoding()
        charset = 'utf-8'
        title = 'Directory listing for %s' % displaypath
        items = []
        item_fmt = '<li><a href="%s">%s</a></li>'

        join = os.path.join
        isdir = os.path.isdir
        islink = os.path.islink

        for name in paths:
            fullname = join(path, name)
            displayname = linkname = name

            # Append / for directories or @ for symbolic links
            if isdir(fullname):
                displayname = name + "/"
                linkname = name + "/"

            if islink(fullname):
                # Note: a link to a directory displays with @ and links with /
                displayname = name + "@"

            item = item_fmt % (url_unquote(linkname), html_escape(displayname))
            items.append(item)

        items = '\n'.join(items)
        output = DIRECTORY_LISTING % locals()

        response = request.response
        response.code = 200
        response.message = 'OK'
        response.content_type = "text/html; charset=%s" % charset
        response.body = output
        return

    def sendfile(self, request, path):
        response = request.response
        response.content_type = guess_type(path)
        if not self.use_sendfile:
            try:
                with open(path, 'rb') as f:
                    fs = os.fstat(f.fileno())
                    response.content_length = fs[6]
                    response.last_modified = date_time_string(fs.st_mtime)
                    if request.command == 'GET':
                        response.body = f.read()
                        l = len(response.body)
                    return self.response(request, 200)

            except IOError:
                msg = 'File not found: %s' % path
                return self.error(request, 404, msg)

        st = os.stat(path)
        size = st[6]
        last_modified = date_time_string(st.st_mtime)

        if request.range:
            r = request.range
            try:
                r.set_file_size(size)
            except InvalidRangeRequest:
                return self.error(request, 416)

            try:
                response.content_length = r.num_bytes_to_send
                response.last_modified = last_modified
                response.code = 206
                response.message = 'Partial Content'
                response.content_range = r.content_range
                if request.command == 'GET':
                    response.sendfile = True
                    before = bytes(response)
                    return response.transport.sendfile_ranged(
                        before,
                        path,
                        None, # after
                        r.first_byte, # offset
                        r.num_bytes_to_send
                    )
                else:
                    return

            except InvalidFileRangeError:
                return self.error(request, 416)

            except IOError:
                msg = 'File not found: %s' % path
                return self.error(request, 404, msg)

            except Exception as e:
                # As per spec, follow-through to a normal 200/sendfile.
                pass

        try:
            response.content_length = st[6]
            response.last_modified = date_time_string(st.st_mtime)
            response.code = 200
            response.message = 'OK'
            if request.command == 'GET':
                response.sendfile = True
                before = bytes(response)
                return response.transport.sendfile(before, path, None)
            else:
                return

        except FileTooLargeError:
            msg = "File too large (>2GB); use ranged requests."
            return self.error(request, 413, msg)

        except IOError:
            msg = 'File not found: %s' % path
            return self.error(request, 404, msg)

    def error(self, request, code, message=None):
        r = RESPONSES[code]
        if not message:
            message = r[0]

        response = request.response
        response.code = code
        response.content_type = DEFAULT_ERROR_CONTENT_TYPE
        response.message = message
        response.explain = r[1]


        response.body = DEFAULT_ERROR_MESSAGE % {
            'code' : code,
            'message' : message,
            'explain' : response.explain,
        }

    def redirect(self, request, path):
        response = request.response
        response.other_headers.append('Location: %s' % path)
        return self.response(request, 301)

    def response(self, request, code, message=None):
        r = RESPONSES[code]
        if not message:
            message = r[0]

        response = request.response
        response.code = code
        response.message = message
        response.explain = r[1]

def main():
    import socket
    ipaddr = socket.gethostbyname(socket.gethostname())
    server = async.server(ipaddr, 8080)
    async.register(transport=server, protocol=HttpServer)
    async.run()

if __name__ == '__main__':
    main()


# vim:set ts=8 sw=4 sts=4 tw=78 et:
