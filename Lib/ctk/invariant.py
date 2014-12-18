#===============================================================================
# Imports
#===============================================================================
import os
import re
import inspect
import datetime
import linecache
import itertools

from ctk.util import (
    endswith,
)

from os.path import (
    isdir,
    exists,
    abspath,
    dirname,
    basename,
    normpath,
)

#===============================================================================
# Globals
#===============================================================================
SUFFIXES = (
    'Option',
    'Error',
    'Arg',
)

# Quick hack for 3.x support.
try:
    STRING_TYPES = (str, unicode)
except NameError:
    STRING_TYPES = (str,)

#===============================================================================
# Base Invariant Class
#===============================================================================
class Invariant(BaseException):
    _arg = None
    _type = None
    _name = None
    _help = None
    _maxlen = None
    _minlen = None
    _action = None
    _default = None
    _metavar = None
    _opt_long = None
    _opt_type = None
    _opt_short = None
    _mandatory = True
    _type_desc = None
    _capitalized_name = None

    __filter = lambda _, n: (n[0] != '_' and n not in ('message', 'args'))
    __keys = lambda _, n: (
        n[0] != '_' and n not in {
            'args',
            'actual',
            'message',
            'expected',
        }
    )

    def __init__(self, obj, name):
        self._obj = obj
        self._name = name
        self.actual = None
        self.dst_attr = None
        self.dst_value = None
        self._existing = None
        self._existing_str = None
        n = self.__class__.__name__.replace('Error', '').lower()
        if n.endswith('arg'):
            n = n[:-3]
        if hasattr(self, '_regex'):
            self._pattern = re.compile(self._regex)
            if not hasattr(self, 'expected'):
                self.expected = "%s to match regex '%s'" % (n, self._regex)
            self._test = self._test_regex

        if not hasattr(self, '_test'):
            self._test = self._test_simple_equality

        if not self._opt_type and self._type:
            if self._type in STRING_TYPES:
                self._opt_type = 'string'
            elif self._type  == int:
                self._opt_type = 'int'
            elif self._type == float:
                self._opt_type = 'float'
            elif self._type == complex:
                self._opt_type = 'complex'

        if not self._type_desc and self._opt_type:
            self._type_desc = self._opt_type

        if not self._type_desc:
            self._type_desc = self._type.__name__

        if not self._metavar:
            self._metavar = name.upper()

        s = None
        l = None
        long_opts = obj._long_opts
        short_opts = obj._short_opts

        if self._arg:
            a = self._arg
            assert len(a) >= 2, a
            if '/' in a:
                (s, l) = a.split('/')
                if s.startswith('-'):
                    s = s[1:]
                if l.startswith('--'):
                    l = l[2:]
                assert s, s
                assert l, l

            else:
                if a[0] == '-' and a[1] != '-':
                    s = a[1:]
                else:
                    assert a.startswith('--') and len(a) >= 4, a
                    l = a[2:]
        else:
            l = name.replace('_', '-')

            chars = [ (c, c.upper()) for c in list(name) ]
            for c in itertools.chain.from_iterable(chars):
                if c not in short_opts:
                    s = c
                    break

        if l:
            assert l not in long_opts, (l, long_opts)
            long_opts[l] = self
        if s:
            assert s not in short_opts, (s, short_opts)
            short_opts[s] = self

        self._opt_long = l
        self._opt_short = s

        tokens = re.findall('[A-Z][^A-Z]*', self.__class__.__name__)
        if tokens[-1] == 'Error':
            tokens = tokens[:-1]
        elif tokens[-1] == 'Arg':
            tokens = tokens[:-1]
        self._capitalized_name = ' '.join(tokens)

    def _test_regex(self):
        return bool(self._pattern.match(self.actual))

    def _test_simple_equality(self):
        return (self.actual == self.expected)

    def __save(self, value, force, retval):
        assert force in (True, False)
        assert retval in (True, False)
        try:
            setattr(self._obj, '_' + self._name, value)
        except AttributeError:
            if force:
                raise
        return retval

    def _try_save(self, value, retval=True):
        force = False
        return self.__save(value, force, retval)

    def _save(self, value, retval=True):
        force = True
        return self.__save(value, force, retval)

    def __check_existing(self):
        obj = self._obj
        name = self._name
        actual = self.actual

        check_existing = (
            not hasattr(self, '_check_existing_') or (
                hasattr(self, '_check_existing_') and
                self._check_existing_
            )
        )

        has_existing = (
            hasattr(obj, '_existing') and
            obj._existing
        )

        if not has_existing:
            return

        ex_obj = obj._existing
        existing = getattr(ex_obj, name)
        existing_str = existing
        actual_str = str(actual)

        if isiterable(existing):
            existing_str = ','.join(str(e) for e in existing)

        elif isinstance(existing, datetime.date):
            existing_str = existing.strftime(self._date_format)

        elif isinstance(existing, datetime.datetime):
            existing_str = existing.strftime(self._datetime_format)

        elif not isinstance(existing, str):
            existing_str = str(existing)

        self._existing = existing
        self._existing_str = existing_str

        if not check_existing:
            return

        if not (existing_str != actual_str):
            message = "%s already has a value of '%s'"
            BaseException.__init__(self, message % (name, actual_str))
            raise self

    def _validate(self, new_value):
        self.actual = new_value
        self.__check_existing()
        result = self._test()
        if result not in (True, False):
            raise RuntimeError(
                "invalid return value from %s's validation "
                "routine, expected True/False, got: %s (did "
                "you forget to 'return True'?)" % (self._name, repr(result))
            )

        if not result:
            if hasattr(self, 'message') and self.message:
                message = self.message
                prefix = ''
            else:
                keys = [
                    'expected',
                    'actual'
                ] + sorted(filter(self.__keys, dir(self)))
                f = lambda k: 'got' if k == 'actual' else k
                items = ((f(k), repr(getattr(self, k))) for k in keys)
                message = ', '.join('%s: %s' % (k, v) for (k, v) in items)
                prefix = "%s is invalid: " % self._name
            BaseException.__init__(self, prefix + message)
            raise self

        obj = self._obj
        dst_attr = self.dst_attr or ('_' + self._name)
        if self.dst_value and hasattr(obj, dst_attr):
            setattr(obj, dst_attr, self.dst_value)


