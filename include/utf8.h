/*      utf8.h  */
/* JNetHack may be freely redistributed.  See license for details. */

/*
 * UTF-8 primitives for the move off EUC-JP.  See UTF8-PLAN.md.
 *
 * Two layers live here, and the split matters:
 *
 *   include/sheredom_utf8.h    vendored, unmodified, public domain
 *                              (https://github.com/sheredom/utf8.h).  String
 *                              level operations -- utf8len(), utf8size(),
 *                              utf8str(), utf8cmp(), utf8rcodepoint().
 *
 *   this header                        the trust boundary: decoding bytes that
 *                              came from outside the program, and deciding
 *                              how wide the result is on a cell grid.
 *
 * The vendored header is not used for the boundary, because it does not
 * validate.  Two measured facts, not guesses:
 *
 *   utf8codepoint() reads str[1..3] without first checking that they are
 *   continuation bytes.  Handed a truncated lead byte at the end of a
 *   buffer -- "\xE6" followed by the terminator -- it consumed three bytes,
 *   read past the NUL, and returned U+603F.  Keyboard and IME text arrives
 *   exactly that way, so this would be an overread on live input.
 *
 *   utf8nvalid() checks continuation bytes and overlong forms, but accepts
 *   surrogates (ED A0 80 = U+D800) and out-of-range values (F4 90 80 80 =
 *   U+110000, F7 BF BF BF = U+1FFFFF).  It is not sufficient as an input
 *   filter on its own.
 *
 * So utf8_decode() below is ours, it is the only thing that should ever see
 * unvalidated bytes, and it never reads past a terminating NUL.  Once text
 * has been through it, the vendored string functions are safe to use on it.
 *
 * The copy in include/ rather than a dependency on Debian's libutf8.h-dev is
 * deliberate: the MinGW-w64 cross build (SDL-WINDOWS.md) has no such package,
 * and a header the tree carries itself cannot skew between the two builds.
 *
 * This header does *not* pull in the vendored one.  Every translation unit
 * that includes sheredom_utf8.h grows by 34K of object code and a quarter of
 * a second of compile time, because the header defines some forty functions
 * as weak symbols rather than declaring them.  One copy survives the link, so
 * the binary does not suffer, but with the include hoisted into hack.h -- the
 * shape Phase 1 would naturally take -- the tree's 250-odd source files would
 * pay both costs for functions almost none of them call.  So: include
 * "sheredom_utf8.h" in the few files that actually want utf8len(), utf8str()
 * or utf8rcodepoint(), and include this one everywhere else.
 *
 * Include order: FDECL comes from tradstdc.h via hack.h, so include hack.h
 * first.  Spelled "extern" rather than the tree's usual E for the same reason
 * sdlterm.h does -- the files under japanese/ include this outside the block
 * in wintty.h where E is defined.
 */

#ifndef NH_UTF8_H
#define NH_UTF8_H

/* Longest UTF-8 sequence, now that U+10FFFF is reachable.  Buffers sized
   for "a character" must use this and not a literal 3; the whole point of
   the plan's option B is that the 3-byte assumption never gets baked in
   anywhere, the way MySQL's utf8mb3 baked it into an index limit. */
#define UTF8_MAXBYTES   4

/* U+FFFD REPLACEMENT CHARACTER.  What utf8_decode() reports for malformed
   input, and what utf8_encode() substitutes for an impossible code point. */
#define UTF8_REPLACEMENT 0xFFFDL

#define UTF8_MAXCP      0x10FFFFL

/*
 * Decode one character.  Returns how many bytes it occupied, 0 at the
 * terminating NUL.
 *
 * On malformed input -- a stray continuation byte, a truncated sequence, an
 * overlong form, a surrogate, or anything above U+10FFFF -- *cp is set to
 * UTF8_REPLACEMENT and 1 is returned, so the caller resynchronises one byte
 * at a time and always makes progress.  It never examines bytes at or past
 * the terminator, which is what makes it safe on a half-typed IME buffer.
 */
extern int FDECL(utf8_decode, (const char *, long *));

/*
 * Encode one code point into at most n bytes.  Returns how many bytes were
 * written, or 0 if n was too small.  Surrogates and out-of-range values are
 * written as UTF8_REPLACEMENT rather than being allowed through; there is no
 * code path in the game that should be producing them, and silently emitting
 * an invalid sequence would push the problem to whoever reads the file.
 */
extern int FDECL(utf8_encode, (long, char *, int));

/*
 * Display width of one code point in grid cells: 0, 1 or 2.
 *
 * The width-2 ranges are the ones sdlterm.c has been using and that
 * NH_SDL_WIDTHTEST already exercises.  The width-0 ranges are new, and are
 * what plan option B-1 rests on: combining marks and variation selectors are
 * kept in the game's strings but occupy no cell.
 *
 * For Japanese specifically, U+3099 and U+309A -- the combining voiced and
 * semi-voiced sound marks -- are width 0 and sit inside the width-2 kana
 * block, so the zero-width test has to come first.  Getting that order wrong
 * silently shifts every column after a decomposed daku-on.
 */
extern int FDECL(utf8_cpwidth, (long));

/* Bytes in the sequence at s, validated: 0 at the NUL, 1 for anything
   malformed (matching utf8_decode's resynchronisation). */
extern int FDECL(utf8_seqlen, (const char *));

/* Total display width of a string, in cells. */
extern int FDECL(utf8_colwidth, (const char *));

/*
 * Step back one *character* from p, not one code point: any run of
 * zero-width code points is skipped along with the base character it hangs
 * off.  This is what backspace in getline.c needs -- deleting the base and
 * leaving an orphaned variation selector behind is the UTF-8 version of the
 * half-a-kanji desynchronisation is_kanji2() exists to prevent.
 *
 * Never returns a pointer below base.
 */
extern const char *FDECL(utf8_prev, (const char *, const char *));

/*
 * Validate a whole string.  Returns 1 if every sequence is well formed, or 0
 * with *bad pointing at the first offending byte (pass 0 if not wanted).
 * Stricter than the vendored utf8nvalid(): surrogates and code points above
 * U+10FFFF are rejected here.
 */
extern int FDECL(utf8_valid, (const char *, const char **));

#endif /* NH_UTF8_H */
