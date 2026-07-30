/*	SCCS Id: @(#)win32api.h	 3.2	 96/02/15		  */
/* Copyright (c) NetHack PC Development Team 1996                 */
/* NetHack may be freely redistributed.  See license for details. */

/*
 * This header file is used to clear up some discrepencies with Visual C
 * header files & NetHack before including windows.h, so all NetHack
 * files should include "win32api.h" rather than <windows.h>.
 */
# if defined(_MSC_VER)
# undef strcmpi
# undef min
# undef max
# pragma warning(disable:4142)  /* Warning, Benign redefinition of type */
# pragma pack(8)
# endif

/*
 * Without this, <windows.h> drags in <rpc.h>, whose <rpcndr.h> does an
 * unguarded "typedef unsigned char boolean" -- and NetHack has had its own
 * boolean since 1985.  Nothing here needs RPC, OLE, or the shell API, and
 * leaving them out also keeps youprop.h's Protection/Warning/Confusion
 * macros from colliding with the parameter names in those headers.
 */
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif

#include <windows.h>

# if defined(_MSC_VER)
# pragma pack()
# endif

/*win32api.h*/
