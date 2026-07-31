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

/*
 * Bytes of a *complete* character at s: 0 at the terminating NUL, and 0
 * for a sequence the string is too short to hold or that is not a
 * character at all.
 *
 * The other answer to the same question.  mb_seqlen() reports 1 for a lead
 * byte with nothing after it, so that a scan always makes progress and
 * never reads past the terminator.  Truncation needs to know the byte is
 * not a character, because keeping it is how a name ends up half a
 * character long -- which is what christen_monst() was guarding against.
 */
extern int FDECL(mb_complete, (const char *));

/* Display width of the character at s, in terminal columns: 0, 1 or 2. */
extern int FDECL(mb_width, (const char *));

/*
 * The same question about a code point rather than a character.
 *
 * This is the *text* width, and deliberately not the same thing as the
 * Unicode East Asian Width: the 232 code points JIS X 0208 holds that
 * Unicode files under Ambiguous or Narrow -- Greek and Cyrillic letters,
 * arrows, box drawing, maths -- were two bytes in EUC-JP and the port laid
 * them out as two columns.  Under UTF-8 nothing about the bytes says so,
 * so the rule is stated here instead: a code point JIS X 0208 contains is
 * two columns.  That reproduces the old layout exactly, which matters
 * because the status line, the menus and split_japanese() were all written
 * against it.
 *
 * It must not be used for the graphics character sets.  A byte written
 * between graph_on() and graph_off() also becomes a box-drawing code point
 * such as U+2500, and that one occupies a single cell -- it is a line on
 * the map, not text.  win/tty/sdlterm.c keeps its own sdl_cp_width() for
 * that path, which answers the plain Unicode question.
 */
extern int FDECL(mb_cpwidth, (long));

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
 *
 * An incomplete character at the end is dropped rather than kept, so the
 * result is always a whole string: cutting a 63-byte buffer at 62 in the
 * middle of a kanji yields 61, not 62.
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
 * The character at s as a Unicode code point, and the reverse.
 *
 * These are what let code work on characters rather than bytes without
 * knowing the encoding at all -- jrndm_replace() needs to ask "which JIS
 * row is this in", which is a question about the character, not its bytes.
 *
 * mb_decode() returns 0 at the terminating NUL and for a sequence that is
 * not a character.  mb_encode() writes up to n bytes and returns how many,
 * or 0 if the code point has no form in the current encoding -- which under
 * EUC-JP is most of Unicode, and under UTF-8 is nothing.
 */
extern long FDECL(mb_decode, (const char *));
extern int FDECL(mb_encode, (long, char *, int));

/*
 * Replace the character at buf[pos] with rep, moving the rest of the string
 * if the two are not the same number of bytes.  Returns the length of what
 * was written.
 *
 * The move is the part that is easy to forget.  Under EUC-JP every
 * substitution the game makes happened to be two bytes for two, so the
 * callers wrote memcpy() and were right; under UTF-8 a kanji is three bytes
 * and the two spaces that blank it are two.
 */
extern int FDECL(mb_replace, (char *, int, const char *));

/*
 * Step back one character from p.  Under UTF-8 this skips a run of
 * zero-width code points along with the base character they hang off, per
 * UTF8-PLAN.md option B-1, so a backspace cannot leave an orphaned
 * combining mark behind.  Never returns a pointer below base.
 */
extern const char *FDECL(mb_prev, (const char *, const char *));

#endif /* MBCHAR_H */
