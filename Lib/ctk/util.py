#===============================================================================
# Imports
#===============================================================================
import os
import re
import sys
import json
import shutil
import inspect
import calendar
import datetime
import itertools
import collections

try:
    import cStringIO as StringIO
except ImportError:
    import io
    StringIO = io.StringIO

from datetime import (
    timedelta,
)

from os.path import (
    join,
    isdir,
    abspath,
    dirname,
    basename,
    normpath,
)

from itertools import (
    chain,
    repeat,
)

from collections import (
    namedtuple,
    defaultdict,
)

from pprint import (
    pformat,
)

from functools import (
    wraps,
    partial,
)

from subprocess import (
    Popen,
    PIPE,
)

from csv import reader as csv_reader

from ctk import (
    logic,
)


#===============================================================================
# Globals
#===============================================================================
SHORT_MONTHS = (
    'Jan',
    'Feb',
    'Mar',
    'Apr',
    'May',
    'June',
    'July',
    'Aug',
    'Sept',
    'Oct',
    'Nov',
    'Dec',
)
SHORT_MONTHS_UPPER = [ m.upper() for m in SHORT_MONTHS ]

SHORT_MONTHS_SET = set(SHORT_MONTHS)
SHORT_MONTHS_UPPER_SET = set(SHORT_MONTHS_UPPER)

is_linux = (sys.platform.startswith('linux'))
is_darwin = (sys.platform == 'darwin')
is_win32 = (sys.platform == 'win32')

assert sum((is_linux, is_darwin, is_win32)) == 1
#===============================================================================
# Helper Methods
#===============================================================================
def bytes_to_mb(b):
    return '%0.3fMB' % (float(b)/1024.0/1024.0)

def bytes_to_kb(b):
    return '%0.3fKB' % (float(b)/1024.0)

def lower(l):
    return [ s.lower() for s in l ]

def iterable(i):
    if not i:
        return []
    return (i,) if not hasattr(i, '__iter__') else i

def isiterable(i):
    return hasattr(i, '__iter__') or hasattr(i, 'next')

def try_int(i):
    if not i:
        return
    try:
        i = int(i)
    except ValueError:
        return
    else:
        return i

def is_int(i):
    try:
        int(i)
    except ValueError:
        return False
    else:
        return True

def progressbar(i, total=None, leave=False):
    try:
        from tqdm import tqdm
        return tqdm(i, total=total, leave=leave)
    except ImportError:
        import sys
        e = sys.stderr.write
        e("tqdm not installed, not displaying progressbar\n")
        e("tip: run `pip install tqdm` from commandline to fix\n")
        return i

def null_progressbar(r, *args, **kwds):
    return iter(r)

def flatten(l):
    return [ item for sublist in l for item in sublist ]

def version_combinations(s):
    """
    >>> version_combinations('5.10.0')
    ['5.10.0', '5.10', '5']
    >>> version_combinations('3.2')
    ['3.2', '3']
    >>> version_combinations('6')
    ['6']
    """
    if '.' not in s:
        return [s] if try_int(s) else None

    ints = s.split('.')
    if not all(i.isdigit() for i in ints):
        return None

    return [ '.'.join(ints[:x]) for x in reversed(range(1, len(ints)+1)) ]

def invert_defaultdict_by_value_len(d):
    i = defaultdict(list)
    for (k, v) in d.items():
        i[len(v)].append(k)
    return i

def ensure_sorted(d):
    keys = d.keys()
    sorted_keys = [ k for k in sorted(keys) ]
    assert keys == sorted_keys, (keys, sorted_keys)

def yield_scalars(obj, scalar_types=None):
    if not scalar_types:
        try:
            scalar_types = frozenset((int, float, str, unicode))
        except NameError:
            scalar_types = frozenset((int, float, str))

    for k in dir(obj.__class__):
        v = getattr(obj, k)
        t = type(v)
        y = None
        if t in scalar_types:
            y = (k, v)
        if y:
            yield y

def generate_repr(obj, exclude=None, include=None, yielder=None):
    cls = obj.__class__
    ex = set(exclude if exclude else [])
    inc = set(include if include else [])
    yielder = yielder or yield_scalars
    p = lambda v: v if (not v or isinstance(v, int)) else '"%s"' % v
    return '<%s %s>' % (
        (cls.__name__, ', '.join(
            '%s=%s' % (k, p(v)) for (k, v) in (
                (k, v) for (k, v) in yielder(obj) if (
                    k in inc or (
                        k[0] != '_' and
                        k not in ex and
                        not k.endswith('_id')
                    )
                )
            ) if (True if inc else bool(v))
        ))
    )

def get_query_slices(total_size, ideal_chunk_size=None, min_chunk_size=None):
    from ctk.config import get_config
    conf = get_config()
    if not ideal_chunk_size:
        ideal_chunk_size = conf.sqlalchemy_ideal_chunk_size
    if not min_chunk_size:
        min_chunk_size = conf.sqlalchemy_min_chunk_size

    if ideal_chunk_size >= total_size:
        yield (0, total_size)
        raise StopIteration

    start = 0 - ideal_chunk_size
    while True:
        start += ideal_chunk_size
        end = start + ideal_chunk_size - 1
        if end > total_size:
            yield (start, total_size)
            raise StopIteration

        next_start = start + ideal_chunk_size
        next_end = min(next_start + ideal_chunk_size - 1, total_size)
        if next_end - next_start < min_chunk_size:
            yield (start, total_size)
            raise StopIteration

        yield (start, end)

