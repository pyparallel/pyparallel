import sys

from win32con import (
    CF_UNICODETEXT,
)

from win32clipboard import (
    OpenClipboard,
    CloseClipboard,
    EmptyClipboard,
    GetClipboardData,
    SetClipboardData,
)

def cb(text=None, fmt=CF_UNICODETEXT, cls=unicode):
    OpenClipboard()
    if not text:
        text = GetClipboardData(fmt)
        CloseClipboard()
        return text
    EmptyClipboard()
    SetClipboardData(fmt, cls(text) if cls else text)
    CloseClipboard()
    sys.stdout.write("copied %d bytes into clipboard...\n" % len(text))


# vim:set ts=8 sw=4 sts=4 tw=78 et:
