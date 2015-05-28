/* Socket module header file */

/* Includes needed for the sockaddr_* symbols below */
#ifndef MS_WINDOWS
#ifdef __VMS
#   include <socket.h>
# else
#   include <sys/socket.h>
# endif
# include <netinet/in.h>
# if !(defined(__CYGWIN__) || (defined(PYOS_OS2) && defined(PYCC_VACPP)))
#  include <netinet/tcp.h>
# endif

#else /* MS_WINDOWS */
# include <winsock2.h>
# include <ws2tcpip.h>
# ifdef WITH_PARALLEL
#  include <MSWSock.h>
# endif
/* VC6 is shipped with old platform headers, and does not have MSTcpIP.h
 * Separate SDKs have all the functions we want, but older ones don't have
 * any version information.
 * I use SIO_GET_MULTICAST_FILTER to detect a decent SDK.
 */
# ifdef SIO_GET_MULTICAST_FILTER
#  include <MSTcpIP.h> /* for SIO_RCVALL */
#  define HAVE_ADDRINFO
#  define HAVE_SOCKADDR_STORAGE
#  define HAVE_GETADDRINFO
#  define HAVE_GETNAMEINFO
#  define ENABLE_IPV6
# else
typedef int socklen_t;
# endif /* IPPROTO_IPV6 */
#endif /* MS_WINDOWS */

#ifdef HAVE_SYS_UN_H
# include <sys/un.h>
#else
# undef AF_UNIX
#endif

#ifdef HAVE_LINUX_NETLINK_H
# ifdef HAVE_ASM_TYPES_H
#  include <asm/types.h>
# endif
# include <linux/netlink.h>
#else
#  undef AF_NETLINK
#endif

#ifdef HAVE_BLUETOOTH_BLUETOOTH_H
#include <bluetooth/bluetooth.h>
#include <bluetooth/rfcomm.h>
#include <bluetooth/l2cap.h>
#include <bluetooth/sco.h>
#include <bluetooth/hci.h>
#endif

#ifdef HAVE_BLUETOOTH_H
#include <bluetooth.h>
#endif

#ifdef HAVE_NET_IF_H
# include <net/if.h>
#endif

#ifdef HAVE_NETPACKET_PACKET_H
# include <sys/ioctl.h>
# include <netpacket/packet.h>
#endif

#ifdef HAVE_LINUX_TIPC_H
# include <linux/tipc.h>
#endif

#ifdef HAVE_LINUX_CAN_H
#include <linux/can.h>
#endif

#ifdef HAVE_LINUX_CAN_RAW_H
#include <linux/can/raw.h>
#endif

#ifdef HAVE_SYS_SYS_DOMAIN_H
#include <sys/sys_domain.h>
#endif
#ifdef HAVE_SYS_KERN_CONTROL_H
#include <sys/kern_control.h>
#endif

