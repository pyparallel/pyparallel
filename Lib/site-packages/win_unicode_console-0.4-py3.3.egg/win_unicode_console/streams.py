
import io
import sys
import time
from ctypes import byref, c_ulong

from .buffer import get_buffer
from .info import WINDOWS, PY2

if PY2:
	from .file_object import FileObject


if WINDOWS:
	from ctypes import windll
	
	kernel32 = windll.kernel32
	GetStdHandle = kernel32.GetStdHandle
	ReadConsoleW = kernel32.ReadConsoleW
	WriteConsoleW = kernel32.WriteConsoleW
	GetLastError = kernel32.GetLastError
	
	STDIN_HANDLE = GetStdHandle(-10)
	STDOUT_HANDLE = GetStdHandle(-11)
	STDERR_HANDLE = GetStdHandle(-12)


ERROR_SUCCESS = 0
ERROR_NOT_ENOUGH_MEMORY = 8
ERROR_OPERATION_ABORTED = 995


STDIN_FILENO = 0
STDOUT_FILENO = 1
STDERR_FILENO = 2

EOF = b"\x1a"

MAX_BYTES_WRITTEN = 32767	# arbitrary because WriteConsoleW ability to write big buffers depends on heap usage


class _ReprMixin:
	def __repr__(self):
		modname = self.__class__.__module__
		
		if PY2:
			clsname = self.__class__.__name__
		else:
			clsname = self.__class__.__qualname__
		
		attributes = []
		for name in ["name", "encoding"]:
			try:
				value = getattr(self, name)
			except AttributeError:
				pass
			else:
				attributes.append("{}={}".format(name, repr(value)))
		
		return "<{}.{} {}>".format(modname, clsname, " ".join(attributes))


class WindowsConsoleRawIOBase(_ReprMixin, io.RawIOBase):
	def __init__(self, name, handle, fileno):
		self.name = name
		self.handle = handle
		self.file_no = fileno
	
	def fileno(self):
		return self.file_no
	
	def isatty(self):
		# PY3 # super().isatty()	# for close check in default implementation
		super(WindowsConsoleRawIOBase, self).isatty()
		return True

class WindowsConsoleRawReader(WindowsConsoleRawIOBase):
	def readable(self):
		return True
	
	def readinto(self, b):
		bytes_to_be_read = len(b)
		if not bytes_to_be_read:
			return 0
		elif bytes_to_be_read % 2:
			raise ValueError("cannot read odd number of bytes from UTF-16-LE encoded console")
		
		buffer = get_buffer(b, writable=True)
		code_units_to_be_read = bytes_to_be_read // 2
		code_units_read = c_ulong()
		
		retval = ReadConsoleW(self.handle, buffer, code_units_to_be_read, byref(code_units_read), None)
		if GetLastError() == ERROR_OPERATION_ABORTED:
			time.sleep(0.1)	# wait for KeyboardInterrupt
		if not retval:
			raise OSError("Windows error {}".format(GetLastError()))
		
		if buffer[0] == EOF:
			return 0
		else:
			return 2 * code_units_read.value

class WindowsConsoleRawWriter(WindowsConsoleRawIOBase):
	def writable(self):
		return True
	
	@staticmethod
	def _error_message(errno):
		if errno == ERROR_SUCCESS:
			return "Windows error {} (ERROR_SUCCESS); zero bytes written on nonzero input, probably just one byte given".format(errno)
		elif errno == ERROR_NOT_ENOUGH_MEMORY:
			return "Windows error {} (ERROR_NOT_ENOUGH_MEMORY); try to lower `win_unicode_console.streams.MAX_BYTES_WRITTEN`".format(errno)
		else:
			return "Windows error {}".format(errno)
	
	def write(self, b):
		bytes_to_be_written = len(b)
		buffer = get_buffer(b)
		code_units_to_be_written = min(bytes_to_be_written, MAX_BYTES_WRITTEN) // 2
		code_units_written = c_ulong()
		
		retval = WriteConsoleW(self.handle, buffer, code_units_to_be_written, byref(code_units_written), None)
		bytes_written = 2 * code_units_written.value
		
		# fixes both infinite loop of io.BufferedWriter.flush() on when the buffer has odd length
		#	and situation when WriteConsoleW refuses to write lesser than MAX_BYTES_WRITTEN bytes
		if bytes_written == 0 != bytes_to_be_written:
			raise OSError(self._error_message(GetLastError()))
		else:
			return bytes_written


class _TextStreamWrapperMixin(_ReprMixin):
	def __init__(self, base):
		self.base = base
	
	@property
	def encoding(self):
		return self.base.encoding
	
	@property
	def errors(self):
		return self.base.errors
	
	@property
	def line_buffering(self):
		return self.base.line_buffering
	
	def seekable(self):
		return self.base.seekable()
	
	def readable(self):
		return self.base.readable()
	
	def writable(self):
		return self.base.writable()
	
	def flush(self):
		self.base.flush()
	
	def close(self):
		self.base.close()
	
	@property
	def closed(self):
		return self.base.closed
	
	@property
	def name(self):
		return self.base.name
	
	def fileno(self):
		return self.base.fileno()
	
	def isatty(self):
		return self.base.isatty()
	
	def write(self, s):
		return self.base.write(s)
	
	def tell(self):
		return self.base.tell()
	
	def truncate(self, pos=None):
		return self.base.truncate(pos)
	
	def seek(self, cookie, whence=0):
		return self.base.seek(cookie, whence)
	
	def read(self, size=None):
		return self.base.read(size)
	
	def __next__(self):
		return next(self.base)
	
	def readline(self, size=-1):
		return self.base.readline(size)
	
	@property
	def newlines(self):
		return self.base.newlines

