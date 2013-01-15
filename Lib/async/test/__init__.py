CHARGEN = [
r""" !"#$%&'()*+,-./0123456789:;<=>?@ABCDEFGHIJKLMNOPQRSTUVWXYZ[\]^_`abcdefg""",
r"""!"#$%&'()*+,-./0123456789:;<=>?@ABCDEFGHIJKLMNOPQRSTUVWXYZ[\]^_`abcdefgh""",
r""""#$%&'()*+,-./0123456789:;<=>?@ABCDEFGHIJKLMNOPQRSTUVWXYZ[\]^_`abcdefghi""",
r"""#$%&'()*+,-./0123456789:;<=>?@ABCDEFGHIJKLMNOPQRSTUVWXYZ[\]^_`abcdefghij""",
r"""$%&'()*+,-./0123456789:;<=>?@ABCDEFGHIJKLMNOPQRSTUVWXYZ[\]^_`abcdefghijk""",
]

QOTD = b'An apple a day keeps the doctor away.\r\n'

ECHO_HOST    = ('echo.snakebite.net',     7)
QOTD_HOST    = ('qotd.snakebite.net',    17)
DISCARD_HOST = ('discard.snakebite.net',  9)
DAYTIME_HOST = ('daytime.snakebite.net', 13)
CHARGEN_HOST = ('chargen.snakebite.net', 19)

SERVICES_IP = socket.getaddrinfo(*ECHO_HOST)[0][4][0]

ECHO_IP     = (SERVICES_IP,  7)
QOTD_IP     = (SERVICES_IP, 17)
DISCARD_IP  = (SERVICES_IP,  9)
DAYTIME_IP  = (SERVICES_IP, 13)
CHARGEN_IP  = (SERVICES_IP, 19)

NO_CB = None
NO_EB = None

HOST = '127.0.0.1'
ADDR = (HOST, 0)

