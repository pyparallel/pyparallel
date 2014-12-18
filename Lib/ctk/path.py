#===============================================================================
# Imports
#===============================================================================
import os
import re

#===============================================================================
# Aliases
#===============================================================================
join = os.path.join
abspath = os.path.abspath
dirname = os.path.dirname
normpath = os.path.normpath

#===============================================================================
# Helper Methods
#===============================================================================
def get_files(base):
    res = set()
    for root, dirs, files in os.walk(base):
        for fn in files:
            res.add(join(root, fn)[len(base) + 1:])
        for dn in dirs:
            path = join(root, dn)
            if islink(path):
                res.add(path[len(base) + 1:])
    return res

def get_base_dir(path):
    p = path
    pc = p.count('/')
    assert p and p[0] == '/' and pc >= 1
    if p == '/' or pc == 1 or (pc == 2 and p[-1] == '/'):
        return '/'

    assert pc >= 2
    return dirname(p[:-1] if p[-1] == '/' else p) + '/'

def reduce_path(p):
    assert p and p[0] == '/'
    r = list()
    end = p.rfind('/')
    while end != -1:
        r.append(p[:end+1])
        end = p.rfind('/', 0, end)
    return r

def join_path(*args):
    return abspath(normpath(join(*args)))

def find_all_files_ending_with(dirname, suffix):
    results = []
    for (root, dirs, files) in os.walk(dirname):
        results += [
            join_path(root, file)
                for file in files
                    if file.endswith(suffix)
        ]
    return results

def format_path(path, is_dir=None):
    """
    >>> format_path('src', True)
    '/src/'

    >>> format_path('src', False)
    '/src'

    >>> format_path('src/foo', True)
    '/src/foo/'

    >>> format_path('///src///foo///mexico.txt//', False)
    '/src/foo/mexico.txt'

    >>> format_path('///src///foo///mexico.txt//')
    '/src/foo/mexico.txt/'

    >>> format_path('///src///foo///mexico.txt')
    '/src/foo/mexico.txt'

    >>> format_path(r'\\the\\quick\\brown\\fox.txt', False)
    '/\\\\the\\\\quick\\\\brown\\\\fox.txt'

    >>> format_path('/')
    '/'

    >>> format_path('/', True)
    '/'

    >>> format_path('/', False)
    Traceback (most recent call last):
        ...
    AssertionError

    >>> format_path('/a')
    '/a'

    >>> format_path('/ab')
    '/ab'

    >>> format_path(None)
    Traceback (most recent call last):
        ...
    AssertionError

    >>> format_path('//')
    Traceback (most recent call last):
        ...
    AssertionError

    >>> format_path('/', True)
    '/'

    # On Unix, '\' is a legitimate file name.  Trying to wrangle the right
    # escapes when testing '/' and '\' combinations is an absolute 'mare;
    # so we use ord() instead to compare numerical values of characters.
    >>> _w = lambda p: [ ord(c) for c in p ]
    >>> b = chr(92) # forward slash
    >>> f = chr(47) # backslash
    >>> foo = [102, 111, 111] # ord repr for 'foo'
    >>> b2 = b*2
    >>> _w(format_path('/'+b))
    [47, 92]

    >>> _w(format_path('/'+b2))
    [47, 92, 92]

    >>> _w(format_path('/'+b2, is_dir=False))
    [47, 92, 92]

    >>> _w(format_path('/'+b2, is_dir=True))
    [47, 92, 92, 47]

    >>> _w(format_path(b2*2))
    [47, 92, 92, 92, 92]

    >>> _w(format_path(b2*2, is_dir=True))
    [47, 92, 92, 92, 92, 47]

    >>> _w(format_path('/foo/'+b))
    [47, 102, 111, 111, 47, 92]

    >>> _w(format_path('/foo/'+b, is_dir=False))
    [47, 102, 111, 111, 47, 92]

    >>> _w(format_path('/foo/'+b, is_dir=True))
    [47, 102, 111, 111, 47, 92, 47]

    """
    assert (
        path and
        path not in ('//', '///') and
        is_dir in (True, False, None)
    )

    if path == '/':
        assert is_dir in (True, None)
        return '/'

    p = path
    while True:
        if re.search('//', p):
            p = p.replace('//', '/')
        else:
            break

    if p == '/':
        assert is_dir in (True, None)
        return '/'

    if p[0] != '/':
        p = '/' + p

    if is_dir is True:
        if p[-1] != '/':
            p += '/'
    elif is_dir is False:
        if p[-1] == '/':
            p = p[:-1]

    return p

