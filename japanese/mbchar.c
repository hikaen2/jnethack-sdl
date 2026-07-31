/*      mbchar.c        */
/* JNetHack may be freely redistributed.  See license for details. */

/* The internal multibyte encoding, behind one interface.  The rationale for
   the whole arrangement is in include/mbchar.h; the migration it belongs to
   is UTF8-PLAN.md. */

#include "hack.h"
#include "mbchar.h"
#include "utf8.h"
#include "jiscode.h"

/*
 * Define JP_INTERNAL_UTF8 (Phase 3) to switch the bodies below.  Only this
 * file changes; every caller is already written in terms of characters.
 */

#ifndef JP_INTERNAL_UTF8

/*
 * EUC-JP.
 *
 *   0x00..0x7F              ASCII, one byte, one column
 *   0x8E (SS2) + 0xA1..0xDF two bytes, halfwidth katakana, one column
 *   0x8F (SS3) + two bytes  three bytes, JIS X 0212, two columns
 *   0xA1..0xFE + 0xA1..0xFE two bytes, JIS X 0208, two columns
 *
 * Each function checks that the continuation bytes are actually present
 * before claiming them.  The code this replaces did not -- is_kanji1() and
 * friends simply stepped two at a time on seeing a high bit -- so a string
 * ending in a lone lead byte walked off the end.  That could not easily
 * happen while every string came from the game's own literals, but names
 * and engravings come from the player.
 */

int
mb_seqlen(s)
const char *s;
{
    const unsigned char *p = (const unsigned char *) s;

    if (!p[0]) return 0;
    if (p[0] < 0x80) return 1;

    if (p[0] == 0x8E)                   /* SS2 */
        return (p[1] >= 0xA1 && p[1] <= 0xDF) ? 2 : 1;

    if (p[0] == 0x8F)                   /* SS3 */
        return (p[1] >= 0xA1 && p[2] >= 0xA1) ? 3 : 1;

    if (p[0] >= 0xA1 && p[0] <= 0xFE)
        return (p[1] >= 0xA1 && p[1] <= 0xFE) ? 2 : 1;

    return 1;                           /* 0x80..0xA0: not EUC-JP at all */
}

int
mb_lead_len(c)
int c;
{
    unsigned int b = (unsigned int) c & 0xFF;

    if (b < 0x80) return 1;
    if (b == 0x8E) return 2;            /* SS2 */
    if (b == 0x8F) return 3;            /* SS3 */
    if (b >= 0xA1 && b <= 0xFE) return 2;
    return 1;                           /* 0x80..0xA0: not EUC-JP */
}

int
mb_complete(s)
const char *s;
{
    const unsigned char *p = (const unsigned char *) s;

    if (!p[0]) return 0;
    if (p[0] < 0x80) return 1;

    if (p[0] == 0x8E)
        return (p[1] >= 0xA1 && p[1] <= 0xDF) ? 2 : 0;

    if (p[0] == 0x8F)
        return (p[1] >= 0xA1 && p[2] >= 0xA1) ? 3 : 0;

    if (p[0] >= 0xA1 && p[0] <= 0xFE)
        return (p[1] >= 0xA1 && p[1] <= 0xFE) ? 2 : 0;

    return 0;                           /* 0x80..0xA0: not EUC-JP at all */
}

int
mb_width(s)
const char *s;
{
    const unsigned char *p = (const unsigned char *) s;
    int n = mb_seqlen(s);

    if (n <= 1) return n;               /* 0 at the NUL, 1 for ASCII */
    if (p[0] == 0x8E) return 1;         /* halfwidth katakana */
    return 2;
}

long
mb_decode(s)
const char *s;
{
    int n;

    return euc_to_ucs(s, &n);
}

int
mb_encode(cp, buf, n)
long cp;
char *buf;
int n;
{
    return ucs_to_euc(cp, buf, n);
}

#else /* JP_INTERNAL_UTF8 */

int
mb_seqlen(s)
const char *s;
{
    return utf8_seqlen(s);
}

int
mb_lead_len(c)
int c;
{
    unsigned int b = (unsigned int) c & 0xFF;

    if (b < 0x80) return 1;
    if ((b & 0xE0) == 0xC0) return 2;
    if ((b & 0xF0) == 0xE0) return 3;
    if ((b & 0xF8) == 0xF0) return 4;
    return 1;                           /* continuation byte, or F8..FF */
}

int
mb_complete(s)
const char *s;
{
    long cp;
    int n;

    if (!*s) return 0;
    n = utf8_decode(s, &cp);
    /* utf8_decode() reports a malformed or truncated sequence as one byte
       of U+FFFD; a real U+FFFD in the text comes back as three. */
    if (n == 1 && cp == UTF8_REPLACEMENT) return 0;
    return n;
}

int
mb_width(s)
const char *s;
{
    long cp;

    if (!*s) return 0;
    (void) utf8_decode(s, &cp);

    return mb_cpwidth(cp);
}

long
mb_decode(s)
const char *s;
{
    long cp;
    int n;

    if (!*s) return 0L;
    n = utf8_decode(s, &cp);
    if (n == 1 && cp == UTF8_REPLACEMENT) return 0L;
    return cp;
}

int
mb_encode(cp, buf, n)
long cp;
char *buf;
int n;
{
    return utf8_encode(cp, buf, n);
}

#endif /* JP_INTERNAL_UTF8 */

/* --- encoding independent, given the two above --------------------- */

int
mb_cpwidth(cp)
long cp;
{
    int w, row, cell;

    w = utf8_cpwidth(cp);
    if (w != 1) return w;               /* settled: wide, or zero-width */

    /*
     * East Asian Ambiguous.  See the note in include/mbchar.h for why
     * membership of JIS X 0208 is the test, and why the graphics character
     * sets must not come through here.
     *
     * The lookup only runs for code points utf8_cpwidth() called narrow,
     * so kana and kanji have already returned above.
     */
    if (ucs_to_jis(cp, &row, &cell)) return 2;

    return 1;
}

int
mb_colwidth(s)
const char *s;
{
    int n, w = 0;

    while ((n = mb_seqlen(s)) > 0) {
        w += mb_width(s);
        s += n;
    }
    return w;
}

int
mb_chars(s)
const char *s;
{
    int n, c = 0;

    while ((n = mb_seqlen(s)) > 0) {
        c++;
        s += n;
    }
    return c;
}

int
mb_is_boundary(s, pos)
const char *s;
int pos;
{
    int i = 0, n;

    if (pos <= 0) return 1;

    /*
     * Scan forward from the start.  EUC-JP is not self-synchronising -- a
     * byte in 0xA1..0xFE is a lead or a trail depending only on what came
     * before it -- so there is no way to answer this by looking near pos.
     * jlib.c's is_kanji1()/is_kanji2() scanned from the start for exactly
     * this reason; keeping that is not a regression.
     */
    while ((n = mb_seqlen(s + i)) > 0) {
        if (i == pos) return 1;
        if (i > pos) return 0;          /* pos landed inside this character */
        i += n;
    }
    return i == pos;                    /* the end of the string counts */
}

int
mb_trunc_bytes(s, max)
const char *s;
int max;
{
    int i = 0, n;

    if (max <= 0) return 0;

    while ((n = mb_complete(s + i)) > 0) {
        if (i + n > max) break;
        i += n;
    }
    return i;
}

int
mb_trunc_cols(s, cols)
const char *s;
int cols;
{
    int i = 0, n, w = 0;

    if (cols <= 0) return 0;

    while ((n = mb_complete(s + i)) > 0) {
        int cw = mb_width(s + i);

        if (w + cw > cols) break;
        w += cw;
        i += n;
    }
    return i;
}

int
mb_replace(buf, pos, rep)
char *buf;
int pos;
const char *rep;
{
    int n = mb_seqlen(buf + pos);
    int r = (int) strlen(rep);

    if (n != r)
        (void) memmove(buf + pos + r, buf + pos + n,
                       strlen(buf + pos + n) + 1);
    (void) memcpy(buf + pos, rep, r);
    return r;
}

const char *
mb_prev(base, p)
const char *base;
const char *p;
{
    const char *q, *last;
    int n;

    if (p <= base) return base;

    /*
     * The last character boundary strictly before p.  Stated that way
     * rather than as "step back one character" so that p landing inside a
     * character has an answer too: it comes back to that character's start
     * rather than skipping past it to the one before.  Callers reach that
     * case whenever a byte count came from somewhere that did not know
     * about characters, which during this migration is most of them.
     *
     * Walk forward, because EUC-JP is not self-synchronising -- a byte in
     * 0xA1..0xFE is a lead or a trail depending only on what came before.
     * Under UTF-8 this could step backwards directly, but doing it the
     * same way for both encodings keeps one behaviour to reason about,
     * and these strings are a line of text at most.
     */
    last = base;
    q = base;
    while (q < p && (n = mb_seqlen(q)) > 0) {
        last = q;
        q += n;
    }

#ifdef JP_INTERNAL_UTF8
    /*
     * Option B-1: a zero-width code point is not a character the player
     * can see, so backspace has to take it together with the base
     * character it hangs off.  Deleting the base and leaving the combining
     * mark orphaned is the UTF-8 form of the half-a-kanji bug is_kanji2()
     * existed to prevent.
     */
    while (last > base && mb_width(last) == 0)
        last = mb_prev(base, last);
#endif

    return last;
}

/*mbchar.c*/
