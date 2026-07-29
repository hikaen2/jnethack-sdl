/*	sdlterm.h	*/
/* SDL2 cell-grid backend for the tty window port.  See SDL-POC-PLAN.md. */

#ifndef SDLTERM_H
#define SDLTERM_H

/*
 * The tty window port writes plain characters straight to stdout in a
 * number of places (24 of them in stock 3.2.3: 21 in wintty.c, 2 in
 * termcap.c, 1 in topl.c).  Those calls bypass xputc() entirely, so a
 * backend that owns its own cell grid never sees them.
 *
 * Rather than edit wintty.c / topl.c / getline.c -- hypothesis H1 says
 * they should stay byte-for-byte identical -- we retarget the four stdio
 * entry points they use.  wintty.h includes this header, and all three
 * files include wintty.h after <stdio.h> has been pulled in via hack.h,
 * so the macros below win.
 *
 * sdlterm.c itself defines SDLTERM_INTERNAL so that it can still reach
 * the real stdio functions.
 */

/*
 * wintty.c only pulls in <signal.h> on BSD-flavoured systems, so under
 * SYSV its SIGWINCH handler compiles away and the window can never tell
 * the port that it was resized.  wintty.c includes this header before it
 * makes that test, so including <signal.h> here is enough to turn the
 * handler back on without editing wintty.c.
 */
#include <signal.h>

/*
 * Two entry points, deliberately separated:
 *
 *   sdl_putbyte()  takes a byte off the port and decides what character
 *                  it stands for -- which depends on whether a graphics
 *                  set is active.  This is the layer where JNetHack will
 *                  reassemble EUC-JP pairs.
 *   sdl_putcp()    takes a Unicode code point and puts it in a cell.
 */
E void FDECL(sdl_putbyte, (int));	/* one byte from the port */
E void FDECL(sdl_putcp, (int));		/* one code point at the cursor */
E void FDECL(sdl_puts, (const char *));	/* string + newline */
E void FDECL(sdl_fputs, (const char *, FILE *));
E int FDECL(sdl_fflush, (FILE *));
E int NDECL(sdl_getch);
E void NDECL(sdl_dump_grid);		/* NH_SDL_DUMP hook (A3) */
E void NDECL(sdl_width_test);		/* NH_SDL_WIDTHTEST hook (A4) */

#ifndef SDLTERM_INTERNAL

# undef putchar
# define putchar(c)	(sdl_putbyte((int)(unsigned char)(c)), (int)(c))
# undef puts
# define puts(s)	(sdl_puts(s), 0)
# undef fputs
# define fputs(s,f)	(sdl_fputs(s,f), 0)
# undef fflush
# define fflush(f)	sdl_fflush(f)

#endif /* !SDLTERM_INTERNAL */

#endif /* SDLTERM_H */