def stream(query, size=None, limit=None,
                  ideal_chunk_size=None,
                  min_chunk_size=None):

    if limit:
        query.limit(limit)

    if not size:
        size = query.count()

    slice_offsets = get_query_slices(
        size if not limit else min(size, limit),
        ideal_chunk_size=ideal_chunk_size,
        min_chunk_size=min_chunk_size,
    )

    for (start, end) in slice_offsets:
        for result in query.slice(start, end):
            yield result

def stream_results(query):
    size = query.count()
    return progressbar(stream(query, size), total=size, leave=True)

def ensure_unique(d):
    seen = set()
    for k in d:
        assert k not in seen
        seen.add(k)

def endswith(string, suffixes):
    """
    Return True if string ``s`` endswith any of the items in sequence ``l``.
    """
    for suffix in iterable(suffixes):
        if string.endswith(suffix):
            return True
    return False

def startswith(string, prefixes):
    """
    Return True if string ``s`` startswith any of the items in sequence ``l``.
    """
    for prefix in iterable(prefixes):
        if string.startswith(prefix):
            return True
    return False

def extract_columns(line, cols, sep='|'):
    values = []
    last_ix = 0
    ix = 0
    i = 0
    for col_ix in cols:
        assert col_ix > i
        while True:
            i += 1
            ix = line.find(sep, last_ix)
            if i == col_ix:
                values.append(line[last_ix:ix])
                last_ix = ix + 1
                break
            else:
                last_ix = ix + 1
    return values

def extract_column(line, col, sep='|'):
    return extract_columns(line, (col,))[0]

def strip_crlf(line):
    if line[-2:] == u'\r\n':
        line = line[:-2]
    elif line[-1:] == u'\n':
        line = line[:-1]
    return line

def clone_dict(d):
    """
    Helper method intended to be used with defaultdicts -- returns a new dict
    with all defaultdicts converted to dicts (recursively).
    """
    r = {}
    for (k, v) in d.iteritems():
        if hasattr(v, 'iteritems'):
            v = clone_dict(v)
        elif isinstance(v, set):
            v = [ i for i in v ]
        r[k] = v
    return r

def find_all_files_ending_with(dirname, suffix):
    results = []
    from ctk.path import join_path
    for (root, dirs, files) in os.walk(dirname):
        results += [
            join_path(root, file)
                for file in files
                    if file.endswith(suffix)
        ]
    return results

def guess_gzip_filesize(path):
    """
    Only works when file is <= 2GB.
    """
    import struct
    with open(path, 'rb') as f:
        f.seek(-4, 2)
        size = struct.unpack('<i', f.read())[0]
    return size

def requires_context(f):
    @wraps(f)
    def wrapper(*args, **kwds):
        obj = args[0]
        fname = f.func_name
        n = '%s.%s' % (obj.__class__.__name__, fname)
        if not obj.entered:
            m = "%s must be called from within an 'with' statement." % n
            raise RuntimeError(m)
        elif obj.exited:
            allow = False
            try:
                allow = obj.allow_reentry_after_exit
            except AttributeError:
                pass
            if not allow:
                m = "%s can not be called after leaving a 'with' statement."
                raise RuntimeError(m % n)
            else:
                obj.exited = False
        return f(*args, **kwds)
    return wrapper

def implicit_context(f):
    @wraps(f)
    def wrapper(*args, **kwds):
        obj = args[0]
        fname = f.func_name
        n = '%s.%s' % (obj.__class__.__name__, fname)
        if not obj.entered:
            with obj as obj:
                return f(*args, **kwds)
        else:
            return f(*args, **kwds)
    return wrapper

if is_linux:
    def set_process_name(name):
        import ctypes
        libc = ctypes.cdll.LoadLibrary('libc.so.6')
        PR_SET_NAME = 15
        libc.prctl(PR_SET_NAME, name, 0, 0, 0)

class classproperty(property):
    def __get__(self, obj, type_):
        return self.fget.__get__(None, type_)()

    def __set__(self, obj, value):
        cls = type(obj)
        return self.fset.__get__(None, cls)(value)

def add_linesep_if_missing(s):
    return '' if not s else (s if s[-1] is os.linesep else s + os.linesep)

def strip_linesep_if_present(s):
    if s.endswith('\r\n'):
        return s[-2]
    elif s[-1] == '\n':
        return s[-1]
    else:
        return s

def prepend_warning_if_missing(s):
    return add_linesep_if_missing(
        s if s.startswith('warning: ') else 'warning: ' + s
    )

def prepend_error_if_missing(s):
    return add_linesep_if_missing(
        s if s.startswith('error: ') else 'error: ' + s
    )

