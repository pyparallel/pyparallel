"""
Logic library: contains a logical mutual exclusion class (Mutex) that is
useful for structuring complex logic permutations in a robust fashion.
"""

#===============================================================================
# Classes
#===============================================================================
class Break:
    pass

class Mutex(object):
    """
    Capture a set of mutually-exclusive logic conditions.


    Doctests
    ========

    Only one attribute can be set to true:
        >>> m = Mutex()
        >>> m.foo = True
        >>> m.bar = True
        Traceback (most recent call last):
            ...
        AssertionError

    An attribute can only be set to True once:
        >>> m = Mutex()
        >>> m.foo = True
        >>> m.foo = True
        Traceback (most recent call last):
            ...
        AssertionError

    An attribute can only be set to False once:
        >>> m = Mutex()
        >>> m.foo = False
        >>> m.foo = False
        Traceback (most recent call last):
            ...
        AssertionError

    Testing can't begin until we've received our True attribute:
        >>> m = Mutex()
        >>> m.tomcat = False
        >>> with m as f:
        ...     f.tomcat
        Traceback (most recent call last):
            ...
        AssertionError

        >>> m = Mutex()
        >>> m.viper = False
        >>> m.eagle = True
        >>> with m as f:
        ...     f.viper
        ...     f.eagle
        False
        True

    You can't access attributes that haven't been set:
        >>> m = Mutex()
        >>> m.eagle = True
        >>> m.raptor
        Traceback (most recent call last):
            ...
        AssertionError
        >>> m = Mutex()
        >>> m.eagle = True
        >>> with m as f:
        ...     f.tomcat
        Traceback (most recent call last):
            ...
        AssertionError

    Once testing's begun, no more attribute assignment is allowed:
        >>> m = Mutex()
        >>> m.viper = False
        >>> m.eagle = True
        >>> with m as f:
        ...     f.tomcat = False
        Traceback (most recent call last):
            ...
        AssertionError

    Testing's finished once the True attribute has been accessed (if it hasn't
    been accessed, bomb out):
        >>> m = Mutex()
        >>> m.viper = True
        >>> with m as f:
        ...     f.viper
        True

        >>> m = Mutex()
        >>> m.viper = True
        >>> m.eagle = False
        >>> with m as f:
        ...     f.eagle
        Traceback (most recent call last):
            ...
        AssertionError

    Once testing's finished, no more attribute assignment or access is allowed
    (for any/all attributes):

        >>> m = Mutex()
        >>> m.viper = True
        >>> m.eagle = False
        >>> with m as f:
        ...     f.viper
        True
        >>> m.viper
        Traceback (most recent call last):
            ...
        AssertionError

    No assignment allowed after testing has completed:
        >>> m = Mutex()
        >>> m.viper = True
        >>> with m as f:
        ...     f.viper
        True
        >>> m.hornet = False
        Traceback (most recent call last):
            ...
        AssertionError

    No accessing attributes outside the context of an, er, context manager:
        >>> m = Mutex()
        >>> m.viper  = True
        >>> m.hornet = False
        >>> m.hornet
        Traceback (most recent call last):
            ...
        AssertionError
        >>> m = Mutex()
        >>> m.viper  = True
        >>> m.viper
        Traceback (most recent call last):
            ...
        AssertionError
        >>> m.viper = False
        Traceback (most recent call last):
            ...
        AssertionError

    A false attribute can only be accessed once during testing:
        >>> m = Mutex()
        >>> m.viper  = True
        >>> m.hornet = False
        >>> with m as f:
        ...     f.hornet
        ...     f.hornet
        Traceback (most recent call last):
            ...
        AssertionError

    If you haven't accessed the True attribute... you'll find out about it
    during the with statement's closure:

        >>> m = Mutex()
        >>> m.eagle  = False
        >>> m.viper  = True
        >>> with m as f:
        ...     m.eagle
        Traceback (most recent call last):
            ...
        AssertionError

        >>> m = Mutex()
        >>> m.eagle  = False
        >>> m.hornet = False
        >>> m.viper  = True
        >>> with m as f:
        ...     m.eagle
        ...     m.hornet
        Traceback (most recent call last):
            ...
        AssertionError

        >>> m = Mutex()
        >>> m.eagle  = False
        >>> m.hornet = False
        >>> m.viper  = True
        >>> with m as f:
        ...     m.viper
        ...     m.hornet
        Traceback (most recent call last):
            ...
        AssertionError

    Check that we can unlock after a successful run and that there are no
    restrictions on how many times we can access an element (make sure we
    still can't set stuff, though):

        >>> m = Mutex()
        >>> m.viper = False
        >>> m.eagle = True
        >>> with m as f:
        ...     f.viper
        ...     f.eagle
        False
        True
        >>> m._unlock()
        >>> m.viper
        False
        >>> m.eagle
        True
        >>> m.viper
        False
        >>> m.tomcat = False
        Traceback (most recent call last):
            ...
        AssertionError

    Test our ability to reset after unlocking:
        >>> m = Mutex()
        >>> m.viper = False
        >>> m.eagle = True
        >>> with m as f:
        ...     f.viper
        ...     f.eagle
        False
        True
        >>> m._unlock()
        >>> m.viper
        False
        >>> m.eagle
        True
        >>> m._reset()
        >>> m.hornet = False
        >>> with m as f:
        ...     m.hornet
        ...     m.viper
        ...     m.eagle
        False
        False
        True
        >>> m.hornet
        Traceback (most recent call last):
            ...
        AssertionError
        >>> m._unlock()
        >>> m.hornet
        False
        >>> m._unlock()
        Traceback (most recent call last):
            ...
        AssertionError

    Test our ability to set an error message when invalid inputs are used.
        >>> m = Mutex()
        >>> m._set_invalid_setup_error_msg('foobar')
        >>> m.viper = False
        >>> m.hornet = False
        >>> with m as f:
        ...     m.viper
        Traceback (most recent call last):
            ...
        RuntimeError: foobar

    Test raising BreakWith cleanly exits the context.

        >>> m = Mutex()
        >>> m.viper = True
        >>> m.eagle = False
        >>> with m as f:
        ...     f.viper
        ...     raise Break
        ...     f.eagle
        True
    """
    def __init__(self):
        self.__d = dict()
        self.__s = set()
        self.__invalid_setup_error_msg = None
        self._mode = 'setup'
        self._name = None
        self._have_true = False
        self._have_started = False

    @property
    def _is_setup(self):
        return (self._mode == 'setup')

    @property
    def _is_test(self):
        return (self._mode == 'test')

    @property
    def _is_end(self):
        return (self._mode == 'end')

    @property
    def _is_exit(self):
        return (self._mode == 'exit')

    @property
    def _is_unlocked(self):
        return (self._mode == 'unlocked')

    def __getattr__(self, name):
        assert name[0] != '_'

        d = self.__d
        s = self.__s

        if self._is_unlocked:
            return d[name]

        assert self._is_test

        assert name in s
        s.remove(name)
        value = d[name]
        if value == True:
            self._mode = 'end'
        return value

    def __setattr__(self, name, value):
        if name[0] == '_':
            return object.__setattr__(self, name, value)

        assert self._is_setup
        assert isinstance(value, bool), (value, type(value))
        d = self.__d
        s = self.__s
        assert name not in s
        if value == True:
            assert not self._have_true
            self._have_true = True
            self._name = name
        d[name] = value
        s.add(name)

    def _peek(self, name):
        return (self.__d.get(name, False) is True)

    def __enter__(self):
        assert self._is_setup
        if not self._have_true:
            if self.__invalid_setup_error_msg is not None:
                raise RuntimeError(self.__invalid_setup_error_msg)
        assert self._have_true
        self._mode = 'test'
        return self

    def __exit__(self, *exc_info):
        clean_exit = (
            not exc_info or
            exc_info == (None, None, None) or
            exc_info[0] == Break
        )

        if clean_exit:
            assert self._is_end
            self._mode = 'exit'
            return True

    def __repr__(self):
        return self._name

    def _unlock(self):
        assert self._is_exit
        self._mode = 'unlocked'

    def _set_invalid_setup_error_msg(self, msg):
        assert self._is_setup
        assert self.__invalid_setup_error_msg is None
        self.__invalid_setup_error_msg = msg

    def _reset(self):
        assert self._is_exit or self._is_unlocked
        self.__s = set(self.__d.keys())
        self._mode = 'setup'

# vim:set ts=8 sw=4 sts=4 expandtab tw=80:
