/*	SCCS Id: @(#)ntconf.h	3.2	96/10/14	*/
/* Copyright (c) NetHack PC Development Team 1993, 1994.  */
/* NetHack may be freely redistributed.  See license for details. */

#ifndef NTCONF_H
#define NTCONF_H

/* #define SHELL	/* nt use of pcsys routines caused a hang */

/*
 * Deliberately not defined: sys/share/random.c and the lrand48() that
 * include/unixconf.h:304 picks for Unix are different generators, so the
 * same NETHACK_SEED would build a different dungeon on each platform and
 * test/wincompare.sh could not diff one screen against the other.  MinGW
 * has no lrand48(), so sys/share/rand48.c supplies the POSIX one.
 */
/* #define RANDOM	*//* have Berkeley random(3) */

#define TEXTCOLOR	/* Color text */

/* 64 was the 1996 value and it is far too small now: getcwd() into
   orgdir[PATHLEN] fails outright on any ordinary install path, and
   pcmain.c turns that into "current directory path too long" before the
   game starts.  260 is Windows' MAX_PATH. */
#define PATHLEN		260	/* maximum pathlength */
#define FILENAME	80	/* maximum filename length (conservative) */
#define EXEPATH			/* Allow .exe location to be used as HACKDIR */
/*
 * -----------------------------------------------------------------
 *  The remaining code shouldn't need modification.
 * -----------------------------------------------------------------
 */
/* #define SHORT_FILENAMES	/* All NT filesystems support long names now */

#define MICRO		/* always define this! */
/*
 * Not for the SDL build.  NO_TERMS means "this port draws the screen
 * without a termcap-style layer", and win/tty/sdlterm.c *is* that layer --
 * it replaces win/tty/termcap.c function for function and supplies CM and
 * ul_hack through tc_lcl_data for the very code NO_TERMS would switch off.
 *
 * Defining it here would silently change win/tty/wintty.c's behaviour
 * relative to the Unix SDL build, which is the thing the port is verified
 * against.  Concretely: wintty.c includes termcap.h only #ifndef NO_TERMS,
 * termcap.h is where ASCIIGRAPH comes from, and g_putch() needs
 * ASCIIGRAPH && !NO_TERMS to strip bit 7 off the dec_graphics[] bytes and
 * call graph_on().  Without it those bytes reach jlib.c's jbuffer() with
 * the high bit set, get paired up as EUC-JP, and the map walls come out as
 * kanji.  It also loses the tty_shutdown() call in tty_exit_nhwindows().
 */
#ifndef SDL_GRAPHICS
#define NO_TERMS
#endif
#define ASCIIGRAPH

/* The following is needed for prototypes of certain functions */
#if defined(_MSC_VER) || defined(__MINGW32__)
#include <process.h>	/* Provides prototypes of exit(), spawn()      */
#endif

#include <string.h>     /* Provides prototypes of strncmpi(), etc.     */
#ifdef STRNCMPI
# ifdef __MINGW32__
#define strncmpi(a,b,c) _strnicmp(a,b,c)        /* strnicmp is the deprecated name */
# else
#define strncmpi(a,b,c) strnicmp(a,b,c)
# endif
#endif

#ifndef SYSTEM_H
#include "system.h"
#endif
#define index	strchr
#define rindex	strrchr
#include <time.h>

#ifdef RANDOM
/* Use the high quality random number routines. */
#define Rand()	random()
#else
#define Rand()	lrand48()	/* sys/share/rand48.c */
#endif

#define FCMASK	0660	/* file creation mask */
#define regularize	nt_regularize

#ifndef M
#define M(c)		(0x80 | (c))
/* #define M(c)		((c) - 128) */
#endif

#ifndef C
#define C(c)		(0x1f & (c))
#endif

#if defined(DLB)
#define FILENAME_CMP  stricmp                 /* case insensitive */
#endif

#ifdef MICRO
# ifndef MICRO_H
#include "micro.h"      /* contains necessary externs for [os_name].c */
# endif
#endif

#include <fcntl.h>
#include <io.h>
#include <direct.h>
#include <conio.h>
#undef kbhit	        /* Use our special NT kbhit */
#define kbhit (*nt_kbhit)

#ifndef alloca
#define ALLOCA_HACK	/* used in util/panic.c */
#endif

#ifndef REDO
#undef	Getchar
#define Getchar nhgetch
#endif

#ifdef SDL_GRAPHICS
/* The entire input hook: keys come from the window, not from the console.
   Same as include/unixconf.h.  Without this tgetch() would have to come
   from sys/winnt/nttty.c, which the SDL build does not compile. */
extern int sdl_getch(void);
#define tgetch sdl_getch
#endif

#ifdef _MSC_VER
#pragma warning(disable:4018)	/* signed/unsigned mismatch */
#pragma warning(disable:4305)	/* init, conv from 'const int' to 'char' */
#endif

#endif /* NTCONF_H */