def render_text_table(rows, **kwds):
    banner = kwds.get('banner')
    footer = kwds.get('footer')
    output = kwds.get('output', sys.stdout)
    balign = kwds.get('balign', str.center)
    formats = kwds.get('formats')
    special = kwds.get('special')
    rows = list(rows)
    if not formats:
        formats = lambda: chain((str.ljust,), repeat(str.rjust))

    cols = len(rows[0])
    paddings = [
        max([len(str(r[i])) for r in rows]) + 2
            for i in xrange(cols)
    ]

    length = sum(paddings) + cols
    strip = '+%s+' % ('-' * (length-1))
    out = list()
    if banner:
        lines = iterable(banner)
        banner = [ strip ] + \
                 [ '|%s|' % balign(l, length-1) for l in lines ] + \
                 [ strip, ]
        out.append('\n'.join(banner))

    rows.insert(1, [ '-', ] * cols)
    out += [
        '\n'.join([
            k + '|'.join([
                fmt(str(column), padding, (
                    special if column == special else fill
                )) for (column, fmt, padding) in zip(row, fmts(), paddings)
            ]) + k for (row, fmts, fill, k) in zip(
                rows,
                chain(
                    repeat(lambda: repeat(str.center,), 1),
                    repeat(formats,)
                ),
                chain((' ',), repeat('-', 1), repeat(' ')),
                chain(('|', '+'), repeat('|'))
            )
        ] + [strip,])
    ]

    if footer:
        footers = iterable(footer)
        footer = [ strip ] + \
                 [ '|%s|' % balign(f, length-1) for f in footers ] + \
                 [ strip, '' ]
        out.append('\n'.join(footer))

    output.write(add_linesep_if_missing('\n'.join(out)))

def render_unicode_table(rows, **kwds):
    """
    Unicode version of above.  Such code repetition!
    """
    banner = kwds.get('banner')
    footer = kwds.get('footer')
    output = kwds.get('output', sys.stdout)
    balign = kwds.get('balign', unicode.center)
    formats = kwds.get('formats')
    special = kwds.get('special')
    rows = list(rows)
    if not formats:
        formats = lambda: chain((unicode.ljust,), repeat(unicode.rjust))

    cols = len(rows[0])
    paddings = [
        max([len(unicode(r[i])) for r in rows]) + 2
            for i in xrange(cols)
    ]

    length = sum(paddings) + cols
    strip = u'+%s+' % (u'-' * (length-1))
    out = list()
    if banner:
        lines = iterable(banner)
        banner = [ strip ] + \
                 [ u'|%s|' % balign(l, length-1) for l in lines ] + \
                 [ strip, ]
        out.append(u'\n'.join(banner))

    rows.insert(1, [ u'-', ] * cols)
    out += [
        u'\n'.join([
            k + u'|'.join([
                fmt(unicode(column), padding, (
                    special if column == special else fill
                )) for (column, fmt, padding) in zip(row, fmts(), paddings)
            ]) + k for (row, fmts, fill, k) in zip(
                rows,
                chain(
                    repeat(lambda: repeat(unicode.center,), 1),
                    repeat(formats,)
                ),
                chain((u' ',), repeat(u'-', 1), repeat(u' ')),
                chain((u'|', u'+'), repeat(u'|'))
            )
        ] + [strip,])
    ]

    if footer:
        footers = iterable(footer)
        footer = [ strip ] + \
                 [ u'|%s|' % balign(f, length-1) for f in footers ] + \
                 [ strip, u'' ]
        out.append(u'\n'.join(footer))

    l = u'\n'.join(out)
    if l[-1] != u'\n':
        l = l + u'\n'
    output.write(l)


def render_rst_grid(rows, **kwds):
    output  = kwds.get('output', sys.stdout)
    formats = kwds.get('formats')
    special = kwds.get('special')
    rows = list(rows)
    if not formats:
        formats = lambda: chain((str.ljust,), repeat(str.rjust))

    cols = len(rows[0])
    paddings = [
        max([len(str(r[i])) for r in rows]) + 2
            for i in xrange(cols)
    ]

    length = sum(paddings) + cols
    strip = '+%s+' % ('-' * (length-1))
    out = list()
    if banner:
        lines = iterable(banner)
        banner = [ strip ] + \
                 [ '|%s|' % balign(l, length-1) for l in lines ] + \
                 [ strip, ]
        out.append('\n'.join(banner))

    rows.insert(1, [ '-', ] * cols)
    out += [
        '\n'.join([
            k + '|'.join([
                fmt(str(column), padding, (
                    special if column == special else fill
                )) for (column, fmt, padding) in zip(row, fmts(), paddings)
            ]) + k for (row, fmts, fill, k) in zip(
                rows,
                chain(
                    repeat(lambda: repeat(str.center,), 1),
                    repeat(formats,)
                ),
                chain((' ',), repeat('-', 1), repeat(' ')),
                chain(('|', '+'), repeat('|'))
            )
        ] + [strip,])
    ]

    if footer:
        footers = iterable(footer)
        footer = [ strip ] + \
                 [ '|%s|' % balign(f, length-1) for f in footers ] + \
                 [ strip, '' ]
        out.append('\n'.join(footer))

    output.write(add_linesep_if_missing('\n'.join(out)))

def literal_eval(v):
    try:
        import ast
    except ImportError:
        return eval(v)
    else:
        return ast.literal_eval(v)

