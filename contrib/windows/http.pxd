
from types cimport *

cdef extern from *:

    cpdef enum:
        ERROR_HANDLE_EOF
        ERROR_INVALID_PARAMETER
        ERROR_DLL_INIT_FAILED

    cpdef enum:
        HTTP_REQUEST_FLAG_MORE_ENTITY_BODY_EXISTS
        #HTTP_REQUEST_IP_ROUTED /* doesn't seem to exist? */
        HTTP_SEND_RESPONSE_FLAG_DISCONNECT
        HTTP_SEND_RESPONSE_FLAG_MORE_DATA
        HTTP_SEND_RESPONSE_FLAG_BUFFER_DATA
        HTTP_SEND_RESPONSE_FLAG_ENABLE_NAGLING
        HTTP_SEND_RESPONSE_FLAG_PROCESS_RANGES
        HTTP_SEND_RESPONSE_FLAG_OPAQUE

    cpdef enum:
        CERT_E_EXPIRED
        CERT_E_UNTRUSTEDCA
        CERT_E_WRONG_USAGE
        CERT_E_UNTRUSTEDROOT
        CERT_E_REVOKED
        CERT_E_CN_NO_MATCH

    cpdef enum:
        HTTP_REQUEST_AUTH_FLAG_TOKEN_FOR_CACHED_CRED

    ctypedef enum HTTP_VERB:
        HttpVerbUnparsed
        HttpVerbUnknown
        HttpVerbInvalid
        HttpVerbOPTIONS
        HttpVerbGET
        HttpVerbHEAD
        HttpVerbPOST
        HttpVerbPUT
        HttpVerbDELETE
        HttpVerbTRACE
        HttpVerbCONNECT
        HttpVerbTRACK
        HttpVerbMOVE
        HttpVerbCOPY
        HttpVerbPROPFIND
        HttpVerbPROPPATCH
        HttpVerbMKCOL
        HttpVerbLOCK
        HttpVerbUNLOCK
        HttpVerbSEARCH
        HttpVerbMaximum
    ctypedef HTTP_VERB *PHTTP_VERB

    ctypedef enum HTTP_HEADER_ID:
        HttpHeaderCacheControl        = 0
        HttpHeaderConnection          = 1
        HttpHeaderDate                = 2
        HttpHeaderKeepAlive           = 3
        HttpHeaderPragma              = 4
        HttpHeaderTrailer             = 5
        HttpHeaderTransferEncoding    = 6
        HttpHeaderUpgrade             = 7
        HttpHeaderVia                 = 8
        HttpHeaderWarning             = 9
        HttpHeaderAllow               = 10
        HttpHeaderContentLength       = 11
        HttpHeaderContentType         = 12
        HttpHeaderContentEncoding     = 13
        HttpHeaderContentLanguage     = 14
        HttpHeaderContentLocation     = 15
        HttpHeaderContentMd5          = 16
        HttpHeaderContentRange        = 17
        HttpHeaderExpires             = 18
        HttpHeaderLastModified        = 19
        HttpHeaderAccept              = 20
        HttpHeaderAcceptCharset       = 21
        HttpHeaderAcceptEncoding      = 22
        HttpHeaderAcceptLanguage      = 23
        HttpHeaderAuthorization       = 24
        HttpHeaderCookie              = 25
        HttpHeaderExpect              = 26
        HttpHeaderFrom                = 27
        HttpHeaderHost                = 28
        HttpHeaderIfMatch             = 29
        HttpHeaderIfModifiedSince     = 30
        HttpHeaderIfNoneMatch         = 31
        HttpHeaderIfRange             = 32
        HttpHeaderIfUnmodifiedSince   = 33
        HttpHeaderMaxForwards         = 34
        HttpHeaderProxyAuthorization  = 35
        HttpHeaderReferer             = 36
        HttpHeaderRange               = 37
        HttpHeaderTe                  = 38
        HttpHeaderTranslate           = 39
        HttpHeaderUserAgent           = 40
        HttpHeaderRequestMaximum      = 41
        HttpHeaderAcceptRanges        = 20
        HttpHeaderAge                 = 21
        HttpHeaderEtag                = 22
        HttpHeaderLocation            = 23
        HttpHeaderProxyAuthenticate   = 24
        HttpHeaderRetryAfter          = 25
        HttpHeaderServer              = 26
        HttpHeaderSetCookie           = 27
        HttpHeaderVary                = 28
        HttpHeaderWwwAuthenticate     = 29
        HttpHeaderResponseMaximum     = 30
        HttpHeaderMaximum             = 41
    ctypedef HTTP_HEADER_ID *PHTTP_HEADER_ID

    ctypedef struct HTTP_TRANSPORT_ADDRESS:
        PSOCKADDR pRemoteAddress
        PSOCKADDR pLocalAddress
    ctypedef HTTP_TRANSPORT_ADDRESS *PHTTP_TRANSPORT_ADDRESS

    cpdef enum HTTP_LOG_DATA_TYPE:
        HttpLogDataTypeFields  = 0
    ctypedef HTTP_LOG_DATA_TYPE *PHTTP_LOG_DATA_TYPE

    cpdef enum HTTP_CACHE_POLICY_TYPE:
        HttpCachePolicyNocache
        HttpCachePolicyUserInvalidates
        HttpCachePolicyTimeToLive

    ctypedef enum HTTP_REQUEST_INFO_TYPE:
        HttpRequestInfoTypeAuth
    ctypedef HTTP_REQUEST_INFO_TYPE *PHTTP_REQUEST_INFO_TYPE

    ctypedef enum HTTP_REQUEST_AUTH_TYPE:
        HttpRequestAuthTypeNone = 0
        HttpRequestAuthTypeBasic
        HttpRequestAuthTypeDigest
        HttpRequestAuthTypeNTLM
        HttpRequestAuthTypeNegotiate
        HttpRequestAuthTypeKerberos
    ctypedef HTTP_REQUEST_AUTH_TYPE *PHTTP_REQUEST_AUTH_TYPE

    ctypedef struct HTTP_COOKED_URL:
        USHORT FullUrlLength
        USHORT HostLength
        USHORT AbsPathLength
        USHORT QueryStringLength
        PCWSTR pFullUrl
        PCWSTR pHost
        PCWSTR pAbsPath
        PCWSTR pQueryString
    ctypedef HTTP_COOKED_URL *PHTTP_COOKED_URL

    ctypedef enum HTTP_AUTH_STATUS:
        HttpAuthStatusSuccess
        HttpAuthStatusNotAuthenticated
        HttpAuthStatusFailure
    ctypedef HTTP_AUTH_STATUS *PHTTP_AUTH_STATUS

    ctypedef struct HTTP_REQUEST_AUTH_INFO:
        HTTP_AUTH_STATUS       AuthStatus
        SECURITY_STATUS        SecStatus
        ULONG                  Flags
        HTTP_REQUEST_AUTH_TYPE AuthType
        HANDLE                 AccessToken
        ULONG                  ContextAttributes
        ULONG                  PackedContextLength
        ULONG                  PackedContextType
        PVOID                  PackedContext
        ULONG                  MutualAuthDataLength
        PCHAR                  pMutualAuthData
    ctypedef HTTP_REQUEST_AUTH_INFO *PHTTP_REQUEST_AUTH_INFO

    ctypedef struct HTTP_KNOWN_HEADER:
        USHORT RawValueLength
        PCSTR  pRawValue
    ctypedef HTTP_KNOWN_HEADER *PHTTP_KNOWN_HEADER

    ctypedef struct HTTP_MULTIPLE_KNOWN_HEADERS:
        HTTP_HEADER_ID     HeaderId
        ULONG              Flags
        USHORT             KnownHeaderCount
        PHTTP_KNOWN_HEADER KnownHeaders
    ctypedef HTTP_MULTIPLE_KNOWN_HEADERS *PHTTP_MULTIPLE_KNOWN_HEADERS

    ctypedef struct HTTP_UNKNOWN_HEADER:
        USHORT NameLength
        USHORT RawValueLength
        PCSTR  pName
        PCSTR  pRawValue
    ctypedef HTTP_UNKNOWN_HEADER *PHTTP_UNKNOWN_HEADER

    ctypedef struct HTTP_LOG_DATA:
        HTTP_LOG_DATA_TYPE  Type
    ctypedef HTTP_LOG_DATA *PHTTP_LOG_DATA

    ctypedef struct HTTP_LOG_FIELDS_DATA:
        HTTP_LOG_DATA Base
        USHORT        UserNameLength
        USHORT        UriStemLength
        USHORT        ClientIpLength
        USHORT        ServerNameLength
        USHORT        ServerIpLength
        USHORT        MethodLength
        USHORT        UriQueryLength
        USHORT        HostLength
        USHORT        UserAgentLength
        USHORT        CookieLength
        USHORT        ReferrerLength
        PWCHAR        UserName
        PWCHAR        UriStem
        PCHAR         ClientIp
        PCHAR         ServerName
        PCHAR         ServiceName
        PCHAR         ServerIp
        PCHAR         Method
        PCHAR         UriQuery
        PCHAR         Host
        PCHAR         UserAgent
        PCHAR         Cookie
        PCHAR         Referrer
        USHORT        ServerPort
        USHORT        ProtocolStatus
        ULONG         Win32Status
        HTTP_VERB     MethodNum
        USHORT        SubStatus
    ctypedef HTTP_LOG_FIELDS_DATA *PHTTP_LOG_FIELDS_DATA

    ctypedef struct HTTP_BYTE_RANGE:
        ULARGE_INTEGER StartingOffset
        ULARGE_INTEGER Length
    ctypedef HTTP_BYTE_RANGE *PHTTP_BYTE_RANGE

    ctypedef struct HTTPAPI_VERSION:
        USHORT HttpApiMajorVersion
        USHORT HttpApiMinorVersion
    ctypedef HTTPAPI_VERSION *PHTTPAPI_VERSION

    ctypedef struct HTTP_CACHE_POLICY:
        HTTP_CACHE_POLICY_TYPE Policy
        ULONG                  SecondsToLive
    ctypedef HTTP_CACHE_POLICY *PHTTP_CACHE_POLICY

    ctypedef enum HTTP_DATA_CHUNK_TYPE:
        HttpDataChunkFromMemory
        HttpDataChunkFromFileHandle
        HttpDataChunkFromFragmentCache
        HttpDataChunkFromFragmentCacheEx
        HttpDataChunkMaximum
    ctypedef HTTP_DATA_CHUNK_TYPE *PHTTP_DATA_CHUNK_TYPE

    ctypedef struct HTTP_DATA_CHUNK:
        HTTP_DATA_CHUNK_TYPE DataChunkType
        PVOID pBuffer
        ULONG BufferLength
        HTTP_BYTE_RANGE ByteRange
        HANDLE          FileHandle
        USHORT FragmentNameLength
        PCWSTR pFragmentName
    ctypedef HTTP_DATA_CHUNK *PHTTP_DATA_CHUNK

    ctypedef struct HTTP_SSL_CLIENT_CERT_INFO:
        ULONG   CertFlags
        ULONG   CertEncodedSize
        PUCHAR  pCertEncoded
        HANDLE  Token
        BOOLEAN CertDeniedByMapper
    ctypedef HTTP_SSL_CLIENT_CERT_INFO *PHTTP_SSL_CLIENT_CERT_INFO

    ctypedef struct HTTP_SSL_INFO:
        USHORT                     ServerCertKeySize
        USHORT                     ConnectionKeySize
        ULONG                      ServerCertIssuerSize
        ULONG                      ServerCertSubjectSize
        PCSTR                      pServerCertIssuer
        PCSTR                      pServerCertSubject
        PHTTP_SSL_CLIENT_CERT_INFO pClientCertInfo
        ULONG                      SslClientCertNegotiated
    ctypedef HTTP_SSL_INFO *PHTTP_SSL_INFO

    ctypedef struct HTTP_REQUEST_HEADERS:
        USHORT               UnknownHeaderCount
        PHTTP_UNKNOWN_HEADER pUnknownHeaders
        USHORT               TrailerCount
        PHTTP_UNKNOWN_HEADER pTrailers
        HTTP_KNOWN_HEADER    KnownHeaders[41]
    ctypedef HTTP_REQUEST_HEADERS *PHTTP_REQUEST_HEADERS

    ctypedef struct HTTP_REQUEST:
        ULONG                  Flags
        HTTP_CONNECTION_ID     ConnectionId
        HTTP_REQUEST_ID        RequestId
        HTTP_URL_CONTEXT       UrlContext
        HTTP_VERSION           Version
        HTTP_VERB              Verb
        USHORT                 UnknownVerbLength
        USHORT                 RawUrlLength
        PCSTR                  pUnknownVerb
        PCSTR                  pRawUrl
        HTTP_COOKED_URL        CookedUrl
        HTTP_TRANSPORT_ADDRESS Address
        HTTP_REQUEST_HEADERS   Headers
        ULONGLONG              BytesReceived
        USHORT                 EntityChunkCount
        PHTTP_DATA_CHUNK       pEntityChunks
        HTTP_RAW_CONNECTION_ID RawConnectionId
        PHTTP_SSL_INFO         pSslInfo
    ctypedef HTTP_REQUEST *PHTTP_REQUEST_V1

    ctypedef struct HTTP_REQUEST_INFO:
        HTTP_REQUEST_INFO_TYPE InfoType
        ULONG                  InfoLength
        PVOID                  pInfo
    ctypedef HTTP_REQUEST_INFO *PHTTP_REQUEST_INFO

    ctypedef struct HTTP_REQUEST_V2:
        # HTTP_REQUEST_V1 start
        ULONG                  Flags
        HTTP_CONNECTION_ID     ConnectionId
        HTTP_REQUEST_ID        RequestId
        HTTP_URL_CONTEXT       UrlContext
        HTTP_VERSION           Version
        HTTP_VERB              Verb
        USHORT                 UnknownVerbLength
        USHORT                 RawUrlLength
        PCSTR                  pUnknownVerb
        PCSTR                  pRawUrl
        HTTP_COOKED_URL        CookedUrl
        HTTP_TRANSPORT_ADDRESS Address
        HTTP_REQUEST_HEADERS   Headers
        ULONGLONG              BytesReceived
        USHORT                 EntityChunkCount
        PHTTP_DATA_CHUNK       pEntityChunks
        HTTP_RAW_CONNECTION_ID RawConnectionId
        PHTTP_SSL_INFO         pSslInfo
        # HTTP_REQUEST_V1 end
        # HTTP_REQUEST_V2 start
        USHORT                 RequestInfoCount
        PHTTP_REQUEST_INFO     pRequestInfo
        # HTTP_REQUEST_V2 end
    ctypedef HTTP_REQUEST_V2 *PHTTP_REQUEST_V2

    ctypedef enum HTTP_RESPONSE_INFO_TYPE:
        HttpResponseInfoTypeMultipleKnownHeaders
        HttpResponseInfoTypeAuthenticationProperty
        HttpResponseInfoTypeQoSProperty
        HttpResponseInfoTypeChannelBind
    ctypedef HTTP_RESPONSE_INFO_TYPE PHTTP_RESPONSE_INFO_TYPE

    ctypedef struct HTTP_RESPONSE_INFO:
        HTTP_RESPONSE_INFO_TYPE Type
        ULONG                   Length
        PVOID                   pInfo
    ctypedef HTTP_RESPONSE_INFO *PHTTP_RESPONSE_INFO

    ctypedef struct HTTP_RESPONSE_HEADERS:
        USHORT               UnknownHeaderCount
        PHTTP_UNKNOWN_HEADER pUnknownHeaders
        USHORT               TrailerCount
        PHTTP_UNKNOWN_HEADER pTrailers
        HTTP_KNOWN_HEADER    KnownHeaders[30]
    ctypedef HTTP_RESPONSE_HEADERS *PHTTP_RESPONSE_HEADERS

    ctypedef struct HTTP_RESPONSE:
        ULONG                 Flags
        HTTP_VERSION          Version
        USHORT                StatusCode
        USHORT                ReasonLength
        PCSTR                 pReason
        HTTP_RESPONSE_HEADERS Headers
        USHORT                EntityChunkCount
        PHTTP_DATA_CHUNK      pEntityChunks
    ctypedef HTTP_RESPONSE *PHTTP_RESPONSE_V1

    ctypedef struct HTTP_RESPONSE_V2:
        # HTTP_RESPONSE_V1 start
        ULONG                 Flags
        HTTP_VERSION          Version
        USHORT                StatusCode
        USHORT                ReasonLength
        PCSTR                 pReason
        HTTP_RESPONSE_HEADERS Headers
        USHORT                EntityChunkCount
        PHTTP_DATA_CHUNK      pEntityChunks
        # HTTP_RESPONSE_V1 end
        # HTTP_RESPONSE_V2 start
        USHORT              ResponseInfoCount
        PHTTP_RESPONSE_INFO pResponseInfo
        # HTTP_RESPONSE_V2 end
    ctypedef HTTP_RESPONSE_V2 *PHTTP_RESPONSE_V2
    ctypedef HTTP_RESPONSE_V2 *PHTTP_RESPONSE

    ULONG HttpReceiveRequestEntityBody(
        HANDLE          ReqQueueHandle,
        HTTP_REQUEST_ID RequestId,
        ULONG           Flags,
        PVOID           pBuffer,
        ULONG           BufferLength,
        PULONG          pBytesReceived,
        LPOVERLAPPED    pOverlapped
    )
    ctypedef ULONG (*LPFN_HttpReceiveRequestEntityBody)(
        HANDLE          ReqQueueHandle,
        HTTP_REQUEST_ID RequestId,
        ULONG           Flags,
        PVOID           pBuffer,
        ULONG           BufferLength,
        PULONG          pBytesReceived,
        LPOVERLAPPED    pOverlapped
    )

    ULONG HttpSendHttpResponse(
        HANDLE             ReqQueueHandle,
        HTTP_REQUEST_ID    RequestId,
        ULONG              Flags,
        PHTTP_RESPONSE     pHttpResponse,
        PHTTP_CACHE_POLICY pCachePolicy,
        PULONG             pBytesSent,
        PVOID              pReserved2,
        ULONG              Reserved3,
        LPOVERLAPPED       pOverlapped,
        PHTTP_LOG_DATA      pLogData
    )
    ctypedef ULONG (*LPFN_HttpSendHttpResponse)(
        HANDLE             ReqQueueHandle,
        HTTP_REQUEST_ID    RequestId,
        ULONG              Flags,
        PHTTP_RESPONSE     pHttpResponse,
        PHTTP_CACHE_POLICY pCachePolicy,
        PULONG             pBytesSent,
        PVOID              pReserved2,
        ULONG              Reserved3,
        LPOVERLAPPED       pOverlapped,
        PHTTP_LOG_DATA     pLogData
    )

# vim:set ts=8 sw=4 sts=4 tw=80 et nospell:                                    #
