/*      utf8test.c      */
/* JNetHack may be freely redistributed.  See license for details. */

/*
 * Self-test for japanese/utf8.c.  Build and run it with test/utf8test.sh.
 *
 * It is a standalone program rather than a hook in the game because the
 * things worth testing here -- malformed input, surrogates, code points
 * past U+FFFF -- are exactly the ones that are awkward to type into a
 * running NetHack.
 *
 * Where the vendored sheredom header agrees with us it is used as an
 * independent check, so a mistake would have to be made twice to pass.
 * Where it deliberately disagrees (it does not reject surrogates,
 * out-of-range values, or truncated sequences) the difference is asserted,
 * so that a future update to the vendored file cannot quietly change what
 * we depend on without this failing.
 */

#include <stdio.h>
#include <string.h>

#include "hack.h"
#include "utf8.h"
#include "sheredom_utf8.h"      /* the independent check; see utf8.h on why
                                   this is a separate, opt-in include */

static int failures = 0;
static int checks = 0;

static void
ok(cond, what)
int cond;
const char *what;
{
    checks++;
    if (!cond) {
        failures++;
        (void) printf("    FAIL: %s\n", what);
    }
}

/* --- decoding ------------------------------------------------------ */

static void
test_decode()
{
    static const struct {
        const char *bytes;
        int len;
        long cp;
        const char *what;
    } good[] = {
        { "A",                  1, 0x41L,     "ASCII" },
        { "\xC3\xA9",           2, 0xE9L,     "2-byte U+00E9" },
        { "\xE6\xBC\xA2",       3, 0x6F22L,   "3-byte U+6F22 kanji" },
        { "\xEF\xBD\xB1",       3, 0xFF71L,   "3-byte U+FF71 halfwidth kana" },
        { "\xF0\xA0\xAE\x9F",   4, 0x20B9FL,  "4-byte U+20B9F CJK ext B" },
        { "\xF0\x9F\x8D\x84",   4, 0x1F344L,  "4-byte U+1F344 emoji" },
        { "\xF4\x8F\xBF\xBF",   4, 0x10FFFFL, "4-byte U+10FFFF, the ceiling" },
        { 0, 0, 0, 0 }
    };
    /* Every one of these must come back as one byte consumed and U+FFFD. */
    static const struct {
        const char *bytes;
        const char *what;
    } bad[] = {
        { "\xED\xA0\x80",       "surrogate U+D800 (utf8nvalid accepts this)" },
        { "\xED\xBF\xBF",       "surrogate U+DFFF" },
        { "\xF4\x90\x80\x80",   "U+110000, past the ceiling" },
        { "\xF7\xBF\xBF\xBF",   "U+1FFFFF, past the ceiling" },
        { "\xC0\xAF",           "overlong '/' -- the path-check bypass" },
        { "\xE0\x80\xAF",       "overlong 3-byte" },
        { "\xF0\x80\x80\xAF",   "overlong 4-byte" },
        { "\x80",               "continuation byte with no lead" },
        { "\xFE",               "0xFE, never valid in UTF-8" },
        { "\xFF",               "0xFF, never valid in UTF-8" },
        { 0, 0 }
    };
    long cp;
    int i, n;

    (void) printf("  decode:\n");

    for (i = 0; good[i].bytes; i++) {
        cp = -1L;
        n = utf8_decode(good[i].bytes, &cp);
        if (n != good[i].len || cp != good[i].cp) {
            (void) printf("    FAIL: %s: got len %d cp U+%lX, want len %d "
                          "cp U+%lX\n", good[i].what, n, cp,
                          good[i].len, good[i].cp);
            failures++;
        }
        checks++;

        /* the vendored decoder must agree on everything well formed */
        {
            utf8_int32_t vcp = 0;
            const char *next =
                (const char *) utf8codepoint(good[i].bytes, &vcp);
            ok((long) vcp == good[i].cp
               && (int) (next - good[i].bytes) == good[i].len,
               "vendored utf8codepoint disagrees on well-formed input");
        }
    }

    for (i = 0; bad[i].bytes; i++) {
        cp = -1L;
        n = utf8_decode(bad[i].bytes, &cp);
        if (n != 1 || cp != UTF8_REPLACEMENT) {
            (void) printf("    FAIL: %s: got len %d cp U+%lX, want len 1 "
                          "U+FFFD\n", bad[i].what, n, cp);
            failures++;
        }
        checks++;
    }

    /* empty string */
    cp = -1L;
    ok(utf8_decode("", &cp) == 0 && cp == 0L, "empty string decodes to 0");
}