#===============================================================================
# Common Invariants
#===============================================================================
class BoolInvariant(Invariant):
    expected = None
    _type = bool
    _metavar = None
    _action = 'store_true'
    def _test(self):
        return True

class StringInvariant(Invariant):
    _type = str
    _type_desc = 'string'
    _maxlen = 1024
    _minlen = 2
    @property
    def expected(self):
        assert isinstance(self._maxlen, int), (self._maxlen,type(self._maxlen))
        assert self._maxlen > 0, self._maxlen
        return '%s with length between %d and %d characters' % (
            self._type_desc,
            self._minlen,
            self._maxlen
        )

    def _test(self):
        if not isinstance(self.actual, self._type):
            return False

        l = len(self.actual)
        return (
            l >= self._minlen and
            l <= self._maxlen
        )

try:
    class UnicodeInvariant(StringInvariant):
        _type = unicode
        _type_desc = 'unicode string'
except NameError:
    UnicodeInvariant = StringInvariant

class PositiveIntegerInvariant(Invariant):
    _type = int
    _min = 1
    _max = None
    expected = "an integer greater than 0"

    def _test(self):
        try:
            i = int(self.actual)
            if self._min:
                assert i >= self._min
            if self._max:
                assert i <= self._max
            return self._try_save(i)
        except:
            return False

class AscendingCSVSepPositiveIntegersInvariant(Invariant):
    _type = str

    expected = (
        "one or more positive integers separated by ',' "
        "in ascending order"
    )

    def _test(self):
        numbers = None
        try:
            numbers = [ int(i) for i in self.actual.split(',') ]
            sorted_numbers = sorted(numbers)
            assert numbers == sorted_numbers
        except (ValueError, AssertionError):
            return False
        assert numbers
        try:
            setattr(self._obj, '_' + self._name, numbers)
        except AttributeError:
            pass
        return True

class NonNegativeIntegerInvariant(Invariant):
    _type = int
    expected = "an integer greater than or equal to 0"

    def _test(self):
        try:
            return (int(self.actual) >= 0)
        except:
            return False

class FloatInvariant(Invariant):
    _type = float
    _min = None
    _max = None
    expected = "a float"

    def _test(self):
        try:
            f = float(self.actual)
            if self._min:
                assert f >= self._min
            if self._max:
                assert f <= self._max
            return True
        except:
            return False

class NonEmptyDictInvariant(Invariant):
    _type = dict
    expected = "a non-empty dict"
    def _test(self):
        try:
            d = self.actual
            return isinstance(d, dict) and d
        except:
            return False

