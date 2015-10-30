
from types cimport *

cdef extern from *:

    SECURITY_STATUS SEC_Entry AcceptSecurityContext(
        PCredHandle    phCredential,
        PCtxtHandle    phContext,
        PSecBufferDesc pInput,
        ULONG          fContextReq,
        ULONG          TargetDataRep,
        PCtxtHandle    phNewContext,
        PSecBufferDesc pOutput,
        PULONG         pfContextAttr,
        PTimeStamp     ptsTimeStamp
    )
    ctypedef SECURITY_STATUS (SEC_Entry *LPFN_AcceptSecurityContext)(
        PCredHandle    phCredential,
        PCtxtHandle    phContext,
        PSecBufferDesc pInput,
        ULONG          fContextReq,
        ULONG          TargetDataRep,
        PCtxtHandle    phNewContext,
        PSecBufferDesc pOutput,
        PULONG         pfContextAttr,
        PTimeStamp     ptsTimeStamp
    )


# vim:set ts=8 sw=4 sts=4 tw=80 et nospell:                                    #
