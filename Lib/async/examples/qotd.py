import async

class QOTD:
    initial_bytes_to_send = b'An apple a day keeps the doctor away.\r\n.'

server = async.server('10.211.55.3', 20019)
async.register(transport=server, protocol=QOTD)
async.run()

