import async
import async.services

CHARGEN_IP = '10.211.55.3'
CHARGEN_PORT = 20019

server = async.server(CHARGEN_IP, CHARGEN_PORT)
async.register(transport=server, protocol=async.services.Chargen)
async.run()

