import async
#import pdb
QOTD_IP = '10.211.55.3'
QOTD_PORT = 20017
QOTD_DATA = b'An apple a day keeps the doctor away.\r\n'

protocol = async.ChattyLineProtocol
#dbg = pdb.Pdb()
transport = async.client(QOTD_IP, QOTD_PORT)
#dbg.set_trace()
async.register(transport=transport, protocol=async.ChattyLineProtocol)
async.run()
#async.wait(transport)
