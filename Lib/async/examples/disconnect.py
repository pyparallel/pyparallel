import time
import async

class Disconnect:
    pass

server = async.server('10.211.55.3', 20019)
async.register(transport=server, protocol=Disconnect)
async.run()