def format_dir(path):
    return format_path(path, is_dir=True)

def format_file(path):
    return format_path(path, is_dir=False)

def assert_no_file_dir_clash(paths):
    """
    >>> assert_no_file_dir_clash('lskdjf')
    Traceback (most recent call last):
        ...
    AssertionError

    >>> assert_no_file_dir_clash(False)
    Traceback (most recent call last):
        ...
    AssertionError

    >>> assert_no_file_dir_clash(['/src/', '/src/'])
    Traceback (most recent call last):
        ...
    AssertionError

    >>> assert_no_file_dir_clash(['/src', '/src/'])
    Traceback (most recent call last):
        ...
    AssertionError

    >>> assert_no_file_dir_clash(['/sr', '/src/', '/srcb/'])
    >>>

    """
    assert paths and hasattr(paths, '__iter__')
    seen = set()
    for p in paths:
        assert not p in seen
        seen.add(p)

    assert all(
        (p[:-1] if p[-1] == '/' else p + '/') not in seen
            for p in paths
    )


def get_root_path(paths):
    """
    Given a list of paths (directories or files), return the root directory or
    an empty string if no root can be found.

    >>> get_root_path(['/src/', '/src/trunk/', '/src/trunk/test.txt'])
    '/src/'
    >>> get_root_path(['/src/', '/src/trk/', '/src/trk/test.txt', '/src/a'])
    '/src/'
    >>> get_root_path(['/', '/laksdjf', '/lkj'])
    '/'
    >>> get_root_path(['/'])
    '/'
    >>> get_root_path(['/a'])
    '/'
    >>>
    >>> get_root_path(['/src/trunk/foo.txt', '/src/tas/2009.01.00/foo.txt'])
    '/src/'
    >>> get_root_path(['/src/branches/foo/'])
    '/src/branches/foo/'

    >>> get_root_path(['',])
    Traceback (most recent call last):
        ...
    AssertionError

    >>> get_root_path(['lskdjf',])
    Traceback (most recent call last):
        ...
    AssertionError

    >>> get_root_path(['src/trunk/',])
    Traceback (most recent call last):
        ...
    AssertionError

    >>> get_root_path(['/src/trunk/', '/src/trunk'])
    Traceback (most recent call last):
        ...
    AssertionError
    """
    assert (
        hasattr(paths, '__iter__')   and
        all(d and d[0] == '/' for d in paths)
    )

    def _parts(p):
        parts = p.split('/')
        return parts if p[-1] == '/' else parts[:-1]

    paths = [ format_path(p) for p in paths ]
    assert_no_file_dir_clash(paths)

    common = _parts(paths[0])

    for j in range(1, len(paths)):
        parts =  _parts(paths[j])
        for i in range(len(common)):
            if i == len(parts) or common[i] != parts[i]:
                del common[i:]
                break
    if not common or (len(common) == 1 and common[0] == ''):
        return '/'

    return format_dir('/'.join(common))

def relative_path(library_path, target_dir):
    """
    >>> p = 'lib/python2.7/site-packages/rpy2/rinterface/_rinterface.so'
    >>> relative_path(p, 'lib')
    '../../../..'
    >>> relative_path(p, 'lib64/R/lib')
    '../../../../../lib64/R/lib'
    >>> relative_path(p, 'lib/python2.7/site-packages/rpy2/rinterface')
    '.'
    >>> relative_path('lib64/R/bin/exec/R', 'lib64/R/lib')
    '../../lib'
    >>> relative_path('lib64/R/lib/libRblas.so', 'lib64/R/lib')
    '.'
    """
    if dirname(library_path) == target_dir:
        return '.'

    paths = [ format_file(library_path), format_dir(target_dir) ]
    root = get_root_path(paths)

    slashes = library_path.count('/')
    if root != '/':
        slashes -= 1
        suffix = target_dir.replace(root[1:-1], '')
        if suffix:
            slashes -= 1
    else:
        suffix = '/' + target_dir

    return normpath(slashes * '../') + suffix


# vim:set ts=8 sw=4 sts=4 tw=78 et:
