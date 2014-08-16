import os
import sys
import time
import html
import async
import urllib
import mimetypes
import posixpath

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
        'date',
        'server',
        'version',
        'headers',
        'request',
        'command',
        'explain',
        'message',
        'sendfile',
        'transport',
        'content_type',
        'last_modified',
        'other_headers',
        'content_length',
    )

    def __init__(self, request):
        self.body = ''
        self.code = 0
        self.server = DEFAULT_SERVER_RESPONSE
        self.request = request
        self.explain = ''
        self.sendfile = False
        self.transport = request.transport
        self.last_modified = None
        self.content_type = DEFAULT_CONTENT_TYPE
        self.content_length = 0
        self.other_headers = []

    def __bytes__(self):
        self.date = date_time_string()

        body = self.body
        code = self.code
        date = self.date
        server = self.server
        explain = self.explain
        message = self.message
        content_type = self.content_type

        if not self.content_length:
            self.content_length = len(body)

        content_length = ''
        if self.content_length:
            content_length = 'Content-Length: %d' % self.content_length

        connection = ''
        if not self.request.keep_alive:
            connection = 'Connection: close'

        if connection:
            self.other_headers.append(connection)

        if self.last_modified:
            lm = 'Last-Modified: %s' % self.last_modified
            self.other_headers.append(lm)

        bytes_body = None
        if body and isinstance(body, bytes):
            bytes_body = body
            body = None

        other_headers = '\r\n'.join(self.other_headers)
        response = (DEFAULT_RESPONSE % locals()).encode('UTF-8', 'replace')

        if bytes_body:
            response += bytes_body

        return response


class Request:
    __slots__ = (
        'data',
        'body',
        'path',
        'version',
        'headers',
        'command',
        'response',
        'transport',
        'keep_alive',
    )

    def __init__(self, transport, data):
        self.transport = transport
        self.data = data

        self.body = None
        self.version = None
        self.headers = None
        self.command = None
        self.keep_alive = False
        self.response = Response(self)

class HttpServer:

    use_sendfile = True

    def data_received(self, transport, data):
        request = Request(transport, data)
        self.process_new_request(request)
        if not request.keep_alive:
            transport.close()

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
            (command, path, version) = words
            if version[:5] != b'HTTP/':
                msg = "Bad request version (%r)" % version
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
                msg = "Bad request version (%r)" % version
                return self.error(request, 400, msg)
            if version_number >= (1, 1):
                request.keep_alive = True
            if version_number >= (2, 0):
                msg = "Invalid HTTP Version (%s)" % base_version_number
                return self.error(request, 505, msg)

        elif num_words == 2:
            (command, path) = words
            if command != b'GET':
                msg = "Bad HTTP/0.9 request type (%r)" % command
                return self.error(request, 400, msg)

        elif not words:
            request.response = None
            request.keep_alive = False
            return
        else:
            msg = "Bad request syntax (%r)" % requestline
            return self.error(request, 400, msg)

        command = command.decode()
        funcname = 'do_%s' % command
        if not hasattr(self, funcname):
            msg = 'Unsupported method (%r)' % funcname
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

        path = path.decode()
        version = version.decode()

        request.path = path
        request.command = command
        request.version = version

        if h.connection == 'close':
            request.keep_alive = False
        elif h.connection == 'keep-alive' or version >= 'HTTP/1.1':
            request.keep_alive = True

        func = getattr(self, funcname)
        return func(request)

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

        for name in paths:
            fullname = os.path.join(path, name)
            displayname = linkname = name

            # Append / for directories or @ for symbolic links
            if os.path.isdir(fullname):
                displayname = name + "/"
                linkname = name + "/"

            if os.path.islink(fullname):
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
                    response.body = f.read()
                    l = len(response.body)
                    return self.response(request, 200)

            except IOError:
                msg = 'File not found: %s' % path
                return self.error(request, 404, msg)
        else:
            try:
                st = os.stat(path)
                response.content_length = st[6]
                response.last_modified = date_time_string(st.st_mtime)
                response.code = 200
                response.message = 'OK'
                response.sendfile = True
                before = bytes(response)
                return response.transport.sendfile(before, path, None)

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
