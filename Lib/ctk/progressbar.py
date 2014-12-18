# Based off https://github.com/noamraph/tqdm
__all__ = [
    'brogressbar',
    'rowgressbar',
]

import sys
import time

def format_interval(t):
    mins, s = divmod(int(t), 60)
    h, m = divmod(mins, 60)
    if h:
        return '%d:%02d:%02d' % (h, m, s)
    else:
        return '%02d:%02d' % (m, s)


def format_meter(n, total, elapsed):
    # n - number of finished iterations
    # total - total number of iterations, or None
    # elapsed - number of seconds passed since start
    if n > total:
        total = None

    elapsed_str = format_interval(elapsed)
    rate = '%5.2f' % (n / elapsed) if elapsed else '?'

    if total:
        frac = float(n) / total

        N_BARS = 10
        bar_length = int(frac*N_BARS)
        bar = '#'*bar_length + '-'*(N_BARS-bar_length)

        percentage = '%3d%%' % (frac * 100)

        left_str = format_interval(elapsed / n * (total-n)) if n else '?'

        return '|%s| %d/%d %s [elapsed: %s left: %s, %s iters/sec]' % (
            bar, n, total, percentage, elapsed_str, left_str, rate)

    else:
        return '%d [elapsed: %s, %s iters/sec]' % (n, elapsed_str, rate)

def format_bytes_meter(rows_read, bytes_read, total_bytes, elapsed):
    # elapsed - number of seconds passed since start
    if bytes_read > total_bytes:
        total_bytes = None

    elapsed_str = format_interval(elapsed)

    elapsed = float(elapsed)
    bytes_read = float(bytes_read)

    rows_per_sec = (rows_read / elapsed) if elapsed else 0.0
    bytes_per_sec = (bytes_read / elapsed) if elapsed else 0.0
    mb_read = (bytes_read / 1024.0 / 1024.0)
    mb_per_sec = (bytes_per_sec / 1024.0 / 1024.0)

    if total_bytes:
        total_bytes = float(total_bytes)
        mb_total = total_bytes / 1024.0 / 1024.0
        frac = bytes_read / total_bytes

        N_BARS = 20
        bar_length = int(frac*N_BARS)
        bar = '#'*bar_length + '-'*(N_BARS-bar_length)

        percentage_str = '%0.2f%%' % (frac * 100)

        bytes_left = total_bytes - bytes_read
        seconds_left = int(bytes_left / bytes_per_sec) if bytes_per_sec else 0

        left_str = format_interval(seconds_left)

        #import ipdb
        #ipdb.set_trace()
        fmt = (
            '|%s| %s '
            '[%d rows @ %0.2f rows/s, '
            '%0.2fMB @ %0.2f MB/s, '
            'elapsed: %s, left: %s]'
        )
        args = (
            bar,
            percentage_str,
            rows_read,
            rows_per_sec,
            mb_read,
            mb_per_sec,
            elapsed_str,
            left_str,
        )
        return fmt % args

    else:
        fmt = (
            '%0.3fMB %d '
            '[elapsed: %s, %0.2f rows/s, %0.2f MB/s]'
        )
        args = (
            mb_read,
            rows_read,
            elapsed_str,
            rows_per_sec,
            mb_per_sec,
        )
        return fmt % args

def format_rows_meter(rows_read, total_rows, elapsed):
    # elapsed - number of seconds passed since start
    if rows_read > total_rows:
        total_rows = None

    elapsed_str = format_interval(elapsed)

    elapsed = float(elapsed)
    rows_read = float(rows_read)

    rows_per_sec = (rows_read / elapsed) if elapsed else 0.0

    if total_rows:
        total_rows = float(total_rows)
        frac = rows_read / total_rows

        N_BARS = 20
        bar_length = int(frac*N_BARS)
        bar = '#'*bar_length + '-'*(N_BARS-bar_length)

        percentage_str = '%0.2f%%' % (frac * 100)

        rows_left = total_rows - rows_read
        seconds_left = int(rows_left / rows_per_sec) if rows_per_sec else 0

        left_str = format_interval(seconds_left)

        fmt = (
            '|%s| %s '
            '[%d rows @ %0.2f rows/s, '
            'elapsed: %s, left: %s]'
        )
        args = (
            bar,
            percentage_str,
            rows_read,
            rows_per_sec,
            elapsed_str,
            left_str,
        )
        return fmt % args

    else:
        fmt = '%d [elapsed: %s, %0.2f rows/s]'
        args = (int(rows_read), elapsed_str, rows_per_sec)
        return fmt % args


