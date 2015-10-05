#===============================================================================
# Imports
#===============================================================================
import inspect

# We need datetime.timedelta for threadpool time deltas.
import datetime

#===============================================================================
# Aliases
#===============================================================================
isfunction = inspect.isfunction

#===============================================================================
# Helpers
#===============================================================================

def get_methods_for_class(class_or_obj):
    """
    Returns a list of method names defined only in the given class_or_obj input
    variable (i.e. methods inherited from other objects are not returned).

    (XXX TODO: tweak the doctest so that the 0x00... addresses don't make it
    barf.)

    >>> class A:
    ...     def foo(self): pass
    >>> class B:
    ...     def bar(self): pass
    >>> get_methods_for_class(B)
    [('bar', '<function B.bar at ...>')]
    >>> b = B()
    >>> get_methods_for_class(B)
    [('bar', '<function B.bar at ...>')]
    """
    if isinstance(class_or_obj, type):
        cls = class_or_obj
    else:
        cls = class_or_obj.__class__
    classname = cls.__name__
    def predicate(v):
        if not isfunction(v):
            return
        qualname = v.__qualname__
        ix2 = qualname.rfind('.')
        ix1 = qualname.rfind('.', 0, ix2-1)
        clsname = qualname[ix1+1:ix2]
        return clsname == classname
    return inspect.getmembers(cls, predicate=predicate)

# vim:set ts=8 sw=4 sts=4 tw=80 et                                             :