def load_propval(orig_value, propname, attempts):
    c = itertools.count(0)

    eval_value = None
    conv_value = None

    last_attempt = False

    attempt = attempts.next()

    try:
        if attempt == c.next():
            assert orig_value == literal_eval(orig_value)
            return orig_value

        if attempt == c.next():
            conv_value = pformat(orig_value)
            eval_value = literal_eval(conv_value)
            assert eval_value == orig_value
            return conv_value

        if attempt == c.next():
            conv_value = '"""%s"""' % pformat(orig_value)
            eval_value = literal_eval(conv_value)
            assert eval_value == orig_value
            return conv_value

        if attempt == c.next():
            conv_value = repr(orig_value)
            eval_value = literal_eval(conv_value)
            assert eval_value == orig_value
            return conv_value

        if attempt == c.next():
            conv_value = str(orig_value)
            eval_value = literal_eval(conv_value)
            assert eval_value == orig_value
            return conv_value

        last_attempt = True

    except:
        if not last_attempt:
            return load_propval(orig_value, propname, attempts)
        else:
            raise ValueError(
                "failed to convert property '%s' value: %s" % (
                    propname,
                    orig_value,
                )
            )

def get_methods_in_order(obj, predicate=None):
    """
    Return a tuple consisting of two-pair tuples.  The first value is an
    integer starting at 0 and the second is the value of the method name.

    If predicate is not None, predicate(method_name) will be called with
    the method name (string).  Return True to add the value to the list.

    >>> class Test(object):
    ...     def __init__(self): pass
    ...     def xyz(self): pass
    ...     def abc(self): pass
    ...     def kef(self): pass
    >>>
    >>> t = Test()
    >>> get_methods_in_order(t)
    ((0, 'xyz'), (1, 'abc'), (2, 'kef'))
    >>> [ n for n in dir(t) if n[0] != '_' ]
    ['abc', 'kef', 'xyz']
    >>>

    >>> class PredicateTest(object):
    ...     def f_z(self): pass
    ...     def xyz(self): pass
    ...     def f_x(self): pass
    ...     def abc(self): pass
    ...     def f_a(self): pass
    ...     def kef(self): pass
    >>>
    >>> t = PredicateTest()
    >>> get_methods_in_order(t, lambda s: s.startswith('f_'))
    ((0, 'f_z'), (1, 'f_x'), (2, 'f_a'))
    >>> [ n for n in dir(t) if n[0] != '_' ]
    ['abc', 'f_a', 'f_x', 'f_z', 'kef', 'xyz']
    >>>
    """
    return tuple(
        (i, m) for (i, m) in enumerate(
              m[1] for m in sorted(
                  (m[1].im_func.func_code.co_firstlineno, m[0]) for m in (
                      inspect.getmembers(obj, lambda v:
                          inspect.ismethod(v) and
                          v.im_func.func_name[0] != '_'
                      )
                  )
              ) if not predicate or predicate(m[1])
        )
    )


def get_source(obj):
    src = None
    try:
        src = inspect.getsource(obj)
    except (TypeError, IOError):
        pass

    if src:
        return src

    try:
        from IPython.core import oinspect
    except ImportError:
        pass
    else:
        try:
            src = oinspect.getsource(obj)
        except TypeError:
            pass

    if src:
        return src

    main = sys.modules['__main__']
    pattern = re.compile('class %s\(' % obj.__class__.__name__)
    for src in reversed(main.In):
        if pattern.search(src):
            return src

def timestamp():
    return datetime.datetime.now()

def timestamp_string(strftime='%Y%m%d%H%M%S-%f'):
    return datetime.datetime.now().strftime(strftime)

def friendly_timedelta(td):
    parts = []
    s = str(td)
    ix = s.find(',')
    if ix != -1:
        parts.append(s[:ix])
        hhmmss = s[ix+2:]
    else:
        hhmmss = s

    values = (hh, mm, ss) = [ v.lstrip('0') for v in hhmmss.split(':') ]
    names = ('hours', 'minutes', 'seconds')
    for (value, name) in zip(values, names):
        if not value:
            continue
        if value == '1':
            # Make singular
            name = name[:-1]
        parts.append('%s %s' % (value, name))

    return ', '.join(parts)

def touch_file(path):
    if os.path.exists(path):
        return

    with open(path, 'w') as f:
        f.truncate(0)
        f.flush()
        f.close()

    assert os.path.exists(path)

def try_remove_file(path):
    try:
        os.unlink(path)
    except:
        pass

def try_remove_file_atexit(path):
    import atexit
    atexit.register(try_remove_file, path)

def try_remove_dir(path):
    shutil.rmtree(path, ignore_errors=True)

def try_remove_dir_atexit(path):
    import atexit
    atexit.register(try_remove_dir, path)

def pid_exists(pid):
    if os.name == 'nt':
        import psutil
        return psutil.pid_exists(pid)
    else:
        try:
            os.kill(pid, 0)
        except OSError as e:
            import errno
            if e.errno == errno.ESRCH:
                return False
            else:
                raise
        else:
            return True

