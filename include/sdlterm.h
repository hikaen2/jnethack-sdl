/*      sdlterm.h       */
/* JNetHack may be freely redistributed.  See license for details. */

/* SDL2 cell-grid backend for the tty window port.  See SDL-PORT.md. */

#ifndef SDLTERM_H
#define SDLTERM_H

/*
 * Most of JNetHack's screen output already goes through jlib.c, which
 * reassembles EUC-JP byte pairs before handing them on; that is where the
 * backend is hooked (see japanese/jlib.c).  A handful of places in
 * wintty.c, topl.c and getline.c still write to stdout directly, though,
 * and those calls would otherwise bypass a backend that owns its own
 * cell grid.
 *
 * Rather than edit those three files -- keeping them untouched is what
 * made the termcap/SDL screen comparison possible in the first place --
 * the stdio entry points they use are retargeted here.  wintty.h includes
 * this header, and all three files include wintty.h after <stdio.h> has
 * arrived via hack.h, so these macros win.
 *
 * This is not a new trick: include/wintty.h has carried
 * "#define putchar(x) xputc(x)" for MSDOS and WIN32CON since 3.2.
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
 * Three entry points, deliberately separated by what they know:
 *
 *   sdl_putbyte()  one byte, interpreted through whichever graphics
 *                  character set the game has selected.
 *   sdl_puteuc()   one EUC-JP two-byte sequence -> one code point.  The
 *                  cell width of the result is decided by the fact that
 *                  it *arrived as two bytes*, not by any property of the
 *                  code point; see the comment on the function.
 *   sdl_putcp()    a Unicode code point, straight into a cell.
 */
/* Spelled "extern" rather than the tree's usual E, because japanese/jlib.c
   includes this header outside the block in wintty.h where E is defined. */
extern void FDECL(sdl_putbyte, (int));
extern void FDECL(sdl_puteuc, (int, int));
extern void FDECL(sdl_putcp, (int));
extern int FDECL(sdl_putchar, (int));
extern void FDECL(sdl_puts, (const char *));
extern void FDECL(sdl_fputs, (const char *, FILE *));
extern int FDECL(sdl_fputc, (int, FILE *));
extern int FDECL(sdl_fflush, (FILE *));
extern int NDECL(sdl_getch);
extern int FDECL(sdl_yn, (const char *));
extern void NDECL(sdl_dump_grid);       /* NH_SDL_DUMP hook */
extern void NDECL(sdl_width_test);      /* NH_SDL_WIDTHTEST hook */

/*
 * Files that need the declarations above but must keep the real stdio --
 * sdlterm.c itself, and japanese/jlib.c, whose whole job is to write
 * bytes -- define this before including the header.
 */
#ifndef SDLTERM_KEEP_STDIO

/*
 * putchar() goes to sdl_putchar(), which is a wrapper round jlib.c's byte
 * accumulator cputchar(), so that a pair of EUC-JP bytes written one
 * putchar() at a time still reaches the backend as one character.
 * tty_askname() echoing a Japanese player name is written exactly that
 * way.  win/tty/termcap.c does the same thing with its own xputc().
 *
 * It has to be a function and not "(cputchar(c), (int)(c))": wintty.c's
 * dmore() writes putchar(*(p++)), and a macro that names its argument
 * twice would advance p twice and print every other character.
 */
# undef putchar
# define putchar(c)     sdl_putchar((int)(unsigned char)(c))
# undef puts
# define puts(s)        (sdl_puts(s), 0)
# undef fputs
# define fputs(s,f)     (sdl_fputs(s,f), 0)
# undef fputc
# define fputc(c,f)     sdl_fputc((int)(unsigned char)(c), f)
# undef fflush
# define fflush(f)      sdl_fflush(f)

#endif /* !SDLTERM_KEEP_STDIO */

#endif /* SDLTERM_H */