/*
 * The overread.  A lead byte followed immediately by the terminator: our
 * decoder must stop at the NUL, the vendored one is known to run past it.
 * Asserting both directions means an update to sheredom_utf8.h that fixed
 * its behaviour would show up here as a failure to re-read, not as a silent
 * change under us.
 */
static void
test_truncated()
{
    char buf[8];
    long cp;
    int n, i;
    static const char *lead[] = { "\xC3", "\xE6", "\xE6\xBC", "\xF0",
                                  "\xF0\xA0", "\xF0\xA0\xAE", 0 };

    (void) printf("  truncated sequences at a buffer end:\n");

    for (i = 0; lead[i]; i++) {
        size_t len = strlen(lead[i]);

        (void) memset(buf, 0x7F, sizeof buf);   /* poison past the NUL */
        (void) memcpy(buf, lead[i], len);
        buf[len] = '\0';
        cp = -1L;
        n = utf8_decode(buf, &cp);
        if (n != 1 || cp != UTF8_REPLACEMENT) {
            (void) printf("    FAIL: truncated %d-byte lead: got len %d "
                          "cp U+%lX, want len 1 U+FFFD\n",
                          (int) strlen(lead[i]), n, cp);
            failures++;
        }
        checks++;
        ok(n <= (int) strlen(buf),
           "utf8_decode consumed more bytes than the string holds");
    }

    /* Document the vendored behaviour we are working around. */
    {
        utf8_int32_t vcp = 0;
        const char *next;

        (void) memset(buf, 0x7F, sizeof buf);
        buf[0] = (char) 0xE6;
        buf[1] = '\0';
        next = (const char *) utf8codepoint(buf, &vcp);
        ok(next - buf > 1,
           "vendored utf8codepoint no longer overreads -- re-read utf8.h's "
           "rationale, this test's premise has changed");
    }
}

/* --- encoding ------------------------------------------------------ */

static void
test_encode()
{
    static const long cps[] = {
        0x41L, 0xE9L, 0x6F22L, 0xFF71L, 0x20B9FL, 0x1F344L, 0x10FFFFL, -1L
    };
    char buf[UTF8_MAXBYTES + 1];
    char vbuf[UTF8_MAXBYTES + 1];
    long back;
    int i, n, m;

    (void) printf("  encode, and round trip:\n");

    for (i = 0; cps[i] != -1L; i++) {
        n = utf8_encode(cps[i], buf, sizeof buf);
        buf[n] = '\0';

        /* length must match the vendored idea of it */
        ok(n == (int) utf8codepointsize((utf8_int32_t) cps[i]),
           "utf8_encode length disagrees with utf8codepointsize");

        /* bytes must match what the vendored encoder produces */
        (void) memset(vbuf, 0, sizeof vbuf);
        (void) utf8catcodepoint(vbuf, (utf8_int32_t) cps[i], sizeof vbuf);
        ok(memcmp(buf, vbuf, (size_t) n) == 0,
           "utf8_encode bytes disagree with utf8catcodepoint");

        /* and it must decode back to what went in */
        m = utf8_decode(buf, &back);
        if (m != n || back != cps[i]) {
            (void) printf("    FAIL: U+%lX did not round trip (got U+%lX, "
                          "%d bytes)\n", cps[i], back, m);
            failures++;
        }
        checks++;
    }

    /* U+10FFFF needs all four bytes; a three-byte buffer must refuse rather
       than truncate.  This is the shape of the old sdl_dump_grid() bug. */
    ok(utf8_encode(0x10FFFFL, buf, 3) == 0,
       "utf8_encode wrote a 4-byte code point into 3 bytes");
    ok(utf8_encode(0x6F22L, buf, 2) == 0,
       "utf8_encode wrote a 3-byte code point into 2 bytes");

    /* invalid input becomes U+FFFD rather than an invalid sequence */
    n = utf8_encode(0xD800L, buf, sizeof buf);
    buf[n] = '\0';
    (void) utf8_decode(buf, &back);
    ok(back == UTF8_REPLACEMENT, "encoding a surrogate did not yield U+FFFD");

    n = utf8_encode(0x110000L, buf, sizeof buf);
    buf[n] = '\0';
    (void) utf8_decode(buf, &back);
    ok(back == UTF8_REPLACEMENT, "encoding U+110000 did not yield U+FFFD");
}