class MonthDayRangeInvariant(StringInvariant):
    _minlen = 3
    _maxlen = 5
    expected = "a month range in the format n-m, i.e. '1-15'"

    def _test(self):
        if not StringInvariant._test(self):
            return False

        try:
            (s, e) = (int(i) for i in self.actual.split('-'))
            assert s < e
            assert s >= 1 and s <= 27
            assert e >= 2 and e <= 31
        except:
            return False

        return True

class SetInvariant(Invariant):
    _type = str
    _expected_fmt = "a member of the following set: %s"

    def _test(self):
        set_str = ', '.join(("'%s'" % s for s in self._set))
        self.expected = self._expected_fmt % set_str
        try:
            self.dst_value = set((self.actual,))
            assert ((self._set & self.dst_value) == self.dst_value)
        except (ValueError, AssertionError):
            return False
        return True

class MultipleSetInvariant(Invariant):
    _type = str
    _expected_fmt = (
        "one or more values (csv separated if more than one) "
        "from the following set: %s"
    )

    def _test(self):
        set_str = ', '.join(("'%s'" % s for s in self._set))
        self.expected = self._expected_fmt % set_str

        try:
            self.dst_value = set(self.actual.split(','))
            assert ((self._set & self.dst_value) == self.dst_value)
        except (ValueError, AssertionError):
            return False
        return True

class PathInvariant(StringInvariant):
    _minlen = 1
    _maxlen = 1024
    _allow_dash = False
    _endswith = None

    @property
    def expected(self):
        if self._endswith:
            return "a valid, existing path ending with '%s'" % self._endswith
        else:
            return "a valid path name"

    def _test(self):
        if not StringInvariant._test(self):
            return False

        if self._endswith and not self.actual.endswith(self._endswith):
            return False

        if self._allow_dash and self.actual == '-':
            return True

        return exists(self.actual)

class YMDPathInvariant(PathInvariant):
    def _test(self):
        dst_name = '_' + self._name + '_ymd'
        assert hasattr(self._obj, dst_name), dst_name
        if not PathInvariant._test(self):
            return False

        path = self.actual
        n = basename(path)
        ix = n.find('2')
        ymd = n[ix:ix+len('yyyy-mm-dd')]
        setattr(self._obj, dst_name, ymd)

        return True

class OutPathInvariant(StringInvariant):
    expected = "a valid path name (path does not have to exist)"
    # If the base directory doesn't exist and _mkdir is True, create the
    # directory.
    _mkdir = True
    _minlen = 1
    _maxlen = 1024

    def _test(self):
        if not StringInvariant._test(self):
            return False

        try:
            path = self.actual
            base = abspath(dirname(path))
            if base:
                if not exists(base) and self._mkdir:
                    os.makedirs(base)
        except:
            return False
        else:
            return True

class MultiplePathsInvariant(StringInvariant):
    expected = "one or more valid path names"
    _minlen = 1
    _maxlen = 1024
    _endswith = None

    def _test(self):
        paths = []
        actual = self.actual.split(',')
        for path in actual:
            path = normpath(abspath(path))
            if not StringInvariant._test(self):
                return False

            if not exists(path):
                return False

            if self._endswith:
                if not endswith(path, self._endswith):
                    return False

            paths.append(path)

        setattr(self._obj, '_' + self._name, paths)
        return True

class DirectoryInvariant(StringInvariant):
    expected = "a valid directory name"
    _minlen = 1
    _maxlen = 1024
    _allow_dash = False

    def _test(self):
        if not StringInvariant._test(self):
            return False

        if self._allow_dash and self.actual == '-':
            return True

        p = abspath(self.actual)
        if not isdir(p):
            return False

        return self._try_save(p)

class ExistingDirectoryInvariant(StringInvariant):
    expected = "an existing directory name"
    _minlen = 1
    _maxlen = 1024

    def _test(self):
        if not StringInvariant._test(self):
            return False

        p = abspath(self.actual)
        if not isdir(self.actual):
            return False

        return self._try_save(p)

class MkDirectoryInvariant(DirectoryInvariant):
    expected = "a valid directory name"
    _minlen = 1
    _maxlen = 1024
    _allow_dash = False

    def _test(self):
        if not DirectoryInvariant._test(self):
            p = abspath(self.actual)
            os.makedirs(p)
        return True

