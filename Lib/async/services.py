import time
import async

class Disconnect:
    pass

class Discard:
    def data_received(self, data):
        pass

class QOTD:
    initial_bytes_to_send = b'An apple a day keeps the doctor away.\r\n.'


class EchoData:
    def data_received(self, data):
        return data

class EchoLine:
    line_mode = True
    def line_received(self, line):
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