/*
 * Exhaustive round trip.  Cheap enough to just do all of it, and it is the
 * check that would catch a bad boundary in either direction.
 */
static void
test_roundtrip_all()
{
    char buf[UTF8_MAXBYTES + 1];
    long cp, back;
    int n, m, bad = 0;

    (void) printf("  exhaustive round trip U+0000..U+10FFFF:\n");

    for (cp = 1L; cp <= UTF8_MAXCP; cp++) {
        if (cp >= 0xD800L && cp <= 0xDFFFL) continue;   /* not encodable */
        n = utf8_encode(cp, buf, sizeof buf);
        buf[n] = '\0';
        m = utf8_decode(buf, &back);
        if (m != n || back != cp) {
            if (bad++ < 5)
                (void) printf("    FAIL: U+%lX -> %d bytes -> U+%lX (%d)\n",
                              cp, n, back, m);
        }
    }
    checks++;
    if (bad) {
        failures++;
        (void) printf("    (%d code points failed to round trip)\n", bad);
    }
}

/* --- widths -------------------------------------------------------- */

static void
test_width()
{
    static const struct {
        long cp;
        int w;
        const char *what;
    } t[] = {
        { 0x41L,     1, "'A'" },
        { 0x6F22L,   2, "U+6F22 kanji" },
        { 0x3042L,   2, "U+3042 hiragana" },
        { 0xFF71L,   1, "U+FF71 halfwidth katakana" },
        { 0xFF21L,   2, "U+FF21 fullwidth 'A'" },
        { 0x20B9FL,  2, "U+20B9F CJK ext B" },
        { 0x1F344L,  2, "U+1F344 emoji" },
        { 0x0301L,   0, "U+0301 combining acute" },
        { 0x3099L,   0, "U+3099 combining voiced mark (inside the kana block)" },
        { 0x309AL,   0, "U+309A combining semi-voiced mark" },
        { 0xFE0FL,   0, "U+FE0F variation selector 16" },
        { 0xE0101L,  0, "U+E0101 variation selector 18 (IVS, 4 bytes)" },
        { 0x200DL,   0, "U+200D zero width joiner" },
        { 0x3040L,   1, "U+3040 unassigned, just below the kana block" },
        { 0x1100L,   2, "U+1100 Hangul Jamo initial, lower bound" },
        { 0x10FFL,   1, "U+10FF, just below that bound" },
        { 0L, 0, 0 }
    };
    int i, w;

    (void) printf("  widths:\n");

    for (i = 0; t[i].what; i++) {
        w = utf8_cpwidth(t[i].cp);
        if (w != t[i].w) {
            (void) printf("    FAIL: %s: width %d, want %d\n",
                          t[i].what, w, t[i].w);
            failures++;
        }
        checks++;
    }

    /* string widths, including the B-1 case: a decomposed daku-on takes the
       same two cells as the precomposed form */
    ok(utf8_colwidth("abc") == 3, "colwidth of \"abc\"");
    ok(utf8_colwidth("\xE6\xBC\xA2\xE5\xAD\x97") == 4, "colwidth of two kanji");
    ok(utf8_colwidth("\xE3\x81\x8B\xE3\x82\x99") == 2,
       "colwidth of decomposed GA (KA + U+3099) is 2, not 4");
    ok(utf8_colwidth("\xE3\x81\x8C") == 2, "colwidth of precomposed GA is 2");
}

/* --- backspace ----------------------------------------------------- */

