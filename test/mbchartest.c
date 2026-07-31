/*      mbchartest.c    */
/* JNetHack may be freely redistributed.  See license for details. */

/*
 * Self-test for japanese/mbchar.c, run twice by test/mbchartest.sh: once
 * built for EUC-JP and once with JP_INTERNAL_UTF8.
 *
 * Building it both ways is the point.  mbchar.c exists so that Phase 1 can
 * rewrite callers in terms of characters while the literals are still
 * EUC-JP, and Phase 3 can then flip the encoding without touching any of
 * them.  That promise only holds if both halves agree on what the answers
 * mean, so the expectations below are written once, in characters, and the
 * test data is given in both encodings.
 *
 * Strings are byte escapes rather than literal text so this file stays pure
 * ASCII and does not itself need converting in Phase 3.
 */

#include <stdio.h>
#include <string.h>

#include "hack.h"
#include "mbchar.h"

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

/*
 * The same text in both encodings.  KANJI is U+6F22 U+5B57 ("kanji"),
 * KANA is U+30A2 (katakana A), HANKAKU is the halfwidth katakana U+FF71,
 * which is the case that separates bytes from columns in EUC-JP: two bytes
 * but only one column.
 */
#ifdef JP_INTERNAL_UTF8
# define KANJI          "\346\274\242\345\255\227"      /* 3 bytes each */
# define KANA           "\343\202\242"                  /* 3 bytes */
# define HANKAKU        "\357\275\261"                  /* 3 bytes, 1 col */
# define KANJI_BYTES    3
# define KANA_BYTES     3
# define HANKAKU_BYTES  3
#else
# define KANJI          "\264\301\273\372"              /* 2 bytes each */
# define KANA           "\245\242"                      /* 2 bytes */
# define HANKAKU        "\216\261"                      /* SS2 + byte, 1 col */
# define KANJI_BYTES    2
# define KANA_BYTES     2
# define HANKAKU_BYTES  2
#endif