class StatusPrinter(object):
    def __init__(self, file):
        self.file = file
        self.last_printed_len = 0

    def print_status(self, s):
        self.file.write('\r'+s+' '*max(self.last_printed_len-len(s), 0))
        self.file.flush()
        self.last_printed_len = len(s)

def isiterable(i):
    return hasattr(i, '__iter__') or hasattr(i, 'next')

def brogressbar(iterable, desc='', total_bytes=None, leave=False,
                file=sys.stderr, mininterval=0.5, miniters=1):

    try:
        dummy = iterable.bytes_read
    except AttributeError:
        from ctk.util import progressbar as _progressbar
        for obj in _progressbar(iterable):
            yield obj
        raise StopIteration

    if total_bytes is None:
        try:
            total_bytes = len(iterable)
        except TypeError:
            total_bytes = None

    prefix = desc+': ' if desc else ''

    sp = StatusPrinter(file)
    sp.print_status(prefix + format_bytes_meter(0, 0, total_bytes, 0))

    start_t = last_print_t = time.time()
    last_print_n = 0
    n = 0
    rows_read = 0
    bytes_read = 0
    for obj in iterable:
        yield obj
        # Now the object was created and processed, so we can print the meter.
        n += 1
        if isiterable(obj):
            rows_read += len(obj)
        else:
            rows_read += 1
        bytes_read = iterable.bytes_read
        if n - last_print_n >= miniters:
            # We check the counter first, to reduce the overhead of time.time()
            cur_t = time.time()
            if cur_t - last_print_t >= mininterval:
                meter = format_bytes_meter(
                    rows_read,
                    bytes_read,
                    total_bytes,
                    cur_t - start_t,
                )
                sp.print_status(prefix + meter)
                last_print_n = n
                last_print_t = cur_t

    if not leave:
        sp.print_status('')
        sys.stdout.write('\r')
    else:
        if last_print_n < n:
            cur_t = time.time()
            meter = format_bytes_meter(
                rows_read,
                bytes_read,
                total_bytes,
                cur_t - start_t,
            )
            sp.print_status(prefix + meter)
        file.write('\ndone\n')

def rowgressbar(iterable, desc='', total_rows=None, leave=False,
                file=sys.stderr, mininterval=0.5, miniters=1):

    if total_rows is None:
        try:
            total_rows = len(iterable)
        except TypeError:
            total_rows = None

    prefix = desc+': ' if desc else ''

    sp = StatusPrinter(file)
    sp.print_status(prefix + format_rows_meter(0, total_rows, 0))

    start_t = last_print_t = time.time()
    last_print_n = 0
    n = 0
    rows_read = 0
    for obj in iterable:
        yield obj
        # Now the object was created and processed, so we can print the meter.
        n += 1
        if isiterable(obj):
            rows_read += len(obj)
        else:
            rows_read += 1

        if rows_read > total_rows:
            total_rows = rows_read

        if n - last_print_n >= miniters:
            # We check the counter first, to reduce the overhead of time.time()
            cur_t = time.time()
            if cur_t - last_print_t >= mininterval:
                meter = format_rows_meter(
                    rows_read,
                    total_rows,
                    cur_t - start_t,
                )
                sp.print_status(prefix + meter)
                last_print_n = n
                last_print_t = cur_t

    if not leave:
        sp.print_status('')
        sys.stdout.write('\r')
    else:
        cur_t = time.time()
        meter = format_rows_meter(
            rows_read,
            total_rows,
            cur_t - start_t,
        )
        sp.print_status(prefix + meter)
        file.write('\ndone\n')

# vim:set ts=8 sw=4 sts=4 tw=78 et:
