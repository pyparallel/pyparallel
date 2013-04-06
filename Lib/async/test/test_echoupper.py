import async
from async.services import EchoUpperData

server = async.server('10.211.55.3', 20007)
async.register(transport=server, protocol=EchoUpperData)
async.run()

