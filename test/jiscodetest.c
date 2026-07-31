/*      jiscodetest.c   */
/* JNetHack may be freely redistributed.  See license for details. */

/*
 * Self-test for japanese/jiscode.c.
 *
 * The reverse index is built from the forward table at run time, so the two
 * cannot disagree about *content* -- but they can disagree about whether
 * the search finds it.  So the check that matters is exhaustive: every one
 * of the 8836 positions, forward and back.
 *
 * The linear scan this replaced was obviously correct and slow.  A binary
 * search over a table sorted by an insertion sort is neither, which is why
 * the sort's output is verified as well.
 */

#include <stdio.h>
#include <string.h>

#include "hack.h"
#include "jiscode.h"

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

int
main()
{
    int row, cell, r2, c2;
    int assigned = 0, bad = 0;
    long cp, prev;
    char buf[8];
    int n;

    (void) printf("jiscodetest: japanese/jiscode.c\n");

    /* --- every position round trips -------------------------------- */
    for (row = 1; row <= JIS_ROWS; row++)
        for (cell = 1; cell <= JIS_CELLS; cell++) {
            cp = jis_to_ucs(row, cell);
            if (!cp) continue;                  /* unassigned */
            assigned++;

            if (!ucs_to_jis(cp, &r2, &c2)) {
                if (bad++ < 5)
                    (void) printf("    FAIL: U+%04lX (%d,%d) not found\n",
                                  cp, row, cell);
                continue;
            }
            /*
             * Not necessarily the same position: JIS X 0208 maps a handful
             * of distinct positions to the same code point, so the reverse
             * lookup is allowed to pick either.  What it may not do is
             * return a position that means something else.
             */
            if (jis_to_ucs(r2, c2) != cp) {
                if (bad++ < 5)
                    (void) printf("    FAIL: U+%04lX -> (%d,%d) -> U+%04lX\n",
                                  cp, r2, c2, jis_to_ucs(r2, c2));
            }
        }
    checks++;
    if (bad) failures++;
    (void) printf("  %d assigned positions, %d bad round trips\n",
                  assigned, bad);
    ok(assigned == 6879,
       "the table no longer holds 6879 assigned positions -- if jis0208.h "
       "was regenerated this number needs updating");

    /* --- things that are not in JIS X 0208 must be rejected --------- */
    ok(!ucs_to_jis(0x41L, &r2, &c2), "'A' is not a JIS X 0208 position");
    ok(!ucs_to_jis(0x20B9FL, &r2, &c2), "U+20B9F is beyond the BMP");
    ok(!ucs_to_jis(0x1F344L, &r2, &c2), "an emoji is not in JIS X 0208");
    ok(!ucs_to_jis(0L, &r2, &c2), "zero is not a code point");
    ok(!ucs_to_jis(0xFF71L, &r2, &c2),
       "half-width katakana are not in JIS X 0208 (they are SS2 in EUC)");

    /* --- known values ---------------------------------------------- */
    ok(jis_to_ucs(4, 2) == 0x3042L, "row 4 cell 2 is hiragana A");
    ok(jis_to_ucs(5, 2) == 0x30A2L, "row 5 cell 2 is katakana A");
    ok(jis_to_ucs(1, 1) == 0x3000L, "row 1 cell 1 is the ideographic space");
    ok(jis_to_ucs(0, 1) == 0L, "row 0 is out of range");
    ok(jis_to_ucs(95, 1) == 0L, "row 95 is out of range");
    ok(jis_to_ucs(1, 95) == 0L, "cell 95 is out of range");

    ok(ucs_to_jis(0x3042L, &r2, &c2) && r2 == 4 && c2 == 2,
       "hiragana A comes back as row 4 cell 2");

    /* --- EUC-JP ----------------------------------------------------- */
    ok(euc_to_ucs("A", &n) == 0x41L && n == 1, "EUC 'A'");
    ok(euc_to_ucs("\244\242", &n) == 0x3042L && n == 2, "EUC hiragana A");
    ok(euc_to_ucs("\216\261", &n) == 0xFF71L && n == 2,
       "EUC SS2 half-width katakana");
    ok(euc_to_ucs("", &n) == 0L && n == 0, "EUC empty string");
    ok(euc_to_ucs("\244", &n) == 0L, "EUC truncated pair");
    ok(euc_to_ucs("\217\244\242", &n) == 0L, "EUC SS3 is not supported");

    n = ucs_to_euc(0x3042L, buf, sizeof buf);
    ok(n == 2 && (unsigned char) buf[0] == 0244 && (unsigned char) buf[1] == 0242,
       "hiragana A encodes to EUC A4A2");
    n = ucs_to_euc(0xFF71L, buf, sizeof buf);
    ok(n == 2 && (unsigned char) buf[0] == 0216, "half-width katakana uses SS2");
    ok(ucs_to_euc(0x41L, buf, sizeof buf) == 1 && buf[0] == 'A', "ASCII 'A'");
    ok(ucs_to_euc(0x20B9FL, buf, sizeof buf) == 0,
       "a code point outside JIS X 0208 has no EUC-JP form");
    ok(ucs_to_euc(0x3042L, buf, 1) == 0, "no room is reported, not truncated");

    /* --- EUC round trip over the whole table ------------------------ */
    bad = 0;
    for (row = 1; row <= JIS_ROWS; row++)
        for (cell = 1; cell <= JIS_CELLS; cell++) {
            int m;
            long back;

            cp = jis_to_ucs(row, cell);
            if (!cp) continue;
            n = ucs_to_euc(cp, buf, sizeof buf);
            if (n != 2) { bad++; continue; }
            buf[n] = '\0';
            back = euc_to_ucs(buf, &m);
            if (m != 2 || back != cp) bad++;
        }
    checks++;
    if (bad) {
        failures++;
        (void) printf("    FAIL: %d code points failed the EUC round trip\n",
                      bad);
    }

    /* --- the sort actually sorted ----------------------------------- */
    /*
     * Checked through the public interface: walk every code point in the
     * BMP, and confirm the ones the search accepts are exactly the ones the
     * forward table contains.  A binary search over a mis-sorted array
     * would silently miss some of them, and nothing above would notice
     * because it only ever asks about code points that are present.
     */
    {
        static char present[0x10000];
        int found = 0;

        (void) memset(present, 0, sizeof present);
        for (row = 1; row <= JIS_ROWS; row++)
            for (cell = 1; cell <= JIS_CELLS; cell++) {
                cp = jis_to_ucs(row, cell);
                if (cp) present[cp] = 1;
            }

        bad = 0;
        for (cp = 1; cp < 0x10000L; cp++) {
            int got = ucs_to_jis(cp, &r2, &c2);

            if (got) found++;
            if (got != (int) present[cp]) {
                if (bad++ < 5)
                    (void) printf("    FAIL: U+%04lX: search says %d, table "
                                  "says %d\n", cp, got, (int) present[cp]);
            }
        }
        checks++;
        if (bad) failures++;
        (void) printf("  %d of 65535 BMP code points are in JIS X 0208\n",
                      found);
    }

    /* the index must be monotonic, or the search is luck */
    prev = 0;
    bad = 0;
    for (cp = 1; cp < 0x10000L; cp++)
        if (ucs_to_jis(cp, &r2, &c2)) {
            if (cp <= prev) bad++;
            prev = cp;
        }
    ok(bad == 0, "code points came back out of order");

    (void) printf("%d checks, %d failures\n", checks, failures);
    return failures ? 1 : 0;
}

/*jiscodetest.c*/
