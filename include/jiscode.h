/*      jiscode.h       */
/* JNetHack may be freely redistributed.  See license for details. */

/*
 * JIS X 0208 <-> Unicode, and EUC-JP <-> Unicode built on top of it.
 *
 * The forward table has been in include/jis0208.h since the SDL port, but
 * it lived there as a "static const" array in a header, so every file that
 * included it got its own 17K copy, and the only reverse lookup was a scan
 * of all 8836 entries inside win/tty/sdlterm.c.  Phase 2 of UTF8-PLAN.md
 * needs the reverse direction in three more places -- jrndm_replace(), the
 * EUC side of mbchar.c, and the tty output path when the terminal is not
 * UTF-8 -- so it moves here, behind an index, with one copy in the program.
 *
 * Include hack.h first; FDECL comes from tradstdc.h through it.  Spelled
 * "extern" rather than E for the same reason mbchar.h and utf8.h are.
 */

#ifndef JISCODE_H
#define JISCODE_H

#define JIS_ROWS        94
#define JIS_CELLS       94

/*
 * Row and cell are 1-based, as in the standard: row 4 cell 2 is hiragana A.
 * Returns 0 for a position the standard leaves unassigned, which is 1957 of
 * the 8836.
 */
extern long FDECL(jis_to_ucs, (int, int));

/*
 * The other way.  Returns 1 and fills in row and cell, or 0 if the code
 * point is not in JIS X 0208 -- which under UTF8-PLAN.md's option B is a
 * routine answer rather than an error, since the game now accepts the whole
 * of Unicode and only the JIS subset can be spelled in EUC-JP.
 *
 * A binary search over an index built on first use, not the linear scan
 * this replaces.  That scan was fine for keyboard input, where it ran once
 * per keystroke; jrndm_replace() calls it once per character of a mangled
 * engraving, and the tty output path would call it once per character
 * written.
 */
extern int FDECL(ucs_to_jis, (long, int *, int *));

/*
 * EUC-JP <-> Unicode.  ASCII, JIS X 0208, and the SS2 half-width katakana
 * are handled; SS3 (JIS X 0212) is not, and euc_to_ucs() reports 0 for it,
 * exactly as the SDL backend did before.
 *
 * euc_to_ucs() returns the code point and stores the number of bytes it
 * consumed; 0 means the sequence was not valid EUC-JP.
 * ucs_to_euc() writes up to n bytes and returns how many, or 0 if the code
 * point has no EUC-JP form or n was too small.
 */
extern long FDECL(euc_to_ucs, (const char *, int *));
extern int FDECL(ucs_to_euc, (long, char *, int));

#endif /* JISCODE_H */