def get_week_bounds_for_day(weeks_from_day=0, day=None):
    """
    Return a tuple consisting of two datetime.date() values that represent
    the first day of the week (Monday) and last day of the week (Sunday)
    based on the input parameters ``weeks_from_day`` and ``day``.

    By default, if no values are provided, the bounds for the week of the
    current day are returned.  ``weeks_from_day`` would be 0 in this case.
    A value of 1 would return the bounds for next week from the current day.

    If a value is provided for ``day``, it will be used instead of today's
    date.  (This was mainly added to assist writing static doctests below,
    but it may be helpful in certain situations.)

    Tests against a mid-week day, including a month and year transition:
        >>> day = datetime.date(2013, 07, 20)
        >>> get_week_bounds_for_day(day=day)
        (datetime.date(2013, 7, 15), datetime.date(2013, 7, 21))
        >>> get_week_bounds_for_day(weeks_from_day=1, day=day)
        (datetime.date(2013, 7, 22), datetime.date(2013, 7, 28))
        >>> get_week_bounds_for_day(weeks_from_day=2, day=day)
        (datetime.date(2013, 7, 29), datetime.date(2013, 8, 4))

        >>> day = datetime.date(2013, 12, 31)
        >>> get_week_bounds_for_day(day=day)
        (datetime.date(2013, 12, 30), datetime.date(2014, 1, 5))
        >>> get_week_bounds_for_day(weeks_from_day=1, day=day)
        (datetime.date(2014, 1, 6), datetime.date(2014, 1, 12))

    Tests against a start-of-week day:
        >>> day = datetime.date(2013, 12, 30)
        >>> get_week_bounds_for_day(day=day)
        (datetime.date(2013, 12, 30), datetime.date(2014, 1, 5))
        >>> get_week_bounds_for_day(weeks_from_day=1, day=day)
        (datetime.date(2014, 1, 6), datetime.date(2014, 1, 12))

    Tests against a end-of-week day:
        >>> day = datetime.date(2014, 2, 2)
        >>> get_week_bounds_for_day(day=day)
        (datetime.date(2014, 1, 27), datetime.date(2014, 2, 2))

    """
    if not day:
        day = datetime.date.today()

    if weeks_from_day:
        day = day + datetime.timedelta(weeks=weeks_from_day)

    day_of_week = calendar.weekday(day.year, day.month, day.day)

    monday = day - datetime.timedelta(days=day_of_week)
    sunday = day + datetime.timedelta(days=(6 - day_of_week))

    return (monday, sunday)

def get_days_between_dates(start_date, end_date):
    """
    Returns a list of datetime.date objects between start_date and end_date.
    """
    days = list()
    next_date = start_date
    while True:
        days.append(next_date)
        next_date = next_date + datetime.timedelta(days=1)
        if next_date > end_date:
            break
    return days

def get_days_for_year(years):
    cal = calendar.Calendar()
    return [
        (year, month, day)
            for year in years
                for month in range(1, 13)
                    for day in cal.itermonthdays(year, month)
                        if day
    ]

def get_days_for_year_dd_MMM_yyyy(years):
    return [
         (year, month, day, '%s-%s-%d' % (
            str(day).zfill(2),
            SHORT_MONTHS_UPPER[month-1][:3],
            year,
        )) for (year, month, day) in iterdays(years)
    ]

def clear_screen():
    if os.name == 'nt':
        os.system('cls')
    else:
        sys.stdout.write(chr(27) + "[2J")

# memoize/memoized lovingly stolen from conda.utils.
class memoized(object):
    """Decorator. Caches a function's return value each time it is called.
    If called later with the same arguments, the cached value is returned
    (not reevaluated).
    """
    def __init__(self, func):
        self.func = func
        self.cache = {}
    def __call__(self, *args):
        if not isinstance(args, collections.Hashable):
            # uncacheable. a list, for instance.
            # better to not cache than blow up.
            return self.func(*args)
        if args in self.cache:
            return self.cache[args]
        else:
            value = self.func(*args)
            self.cache[args] = value
            return value

class memoize(object): # 577452
    def __init__(self, func):
        self.func = func
    def __get__(self, obj, objtype=None):
        if obj is None:
            return self.func
        return partial(self, obj)
    def __call__(self, *args, **kw):
        obj = args[0]
        try:
            cache = obj.__cache
        except AttributeError:
            cache = obj.__cache = {}
        key = (self.func, args[1:], frozenset(kw.items()))
        try:
            res = cache[key]
        except KeyError:
            res = cache[key] = self.func(*args, **kw)
        return res

def list_zfill(l, width):
    """
    Pad a list with empty strings on the left, to fill the list to the
    specified width.  No-op when len(l) >= width.

    >>> list_zfill(['a', 'b'], 5)
    ['', '', '', 'a', 'b']
    >>> list_zfill(['a', 'b', 'c'], 1)
    ['a', 'b', 'c']
    >>> list_zfill(['a', 'b', 'c'], 3)
    ['a', 'b', 'c']
    """
    list_len = len(l)
    if len(l) >= width:
        return l

    return [ '' for _ in range(0, width-list_len) ] + l