int
main()
{
    static const char kanji[] = KANJI;                  /* 2 characters */
    static const char mixed[] = "a" KANJI "b";          /* 4 characters */
    static const char hankaku[] = HANKAKU HANKAKU;      /* 2 characters */
    int i;

#ifdef JP_INTERNAL_UTF8
    (void) printf("mbchartest: UTF-8 mode\n");
#else
    (void) printf("mbchartest: EUC-JP mode\n");
#endif

    /* --- mb_seqlen ------------------------------------------------- */
    ok(mb_seqlen("") == 0, "mb_seqlen at the NUL");
    ok(mb_seqlen("a") == 1, "mb_seqlen of ASCII");
    ok(mb_seqlen(kanji) == KANJI_BYTES, "mb_seqlen of a kanji");
    ok(mb_seqlen(hankaku) == HANKAKU_BYTES, "mb_seqlen of halfwidth katakana");

    /* A lone lead byte at the end of the string must not claim bytes that
       are not there.  is_kanji1() and friends walked off the end here. */
    ok(mb_seqlen(KANJI + 0) > 0, "mb_seqlen made no progress");
    {
        char trunc[8];

        (void) memset(trunc, 0, sizeof trunc);
        trunc[0] = kanji[0];            /* lead byte, then the terminator */
        ok(mb_seqlen(trunc) == 1,
           "mb_seqlen claimed a continuation byte past the NUL");
    }

    /* --- mb_complete, and truncation of a malformed tail ----------- */
    /*
     * The case that made this function necessary.  christen_monst() cuts a
     * pet's name to PL_PSIZ bytes; if the cut lands inside a kanji the
     * buffer ends in a lead byte with nothing after it.  mb_seqlen() calls
     * that one byte, so that scanning always makes progress and never
     * reads past the terminator -- but truncation must not keep it, or the
     * name is stored half a character long.
     */
    {
        char orphan[8];
        int i;

        (void) memset(orphan, 0, sizeof orphan);
        orphan[0] = 'A';
        orphan[1] = 'B';
        orphan[2] = kanji[0];           /* lead byte, then the terminator */

        ok(mb_seqlen(orphan + 2) == 1,
           "mb_seqlen must still make progress over a lone lead byte");
        ok(mb_complete(orphan + 2) == 0,
           "mb_complete must reject a character the string cannot hold");
        ok(mb_trunc_bytes(orphan, 99) == 2,
           "truncation kept an incomplete trailing character");
        ok(mb_trunc_cols(orphan, 99) == 2,
           "column truncation kept an incomplete trailing character");

        /* well-formed input must be unaffected */
        ok(mb_complete(kanji) == KANJI_BYTES, "mb_complete of a whole kanji");
        ok(mb_complete("a") == 1, "mb_complete of ASCII");
        ok(mb_complete("") == 0, "mb_complete at the NUL");
        for (i = 0; i < 2; i++)
            ok(mb_complete(kanji + i * KANJI_BYTES) == KANJI_BYTES,
               "mb_complete disagreed with mb_seqlen on valid input");
    }

    /* --- counting -------------------------------------------------- */
    ok(mb_chars(kanji) == 2, "mb_chars of two kanji");
    ok(mb_chars(mixed) == 4, "mb_chars of a mixed string");
    ok(mb_chars("") == 0, "mb_chars of the empty string");

    ok(mb_colwidth(kanji) == 4, "two kanji are four columns");
    ok(mb_colwidth(mixed) == 6, "a + two kanji + b is six columns");
    ok(mb_colwidth(hankaku) == 2,
       "halfwidth katakana are one column each, not one per byte");

    /* --- boundaries ------------------------------------------------ */
    ok(mb_is_boundary(kanji, 0), "offset 0 is a boundary");
    ok(!mb_is_boundary(kanji, 1), "inside the first kanji is not a boundary");
    ok(mb_is_boundary(kanji, KANJI_BYTES), "the second kanji starts on one");
    ok(mb_is_boundary(kanji, 2 * KANJI_BYTES), "the end of the string counts");
    for (i = 1; i < KANJI_BYTES; i++)
        ok(!mb_is_boundary(kanji, i), "interior byte reported as a boundary");

    ok(mb_is_boundary(mixed, 1), "after leading ASCII");
    ok(mb_is_boundary(mixed, 1 + 2 * KANJI_BYTES), "before trailing ASCII");

    /* --- truncation by bytes --------------------------------------- */
    ok(mb_trunc_bytes(kanji, 0) == 0, "truncating to nothing");
    ok(mb_trunc_bytes(kanji, KANJI_BYTES - 1) == 0,
       "a partial first character must be dropped entirely");
    ok(mb_trunc_bytes(kanji, KANJI_BYTES) == KANJI_BYTES,
       "an exact fit keeps the character");
    ok(mb_trunc_bytes(kanji, 2 * KANJI_BYTES - 1) == KANJI_BYTES,
       "a partial second character must be dropped");
    ok(mb_trunc_bytes(kanji, 99) == 2 * KANJI_BYTES,
       "a generous limit keeps everything");

    /* --- truncation by columns ------------------------------------- */
    ok(mb_trunc_cols(kanji, 1) == 0, "a kanji does not fit in one column");
    ok(mb_trunc_cols(kanji, 2) == KANJI_BYTES, "one kanji in two columns");
    ok(mb_trunc_cols(kanji, 3) == KANJI_BYTES,
       "three columns still hold only one kanji");
    ok(mb_trunc_cols(kanji, 4) == 2 * KANJI_BYTES, "two kanji in four columns");

    /* The case that makes columns and bytes different questions. */
    ok(mb_trunc_cols(hankaku, 1) == HANKAKU_BYTES,
       "one halfwidth katakana fits in one column");
    ok(mb_trunc_bytes(hankaku, 1) == 0,
       "but not in one byte");

    ok(mb_trunc_cols(mixed, 3) == 1 + KANJI_BYTES,
       "a + one kanji is three columns");

    /* --- code points, and replacement ------------------------------ */
    /*
     * These are what let jrndm_replace() ask "which JIS row is this in"
     * without knowing how the character is spelled.  The expectations are
     * the same in both modes, which is the whole point.
     */
    ok(mb_decode(kanji) == 0x6F22L, "mb_decode of U+6F22");
    ok(mb_decode(kanji + KANJI_BYTES) == 0x5B57L, "mb_decode of U+5B57");
    ok(mb_decode("a") == 0x61L, "mb_decode of ASCII");
    ok(mb_decode("") == 0L, "mb_decode at the NUL");
    ok(mb_decode(HANKAKU) == 0xFF71L, "mb_decode of halfwidth katakana");

    {
        char out[MB_MAXBYTES + 1];
        int n;

        n = mb_encode(0x6F22L, out, MB_MAXBYTES);
        ok(n == KANJI_BYTES, "mb_encode length of U+6F22");
        out[n] = '\0';
        ok(memcmp(out, kanji, (size_t) n) == 0, "mb_encode bytes of U+6F22");
        ok(mb_decode(out) == 0x6F22L, "code point round trip");

        ok(mb_encode(0x6F22L, out, 1) == 0, "no room is reported, not truncated");

        /*
         * The one place the two encodings genuinely differ, and must:
         * option B accepts the whole of Unicode, but EUC-JP cannot spell
         * most of it.  mb_encode() says so rather than inventing bytes.
         */
#ifdef JP_INTERNAL_UTF8
        ok(mb_encode(0x20B9FL, out, MB_MAXBYTES) == 4,
           "UTF-8 must encode a code point beyond the BMP");
        ok(mb_encode(0x1F344L, out, MB_MAXBYTES) == 4, "UTF-8 must encode emoji");
#else
        ok(mb_encode(0x20B9FL, out, MB_MAXBYTES) == 0,
           "EUC-JP has no form for a code point beyond the BMP");
        ok(mb_encode(0x1F344L, out, MB_MAXBYTES) == 0,
           "EUC-JP has no form for an emoji");
#endif
    }

    /*
     * mb_replace must move the tail when the lengths differ.  KANJI is two
     * characters, so the second of them is at KANJI + KANJI_BYTES and the
     * expectations are built from that rather than written out.
     */
    {
        char work[64], want[64];

        /* wide -> narrower: the tail moves left */
        (void) strcpy(work, KANJI "tail");
        (void) strcpy(want, "xy");
        (void) strcat(want, KANJI + KANJI_BYTES);       /* the second kanji */
        (void) strcat(want, "tail");
        ok(mb_replace(work, 0, "xy") == 2,
           "mb_replace returned the replacement length");
        ok(strcmp(work, want) == 0, "mb_replace shrank and kept the tail");

        /* same length: nothing moves */
        (void) strcpy(work, "a" KANJI "b");
        (void) strcpy(want, "a" KANJI "b");
        ok(mb_replace(work, 1, KANJI) == 2 * KANJI_BYTES,
           "replacing one character with two reports both");
        ok(strlen(work) == strlen(want) + KANJI_BYTES,
           "mb_replace grew the string by one character");
        ok(mb_chars(work) == 5, "a + three kanji + b");

        /* the engrave.c case: a wide character blanked with two spaces */
        (void) strcpy(work, "ab" KANJI);
        (void) strcpy(want, "ab  ");
        (void) strcat(want, KANJI + KANJI_BYTES);
        ok(mb_replace(work, 2, "  ") == 2, "blanking returns two");
        ok(strcmp(work, want) == 0,
           "blanking a wide character kept what followed it");
        ok(mb_colwidth(work) == 6,
           "two spaces occupy the two columns the kanji did");
    }

    /* --- stepping back --------------------------------------------- */
    {
        const char *end = kanji + 2 * KANJI_BYTES;
        const char *p;

        p = mb_prev(kanji, end);
        ok(p == kanji + KANJI_BYTES, "mb_prev over the last kanji");
        p = mb_prev(kanji, p);
        ok(p == kanji, "mb_prev over the first kanji");
        p = mb_prev(kanji, p);
        ok(p == kanji, "mb_prev walked below base");

        /* from inside a character, not just from a boundary */
        p = mb_prev(kanji, kanji + KANJI_BYTES + 1);
        ok(p == kanji + KANJI_BYTES,
           "mb_prev from inside a character did not land on its start");

        /* it must always make progress */
        {
            const char *q = end;
            int guard = 0;

            while (q > kanji) {
                const char *nq = mb_prev(kanji, q);
                if (nq >= q) { ok(0, "mb_prev made no progress"); break; }
                q = nq;
                if (++guard > 32) { ok(0, "mb_prev looped"); break; }
            }
            checks++;
        }
    }

#ifdef JP_INTERNAL_UTF8
    /*
     * Option B-1, reachable only once the encoding is UTF-8: KA followed by
     * U+3099 (combining voiced sound mark) is one character to the player.
     * One backspace has to take both.
     */
    {
        static const char daku[] = "\343\201\213\343\202\231";  /* KA + U+3099 */

        ok(mb_width(daku) == 2, "KA is two columns");
        ok(mb_width(daku + 3) == 0, "U+3099 is zero columns");
        ok(mb_colwidth(daku) == 2,
           "decomposed GA is two columns, not four");
        ok(mb_prev(daku, daku + 6) == daku,
           "backspace over a combining sequence left the mark orphaned");
    }
#endif

    (void) printf("%d checks, %d failures\n", checks, failures);
    return failures ? 1 : 0;
}

/*mbchartest.c*/
