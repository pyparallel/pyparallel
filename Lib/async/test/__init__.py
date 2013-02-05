CHARGEN_DATA = [
r""" !"#$%&'()*+,-./0123456789:;<=>?@ABCDEFGHIJKLMNOPQRSTUVWXYZ[\]^_`abcdefg""",
r"""!"#$%&'()*+,-./0123456789:;<=>?@ABCDEFGHIJKLMNOPQRSTUVWXYZ[\]^_`abcdefgh""",
r""""#$%&'()*+,-./0123456789:;<=>?@ABCDEFGHIJKLMNOPQRSTUVWXYZ[\]^_`abcdefghi""",
r"""#$%&'()*+,-./0123456789:;<=>?@ABCDEFGHIJKLMNOPQRSTUVWXYZ[\]^_`abcdefghij""",
r"""$%&'()*+,-./0123456789:;<=>?@ABCDEFGHIJKLMNOPQRSTUVWXYZ[\]^_`abcdefghijk""",
]

QOTD_DATA = b'An apple a day keeps the doctor away.\r\n'

ECHO_PORT     = 7
QOTD_PORT     = 17
DISCARD_PORT  = 9
DAYTIME_PORT  = 13
CHARGEN_PORT  = 19

ECHO_HOST    = 'echo.snakebite.net'
QOTD_HOST    = 'qotd.snakebite.net'
DISCARD_HOST = 'discard.snakebite.net'
DAYTIME_HOST = 'daytime.snakebite.net'
CHARGEN_HOST = 'chargen.snakebite.net'

#SERVICES_IP = socket.getaddrinfo(*ECHO_HOST)[0][4][0]
SERVICES_IP = '10.31.8.61'

ECHO_IP     = SERVICES_IP
QOTD_IP     = SERVICES_IP
DISCARD_IP  = SERVICES_IP
DAYTIME_IP  = SERVICES_IP
CHARGEN_IP  = SERVICES_IP

ECHO_ADDR     = (SERVICES_IP, ECHO_PORT)
QOTD_ADDR     = (SERVICES_IP, QOTD_PORT)
DISCARD_ADDR  = (SERVICES_IP, DISCARD_PORT)
DAYTIME_ADDR  = (SERVICES_IP, DAYTIME_PORT)
CHARGEN_ADDR  = (SERVICES_IP, CHARGEN_PORT)

HOST = '127.0.0.1'
ADDR = (HOST, 0)

