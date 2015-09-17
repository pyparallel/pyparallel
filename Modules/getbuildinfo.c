#define GETBUILDINFO
#include "Python.h"

#ifndef DONT_HAVE_STDIO_H
#include <stdio.h>
#endif

#ifndef DATE
#ifdef __DATE__
#define DATE __DATE__
#else
#define DATE "xx/xx/xx"
#endif
#endif

#ifndef TIME
#ifdef __TIME__
#define TIME __TIME__
#else
#define TIME "xx:xx:xx"
#endif
#endif

#ifdef GITTAG

const char *
_Py_gitversion(void)
{
    return GITVERSION;
}

const char *
_Py_gitidentifier(void)
{
    return GITTAG;
}

const char *
_Py_version(void)
{
    return _Py_gitversion();
}

const char *
_Py_identifier(void)
{
    return _Py_gitidentifier();
}

const char *
_Py_hgversion(void)
{
    return _Py_gitversion();
}

const char *
_Py_hgidentifier(void)
{
    return _Py_gitidentifier();
}


const char *
Py_GetBuildInfo(void)
{
    static char buildinfo[50 + sizeof(GITVERSION) + sizeof(GITTAG)];
    const char *revision = _Py_gitversion();
    const char *sep = *revision ? ":" : "";
    const char *gitid = _Py_gitidentifier();
    if (!(*gitid))
        gitid = "3.3-px";
    PyOS_snprintf(buildinfo, sizeof(buildinfo),
                  "%s%s%s, %.20s, %.9s", gitid, sep, revision,
                  DATE, TIME);
    return buildinfo;
}

#else /* GITTAG */
/* XXX Only unix build process has been tested */
#ifndef HGVERSION
#define HGVERSION ""
#endif
#ifndef HGTAG
#define HGTAG ""
#endif
#ifndef HGBRANCH
#define HGBRANCH ""
#endif

const char *
Py_GetBuildInfo(void)
{
    static char buildinfo[50 + sizeof(HGVERSION) +
                          ((sizeof(HGTAG) > sizeof(HGBRANCH)) ?
                           sizeof(HGTAG) : sizeof(HGBRANCH))];
    const char *revision = _Py_hgversion();
    const char *sep = *revision ? ":" : "";
    const char *hgid = _Py_hgidentifier();
    if (!(*hgid))
        hgid = "default";
    PyOS_snprintf(buildinfo, sizeof(buildinfo),
                  "%s%s%s, %.20s, %.9s", hgid, sep, revision,
                  DATE, TIME);
    return buildinfo;
}

const char *
_Py_hgversion(void)
{
    return HGVERSION;
}

const char *
_Py_hgidentifier(void)
{
    const char *hgtag, *hgid;
    hgtag = HGTAG;
    if ((*hgtag) && strcmp(hgtag, "tip") != 0)
        hgid = hgtag;
    else
        hgid = HGBRANCH;
    return hgid;
}

const char *
_Py_version(void)
{
    return _Py_hgversion();
}

const char *
_Py_identifier(void)
{
    return _Py_hgidentifier();
}

const char *
_Py_gitversion(void)
{
    return _Py_hgversion();
}

const char *
_Py_gitidentifier(void)
{
    return _Py_hgidentifier();
}

#endif /* !GITTAG */
