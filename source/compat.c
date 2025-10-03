/*
// Program:  Free FDISK
// Module:   COMPAT.C
// Based on code by Brian E. Reifsnyder
// Copyright: 1998-2008, under the terms of the GNU GPL, Version 2
//
// Modified and adapted for Windows by: Andrey Hlus, 2025
*/

#if defined( __WATCOMC__ ) || defined( __GNUC__ ) || defined( _MSC_VER )

#include <tchar.h>
#include <windows.h>
#include <stdlib.h>
#include "compat.h"

/* Watcom C does not have this */
#ifdef __WATCOMC__
char *searchpath( char *fn )
{
    static char full_path[_MAX_PATH];

    _searchenv( fn, "PATH", full_path );
    if ( full_path[0] ) {
        return full_path;
    }

    return NULL;
}
#endif

#if defined(_MSC_VER)

TCHAR *searchpath(const TCHAR *fn)
{
    static TCHAR full_path[_MAX_PATH];

    errno_t err = _tsearchenv_s(fn, _T("PATH"), full_path, _MAX_PATH);

    if (err == 0 && full_path[0] != _T('\0')) {
        return full_path;
    }
    return NULL;
}

#endif

#endif
