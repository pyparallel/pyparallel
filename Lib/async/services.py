import time
import async

class QOTD:
    initial_bytes_to_send = b'An apple a day keeps the doctor away.\r\n.'

class Daytime:
    def initial_bytes_to_send(self):
        return time.ctime() + '\r\n'

def chargen(lineno, nchars=72):
    # Err, this is broken for a bunch of linenos (e.g. 45927-45956).
    start = ord(' ')
    end = ord('~')
    c = lineno + start
    if c > end:
        c = (c % end) + start
    b = bytearray(nchars)
    for i in range(1, nchars-1):
        if c > end:
            c = start
        b[i] = c
        c += 1

    b[nchars-1] = ord('\n')

    return b

class Chargen:
    #long_lived = True

    def initial_bytes_to_send(self):
        return chargen(0)

    def send_complete(self, transport, send_id):
        return chargen(send_id)