#===============================================================================
# Helper Classes
#===============================================================================
class forgiving_list(list):
    """
    Helper class that returns None upon __getitem__ index errors.
    (``forgiving_list`` is a terrible name for this class.)

    >>> l = forgiving_list(['a', 'b'])
    >>> [ l[i] for i in range(0, len(l)+2) ]
    ['a', 'b', None, None]
    >>> l[3]
    >>>
    >>> l[-100]
    >>>
    """

    def __init__(self, seq):
        self.seq = seq
        list.__init__(self, seq)

    def __getitem__(self, i):
        try:
            return list.__getitem__(self, i)
        except IndexError:
            return None

class word_groups(object):
    """
    >>> l = word_groups(['a', 'b', 'c', 'd'], size=2)
    >>> l[0]
    ['a', 'b']
    >>> l[1]
    ['b', 'c']
    >>> l[2]
    ['c', 'd']
    >>> l[3]
    ['d', None]
    >>> l[4]
    [None, None]
    >>> l[100]
    [None, None]
    >>> [ e for e in l ]
    [['a', 'b'], ['b', 'c'], ['c', 'd'], ['d']]
    """
    def __init__(self, seq, size=2):
        if not isinstance(seq, list):
            seq = seq.split(' ')
        self.seq = forgiving_list(seq)
        self.size = size

    def __getitem__(self, i):
        l = self.seq
        m = i + self.size
        return [ l[x] for x in range(i, m) ]

    def __iter__(self):
        i = 0
        while True:
            r = [ e for e in self[i] if e ]
            if not r:
                raise StopIteration
            yield r
            i += 1

    def __repr__(self):
        return repr([ e for e in self ])

class chdir(object):
    def __init__(self, path):
        self.old_path = os.getcwd()
        self.path = path

    def __enter__(self):
        os.chdir(self.path)
        return self

    def __exit__(self, *exc_info):
        os.chdir(self.old_path)

class SlotObject(object):
    # Subclasses need to define __slots__
    _default_ = None

    _to_dict_prefix_ = ''
    _to_dict_suffix_ = ''
    _to_dict_exclude_ = set()

    # Defaults to _to_dict_exclude_ if not set.
    _repr_exclude_ = set()

    def __init__(self, *args, **kwds):
        seen = set()
        slots = list(self.__slots__)
        args = [ a for a in args ]
        while args:
            (key, value) = (slots.pop(0), args.pop(0))
            seen.add(key)
            setattr(self, key, value)

        for (key, value) in kwds.iteritems():
            seen.add(key)
            setattr(self, key, value)

        for slot in self.__slots__:
            if slot not in seen:
                setattr(self, slot, self._default_)

        return

    def _to_dict(self, prefix=None, suffix=None, exclude=None):
        prefix = prefix or self._to_dict_prefix_
        suffix = suffix or self._to_dict_suffix_
        exclude = exclude or self._to_dict_exclude_
        return {
            '%s%s%s' % (prefix, key, suffix): getattr(self, key)
                for key in self.__slots__
                     if key not in exclude
        }

    def __repr__(self):
        slots = self.__slots__
        exclude = self._repr_exclude_ or self._to_dict_exclude_

        q = lambda v: v if (not v or isinstance(v, int)) else '"%s"' % v
        return "<%s %s>" % (
            self.__class__.__name__,
            ', '.join(
                '%s=%s' % (k, q(v))
                    for (k, v) in (
                        (k, getattr(self, k))
                            for k in slots
                                if k not in exclude
                    )
                )
        )

class UnexpectedCodePath(RuntimeError):
    pass

class ContextSensitiveObject(object):
    allow_reentry_after_exit = True

    def __init__(self, *args, **kwds):
        self.context_depth = 0
        self.entered = False
        self.exited = False

    def __enter__(self):
        assert self.entered is False
        if self.allow_reentry_after_exit:
            self.exited = False
        else:
            assert self.exited is False
        result = self._enter()
        self.entered = True
        assert isinstance(result, self.__class__)
        return result

    def __exit__(self, *exc_info):
        assert self.entered is True and self.exited is False
        self._exit()
        self.exited = True
        self.entered = False

    def _enter(self):
        raise NotImplementedError

    def _exit(self, *exc_info):
        raise NotImplementedError

class ImplicitContextSensitiveObject(object):

    def __init__(self, *args, **kwds):
        self.context_depth = 0

    def __enter__(self):
        self.context_depth += 1
        self._enter()
        return self

    def __exit__(self, *exc_info):
        self.context_depth -= 1
        self._exit(*exc_info)

    def _enter(self):
        raise NotImplementedError

    def _exit(self, *exc_info):
        raise NotImplementedError

class ConfigList(list):
    def __init__(self, parent, name, args):
        self._parent = parent
        self._name = name
        list.__init__(self, args)

    def append(self, value):
        list.append(self, value)
        self._parent._save(self._name, self)

