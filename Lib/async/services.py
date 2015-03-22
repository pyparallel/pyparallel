import time
import socket
import datetime

import async
from _async import transport

class Disconnect:
    pass

class Discard:
    def data_received(self, transport, data):
        pass

class Daytime:
    """
    Send a string representation of the current time, then disconnect."
    """
    def initial_bytes_to_send(self):
        return time.ctime() + r'\r\n'

class Time:
    """
    Send a 32-bit unsigned integer in binary format and network byte order,
    representing the number of seconds since 00:00 (midnight) January 1st,
    1900 GMT, then close the connection.
    """
    def initial_bytes_to_send(self):
        delta = datetime.utcnow() - datetime(1900, 1, 1)
        return bytes(socket.htonl(int(delta.total_seconds())))

class StaticQotd:
    """
    Send a static value as soon as a client connects, then disconnect.
    """
    initial_bytes_to_send = b'An apple a day keeps the doctor away.\r\n'

class DynamicQotd:
    """
    Send a dynamic (value is computed just before sending) string to a client,
    then disconnect.
    """
    def initial_bytes_to_send(self):
        return b'An apple a day keeps the doctor away.\r\n'

class EchoData:
    def data_received(self, transport, data):
        return data

class TimestampData:
    def data_received(self, transport, data):
        return '%d%s' % (async.rdtsc(), '\r\n')

class EchoUpperData:
    def data_received(self, transport, data):
        a = ord('a')
        z = ord('z')
        nchars = len(data)
        b = bytearray(nchars)
        for i in range(0, nchars):
            c = data[i]
            if c >= a and c <= z:
                c -= 32
            b[i] = c

        return b

class EchoLine:
    line_mode = True
    def line_received(self, transport, line):
        return line

def chargen(lineno, nchars=72):
    start = ord(' ')
    end = ord('~')
    c = lineno + start
    while c > end:
        c = (c % end) + start
    b = bytearray(nchars)
    for i in range(0, nchars-2):
        if c > end:
            c = start
        b[i] = c
        c += 1

    b[nchars-1] = ord('\n')

    return b

class Chargen:
    def initial_bytes_to_send(self):
        return chargen(0)

    def send_complete(self, transport, send_id):
        return chargen(send_id)

