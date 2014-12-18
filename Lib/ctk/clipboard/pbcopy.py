import sys

copy_cmd = None
paste_cmd = None

if sys.platform == 'darwin':
    (copy_cmd, paste_cmd) = ('pbcopy', 'pbpaste')

assert copy_cmd and paste_cmd

import cStringIO as StringIO
from subprocess import Popen, PIPE

def copy(text):
    p = Popen([copy_cmd], stdin=PIPE)
    p.stdin.write(text)
    p.stdin.close()
    p.wait()
    sys.stdout.write('copied %d bytes into clipboard...' % len(text))

def paste():
    p = Popen([paste_cmd], stdout=PIPE)
    p.wait()
    return p.stdout.read()

def cb(text=None):
    if not text:
        return paste()
    else:
        copy(text)

# vim:set ts=8 sw=4 sts=4 tw=78 et:
