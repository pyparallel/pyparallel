import async
import async.services

QOTD_IP = '10.211.55.3'
QOTD_PORT = 20017
QOTD_DATA = b'An apple a day keeps the doctor away.\r\n'

server = async.server('10.211.55.3', 20013)
async.register(transport=server, protocol=async.services.Daytime)
async.run()