class TextStreamWrapper(_TextStreamWrapperMixin, io.TextIOBase):
	pass

class TextTranscodingWrapper(TextStreamWrapper):
	encoding = None # disable the descriptor
	
	def __init__(self, base, encoding):
		# PY3 # super().__init__(base)
		super(TextTranscodingWrapper, self).__init__(base)
		self.encoding = encoding

class StrStreamWrapper(TextStreamWrapper):
	def write(self, s):
		if isinstance(s, bytes):
			s = s.decode(self.encoding)
		
		self.base.write(s)

if PY2:
	class FileobjWrapper(_TextStreamWrapperMixin, file):
		def __init__(self, base, f):
			super(FileobjWrapper, self).__init__(base)
			fileobj = self._fileobj = FileObject.from_file(self)
			fileobj.set_encoding(base.encoding)
			fileobj.copy_file_pointer(f)
			fileobj.readable = base.readable()
			fileobj.writable = base.writable()
		
		# needed for the right interpretation of unicode literals in interactive mode when win_unicode_console is enabled in sitecustomize since Py_Initialize changes encoding afterwards
		def _reset_encoding(self):
			self._fileobj.set_encoding(self.base.encoding)
		
		def readline(self, size=-1):
			self._reset_encoding()
			return self.base.readline(size)


if WINDOWS:
	stdin_raw = WindowsConsoleRawReader("<stdin>", STDIN_HANDLE, STDIN_FILENO)
	stdout_raw = WindowsConsoleRawWriter("<stdout>", STDOUT_HANDLE, STDOUT_FILENO)
	stderr_raw = WindowsConsoleRawWriter("<stderr>", STDERR_HANDLE, STDERR_FILENO)
	
	stdin_text = io.TextIOWrapper(io.BufferedReader(stdin_raw), encoding="utf-16-le", line_buffering=True)
	stdout_text = io.TextIOWrapper(io.BufferedWriter(stdout_raw), encoding="utf-16-le", line_buffering=True)
	stderr_text = io.TextIOWrapper(io.BufferedWriter(stderr_raw), encoding="utf-16-le", line_buffering=True)
	
	stdin_text_transcoded = TextTranscodingWrapper(stdin_text, encoding="utf-8")
	stdout_text_transcoded = TextTranscodingWrapper(stdout_text, encoding="utf-8")
	stderr_text_transcoded = TextTranscodingWrapper(stderr_text, encoding="utf-8")
	
	stdout_text_str = StrStreamWrapper(stdout_text_transcoded)
	stderr_text_str = StrStreamWrapper(stderr_text_transcoded)
	if PY2:
		stdin_text_fileobj = FileobjWrapper(stdin_text_transcoded, sys.__stdin__)
		stdout_text_str_fileobj = FileobjWrapper(stdout_text_str, sys.__stdout__)


def disable():
	sys.stdin.flush()
	sys.stdout.flush()
	sys.stderr.flush()
	sys.stdin = sys.__stdin__
	sys.stdout = sys.__stdout__
	sys.stderr = sys.__stderr__

def check_stream(stream, fileno):
	if stream is None:	# e.g. with IDLE
		return True
	
	try:
		_fileno = stream.fileno()
	except io.UnsupportedOperation:
		return False
	else:
		if _fileno == fileno and stream.isatty():
			stream.flush()
			return True
		else:
			return False

# PY3 # def enable(*, stdin=Ellipsis, stdout=Ellipsis, stderr=Ellipsis):
def enable(stdin=Ellipsis, stdout=Ellipsis, stderr=Ellipsis):
	if not WINDOWS:
		return
	
	# defaults
	if PY2:
		if stdin is Ellipsis:
			stdin = stdin_text_fileobj
		if stdout is Ellipsis:
			stdout = stdout_text_str
		if stderr is Ellipsis:
			stderr = stderr_text_str
	else: # transcoding because Python tokenizer cannot handle UTF-16
		if stdin is Ellipsis:
			stdin = stdin_text_transcoded
		if stdout is Ellipsis:
			stdout = stdout_text_transcoded
		if stderr is Ellipsis:
			stderr = stderr_text_transcoded
	
	if stdin is not None and check_stream(sys.stdin, STDIN_FILENO):
		sys.stdin = stdin
	if stdout is not None and check_stream(sys.stdout, STDOUT_FILENO):
		sys.stdout = stdout
	if stderr is not None and check_stream(sys.stderr, STDERR_FILENO):
		sys.stderr = stderr

# PY3 # def enable_only(*, stdin=None, stdout=None, stderr=None):
def enable_only(stdin=None, stdout=None, stderr=None):
	enable(stdin=stdin, stdout=stdout, stderr=stderr)