class ConfigDict(dict):
    def __init__(self, parent, name, kwds):
        self._parent = parent
        self._name = name
        dict.__init__(self, kwds)

    def __getattr__(self, name):
        if name[0] == '_':
            return dict.__getattribute__(self, name)
        else:
            return self.__getitem__(name)

    def __setattr__(self, name, value):
        if name[0] == '_':
            dict.__setattr__(self, name, value)
        else:
            self.__setitem__(name, value)

    def __getitem__(self, name):
        i = dict.__getitem__(self, name)
        if isinstance(i, dict):
            return ConfigDict(self, name, i)
        elif isinstance(i, list):
            return ConfigList(self, name, i)
        else:
            return i

    def __delitem__(self, name):
        dict.__delitem__(self, name)
        self._parent._save(self._name, self)

    def __setitem__(self, name, value):
        dict.__setitem__(self, name, value)
        self._parent._save(self._name, self)

    def _save(self, name, value):
        self[name] = value


class Options(dict):
    def __init__(self, values=dict()):
        assert isinstance(values, dict)
        dict.__init__(self, **values)

    def __getattr__(self, name):
        if name not in self:
            return False
        else:
            return self.__getitem__(name)

#===============================================================================
# Helper Classes
#===============================================================================
class Constant(dict):
    def __init__(self):
        items = self.__class__.__dict__.items()
        filtered = filter(lambda t: t[0][:2] != '__', items)
        for (key, value) in filtered:
            try:
                self[value] = key
                if isinstance(key, str) and isinstance(value, int):
                    self[str(value)] = key
            except:
                pass

        for (key, value) in filtered:
            l = key.lower()
            if l not in self:
                self[l] = value

            u = key.upper()
            if u not in self:
                self[u] = value

    def __getattr__(self, name):
        return self.__getitem__(name)

def invert_counts(d, sort=True, reverse=True):
    i = {}
    for (k, v) in d.items():
        if k[0] == '_' or k == 'trait_names':
            continue
        i.setdefault(v, []).append(k)
    if not sort:
        return i
    else:
        keys = [ k for k in sorted(i.keys(), reverse=reverse) ]
        return [ (key, value) for key in keys for value in i[key] ]

class Stats(defaultdict):
    def __init__(self, typename=int):
        defaultdict.__init__(self, typename)

    def __getattr__(self, name):
        return self.__getitem__(name)

    def __setattr__(self, name, value):
        return self.__setitem__(name, value)

    def keys(self):
        return [
            k for k in defaultdict.keys(self)
                if k[0] != '_' and k != 'trait_names'
        ]

    def _to_dict(self):
        return { k: self[k] for k in self.keys() }

    def _to_json(self):
        return json.dumps(self)

    def _save(self, path):
        with open(path, 'w') as f:
            json.dump(f, self)

    def _invert(self):
        return invert_counts(self)

class KeyedStats(Stats):
    def __init__(self):
        Stats.__init__(self, typename=lambda: Stats())

    def _invert(self):
        return { k: self[k]._invert() for k in self.keys() }

class Dict(dict):
    """
    A dict that allows direct attribute access to keys.
    """
    def __init__(self, *args, **kwds):
        dict.__init__(self, *args, **kwds)
    def __getattr__(self, name):
        return self.__getitem__(name)
    def __setattr__(self, name, value):
        return self.__setitem__(name, value)

class DecayDict(Dict):
    """
    A dict that allows once-off direct attribute access to keys.  The key/
    attribute is subsequently deleted after a successful get.
    """
    def __getitem__(self, name):
        v = dict.__getitem__(self, name)
        del self[name]
        return v

    def get(self, name, default=None):
        v = dict.get(self, name, default)
        if name in self:
            del self[name]
        return v

    def __getattr__(self, name):
        return self.__getitem__(name)
    def __setattr__(self, name, value):
        return self.__setitem__(name, value)

    def assert_empty(self, obj):
        if self:
            raise RuntimeError(
                "%s:%s: unexpected keywords: %s" % (
                    obj.__class__.__name__,
                    inspect.currentframe().f_back.f_code.co_name,
                    repr(self)
                )
            )

class ProcessWrapper(object):
    def __init__(self, exe, *args, **kwds):
        self.exe      = exe
        self.rc       = int()
        self.cwd      = None
        self.wait     = True
        self.error    = str()
        self.output   = str()
        self.ostream  = kwds.get('ostream', sys.stdout)
        self.estream  = kwds.get('estream', sys.stderr)
        self.verbose  = kwds.get('verbose', False)
        self.safe_cmd = None
        self.exception_class = RuntimeError
        self.raise_exception_on_error = True

    def __getattr__(self, attr):
        if not attr.startswith('_') and not attr == 'trait_names':
            return lambda *args, **kwds: self.execute(attr, *args, **kwds)
        else:
            raise AttributeError(attr)

    def __call__(self, *args, **kwds):
        return self.execute(*args, **kwds)

    def build_command_line(self, exe, action, *args, **kwds):
        cmd  = [ exe, action ]
        for (k, v) in kwds.items():
            cmd.append(
                '-%s%s' % (
                    '-' if len(k) > 1 else '', k.replace('_', '-')
                )
            )
            if not isinstance(v, bool):
                cmd.append(v)
        cmd += list(args)
        return cmd

    def kill(self):
        self.p.kill()

    def execute(self, *args, **kwds):
        self.rc = 0
        self.error = ''
        self.output = ''

        self.cmd = self.build_command_line(self.exe, *args, **kwds)

        if self.verbose:
            cwd = self.cwd or os.getcwd()
            cmd = ' '.join(self.safe_cmd or self.cmd)
            self.ostream.write('%s>%s\n' % (cwd, cmd))

        self.p = Popen(self.cmd, executable=self.exe, cwd=self.cwd,
                       stdin=PIPE, stdout=PIPE, stderr=PIPE)
        if not self.wait:
            return

        self.outbuf = StringIO.StringIO()
        self.errbuf = StringIO.StringIO()

        while self.p.poll() is None:
            out = self.p.stdout.read().decode('utf-8')
            self.outbuf.write(out)
            if self.verbose and out:
                self.ostream.write(out)

            err = self.p.stderr.read().decode('utf-8')
            self.errbuf.write(err)
            if self.verbose and err:
                self.estream.write(err)

        self.rc = self.p.returncode
        self.error = self.errbuf.getvalue()
        self.output = self.outbuf.getvalue()
        if self.rc != 0 and self.raise_exception_on_error:
            if self.error:
                error = self.error
            elif self.output:
                error = 'no error info available, output:\n' + self.output
            else:
                error = 'no error info available'
            printable_cmd = ' '.join(self.safe_cmd or self.cmd)
            raise self.exception_class(printable_cmd, error)
        if self.output and self.output.endswith('\n'):
            self.output = self.output[:-1]

        return self.process_output(self.output)

    def process_output(self, output):
        return output

    def clone(self):
        return self.__class__(self.exe)

