/*      utf8.c  */
/* JNetHack may be freely redistributed.  See license for details. */

/* UTF-8 primitives.  The rationale for every choice made here is in
   include/utf8.h; the migration this belongs to is in UTF8-PLAN.md. */

#include "hack.h"
#include "utf8.h"

struct cprange {
    long lo, hi;
};

/*
 * Zero-width code points: combining marks, variation selectors, joiners.
 *
 * Under plan option B-1 these stay in the game's strings -- names, engravings
 * and fruit names round-trip through save files intact -- but they are not
 * given a cell of their own, because struct sdl_cell holds a single code
 * point.  The visible effect is that a decomposed sequence renders as its
 * base character alone.  Nothing is lost from the data, which is the property
 * that matters and the one option B was chosen for.
 *
 * This list is deliberately not the whole of Unicode's Mn/Me/Cf categories --
 * that would be a generated table, and the plan does not need one yet.  It
 * covers what can actually arrive from a Japanese IME plus the widely used
 * Latin, Hebrew, Arabic, Thai and Hangul marks.
 */
static const struct cprange zero_width[] = {
    {0x00ADL, 0x00ADL},         /* soft hyphen */
    {0x0300L, 0x036FL},         /* combining diacritical marks */
    {0x0483L, 0x0489L},         /* Cyrillic combining */
    {0x0591L, 0x05BDL},         /* Hebrew points */
    {0x05BFL, 0x05BFL},
    {0x05C1L, 0x05C2L},
    {0x05C4L, 0x05C5L},
    {0x05C7L, 0x05C7L},
    {0x0610L, 0x061AL},         /* Arabic */
    {0x064BL, 0x065FL},
    {0x0670L, 0x0670L},
    {0x0E31L, 0x0E31L},         /* Thai */
    {0x0E34L, 0x0E3AL},
    {0x0E47L, 0x0E4EL},
    {0x1160L, 0x11FFL},         /* Hangul conjoining jamo: vowel + final */
    {0x135DL, 0x135FL},         /* Ethiopic combining */
    {0x1AB0L, 0x1AFFL},         /* combining diacriticals extended */
    {0x1DC0L, 0x1DFFL},         /* combining diacriticals supplement */
    {0x200BL, 0x200FL},         /* ZWSP, ZWNJ, ZWJ, LRM, RLM */
    {0x2028L, 0x202EL},         /* line/para separators, bidi overrides */
    {0x2060L, 0x2064L},         /* word joiner, invisible operators */
    {0x206AL, 0x206FL},         /* deprecated format characters */
    {0x20D0L, 0x20FFL},         /* combining marks for symbols */
    {0x3099L, 0x309AL},         /* COMBINING (SEMI-)VOICED SOUND MARK.
                                   Inside the width-2 kana block below, which
                                   is why the zero-width scan runs first. */
    {0xFE00L, 0xFE0FL},         /* variation selectors */
    {0xFE20L, 0xFE2FL},         /* combining half marks */
    {0xFEFFL, 0xFEFFL},         /* BOM / zero width no-break space */
    {0xFFF9L, 0xFFFBL},         /* interlinear annotation */
    {0xE0001L, 0xE0001L},       /* language tag */
    {0xE0020L, 0xE007FL},       /* tag characters */
    {0xE0100L, 0xE01EFL},       /* variation selectors supplement */
    {0L, 0L}
};

/*
 * Double-width code points.  Carried over unchanged from sdlterm.c's
 * sdl_cp_width(), which NH_SDL_WIDTHTEST already exercises against the real
 * grid, so this list is not new ground.
 */
static const struct cprange double_width[] = {
    {0x1100L, 0x115FL},         /* Hangul Jamo initial */
    {0x2E80L, 0x303EL},         /* CJK radicals, Kangxi */
    {0x3041L, 0x33FFL},         /* kana, Hangul, CJK compatibility */
    {0x3400L, 0x4DBFL},         /* CJK ext A */
    {0x4E00L, 0x9FFFL},         /* CJK unified */
    {0xA000L, 0xA4CFL},         /* Yi */
    {0xAC00L, 0xD7A3L},         /* Hangul syllables */
    {0xF900L, 0xFAFFL},         /* CJK compatibility ideographs */
    {0xFE10L, 0xFE19L},         /* vertical forms */
    {0xFE30L, 0xFE6FL},         /* CJK compatibility forms */
    {0xFF00L, 0xFF60L},         /* fullwidth ASCII */
    {0xFFE0L, 0xFFE6L},         /* fullwidth signs */
    {0x1F300L, 0x1F64FL},       /* emoji */
    {0x1F900L, 0x1F9FFL},
    {0x20000L, 0x3FFFDL},       /* CJK ext B and beyond */
    {0L, 0L}
};

/* Smallest code point each sequence length is allowed to encode.  Indexed by
   length, so [0] and [1] are unused.  Rejecting anything below the entry is
   what stops overlong forms -- C0 AF decoding to '/' and slipping past a
   path check written in terms of bytes. */
static const long utf8_minval[5] = { 0L, 0L, 0x80L, 0x800L, 0x10000L };

static boolean FDECL(in_ranges, (long, const struct cprange *));

static boolean
in_ranges(cp, tab)
long cp;
const struct cprange *tab;
{
    const struct cprange *r;

    for (r = tab; r->hi; r++) {
        if (cp < r->lo) break;          /* tables are sorted */
        if (cp <= r->hi) return TRUE;
    }
    return FALSE;
}

