/*      jiscode.c       */
/* JNetHack may be freely redistributed.  See license for details. */

/* JIS X 0208 <-> Unicode.  Rationale in include/jiscode.h; the migration
   this belongs to is UTF8-PLAN.md. */

#include "hack.h"
#include "jiscode.h"

/*
 * The only place the table is included.  It is declared "static const" in
 * the header, so every other includer would get a private 17K copy; before
 * this file existed win/tty/sdlterm.c was the one that did.
 */
#include "jis0208.h"

#define JIS_POSITIONS   (JIS_ROWS * JIS_CELLS)

long
jis_to_ucs(row, cell)
int row, cell;
{
    if (row < 1 || row > JIS_ROWS || cell < 1 || cell > JIS_CELLS)
        return 0L;

    return (long) jis0208_to_ucs[(row - 1) * JIS_CELLS + (cell - 1)];
}

/*
 * Reverse index: the assigned positions, sorted by code point.
 *
 * Built once, on first use, rather than generated into jis0208.h beside the
 * forward table.  Two reasons: mkjis0208.py stays as simple as the comment
 * at the top of jis0208.h says it is, and the two directions cannot drift
 * apart, because there is only one set of data.  6879 entries of two bytes
 * each is 14K, and the sort runs once per process.
 */
static unsigned short jis_rev[JIS_POSITIONS];   /* index into jis0208_to_ucs */
static int jis_rev_n = 0;
static boolean jis_rev_built = FALSE;

static void NDECL(build_reverse);

static void
build_reverse()
{
    int i, j;

    for (i = 0; i < JIS_POSITIONS; i++)
        if (jis0208_to_ucs[i])
            jis_rev[jis_rev_n++] = (unsigned short) i;

    /*
     * Insertion sort by code point.  The forward table is already close to
     * sorted within each row, so this does far less work than the O(n^2)
     * suggests, and it runs once.  Bringing in a qsort() comparison
     * function for it would be more code than the loop.
     */
    for (i = 1; i < jis_rev_n; i++) {
        unsigned short v = jis_rev[i];
        unsigned short key = jis0208_to_ucs[v];

        for (j = i - 1; j >= 0 && jis0208_to_ucs[jis_rev[j]] > key; j--)
            jis_rev[j + 1] = jis_rev[j];
        jis_rev[j + 1] = v;
    }

    jis_rev_built = TRUE;
}

int
ucs_to_jis(cp, row, cell)
long cp;
int *row, *cell;
{
    int lo, hi;

    if (cp <= 0L || cp > 0xFFFFL)       /* the table is unsigned short */
        return 0;

    if (!jis_rev_built)
        build_reverse();

    lo = 0;
    hi = jis_rev_n - 1;
    while (lo <= hi) {
        int mid = lo + (hi - lo) / 2;
        long v = (long) jis0208_to_ucs[jis_rev[mid]];

        if (v == cp) {
            int i = (int) jis_rev[mid];

            *row = i / JIS_CELLS + 1;
            *cell = i % JIS_CELLS + 1;
            return 1;
        }
        if (v < cp)
            lo = mid + 1;
        else
            hi = mid - 1;
    }
    return 0;
}

/* --- EUC-JP -------------------------------------------------------- */

long
euc_to_ucs(s, len)
const char *s;
int *len;
{
    const unsigned char *p = (const unsigned char *) s;

    *len = 0;
    if (!p[0]) return 0L;

    if (p[0] < 0x80) {                  /* ASCII */
        *len = 1;
        return (long) p[0];
    }

    if (p[0] == 0x8E) {                 /* SS2: half-width katakana */
        if (p[1] < 0xA1 || p[1] > 0xDF) return 0L;
        *len = 2;
        return 0xFF61L + (long) (p[1] - 0xA1);
    }

    /*
     * SS3 introduces JIS X 0212, three bytes in EUC-JP.  There is no table
     * for it here and there never was; the SDL backend reported failure
     * too.  Nothing in the game's own text uses it.
     */
    if (p[0] == 0x8F) return 0L;

    if (p[0] >= 0xA1 && p[0] <= 0xFE && p[1] >= 0xA1 && p[1] <= 0xFE) {
        long cp = jis_to_ucs((int) (p[0] - 0xA0), (int) (p[1] - 0xA0));

        if (!cp) return 0L;             /* unassigned position */
        *len = 2;
        return cp;
    }

    return 0L;
}

int
ucs_to_euc(cp, buf, n)
long cp;
char *buf;
int n;
{
    int row, cell;

    if (cp <= 0L) return 0;

    if (cp < 0x80L) {
        if (n < 1) return 0;
        buf[0] = (char) cp;
        return 1;
    }

    if (cp >= 0xFF61L && cp <= 0xFF9FL) {       /* half-width katakana */
        if (n < 2) return 0;
        buf[0] = (char) 0x8E;
        buf[1] = (char) (0xA1 + (int) (cp - 0xFF61L));
        return 2;
    }

    if (!ucs_to_jis(cp, &row, &cell)) return 0;
    if (n < 2) return 0;
    buf[0] = (char) (0xA0 + row);
    buf[1] = (char) (0xA0 + cell);
    return 2;
}

/*jiscode.c*/