#===============================================================================
# CSV Tools/Utils
#===============================================================================
def create_namedtuple(name, data, mutable=False):
    header = list()
    wrappers = list()
    first = data.pop(0)
    # Skip over any empty columns.
    columns = [ c for c in first if c ]
    for col in columns:
        if '|' not in col:
            wrappers.append(None)
            header.append(col)
        else:
            import __builtin__
            (colname, classname) = col.split('|')
            if '.' in classname:
                assert classname.count('.') == 1
                (module, classname) = classname.split('.')
                cls = getattr(sys.modules[module], classname)
            elif classname.startswith('timestamp'):
                from datetime import datetime
                cls = lambda d: datetime.strptime(d, classname.split(',')[1])
            elif hasattr(__builtin__, classname):
                cls = getattr(__builtin__, classname)
            else:
                cls = globals()[classname]
            wrappers.append(cls)
            header.append(colname)

    rows = list()
    num_cols = len(header)
    for columns in data:
        if not isinstance(columns, list):
            columns = list(columns)
        l = logic.Mutex()
        l.correct_columns = num_cols == len(columns)
        l.missing_columns = ((num_cols - len(columns)) > 0)
        l.extra_columns   = ((len(columns) - num_cols) > 0)
        need_other = False
        with l as g:
            if g.correct_columns:
                assert all(isinstance(c, str) for c in columns)
            elif g.missing_columns:
                # Pad our columns list with empty strings.
                columns += ['',] * (num_cols - len(columns))
            elif g.extra_columns:
                # Convert everything past the expected column count to
                # one big list and use that as the 'other' column.
                columns = columns[0:num_cols] + [columns[num_cols:]]
                need_other = True

        if need_other:
            columns.append(list())

        rows.append(columns)

    if need_other:
        header.append('other')
        wrappers.append(None)

    if name.endswith('s'):
        name = name[:-1]

    t = wrappers
    values = list()
    results = list()
    if (all(r == '' for r in rows[-1])):
        rows = rows[:-1]
    values = [
       [ c if not t[i] else t[i](c) for (i, c) in enumerate(row) ]
        for row in rows
    ]
    if not mutable:
        _cls = namedtuple(name, header)
        cls = lambda n, h, v: _cls(*v)
    else:
        cls = lambda n, h, v: type(n, (object,), dict(zip(h, v)))
    results = [
        cls(name, header, v) for v in values
    ]

    return results

def create_namedtuple_from_sequence_of_key_value_pairs(name, seq):
    return create_namedtuple(name, zip(*seq))

def create_namedtuple_from_csv(name, csv):
    l = logic.Mutex()

    l.is_filename = (
        isinstance(csv, str) and
        '\n' not in csv and
        os.path.isfile(csv)
    )

    l.is_csv_text = (
        isinstance(csv, str) and
        '\n' in csv and
        ',' in csv
    )

    l.is_csv_lines = (
        not isinstance(csv, str) and (
            hasattr(csv, '__iter__') or
            hasattr(csv, 'next')
        )
    )

    lines = None

    with l as g:
        if g.is_filename:
            with open(csv, 'r') as f:
                lines = f.read().split('\n')

        elif g.is_csv_text:
            lines = csv.split('\n')

        elif g.is_csv_lines:
            lines = csv

    data = [ r for r in csv_reader(lines) ]
    mutable_sheets = set()
    mutable = True if name in mutable_sheets else False
    return create_namedtuple(name, data, mutable=mutable)

def download_url(url):
    from urllib2 import urlopen
    return urlopen(url).read()

def create_namedtuple_from_csv_url(name, url):
    return create_namedtuple_from_csv(name, download_url(url))

if __name__ == '__main__':
    import doctest
    doctest.testmod()

# vim:set ts=8 sw=4 sts=4 tw=78 et:
