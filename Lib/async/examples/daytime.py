import time
import async

class Daytime:
    def initial_bytes_to_send(self):
        return time.ctime() + '\r\n'

server = async.server('10.211.55.3', 20019)
async.register(transport=server, protocol=Daytime)
async.run()

