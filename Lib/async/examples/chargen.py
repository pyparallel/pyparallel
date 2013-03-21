import async

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

server = async.server('10.211.55.3', 20019)
async.register(transport=server, protocol=Chargen)
async.run()
