import async

QOTD_IP = '10.211.55.3'
QOTD_PORT = 20017
QOTD_DATA = b'An apple a day keeps the doctor away.\r\n'

server = async.server(QOTD_IP, QOTD_PORT)
async.register(transport=server, protocol=async.QOTD)
async.run()