#ifndef Py__SOCKET_H
#define Py__SOCKET_H
#ifdef __cplusplus
extern "C" {
#endif

/* Python module and C API name */
#define PySocket_MODULE_NAME    "_socket"
#define PySocket_CAPI_NAME      "CAPI"
#define PySocket_CAPSULE_NAME   PySocket_MODULE_NAME "." PySocket_CAPI_NAME

/* Abstract the socket file descriptor type */
#ifdef MS_WINDOWS
typedef SOCKET SOCKET_T;
#       ifdef MS_WIN64
#               define SIZEOF_SOCKET_T 8
#       else
#               define SIZEOF_SOCKET_T 4
#       endif
#else
typedef int SOCKET_T;
#       define SIZEOF_SOCKET_T SIZEOF_INT
#endif

#ifdef WITH_PARALLEL
#ifdef MS_WINDOWS
static LPFN_ACCEPTEX _AcceptEx;
static LPFN_CONNECTEX _ConnectEx;
static LPFN_WSARECVMSG _WSARecvMsg;
static LPFN_WSASENDMSG _WSASendMsg;
static LPFN_DISCONNECTEX _DisconnectEx;
static LPFN_TRANSMITFILE _TransmitFile;
static LPFN_TRANSMITPACKETS _TransmitPackets;
static LPFN_GETACCEPTEXSOCKADDRS _GetAcceptExSockaddrs;

const static GUID _AcceptEx_GUID = WSAID_ACCEPTEX;
const static GUID _ConnectEx_GUID = WSAID_CONNECTEX;
const static GUID _WSARecvMsg_GUID = WSAID_WSARECVMSG;
const static GUID _WSASendMsg_GUID = WSAID_WSASENDMSG;
const static GUID _DisconnectEx_GUID = WSAID_DISCONNECTEX;
const static GUID _TransmitFile_GUID = WSAID_TRANSMITFILE;
const static GUID _TransmitPackets_GUID = WSAID_TRANSMITPACKETS;
const static GUID _GetAcceptExSockaddrs_GUID = WSAID_GETACCEPTEXSOCKADDRS;

static RIO_EXTENSION_FUNCTION_TABLE _rio = { 0, };
const static GUID _rio_GUID = WSAID_MULTIPLE_RIO;

#endif /* MS_WINDOWS */
#endif /* WITH_PARALLEL */

#if SIZEOF_SOCKET_T <= SIZEOF_LONG
#define PyLong_FromSocket_t(fd) PyLong_FromLong((SOCKET_T)(fd))
#define PyLong_AsSocket_t(fd) (SOCKET_T)PyLong_AsLong(fd)
#else
#define PyLong_FromSocket_t(fd) PyLong_FromLongLong((SOCKET_T)(fd))
#define PyLong_AsSocket_t(fd) (SOCKET_T)PyLong_AsLongLong(fd)
#endif

/* Socket address */
typedef union sock_addr {
    struct sockaddr_in in;
    struct sockaddr sa;
#ifdef AF_UNIX
    struct sockaddr_un un;
#endif
#ifdef AF_NETLINK
    struct sockaddr_nl nl;
#endif
#ifdef ENABLE_IPV6
    struct sockaddr_in6 in6;
    struct sockaddr_storage storage;
#endif
#ifdef HAVE_BLUETOOTH_BLUETOOTH_H
    struct sockaddr_l2 bt_l2;
    struct sockaddr_rc bt_rc;
    struct sockaddr_sco bt_sco;
    struct sockaddr_hci bt_hci;
#endif
#ifdef HAVE_NETPACKET_PACKET_H
    struct sockaddr_ll ll;
#endif
#ifdef HAVE_LINUX_CAN_H
    struct sockaddr_can can;
#endif
#ifdef HAVE_SYS_KERN_CONTROL_H
    struct sockaddr_ctl ctl;
#endif
} sock_addr_t;

/* The object holding a socket.  It holds some extra information,
   like the address family, which is used to decode socket address
   arguments properly. */

typedef struct {
    PyObject_HEAD
    SOCKET_T sock_fd;           /* Socket file descriptor */
    int sock_family;            /* Address family, e.g., AF_INET */
    int sock_type;              /* Socket type, e.g., SOCK_STREAM */
    int sock_proto;             /* Protocol type, usually 0 */
    PyObject *(*errorhandler)(void); /* Error handler; checks
                                        errno, returns NULL and
                                        sets a Python exception */
    double sock_timeout;                 /* Operation timeout in seconds;
                                        0.0 means non-blocking */
#ifdef WITH_PARALLEL
    int sock_backlog;           /* Backlog specified to listen(n). Used for
                                   pre-allocating sockets for AcceptEx when
                                   on Windows. */
#endif
} PySocketSockObject;

/* --- C API ----------------------------------------------------*/

/* Short explanation of what this C API export mechanism does
   and how it works:

    The _ssl module needs access to the type object defined in
    the _socket module. Since cross-DLL linking introduces a lot of
    problems on many platforms, the "trick" is to wrap the
    C API of a module in a struct which then gets exported to
    other modules via a PyCapsule.

    The code in socketmodule.c defines this struct (which currently
    only contains the type object reference, but could very
    well also include other C APIs needed by other modules)
    and exports it as PyCapsule via the module dictionary
    under the name "CAPI".

    Other modules can now include the socketmodule.h file
    which defines the needed C APIs to import and set up
    a static copy of this struct in the importing module.

    After initialization, the importing module can then
    access the C APIs from the _socket module by simply
    referring to the static struct, e.g.

    Load _socket module and its C API; this sets up the global
    PySocketModule:

    if (PySocketModule_ImportModuleAndAPI())
        return;


    Now use the C API as if it were defined in the using
    module:

    if (!PyArg_ParseTuple(args, "O!|zz:ssl",

                          PySocketModule.Sock_Type,

                          (PyObject*)&Sock,
                          &key_file, &cert_file))
        return NULL;

    Support could easily be extended to export more C APIs/symbols
    this way. Currently, only the type object is exported,
    other candidates would be socket constructors and socket
    access functions.

*/

/* C API for usage by other Python modules */
typedef struct {
    PyTypeObject *Sock_Type;
    PyObject *error;
    PyObject *timeout_error;
#ifdef WITH_PARALLEL
    int (*getsockaddrarg)(PySocketSockObject *s,
                          PyObject *args,
                          struct sockaddr *addr_ret,
                          int *len_ret);
    int (*getsockaddrlen)(PySocketSockObject *s, socklen_t *len_ret);
    PyObject *(*makesockaddr)(SOCKET_T sockfd,
                              struct sockaddr *addr,
                              size_t addrlen,
                              int proto);
    PyObject *(*socket_errorhandler)(void);
    PyObject *(*host_errorhandler)(int);
    PyObject *(*gai_errorhandler)(int);
#ifdef MS_WINDOWS
    LPFN_ACCEPTEX AcceptEx;
    LPFN_CONNECTEX ConnectEx;
    LPFN_WSARECVMSG WSARecvMsg;
    LPFN_WSASENDMSG WSASendMsg;
    LPFN_DISCONNECTEX DisconnectEx;
    LPFN_TRANSMITFILE TransmitFile;
    LPFN_TRANSMITPACKETS TransmitPackets;
    LPFN_GETACCEPTEXSOCKADDRS GetAcceptExSockaddrs;

    RIO_EXTENSION_FUNCTION_TABLE rio;
#else /* MS_WINDOWS */
    void (*null01)(void); /* LPFN_ACCEPTEX             */
    void (*null02)(void); /* LPFN_CONNECTEX            */
    void (*null03)(void); /* LPFN_WSARECVMSG           */
    void (*null04)(void); /* LPFN_WSASENDMSG           */
    void (*null05)(void); /* LPFN_DISCONNECTEX         */
    void (*null06)(void); /* LPFN_TRANSMITFILE         */
    void (*null07)(void); /* LPFN_TRANSMITPACKETS      */
    void (*null08)(void); /* LPFN_GETACCEPTEXSOCKADDRS */
    // Erm, embedded the RIO struct is going to blow things up.
    void (*null09)(void); /* RIO_EXTENSION_FUNCTION_TABLE */
#endif /* MS_WINDOWS */
#endif /* WITH_PARALLEL */
} PySocketModule_APIObject;

#define PySocketModule_ImportModuleAndAPI() PyCapsule_Import(PySocket_CAPSULE_NAME, 1)

/* Convert "sock_addr_t *" to "struct sockaddr *". */
#define SAS2SA(x)       (&((x)->sa))

#ifdef __cplusplus
}
#endif
#endif /* !Py__SOCKET_H */