class DateInvariant(Invariant):
    _type_desc = 'datetime'
    _date_format = '%Y-%m-%d'
    _date_format_str = 'YYYY-MM-DD'

    @property
    def expected(self):
        return "a date in the format '%s'" % self._date_format_str

    def _store(self, value):
        attr = '_' + self._name
        setattr(self._obj, attr, value)

    def _test(self):
        fmt = self._date_format
        strptime = lambda d: datetime.datetime.strptime(d, fmt)
        strftime = lambda d: datetime.datetime.strftime(d, fmt)
        try:
            date = strptime(self.actual)
        except ValueError:
            return False

        obj = self._obj


        self._store(datetime.date(date.year, date.month, date.day))
        return True

class EndDateInvariant(DateInvariant):
    def _test(self):
        if not DateInvariant._test(self):
            return False

        obj = self._obj
        start_date = obj._start_date
        end_date= obj._end_date

        if start_date:
            if start_date > end_date:
                self.message = (
                    "end date (%s) is earlier than "
                    "start date (%s)" % (
                        self.actual,
                        start_date.strftime(self._date_format),
                    )
                )
                return False

        return True

#===============================================================================
# Networking Invariants
#===============================================================================
class PortInvariant(PositiveIntegerInvariant):
    _min = 1
    _max = 65535
    expected = "a TCP/IP port (integer between 1 and 65535)"

class NonEphemeralPortInvariant(PortInvariant):
    _min = 1025
    expected = "an non-ephemeral port (i.e. between 1024 and 65535)"

#===============================================================================
# Invariant Aware Object
#===============================================================================
class InvariantDetail(object):
    name = None
    long = None
    type = None
    short = None
    target = None

    @classmethod
    def create_from_invariant(cls, invariant):
        i = invariant
        d = InvariantDetail()
        d.name = i._name


class InvariantAwareObject(object):
    _existing_ = None

    __filter = lambda _, n: (n[0] != '_' and endswith(n, SUFFIXES))
    __convert = lambda _, n: '_'.join(t.lower() for t in n[:-1])
    __pattern = re.compile('[A-Z][^A-Z]*')
    __inner_classes_pattern = re.compile(
        r'    class ([^\s]+(%s))\(.*' % (
            '|'.join(s for s in SUFFIXES)
        )
    )

    def __init__(self, *args, **kwds):
        self._long_opts = kwds.get('long_opts', {})
        self._short_opts = kwds.get('short_opts', {})

        f = self.__filter
        c = self.__convert
        p = self.__pattern

        classes = dict(
            (c(t), getattr(self, n)) for (n, t) in [
                (n, p.findall(n)) for n in filter(f, dir(self))
            ]
        )

        names = dict((v.__name__, k) for (k, v) in classes.items())

        cls = self.__class__
        classname = cls.__name__
        filename = inspect.getsourcefile(cls)
        lines = linecache.getlines(filename)
        lines_len = len(lines)
        prefix = 'class %s(' % classname
        found = False
        for i in range(lines_len):
            line = lines[i]
            if prefix in line:
                found = i
                break

        if not found:
            raise IOError('could not find source code for class')

        block = inspect.getblock(lines[found:])
        text = ''.join(block)
        inner = self.__inner_classes_pattern

        order = [
            (i, names[n[0]]) for (i, n) in enumerate(inner.findall(text))
        ]

        instances = {
            name: cls(self, name)
                for (cls, name) in [
                    (classes[name], name)
                        for (_, name) in order
                ]
        }

        self._invariants = instances
        self._invariant_names = names
        self._invariant_order = order
        self._invariant_classes = classes

        self._invariants_processed = list()

    def __setattr__(self, name, new_value):
        object.__setattr__(self, name, new_value)
        if hasattr(self, '_invariants') and name in self._invariants:
            invariant = self._invariants[name]
            existing = (self._existing_ or (None, None))
            (existing_obj_attr, dependencies) = existing
            if not dependencies or name in dependencies:
                invariant._validate(new_value)
                self._invariants_processed.append(invariant)
                return

            # All necessary dependencies have been set (assuming the class has
            # correctly ordered the invariant classes), so set the object's
            # '_existing' attribute, which signals to future invariants that
            # they are to begin checking existing values during the the normal
            # setattr interception and validation.
            if not self._existing:
                self._existing = getattr(self, existing_obj_attr)

            invariant._validate(new_value)

            # And make a note of the existing value as well as the new value
            # in the 'processed' list.  This is done for convenience of the
            # subclass that may want easy access to the old/new values down
            # the track (i.e. for logging purposes).
            old_value = invariant._existing_str
            self._invariants_processed.append(invariant)


# vim:set ts=8 sw=4 sts=4 tw=78 et:
