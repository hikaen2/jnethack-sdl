/*      mbchar.h        */
/* JNetHack may be freely redistributed.  See license for details. */

/*
 * The game's internal multibyte encoding, behind one interface.
 *
 * This is the layer Phase 1 of UTF8-PLAN.md is about.  The tree is full of
 * code that knows a Japanese character is two bytes -- is_kanji1(),
 * is_kanji2(), split_japanese(), and jconjsub()'s "overwrite tmp[len-2]".
 * None of that survives the switch to UTF-8, where a kanji is three bytes
 * and an IVS or an emoji is four.
 *
 * Rewriting all of it and flipping the literals in one step would mean a
 * commit that cannot be bisected: everything breaks at once and nothing
 * says which change broke it.  So the order is
 *
 *   Phase 1   rewrite the callers in terms of *characters*, using this
 *             header, while the literals are still EUC-JP.  Behaviour is
 *             unchanged and the existing tests still pass.
 *   Phase 3   iconv the tree and define JP_INTERNAL_UTF8.  Only the bodies
 *             below change; no caller does.
 *
 * So mb_seqlen("<kanji>") is 2 today and 3 afterwards, and nothing that
 * calls it has to care.  The EUC-JP half is deliberately kept rather than
 * deleted at Phase 3: it costs thirty lines, and it is what lets the two
 * encodings be compared during the migration.
 *
 * Include hack.h first; FDECL comes from tradstdc.h through it.  Spelled
 * "extern" rather than E for the same reason sdlterm.h and utf8.h are --
 * the files under japanese/ include this outside the block in wintty.h
 * where E is defined.
 */

#ifndef MBCHAR_H
#define MBCHAR_H

/*
 * Longest character in whichever encoding is current.  EUC-JP reaches 3
 * (SS3 + two bytes, JIS X 0212); UTF-8 reaches 4.  Sized for the larger so
 * that a buffer declared with it stays correct across Phase 3 -- the
 * mistake MySQL's utf8mb3 made, and the reason UTF8-PLAN.md picked option
 * B in the first place.
 */
#define MB_MAXBYTES     4

/* Bytes occupied by the character starting at s.  0 at the terminating
   NUL, so "while ((n = mb_seqlen(s)) > 0)" walks a string.  Never returns
   more bytes than the string actually holds. */
extern int FDECL(mb_seqlen, (const char *));

/* Display width of the character at s, in terminal columns: 0, 1 or 2. */
extern int FDECL(mb_width, (const char *));

/* Total display width of the whole string. */
extern int FDECL(mb_colwidth, (const char *));

/* Number of characters, as opposed to bytes. */
extern int FDECL(mb_chars, (const char *));

/*
 * Is byte offset pos the start of a character?  True at the end of the
 * string, and true for pos 0.  This is the generalisation of is_kanji2():
 * where that asked "is this the second byte of a pair", the question that
 * survives a variable-width encoding is "would cutting here split a
 * character".
 */
extern int FDECL(mb_is_boundary, (const char *, int));

/*
 * Largest byte length not exceeding max that ends on a character boundary.
 * What every "truncate a name to fit the field" site wants.
 */
extern int FDECL(mb_trunc_bytes, (const char *, int));

/*
 * Largest byte length whose display width does not exceed cols.
 *
 * Distinct from mb_trunc_bytes() because bytes stopped being a proxy for
 * columns: under EUC-JP a kanji was two of each, so the status line could
 * count either.  Under UTF-8 it is three bytes and still two columns, and
 * the sites that are really laying out a fixed-width field -- botl.c's
 * ten-column name, topten.c's NAMSZ -- have to count columns or the line
 * wraps.
 */
extern int FDECL(mb_trunc_cols, (const char *, int));

/*
 * Step back one character from p.  Under UTF-8 this skips a run of
 * zero-width code points along with the base character they hang off, per
 * UTF8-PLAN.md option B-1, so a backspace cannot leave an orphaned
 * combining mark behind.  Never returns a pointer below base.
 */
extern const char *FDECL(mb_prev, (const char *, const char *));

#endif /* MBCHAR_H */