int
utf8_decode(s, cp)
const char *s;
long *cp;
{
    const unsigned char *p = (const unsigned char *) s;
    long v;
    int n, i;

    if (!p[0]) {
        *cp = 0L;
        return 0;
    }

    if (p[0] < 0x80) {                  /* the overwhelmingly common case */
        *cp = (long) p[0];
        return 1;
    }

    if ((p[0] & 0xE0) == 0xC0) {
        n = 2;
        v = (long) (p[0] & 0x1F);
    } else if ((p[0] & 0xF0) == 0xE0) {
        n = 3;
        v = (long) (p[0] & 0x0F);
    } else if ((p[0] & 0xF8) == 0xF0) {
        n = 4;
        v = (long) (p[0] & 0x07);
    } else {
        /* a continuation byte with no lead, or F8..FF which UTF-8 never
           uses.  Resynchronise by one byte. */
        *cp = UTF8_REPLACEMENT;
        return 1;
    }

    for (i = 1; i < n; i++) {
        /*
         * A NUL is not a continuation byte, so a truncated sequence at the
         * end of a buffer fails here rather than being read through.  This
         * is the overread that the vendored utf8codepoint() has, and the
         * reason this function exists at all.
         */
        if ((p[i] & 0xC0) != 0x80) {
            *cp = UTF8_REPLACEMENT;
            return 1;
        }
        v = (v << 6) | (long) (p[i] & 0x3F);
    }

    if (v < utf8_minval[n]                      /* overlong */
        || (v >= 0xD800L && v <= 0xDFFFL)       /* UTF-16 surrogate half */
        || v > UTF8_MAXCP) {
        *cp = UTF8_REPLACEMENT;
        return 1;
    }

    *cp = v;
    return n;
}

int
utf8_encode(cp, buf, n)
long cp;
char *buf;
int n;
{
    if (cp < 0L || cp > UTF8_MAXCP || (cp >= 0xD800L && cp <= 0xDFFFL))
        cp = UTF8_REPLACEMENT;

    if (cp < 0x80L) {
        if (n < 1) return 0;
        buf[0] = (char) cp;
        return 1;
    }
    if (cp < 0x800L) {
        if (n < 2) return 0;
        buf[0] = (char) (0xC0 | (cp >> 6));
        buf[1] = (char) (0x80 | (cp & 0x3F));
        return 2;
    }
    if (cp < 0x10000L) {
        if (n < 3) return 0;
        buf[0] = (char) (0xE0 | (cp >> 12));
        buf[1] = (char) (0x80 | ((cp >> 6) & 0x3F));
        buf[2] = (char) (0x80 | (cp & 0x3F));
        return 3;
    }
    /* Four bytes.  sdl_dump_grid() used to fall into its three-byte branch
       here and shift the top bits off the end. */
    if (n < 4) return 0;
    buf[0] = (char) (0xF0 | (cp >> 18));
    buf[1] = (char) (0x80 | ((cp >> 12) & 0x3F));
    buf[2] = (char) (0x80 | ((cp >> 6) & 0x3F));
    buf[3] = (char) (0x80 | (cp & 0x3F));
    return 4;
}

int
utf8_cpwidth(cp)
long cp;
{
    if (cp < 0x00ADL) return 1;         /* ASCII and Latin-1 shortcut */

    if (in_ranges(cp, zero_width)) return 0;    /* must precede the next */
    if (in_ranges(cp, double_width)) return 2;

    return 1;
}

int
utf8_seqlen(s)
const char *s;
{
    long cp;

    return utf8_decode(s, &cp);
}

int
utf8_colwidth(s)
const char *s;
{
    long cp;
    int n, w = 0;

    while ((n = utf8_decode(s, &cp)) > 0) {
        w += utf8_cpwidth(cp);
        s += n;
    }
    return w;
}

const char *
utf8_prev(base, p)
const char *base;
const char *p;
{
    const char *q;
    long cp;
    int back;

    while (p > base) {
        /* Walk back to the lead byte of the code point ending at p.  The
           bound stops a run of stray continuation bytes from dragging us
           further than any real sequence could reach. */
        q = p - 1;
        back = 1;
        while (q > base && ((unsigned char) *q & 0xC0) == 0x80
               && back < UTF8_MAXBYTES) {
            q--;
            back++;
        }

        (void) utf8_decode(q, &cp);
        if (utf8_cpwidth(cp) != 0)
            return q;                   /* base character: this is the one */

        p = q;                          /* zero-width: keep going back */
    }
    return base;
}

int
utf8_valid(s, bad)
const char *s;
const char **bad;
{
    long cp;
    int n;

    while (*s) {
        n = utf8_decode(s, &cp);
        /*
         * n == 1 together with U+FFFD is exactly utf8_decode's rejection
         * signal, and it cannot collide with a replacement character that
         * was genuinely in the text: that one is three bytes, so it comes
         * back with n == 3.  Testing the lead byte as well -- which an
         * earlier draft of this did, to "tell the two apart" -- only
         * created a hole, because a lone truncated 0xEF then looked like
         * the legitimate case and was accepted.
         */
        if (n == 1 && cp == UTF8_REPLACEMENT) {
            if (bad) *bad = s;
            return 0;
        }
        s += n;
    }
    if (bad) *bad = (const char *) 0;
    return 1;
}

/*utf8.c*/
