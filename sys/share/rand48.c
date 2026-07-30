/*      SCCS Id: @(#)rand48.c   3.2     2026/07/30      */
/* NetHack may be freely redistributed.  See license for details. */

/*
 * srand48()/lrand48() for ports whose C library has neither.
 *
 * The Unix build reaches the C library's pair through Rand() in
 * include/unixconf.h, and include/ntconf.h now does the same rather than
 * using sys/share/random.c: two different generators would make
 * NETHACK_SEED produce a different dungeon on each platform, and
 * test/wincompare.sh diffs a Windows screen against a Unix one.
 *
 * The generator is the one POSIX specifies, which is what glibc
 * implements, so a given seed yields the same sequence here as there:
 *
 *      X(n+1) = (0x5DEECE66D * X(n) + 0xB) mod 2**48
 *
 * srand48(s) sets X to (s << 16) | 0x330E, and lrand48() returns the top
 * 31 bits of the new X.  Only these two are provided; nothing in NetHack
 * calls drand48(), mrand48(), or the *48 seed-vector variants.
 *
 * This file is compiled only for the MinGW-w64 build, so it uses
 * `unsigned long long' rather than open-coding 48-bit arithmetic out of
 * 32-bit halves: Windows is LLP64, where unsigned long is 32 bits and the
 * multiply below would overflow it.
 */

#include "config.h"

#if defined(WIN32) && !defined(RANDOM)

#define MASK48  0xFFFFFFFFFFFFULL

static unsigned long long x48 = 0x330EULL;

void
srand48(seed)
long seed;
{
    x48 = (((unsigned long long) (unsigned long) seed) << 16) | 0x330EULL;
    x48 &= MASK48;
}

long
lrand48()
{
    x48 = (0x5DEECE66DULL * x48 + 0xBULL) & MASK48;
    return (long) (x48 >> 17);          /* the top 31 bits */
}

#endif /* WIN32 && !RANDOM */

/*rand48.c*/