static void
test_prev()
{
    /* "a" + kanji U+6F22 + KA U+304B + U+3099 (combining voiced mark) */
    static const char s[] = "a\xE6\xBC\xA2\xE3\x81\x8B\xE3\x82\x99";
    const char *end = s + sizeof s - 1;
    const char *p;

    (void) printf("  utf8_prev (backspace):\n");

    /* One backspace must remove KA *and* the combining mark that hangs off
       it -- 6 bytes -- not just the mark.  Leaving the mark orphaned is the
       UTF-8 form of the half-a-kanji bug is_kanji2() guards against. */
    p = utf8_prev(s, end);
    ok(p == s + 4, "backspace over a combining sequence did not take the base");

    p = utf8_prev(s, p);
    ok(p == s + 1, "backspace over a 3-byte kanji");

    p = utf8_prev(s, p);
    ok(p == s, "backspace over ASCII");

    p = utf8_prev(s, p);
    ok(p == s, "utf8_prev walked below base");

    /* it must always make progress, from every byte offset, for any input */
    {
        const char *q = end;
        int guard = 0;

        while (q > s) {
            const char *nq = utf8_prev(s, q);
            if (nq >= q) {
                ok(0, "utf8_prev failed to make progress");
                break;
            }
            q = nq;
            if (++guard > 64) {
                ok(0, "utf8_prev looped");
                break;
            }
        }
        checks++;
    }

    /* a run of stray continuation bytes must not drag it back forever */
    {
        static const char junk[] = "x\x80\x80\x80\x80\x80\x80\x80\x80";
        const char *jend = junk + sizeof junk - 1;

        p = utf8_prev(junk, jend);
        ok(p > junk && p < jend,
           "utf8_prev over stray continuation bytes went too far");
    }
}

/* --- validation ---------------------------------------------------- */

static void
test_valid()
{
    const char *bad;

    (void) printf("  utf8_valid:\n");

    ok(utf8_valid("hello \xE6\xBC\xA2\xE5\xAD\x97", &bad) && !bad,
       "well-formed string rejected");
    ok(utf8_valid("\xF0\xA0\xAE\x9F", &bad) && !bad,
       "4-byte CJK ext B rejected -- option B requires accepting this");
    ok(utf8_valid("\xEF\xBF\xBD", &bad) && !bad,
       "a genuine U+FFFD in the text was treated as an error");

    ok(!utf8_valid("ok\xED\xA0\x80", &bad), "surrogate accepted");
    ok(!utf8_valid("ok\xF4\x90\x80\x80", &bad), "U+110000 accepted");
    ok(!utf8_valid("ok\xC0\xAF", &bad), "overlong accepted");
    ok(!utf8_valid("ok\xE6", &bad), "truncated sequence accepted");

    /*
     * A truncated 0xEF specifically.  U+FFFD is EF BF BD, so 0xEF is the
     * one lead byte that a rejection could be confused with the real
     * replacement character -- and an earlier version of utf8_valid()
     * special-cased it and let this through.  Every lead byte, so that no
     * future shortcut can reintroduce the hole for a different one.
     */
    ok(!utf8_valid("ok\xEF", &bad), "truncated 0xEF accepted");
    {
        static const char *trunc[] = { "\xC3", "\xE6", "\xEF", "\xF0", "\xF4",
                                       "\xEF\xBF", "\xF0\xA0\xAE", 0 };
        char buf[16];
        int i;

        for (i = 0; trunc[i]; i++) {
            (void) strcpy(buf, "ok");
            (void) strcat(buf, trunc[i]);
            if (utf8_valid(buf, &bad)) {
                (void) printf("    FAIL: truncated lead 0x%02X accepted\n",
                              (unsigned char) trunc[i][0]);
                failures++;
            }
            checks++;
        }
    }

    /* and it must point at the offending byte, not the start */
    ok(!utf8_valid("ok\xC0\xAF", &bad) && bad != 0 && *bad == (char) 0xC0,
       "utf8_valid did not point at the bad byte");
}

int
main()
{
    (void) printf("utf8test: japanese/utf8.c\n");

    test_decode();
    test_truncated();
    test_encode();
    test_roundtrip_all();
    test_width();
    test_prev();
    test_valid();

    (void) printf("%d checks, %d failures\n", checks, failures);
    return failures ? 1 : 0;
}

/*utf8test.c*/
