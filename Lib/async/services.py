import time
import async

class QOTD:
    initial_bytes_to_send = b'An apple a day keeps the doctor away.\r\n.'

class Daytime:
    def initial_bytes_to_send(self):
        return time.ctime() + '\r\n'

def chargen(lineno, nchars=72):
    start = ord(' ')
    end = ord('~')
    c = lineno + start
    if c > end:
        c = (c % end) + start
    b = bytearray(nchars)
    for i in range(0, nchars):
        if c > end:
            c = start
        b[i] = c
        c += 1

    return b

class Chargen:
    #long_lived = True

    def initial_bytes_to_send(self):
        return chargen(0)

    def send_complete(self, transport, send_id):
        async.stdout("send_id: %d" % send_id)
        return chargen(send_id)

