/*      sdlterm.c       */
/* JNetHack may be freely redistributed.  See license for details. */

/*
 * SDL2 cell-grid backend for the tty window port.
 *
 * This file is a drop-in replacement for win/tty/termcap.c.  It exports
 * exactly the same set of symbols, but instead of emitting escape
 * sequences it maintains an addressable grid of cells and paints that
 * grid with SDL2 + SDL2_ttf.  wintty.c, topl.c and getline.c are used
 * unmodified; the direct stdio calls they make are retargeted by the
 * macros in include/sdlterm.h.
 *
 * The point of owning the grid is that column position stops being a
 * property of the font or of the terminal's idea of character width.
 * That matters far more for JNetHack than for stock NetHack.  On a
 * terminal, whether a JIS X 0208 character takes one column or two is the
 * terminal's decision, made from the user's locale, and for the East
 * Asian Ambiguous characters -- which include every box-drawing character
 * and a good part of JIS row 1 -- terminals disagree.  Here the answer is
 * settled at the point the character enters the grid, by the encoding it
 * arrived in.  See SDL-PORT.md.
 */

#define SDLTERM_KEEP_STDIO      /* keep the real stdio; see sdlterm.h */
#define NEED_VARARGS            /* for error() under WIN32, below */

#ifdef WIN32
/* The port's entry point is main() in sys/share/pcmain.c.  Without this
   SDL2 renames it to SDL_main and supplies its own, which would bypass
   every bit of NetHack's startup. */
#define SDL_MAIN_HANDLED
#endif

#include <SDL.h>
#include <SDL_ttf.h>
#include <locale.h>
#include <signal.h>

#include "hack.h"

#if defined(TTY_GRAPHICS) && defined(SDL_GRAPHICS)

#include "wintty.h"
#include "termcap.h"
#include "jis0208.h"

#ifndef C       /* this matches src/cmd.c and win/tty/topl.c */
#define C(c)    (0x1f & (c))
#endif
#define SDL_HIGHC(c)    ((c) >= 'a' && (c) <= 'z' ? (c) - 'a' + 'A' : (c))

/* ---------------------------------------------------------------- */
/* configuration                                                        */
/* ---------------------------------------------------------------- */

#define SDL_DEF_COLS    80
#define SDL_DEF_ROWS    24
#define SDL_MIN_COLS    COLNO           /* wintty clips below this */
#define SDL_MIN_ROWS    (ROWNO + 3)
#define SDL_MAX_COLS    400
#define SDL_MAX_ROWS    200
#define SDL_DEF_PTSIZE  18
#define SDL_MIN_PTSIZE  6               /* the floor the window cannot go below */
#define SDL_MAX_PTSIZE  96

/* Blank border, in pixels, between the window edge and the character grid.
   Windows rounds the corners of a window and eats a few pixels of the
   client area with them, which without this clips the bottom-left cell. */
#define SDL_MARGIN      4

/* Fonts tried in order when NETHACK_SDL_FONT is unset.  The first entry
   is a genuine monospace CJK face: its CJK advance is exactly twice its
   ASCII advance, which is what the double-width test needs. */
static const char *const font_candidates[] = {
#ifdef WIN32
    /* MS Gothic is the one face every Japanese Windows has had since NT:
       its CJK advance is exactly twice its ASCII advance.  Meiryo and Yu
       Gothic are proportional for ASCII, so they are last-ditch only. */
    "C:\\Windows\\Fonts\\msgothic.ttc:0",       /* MS Gothic */
    "C:\\Windows\\Fonts\\MSGOTHIC.TTC:0",
    "C:\\Windows\\Fonts\\YuGothM.ttc:0",
    "C:\\Windows\\Fonts\\meiryo.ttc:0",
    "C:\\Windows\\Fonts\\consola.ttf",          /* ASCII only, but monospace */
#endif
    "/usr/share/fonts/opentype/noto/NotoSansCJK-Regular.ttc:5", /* Mono CJK JP */
    "/usr/share/fonts/opentype/ipafont-gothic/ipag.ttf",
    "/usr/share/fonts/truetype/fonts-japanese-gothic.ttf",
    "/usr/share/fonts/truetype/dejavu/DejaVuSansMono.ttf",
    "/usr/share/fonts/truetype/liberation/LiberationMono-Regular.ttf",
    0
};

/* ---------------------------------------------------------------- */
/* the cell grid                                                        */
/* ---------------------------------------------------------------- */

#define SA_BOLD         0x01
#define SA_ULINE        0x02
#define SA_INVERSE      0x04
#define SA_BLINK        0x08

#define SW_NARROW       0       /* ordinary one-cell glyph */
#define SW_LEFT         1       /* left half of a two-cell glyph */
#define SW_RIGHT        2       /* right half; ch repeats, not drawn */

struct sdl_cell {
    long ch;                    /* Unicode code point */
    unsigned char attr;
    unsigned char color;        /* NetHack CLR_*, NO_COLOR = default */
    unsigned char wide;         /* SW_* */
};

static struct sdl_cell *grid = 0;
static int grid_cols = SDL_DEF_COLS;
static int grid_rows = SDL_DEF_ROWS;

#define CELL(x,y)       (grid[(y) * grid_cols + (x)])

static int cur_x = 0, cur_y = 0;
static boolean wrap_pending = FALSE;    /* xterm-style deferred wrap */

static int cur_attr = 0;
static int cur_color = NO_COLOR;

static boolean sdl_up = FALSE;          /* SDL is initialized */
static boolean grid_dirty = TRUE;
static boolean want_quit = FALSE;

/*
 * SDLFONT / SDLFONTSIZE from the configuration file.  Filled in by
 * parse_config_line() in src/files.c, which runs from initoptions() --
 * well before init_nhwindows() reaches sdl_open_font().  Not static:
 * files.c is the only writer.
 *
 * These exist because the Windows zip has nowhere to set an environment
 * variable: the player unpacks it and double-clicks the .exe, and
 * NetHack.cnf beside it is the one place they can be expected to edit.
 */
char sdl_cnf_font[BUFSZ] = "";
int sdl_cnf_ptsize = 0;

/* ---------------------------------------------------------------- */
/* SDL objects                                                          */
/* ---------------------------------------------------------------- */

static SDL_Window *sdl_win = 0;
static SDL_Renderer *sdl_ren = 0;
static TTF_Font *font_norm = 0, *font_bold = 0;
static int cell_w = 8, cell_h = 16;

/* The face that opened, and the size it is currently open at.  A resize
   reopens this same face at another size, so the two are kept together. */
static char font_spec[BUFSZ] = "";
static int font_ptsize = SDL_DEF_PTSIZE;

/* Top-left pixel of the grid within the window: the grid is centred in
   whatever the chosen point size did not use up. */
static int org_x = SDL_MARGIN, org_y = SDL_MARGIN;

/* Palette, indexed by NetHack CLR_*.  NO_COLOR (8) is the default
   foreground rather than a real colour, so it gets the light grey the
   rest of the screen uses. */
static const struct { unsigned char r, g, b; } palette[CLR_MAX] = {
    {  0,   0,   0},    /* CLR_BLACK  - lifted off pure black to stay legible */
    {205,  49,  49},    /* CLR_RED */
    { 13, 188, 121},    /* CLR_GREEN */
    {180, 120,  40},    /* CLR_BROWN */
    { 60, 100, 235},    /* CLR_BLUE */
    {188,  63, 188},    /* CLR_MAGENTA */
    { 17, 168, 205},    /* CLR_CYAN */
    {190, 190, 190},    /* CLR_GRAY */
    {190, 190, 190},    /* NO_COLOR */
    {235, 140,  40},    /* CLR_ORANGE */
    { 35, 209, 139},    /* CLR_BRIGHT_GREEN */
    {229, 229,  16},    /* CLR_YELLOW */
    { 90, 150, 255},    /* CLR_BRIGHT_BLUE */
    {214,  112, 214},   /* CLR_BRIGHT_MAGENTA */
    { 41, 184, 219},    /* CLR_BRIGHT_CYAN */
    {245, 245, 245}     /* CLR_WHITE */
};

#define BG_R 0
#define BG_G 0
#define BG_B 0

/* CLR_BLACK on a black background would be invisible; the tty port has
   the same problem and solves it by never emitting black.  We lift it. */
#define VIS_R(c) ((c) == CLR_BLACK ? 100 : palette[c].r)
#define VIS_G(c) ((c) == CLR_BLACK ? 100 : palette[c].g)
#define VIS_B(c) ((c) == CLR_BLACK ? 100 : palette[c].b)

/* ---------------------------------------------------------------- */
/* termcap-compatible globals                                           */
/* ---------------------------------------------------------------- */

/*
 * wintty.c and topl.c read CO, LI, CM and ul_hack out of these.  CM must
 * be non-null or tty_curs() takes the "terminal has no cursor
 * addressing" path; the string itself is never looked at, since our
 * cmov() does not interpret it.
 */
struct tc_lcl_data tc_lcl_data = { 0, 0, 0, 0, 0, 0, 0, FALSE };

#ifdef TEXTCOLOR
char NEARDATA *hilites[CLR_MAX];        /* only tested for non-null */
#endif

/* `ospeed' stays where it is (sys/share/unixtty.c); nothing here uses it. */

static char sdl_capname[] = "sdl";      /* placeholder for CM et al. */

/* ---------------------------------------------------------------- */
/* forward declarations                                                 */
/* ---------------------------------------------------------------- */

static void FDECL(sdl_die, (const char *));
static void FDECL(sdl_alloc_grid, (int, int));
static void NDECL(sdl_blank_grid);
static void FDECL(sdl_clear_cell, (int, int));
static void FDECL(sdl_scroll_up, (int));
static void NDECL(sdl_wrap_now);
static int FDECL(sdl_cp_width, (long));
static void FDECL(sdl_put_wide, (long, int));
static long FDECL(sdl_euc_to_ucs, (int, int));
static void NDECL(sdl_open_font);
static void NDECL(sdl_repaint);
static void NDECL(sdl_pump);
static void FDECL(sdl_queue_byte, (int));
static void FDECL(sdl_queue_key, (SDL_Keysym *));
static void FDECL(sdl_queue_text, (const char *));
static void NDECL(sdl_dump_if_asked);
static void NDECL(sdl_shot_if_asked);
static void NDECL(sdl_place_ime);

/* ---------------------------------------------------------------- */
/* small helpers                                                        */
/* ---------------------------------------------------------------- */

static void
sdl_die(msg)
const char *msg;
{
    /* error() longjmps out through terminate(); make sure the window is
       gone first so the message is not hidden behind it. */
    if (sdl_win) {
        SDL_DestroyWindow(sdl_win);
        sdl_win = 0;
    }
    if (sdl_up) {
        SDL_Quit();
        sdl_up = FALSE;
    }
    error("sdlterm: %s", msg);
}

static void
sdl_alloc_grid(cols, rows)
int cols, rows;
{
    if (cols < 1) cols = 1;
    if (rows < 1) rows = 1;
    if (cols > SDL_MAX_COLS) cols = SDL_MAX_COLS;
    if (rows > SDL_MAX_ROWS) rows = SDL_MAX_ROWS;

    if (grid) free((genericptr_t) grid);
    grid = (struct sdl_cell *) alloc((unsigned) (cols * rows * sizeof *grid));
    grid_cols = cols;
    grid_rows = rows;
    sdl_blank_grid();
}

/*
 * grid_cols/grid_rows carry their defaults from the moment the program
 * starts, but grid itself is not allocated until sdl_alloc_grid().  Any
 * failure in between -- sdl_open_font() is the one that happens in
 * practice -- reaches sdl_die() -> error() -> settty() -> tty_end_screen()
 * -> clear_screen() -> here, which would then blank 80x24 cells starting
 * at NULL and turn a one-line diagnostic into a SIGSEGV.
 */
static void
sdl_blank_grid()
{
    register int i, n = grid_cols * grid_rows;

    if (!grid) return;
    for (i = 0; i < n; i++) {
        grid[i].ch = ' ';
        grid[i].attr = 0;
        grid[i].color = NO_COLOR;
        grid[i].wide = SW_NARROW;
    }
    grid_dirty = TRUE;
}

/* Blank one cell.  If it is half of a double-width glyph, blank the
   other half too -- leaving one orphaned half behind is exactly the
   column-drift bug the cell grid exists to prevent. */
static void
sdl_clear_cell(x, y)
int x, y;
{
    struct sdl_cell *c;

    if (x < 0 || x >= grid_cols || y < 0 || y >= grid_rows) return;
    c = &CELL(x, y);
    if (c->wide == SW_LEFT && x + 1 < grid_cols) {
        struct sdl_cell *r = &CELL(x + 1, y);
        r->ch = ' '; r->attr = 0; r->color = NO_COLOR; r->wide = SW_NARROW;
    } else if (c->wide == SW_RIGHT && x > 0) {
        struct sdl_cell *l = &CELL(x - 1, y);
        l->ch = ' '; l->attr = 0; l->color = NO_COLOR; l->wide = SW_NARROW;
    }
    c->ch = ' '; c->attr = 0; c->color = NO_COLOR; c->wide = SW_NARROW;
    grid_dirty = TRUE;
}

static void
sdl_scroll_up(n)
int n;
{
    int i, keep;

    if (n <= 0) return;
    if (n >= grid_rows) {
        sdl_blank_grid();
        return;
    }
    keep = (grid_rows - n) * grid_cols;
    (void) memmove((genericptr_t) grid,
                   (genericptr_t) (grid + n * grid_cols),
                   keep * sizeof *grid);
    for (i = keep; i < grid_rows * grid_cols; i++) {
        grid[i].ch = ' ';
        grid[i].attr = 0;
        grid[i].color = NO_COLOR;
        grid[i].wide = SW_NARROW;
    }
    grid_dirty = TRUE;
}

/* Carry out a deferred wrap: called when a printable arrives and the
   cursor is parked on the right margin. */
static void
sdl_wrap_now()
{
    wrap_pending = FALSE;
    cur_x = 0;
    if (++cur_y >= grid_rows) {
        sdl_scroll_up(cur_y - grid_rows + 1);
        cur_y = grid_rows - 1;
    }
}

/* ---------------------------------------------------------------- */
/* byte -> code point: the graphics character sets                      */
/* ---------------------------------------------------------------- */

/*
 * The port hands the backend bytes, not characters.  What a byte in
 * 0x80..0xff means depends on which graphics set the game has selected
 * (see src/drawing.c): with IBMgraphics it is a code page 437 byte, with
 * DECgraphics it is a VT100 line-drawing character that arrives with its
 * high bit stripped between graph_on()/graph_off().
 *
 * A terminal resolves this by being configured for the right code page,
 * or by switching fonts.  We resolve it by translating to Unicode here,
 * which is why the walls can be drawn with proper box-drawing glyphs --
 * and, since we choose the code point, with rounded corners.
 */

/* Code page 437, low half: the symbols a PC console shows for control
   bytes.  Only reachable under IBMgraphics; the entries NetHack actually
   uses are the card pips for rogue-level traps.  Bytes that steer the
   cursor (BEL, BS, HT, LF, CR) are left out on purpose. */
static const unsigned short cp437_low[32] = {
    0x0000, 0x263A, 0x263B, 0x2665, 0x2666, 0x2663, 0x2660, 0x0007,
    0x0008, 0x0009, 0x000A, 0x2642, 0x2640, 0x000D, 0x266B, 0x263C,
    0x25BA, 0x25C4, 0x2195, 0x203C, 0x00B6, 0x00A7, 0x25AC, 0x21A8,
    0x2191, 0x2193, 0x2192, 0x2190, 0x221F, 0x2194, 0x25B2, 0x25BC
};

/* Code page 437, high half. */
static const unsigned short cp437_high[128] = {
    0x00C7, 0x00FC, 0x00E9, 0x00E2, 0x00E4, 0x00E0, 0x00E5, 0x00E7,
    0x00EA, 0x00EB, 0x00E8, 0x00EF, 0x00EE, 0x00EC, 0x00C4, 0x00C5,
    0x00C9, 0x00E6, 0x00C6, 0x00F4, 0x00F6, 0x00F2, 0x00FB, 0x00F9,
    0x00FF, 0x00D6, 0x00DC, 0x00A2, 0x00A3, 0x00A5, 0x20A7, 0x0192,
    0x00E1, 0x00ED, 0x00F3, 0x00FA, 0x00F1, 0x00D1, 0x00AA, 0x00BA,
    0x00BF, 0x2310, 0x00AC, 0x00BD, 0x00BC, 0x00A1, 0x00AB, 0x00BB,
    0x2591, 0x2592, 0x2593, 0x2502, 0x2524, 0x2561, 0x2562, 0x2556,
    0x2555, 0x2563, 0x2551, 0x2557, 0x255D, 0x255C, 0x255B, 0x2510,
    0x2514, 0x2534, 0x252C, 0x251C, 0x2500, 0x253C, 0x255E, 0x255F,
    0x255A, 0x2554, 0x2569, 0x2566, 0x2560, 0x2550, 0x256C, 0x2567,
    0x2568, 0x2564, 0x2565, 0x2559, 0x2558, 0x2552, 0x2553, 0x256B,
    0x256A, 0x2518, 0x250C, 0x2588, 0x2584, 0x258C, 0x2590, 0x2580,
    0x03B1, 0x00DF, 0x0393, 0x03C0, 0x03A3, 0x03C3, 0x00B5, 0x03C4,
    0x03A6, 0x0398, 0x03A9, 0x03B4, 0x221E, 0x03C6, 0x03B5, 0x2229,
    0x2261, 0x00B1, 0x2265, 0x2264, 0x2320, 0x2321, 0x00F7, 0x2248,
    0x00B0, 0x2219, 0x00B7, 0x221A, 0x207F, 0x00B2, 0x25A0, 0x00A0
};

/*
 * VT100 "special graphics" set, 0x5f..0x7e, used while graph_on() is in
 * effect.  This is what DECgraphics sends after g_putch() strips the
 * high bit, and DECgraphics is what the SDL build selects by default.
 *
 * Five entries deliberately differ from the VT100 standard, marked
 * below.  This is where the backend's look is decided: the port asked
 * for a corner and a floor, and we choose which character goes in the
 * cell.  Nothing else in the file knows about it, and the rounded
 * corners do not depend on the font, because sdl_draw_box() draws them.
 *
 * test/walls_compare.sh applies the same five substitutions to its
 * reference, so a change here without a change there will fail.
 */
static const unsigned short dec_special[32] = {
    0x00A0, 0x25C6, 0x2592, 0x2409, 0x240C, 0x240D, 0x240A, 0x00B0,
/*                       j:U+256F  k:U+256E  l:U+256D  m:U+2570 <- rounded */
    0x00B1, 0x2424, 0x240B, 0x256F, 0x256E, 0x256D, 0x2570, 0x253C,
    0x23BA, 0x23BB, 0x2500, 0x23BC, 0x23BD, 0x251C, 0x2524, 0x2534,
    0x252C, 0x2502, 0x2264, 0x2265, 0x03C0, 0x2260, 0x00A3, 0x002E
/*                                                          ~:'.' not U+00B7 */
};

/* Code page 437 above stays faithful: IBMgraphics is the "as a PC
   console would show it" option, so it keeps square corners and the
   centred dot. */

/* TRUE while the port has asked for the alternate character set. */
static boolean alt_charset = FALSE;

/*
 * How many cells does this code point occupy, when all we know about it
 * is the code point?
 *
 * This is only the fallback.  It is used for characters that arrived as a
 * single byte, and for the box-drawing characters the graphics sets
 * produce.  The ranges are East Asian Wide (W) and Fullwidth (F) from
 * UAX #11; Ambiguous (A) is deliberately treated as narrow, which is what
 * makes the walls line up regardless of $LANG.
 *
 * Characters that arrived as an EUC-JP pair do not come through here at
 * all -- see sdl_puteuc().
 */
static int
sdl_cp_width(cp)
long cp;
{
    if (cp < 0x1100L) return 1;

    if ((cp >= 0x1100L && cp <= 0x115FL) ||     /* Hangul Jamo init. */
        (cp >= 0x2E80L && cp <= 0x303EL) ||     /* CJK radicals, Kangxi */
        (cp >= 0x3041L && cp <= 0x33FFL) ||     /* kana, Hangul, CJK compat */
        (cp >= 0x3400L && cp <= 0x4DBFL) ||     /* CJK ext A */
        (cp >= 0x4E00L && cp <= 0x9FFFL) ||     /* CJK unified */
        (cp >= 0xA000L && cp <= 0xA4CFL) ||     /* Yi */
        (cp >= 0xAC00L && cp <= 0xD7A3L) ||     /* Hangul syllables */
        (cp >= 0xF900L && cp <= 0xFAFFL) ||     /* CJK compat ideographs */
        (cp >= 0xFE10L && cp <= 0xFE19L) ||     /* vertical forms */
        (cp >= 0xFE30L && cp <= 0xFE6FL) ||     /* CJK compat forms */
        (cp >= 0xFF00L && cp <= 0xFF60L) ||     /* fullwidth ASCII */
        (cp >= 0xFFE0L && cp <= 0xFFE6L) ||     /* fullwidth signs */
        (cp >= 0x1F300L && cp <= 0x1F64FL) ||   /* emoji */
        (cp >= 0x1F900L && cp <= 0x1F9FFL) ||
        (cp >= 0x20000L && cp <= 0x3FFFDL))     /* CJK ext B..  */
        return 2;

    return 1;
}

/* ---------------------------------------------------------------- */
/* EUC-JP -> Unicode                                                    */
/* ---------------------------------------------------------------- */

/*
 * JNetHack's internal encoding on Unix is EUC-JP, and jlib.c has already
 * collected the two bytes of a character before this is reached (see
 * jbuffer() there).  All that is left is to name the code point.
 *
 * The table comes from japanese/mkjis0208.py, so nothing here depends on
 * iconv, on a locale being installed, or on what the C library thinks
 * EUC-JP means.
 */
static long
sdl_euc_to_ucs(b1, b2)
int b1, b2;
{
    int row, cell;

    b1 &= 0xFF;
    b2 &= 0xFF;

    /* SS2: JIS X 0201 half-width katakana, 0xA1..0xDF -> U+FF61..U+FF9F. */
    if (b1 == 0x8E) {
        if (b2 >= 0xA1 && b2 <= 0xDF) return 0xFF61L + (b2 - 0xA1);
        return 0xFFFDL;
    }

    /*
     * SS3 introduces JIS X 0212, which is a three-byte sequence in EUC-JP.
     * jlib.c's two-byte accumulator cannot deliver one, and JNetHack's own
     * data contains none, so there is nothing to translate.
     */
    if (b1 == 0x8F) return 0xFFFDL;

    row = (b1 & 0x7F) - 0x20;
    cell = (b2 & 0x7F) - 0x20;
    if (row < 1 || row > JIS0208_ROWS || cell < 1 || cell > JIS0208_CELLS)
        return 0xFFFDL;

    {
        unsigned short u = jis0208_to_ucs[(row - 1) * JIS0208_CELLS
                                          + (cell - 1)];
        return u ? (long) u : 0xFFFDL;
    }
}

/* ---------------------------------------------------------------- */
/* font handling and the glyph cache                                    */
/* ---------------------------------------------------------------- */

/*
 * Glyphs are rendered once, in white, and cached as textures.  Colour is
 * applied at blit time with SDL_SetTextureColorMod, so a colour change
 * costs nothing.
 */
#define GLYPH_HASH 1021

struct glyph_ent {
    long ch;
    int bold;
    SDL_Texture *tex;
    int w, h;
    struct glyph_ent *next;
};

static struct glyph_ent *glyph_tab[GLYPH_HASH];

static void
sdl_free_glyphs()
{
    int i;
    struct glyph_ent *g, *nx;

    for (i = 0; i < GLYPH_HASH; i++) {
        for (g = glyph_tab[i]; g; g = nx) {
            nx = g->next;
            if (g->tex) SDL_DestroyTexture(g->tex);
            free((genericptr_t) g);
        }
        glyph_tab[i] = 0;
    }
}

static struct glyph_ent *
sdl_glyph(ch, bold)
long ch;
int bold;
{
    unsigned h = (unsigned) ((ch * 2 + bold) % GLYPH_HASH);
    struct glyph_ent *g;
    SDL_Surface *surf;
    TTF_Font *f;

    for (g = glyph_tab[h]; g; g = g->next)
        if (g->ch == ch && g->bold == bold) return g;

    g = (struct glyph_ent *) alloc((unsigned) sizeof *g);
    g->ch = ch;
    g->bold = bold;
    g->tex = 0;
    g->w = g->h = 0;
    g->next = glyph_tab[h];
    glyph_tab[h] = g;

    f = (bold && font_bold) ? font_bold : font_norm;
    if (f && TTF_GlyphIsProvided32(f, (Uint32) ch)) {
        SDL_Color white;
        white.r = white.g = white.b = white.a = 255;
        surf = TTF_RenderGlyph32_Blended(f, (Uint32) ch, white);
        if (surf) {
            g->tex = SDL_CreateTextureFromSurface(sdl_ren, surf);
            g->w = surf->w;
            g->h = surf->h;
            SDL_FreeSurface(surf);
        }
    }
    return g;
}

/* Is this a path the filesystem will resolve on its own, whatever the
   current directory happens to be? */
static boolean
sdl_abs_path(p)
const char *p;
{
    if (*p == '/') return TRUE;
#ifdef WIN32
    if (*p == '\\') return TRUE;
    if (((*p >= 'A' && *p <= 'Z') || (*p >= 'a' && *p <= 'z')) && p[1] == ':')
        return TRUE;
#endif
    return FALSE;
}

/*
 * "path" or "path:index".
 *
 * A relative path is tried twice.  main() chdir()s to the playground long
 * before the window system starts, so "./foo.ttf" on the command line
 * reaches us meaning HACKDIR/foo.ttf -- never what the player typed it
 * for.  The second try resolves it against orgdir, the directory they
 * were actually in.  The playground is still tried first, so a font
 * shipped alongside the data files keeps working.
 *
 * A forward slash joins the two halves even on Windows: the Win32 API
 * accepts it everywhere, and SDL_ttf passes the name straight through.
 */
static TTF_Font *
sdl_try_font(spec, ptsize)
const char *spec;
int ptsize;
{
    extern char orgdir[];       /* sys/unix/unixmain.c, sys/share/pcmain.c */
    char buf[BUFSZ], alt[PATHLEN + BUFSZ];
    char *colon;
    long idx = 0;
    TTF_Font *f;

    (void) strncpy(buf, spec, sizeof buf - 1);
    buf[sizeof buf - 1] = '\0';
    colon = strrchr(buf, ':');
    if (colon && colon[1] >= '0' && colon[1] <= '9') {
        idx = atol(colon + 1);
        *colon = '\0';
    }
    f = TTF_OpenFontIndex(buf, ptsize, idx);
    if (!f && !sdl_abs_path(buf) && orgdir[0]) {
        /* Joined by hand rather than with Sprintf(): orgdir is an
           incomplete type here, so the compiler cannot see that the length
           test below bounds the result, and warns about every write. */
        unsigned dlen = strlen(orgdir), blen = strlen(buf);

        if (dlen + 1 + blen < sizeof alt) {
            (void) memcpy(alt, orgdir, dlen);
            alt[dlen] = '/';
            (void) memcpy(alt + dlen + 1, buf, blen + 1);
            f = TTF_OpenFontIndex(alt, ptsize, idx);
        }
    }
    return f;
}

/* The cell is sized from an ASCII advance.  Everything else on screen
   is placed as a multiple of it, so glyph metrics never influence
   where a character lands. */
static void
sdl_cell_size(f, cw, ch)
TTF_Font *f;
int *cw, *ch;
{
    int minx, maxx, miny, maxy, adv;

    if (TTF_GlyphMetrics32(f, (Uint32) 'M',
                           &minx, &maxx, &miny, &maxy, &adv) == 0 && adv > 0)
        *cw = adv;
    else
        *cw = TTF_FontHeight(f) / 2;
    *ch = TTF_FontHeight(f);
    if (*cw < 1) *cw = 1;
    if (*ch < 1) *ch = 1;
}

/* What one cell would measure with font_spec at ptsize, without disturbing
   the font that is in use. */
static boolean
sdl_measure(ptsize, cw, ch)
int ptsize, *cw, *ch;
{
    TTF_Font *f = sdl_try_font(font_spec, ptsize);

    if (!f) return FALSE;
    sdl_cell_size(f, cw, ch);
    TTF_CloseFont(f);
    return TRUE;
}

/* Reopen font_spec at ptsize.  On failure the old font stays in use, so a
   size the face cannot supply costs nothing but the attempt. */
static boolean
sdl_load_font(ptsize)
int ptsize;
{
    TTF_Font *nrm, *bld;

    nrm = sdl_try_font(font_spec, ptsize);
    if (!nrm) return FALSE;

    /*
     * Bold is synthesised rather than loaded from a second file: it only
     * has to be distinguishable, and a separate face risks a different
     * advance width, which would break the grid.
     */
    bld = sdl_try_font(font_spec, ptsize);
    if (bld) TTF_SetFontStyle(bld, TTF_STYLE_BOLD);

    /* The cache holds glyphs rendered at the size being replaced. */
    sdl_free_glyphs();
    if (font_bold) TTF_CloseFont(font_bold);
    if (font_norm) TTF_CloseFont(font_norm);
    font_norm = nrm;
    font_bold = bld;
    font_ptsize = ptsize;
    sdl_cell_size(font_norm, &cell_w, &cell_h);
    return TRUE;
}

/* Copied by hand rather than with strncpy(): a spec that filled the buffer
   exactly would come out unterminated, and the compiler warns about it. */
static void
sdl_set_spec(s)
const char *s;
{
    unsigned n = strlen(s);

    if (n > sizeof font_spec - 1) n = sizeof font_spec - 1;
    (void) memcpy(font_spec, s, n);
    font_spec[n] = '\0';
}

static void
sdl_open_font()
{
    /* The environment wins over the configuration file: it is the more
       specific of the two, set for one run rather than for the install. */
    const char *spec = getenv("NETHACK_SDL_FONT");
    const char *szs = getenv("NETHACK_SDL_FONTSIZE");
    int ptsize;
    int i;

    if (!spec || !*spec) spec = sdl_cnf_font[0] ? sdl_cnf_font : 0;
    ptsize = szs ? atoi(szs) : sdl_cnf_ptsize;
    if (ptsize < SDL_MIN_PTSIZE) ptsize = SDL_DEF_PTSIZE;

    /*
     * Whichever spec opens is remembered, because a resize reopens it at
     * another point size.  Walking the candidate list again there could
     * land on a different face, and a different face means a different
     * advance width -- the one thing the grid cannot survive.
     */
    if (spec && *spec) {
        sdl_set_spec(spec);
        if (!sdl_load_font(ptsize))
            sdl_die("cannot open the font named by NETHACK_SDL_FONT or SDLFONT");
        return;
    }
    for (i = 0; font_candidates[i]; i++) {
        sdl_set_spec(font_candidates[i]);
        if (sdl_load_font(ptsize)) return;
    }
    font_spec[0] = '\0';
    sdl_die("no usable monospace font found");
}

/* The largest point size whose grid still fits in a w x h area.  Cell
   metrics are near enough proportional to the point size that the first
   guess is right or one step out; the loops are the correction, not a
   search. */
static int
sdl_fit_ptsize(w, h)
int w, h;
{
    int pt, px, py, cw, ch;

    px = font_ptsize * w / (cell_w * grid_cols);
    py = font_ptsize * h / (cell_h * grid_rows);
    pt = (px < py) ? px : py;
    if (pt > SDL_MAX_PTSIZE) pt = SDL_MAX_PTSIZE;
    if (pt < SDL_MIN_PTSIZE) pt = SDL_MIN_PTSIZE;

#define SDL_PT_FITS(p) \
    (sdl_measure(p, &cw, &ch) && cw * grid_cols <= w && ch * grid_rows <= h)
    while (pt > SDL_MIN_PTSIZE && !SDL_PT_FITS(pt)) pt--;
    while (pt < SDL_MAX_PTSIZE && SDL_PT_FITS(pt + 1)) pt++;
#undef SDL_PT_FITS
    return pt;
}

/* ---------------------------------------------------------------- */
/* painting                                                             */
/* ---------------------------------------------------------------- */

/*
 * Box-drawing characters are drawn rather than rendered from the font.
 *
 * A font's line-drawing glyphs are cut for the font's own em, not for
 * our cell.  With a CJK face the cell is much taller than the em (27px
 * against 18px here), so a column of U+2502 comes out as a dashed line with
 * a gap at every row boundary.  Terminal emulators hit the same problem
 * and answer it the same way.
 *
 * Drawing them also means the look no longer depends on what the font
 * happens to contain -- the rounded corners work even with a face that
 * has no U+256D..U+2570 at all.
 *
 * Returns TRUE if it drew the character.
 */
struct boxdef {
    long cp;
    unsigned char up, down, left, right, round;
};

static const struct boxdef boxchars[] = {
    { 0x2500L, 0, 0, 1, 1, 0 },         /* U+2500 */
    { 0x2502L, 1, 1, 0, 0, 0 },         /* U+2502 */
    { 0x250CL, 0, 1, 0, 1, 0 },         /* U+250C */
    { 0x2510L, 0, 1, 1, 0, 0 },         /* U+2510 */
    { 0x2514L, 1, 0, 0, 1, 0 },         /* U+2514 */
    { 0x2518L, 1, 0, 1, 0, 0 },         /* U+2518 */
    { 0x251CL, 1, 1, 0, 1, 0 },         /* U+251C */
    { 0x2524L, 1, 1, 1, 0, 0 },         /* U+2524 */
    { 0x252CL, 0, 1, 1, 1, 0 },         /* U+252C */
    { 0x2534L, 1, 0, 1, 1, 0 },         /* U+2534 */
    { 0x253CL, 1, 1, 1, 1, 0 },         /* U+253C */
    { 0x256DL, 0, 1, 0, 1, 1 },         /* U+256D */
    { 0x256EL, 0, 1, 1, 0, 1 },         /* U+256E */
    { 0x256FL, 1, 0, 1, 0, 1 },         /* U+256F */
    { 0x2570L, 1, 0, 0, 1, 1 },         /* U+2570 */
    { 0L, 0, 0, 0, 0, 0 }
};

/* One square of the pen. */
static void
sdl_pen(px, py, t)
int px, py, t;
{
    SDL_Rect r;

    r.x = px; r.y = py; r.w = t; r.h = t;
    SDL_RenderFillRect(sdl_ren, &r);
}

/*
 * Quarter arc of radius rad about (ax, ay), sweeping into the quadrant
 * given by the signs sx and sy.  Stepping both coordinates in turn keeps
 * it gap-free without needing trigonometry.
 */
static void
sdl_arc(ax, ay, rad, sx, sy, t)
int ax, ay, rad, sx, sy, t;
{
    int i, j;

    for (i = 0; i <= rad; i++) {
        /* integer sqrt of rad*rad - i*i */
        for (j = rad; j > 0 && j * j > rad * rad - i * i; j--)
            continue;
        sdl_pen(ax + sx * i - t / 2, ay + sy * j - t / 2, t);
        sdl_pen(ax + sx * j - t / 2, ay + sy * i - t / 2, t);
    }
}

static boolean
sdl_draw_box(cp, box, fr, fg, fb)
long cp;
SDL_Rect *box;
int fr, fg, fb;
{
    const struct boxdef *d;
    int t, cx, cy, rad;

    for (d = boxchars; d->cp; d++)
        if (d->cp == cp) break;
    if (!d->cp) return FALSE;

    t = cell_w / 7;
    if (t < 1) t = 1;

    /* centre line of the cell, as the top-left of a t-thick bar */
    cx = box->x + (box->w - t) / 2;
    cy = box->y + (box->h - t) / 2;

    rad = d->round ? (box->w < box->h ? box->w : box->h) / 2 : 0;

    SDL_SetRenderDrawColor(sdl_ren, (Uint8) fr, (Uint8) fg, (Uint8) fb, 255);

    if (d->round) {
        /* the two straight runs stop short of the corner ... */
        int ax = cx + (d->left ? -rad : rad);
        int ay = cy + (d->up ? -rad : rad);
        SDL_Rect r;

        if (d->up) {
            r.x = cx; r.y = box->y; r.w = t; r.h = cy - rad - box->y;
            if (r.h > 0) SDL_RenderFillRect(sdl_ren, &r);
        }
        if (d->down) {
            r.x = cx; r.y = cy + rad; r.w = t;
            r.h = box->y + box->h - r.y;
            if (r.h > 0) SDL_RenderFillRect(sdl_ren, &r);
        }
        if (d->left) {
            r.x = box->x; r.y = cy; r.w = cx - rad - box->x; r.h = t;
            if (r.w > 0) SDL_RenderFillRect(sdl_ren, &r);
        }
        if (d->right) {
            r.x = cx + rad; r.y = cy; r.w = box->x + box->w - r.x; r.h = t;
            if (r.w > 0) SDL_RenderFillRect(sdl_ren, &r);
        }
        /* ... and the arc joins them */
        sdl_arc(ax, ay, rad, d->left ? 1 : -1, d->up ? 1 : -1, t);
        return TRUE;
    }

    if (d->up) {
        SDL_Rect r;
        r.x = cx; r.y = box->y; r.w = t; r.h = cy - box->y + t;
        SDL_RenderFillRect(sdl_ren, &r);
    }
    if (d->down) {
        SDL_Rect r;
        r.x = cx; r.y = cy; r.w = t; r.h = box->y + box->h - cy;
        SDL_RenderFillRect(sdl_ren, &r);
    }
    if (d->left) {
        SDL_Rect r;
        r.x = box->x; r.y = cy; r.w = cx - box->x + t; r.h = t;
        SDL_RenderFillRect(sdl_ren, &r);
    }
    if (d->right) {
        SDL_Rect r;
        r.x = cx; r.y = cy; r.w = box->x + box->w - cx; r.h = t;
        SDL_RenderFillRect(sdl_ren, &r);
    }
    return TRUE;
}

static void
sdl_draw_cell(x, y)
int x, y;
{
    struct sdl_cell *c = &CELL(x, y);
    struct glyph_ent *g;
    SDL_Rect dst, box;
    int span, fr, fg, fb, br, bg, bb;

    if (c->wide == SW_RIGHT) return;    /* drawn with its left half */

    span = (c->wide == SW_LEFT) ? 2 : 1;

    fr = VIS_R(c->color); fg = VIS_G(c->color); fb = VIS_B(c->color);
    br = BG_R; bg = BG_G; bb = BG_B;

    if (c->attr & SA_INVERSE) {
        int t;
        t = fr; fr = br; br = t;
        t = fg; fg = bg; bg = t;
        t = fb; fb = bb; bb = t;
    }

    box.x = org_x + x * cell_w;
    box.y = org_y + y * cell_h;
    box.w = cell_w * span;
    box.h = cell_h;

    if (br != BG_R || bg != BG_G || bb != BG_B) {
        SDL_SetRenderDrawColor(sdl_ren, (Uint8) br, (Uint8) bg, (Uint8) bb, 255);
        SDL_RenderFillRect(sdl_ren, &box);
    }

    if (c->ch != ' ' && c->ch != 0 &&
        !sdl_draw_box(c->ch, &box, fr, fg, fb)) {
        g = sdl_glyph(c->ch, (c->attr & SA_BOLD) != 0);
        if (g->tex) {
            dst.x = box.x;
            dst.y = box.y;
            dst.w = g->w;
            dst.h = g->h;
            /* A glyph wider than the cells it was given is squeezed rather
               than allowed to bleed into the next column. */
            if (dst.w > box.w) dst.w = box.w;
            SDL_SetTextureColorMod(g->tex, (Uint8) fr, (Uint8) fg, (Uint8) fb);
            SDL_RenderCopy(sdl_ren, g->tex, (SDL_Rect *) 0, &dst);
        }
    }

    if (c->attr & SA_ULINE) {
        SDL_SetRenderDrawColor(sdl_ren, (Uint8) fr, (Uint8) fg, (Uint8) fb, 255);
        SDL_RenderDrawLine(sdl_ren, box.x, box.y + cell_h - 1,
                           box.x + box.w - 1, box.y + cell_h - 1);
    }
}

static void
sdl_repaint()
{
    int x, y;
    SDL_Rect cur;

    if (!sdl_ren) return;

    SDL_SetRenderDrawColor(sdl_ren, BG_R, BG_G, BG_B, 255);
    SDL_RenderClear(sdl_ren);

    for (y = 0; y < grid_rows; y++)
        for (x = 0; x < grid_cols; x++)
            sdl_draw_cell(x, y);

    if (cur_x >= 0 && cur_x < grid_cols && cur_y >= 0 && cur_y < grid_rows) {
        cur.x = org_x + cur_x * cell_w;
        cur.y = org_y + cur_y * cell_h + cell_h - 2;
        cur.w = cell_w;
        cur.h = 2;
        SDL_SetRenderDrawColor(sdl_ren, 220, 220, 100, 255);
        SDL_RenderFillRect(sdl_ren, &cur);
    }

    sdl_shot_if_asked();
    SDL_RenderPresent(sdl_ren);
    grid_dirty = FALSE;
}

/* Place the grid in the middle of the window.  Whatever the point size
   could not use becomes an even border; since the size was chosen to fit
   inside SDL_MARGIN, every side keeps at least that many pixels. */
static void
sdl_center()
{
    int w, h;

    if (!sdl_ren) return;
    SDL_GetRendererOutputSize(sdl_ren, &w, &h);
    org_x = (w - grid_cols * cell_w) / 2;
    org_y = (h - grid_rows * cell_h) / 2;
    if (org_x < 0) org_x = 0;
    if (org_y < 0) org_y = 0;
}

/* Tell the IME where the text it is composing will appear, so its
   candidate window does not sit on top of the line being typed. */
static void
sdl_place_ime()
{
    SDL_Rect r;

    if (!sdl_win) return;
    r.x = org_x + cur_x * cell_w;
    r.y = org_y + cur_y * cell_h;
    r.w = cell_w;
    r.h = cell_h;
    SDL_SetTextInputRect(&r);
}

/* ---------------------------------------------------------------- */
/* input                                                                */
/* ---------------------------------------------------------------- */

#define KQ_SIZE 256
static unsigned char kq[KQ_SIZE];
static int kq_head = 0, kq_tail = 0;

/* A scripted key stream, for the headless test harnesses. */
static char *script = 0;
static int script_pos = 0, script_len = 0;

/*
 * Alt held on the SDL_KEYDOWN just seen: the meta prefix, waiting for the
 * SDL_TEXTINPUT that says which character it belongs to.  meta_fallback is
 * what to send if no text arrives at all.  See sdl_queue_key().
 */
static boolean meta_pending = FALSE;
static int meta_fallback = 0;

static void
sdl_queue_byte(b)
int b;
{
    int nxt = (kq_tail + 1) % KQ_SIZE;

    if (nxt == kq_head) return;         /* full; drop, as a tty would */
    kq[kq_tail] = (unsigned char) b;
    kq_tail = nxt;
}

/*
 * Unicode -> EUC-JP, for the way back in.
 *
 * The forward table is the only data; the reverse direction is a scan of
 * it.  That is 8836 comparisons per character, which sounds bad until you
 * notice it only happens when the user commits IME output -- a few times
 * a game, at typing speed.  Building an index would be more code for no
 * observable difference.
 *
 * Returns 0 if the character has no EUC-JP representation.
 */
static int
sdl_ucs_to_euc(cp, b1, b2)
long cp;
int *b1, *b2;
{
    int i;

    if (cp >= 0xFF61L && cp <= 0xFF9FL) {       /* half-width katakana */
        *b1 = 0x8E;
        *b2 = 0xA1 + (int) (cp - 0xFF61L);
        return 1;
    }
    if (cp <= 0L || cp > 0xFFFFL) return 0;

    for (i = 0; i < JIS0208_ROWS * JIS0208_CELLS; i++)
        if ((long) jis0208_to_ucs[i] == cp) {
            *b1 = 0xA1 + i / JIS0208_CELLS;
            *b2 = 0xA1 + i % JIS0208_CELLS;
            return 1;
        }
    return 0;
}

static void
sdl_queue_text(s)
const char *s;
{
    /*
     * SDL hands us UTF-8, from the keyboard or from an IME's committed
     * text.  JNetHack wants EUC-JP bytes, and it wants both bytes of a
     * character: getline.c's backspace handling calls is_kanji2() on what
     * it has collected so far and deletes two bytes when it sees a pair,
     * so a lone lead byte in the queue would desynchronise it.
     */
    const unsigned char *p = (const unsigned char *) s;
    long cp;
    int b1, b2;

    if (meta_pending) {
        meta_pending = FALSE;
        /*
         * One printable ASCII character, delivered while Alt was down: the
         * meta byte src/cmd.c is waiting for.  Anything else -- a kanji an
         * IME committed, a multi-character commit -- cannot be a meta
         * command, so it falls through and is converted as it stands.
         */
        if (s[0] >= ' ' && (unsigned char) s[0] < 0x7f && s[1] == '\0') {
            sdl_queue_byte(0x80 | s[0]);
            return;
        }
    }

    while (*p) {
        if (*p < 0x80) {
            sdl_queue_byte(*p);
            p++;
            continue;
        }

        if ((p[0] & 0xE0) == 0xC0 && (p[1] & 0xC0) == 0x80) {
            cp = ((long) (p[0] & 0x1F) << 6) | (p[1] & 0x3F);
            p += 2;
        } else if ((p[0] & 0xF0) == 0xE0 && (p[1] & 0xC0) == 0x80 &&
                   (p[2] & 0xC0) == 0x80) {
            cp = ((long) (p[0] & 0x0F) << 12) |
                 ((long) (p[1] & 0x3F) << 6) | (p[2] & 0x3F);
            p += 3;
        } else {
            /* malformed or beyond the BMP: skip the whole sequence */
            p++;
            while (*p && (*p & 0xC0) == 0x80) p++;
            continue;
        }

        if (sdl_ucs_to_euc(cp, &b1, &b2)) {
            sdl_queue_byte(b1);
            sdl_queue_byte(b2);
        } else {
            tty_nhbell();               /* nothing JNetHack could store */
        }
    }
}

/* Direction keys.  Stock 3.2.3 has no escape-sequence decoder, so arrows
   are translated here into the movement letters the game already knows. */
static int
sdl_dirkey(sym, numpad)
int sym;
boolean numpad;
{
    switch (sym) {
    case SDLK_LEFT:     case SDLK_KP_4: return numpad ? '4' : 'h';
    case SDLK_RIGHT:    case SDLK_KP_6: return numpad ? '6' : 'l';
    case SDLK_UP:       case SDLK_KP_8: return numpad ? '8' : 'k';
    case SDLK_DOWN:     case SDLK_KP_2: return numpad ? '2' : 'j';
    case SDLK_HOME:     case SDLK_KP_7: return numpad ? '7' : 'y';
    case SDLK_PAGEUP:   case SDLK_KP_9: return numpad ? '9' : 'u';
    case SDLK_END:      case SDLK_KP_1: return numpad ? '1' : 'b';
    case SDLK_PAGEDOWN: case SDLK_KP_3: return numpad ? '3' : 'n';
    case SDLK_KP_5:                             return numpad ? '5' : '.';
    }
    return 0;
}

static void
sdl_queue_key(ks)
SDL_Keysym *ks;
{
    int sym = ks->sym;
    int mod = ks->mod;
    int d;

    /*
     * Only keys that do not also produce an SDL_TEXTINPUT event are
     * handled here; otherwise every letter would arrive twice.
     */
    switch (sym) {
    case SDLK_ESCAPE:           sdl_queue_byte('\033'); return;
    case SDLK_RETURN:
    case SDLK_KP_ENTER:         sdl_queue_byte('\n'); return;
    case SDLK_BACKSPACE:        sdl_queue_byte('\b'); return;
    case SDLK_DELETE:           sdl_queue_byte('\177'); return;
    case SDLK_TAB:              sdl_queue_byte('\t'); return;
    }

    d = sdl_dirkey(sym, (boolean) (iflags.num_pad != 0));
    if (d) {
        /* shift/ctrl on a direction means "run"/"rush", as in the tty port */
        if (!iflags.num_pad && (mod & KMOD_SHIFT)) d = SDL_HIGHC(d);
        else if (!iflags.num_pad && (mod & KMOD_CTRL)) d = C(d);
        sdl_queue_byte(d);
        return;
    }

    if ((mod & KMOD_CTRL) && sym >= SDLK_a && sym <= SDLK_z) {
        sdl_queue_byte(C(sym));
        return;
    }
    if ((mod & KMOD_CTRL) && sym == SDLK_LEFTBRACKET) {
        sdl_queue_byte('\033');
        return;
    }

    /*
     * Alt is the meta prefix.  src/cmd.c holds its meta commands as
     * M(c) == 0x80|c -- #name is M('n'), #pray is M('p') -- and nothing
     * between here and rhack() decodes an ESC prefix the way a terminal
     * would, so the high bit is the only way to reach them.  Without this
     * the letter still arrived, as itself, and the game answered "Unknown
     * command 'n'".
     *
     * Which character Alt was pressed with is a question for the keyboard
     * layout, not for us: M('?') is Alt+Shift+/ on a US layout and
     * somewhere else on a Japanese one.  SDL has already answered it in
     * the SDL_TEXTINPUT it pushes alongside this event -- both come out of
     * one key press, so sdl_pump() sees them in the same drain -- so only
     * note the prefix here and let sdl_queue_text() put the two together.
     *
     * meta_fallback is for the case where no text follows: a layout or a
     * window manager that swallows Alt combinations still leaves us the
     * keysym, which is the unshifted ASCII character for these keys.
     */
    if ((mod & KMOD_ALT) && !(mod & KMOD_CTRL)
        && sym >= SDLK_SPACE && sym <= SDLK_z) {
        meta_pending = TRUE;
        meta_fallback = (mod & KMOD_SHIFT) ? SDL_HIGHC(sym) : sym;
    }
}

/*
 * The window resizes; the grid does not.  Dragging a corner picks a new
 * point size rather than a new number of columns, which is what a player
 * reaching for the corner is really after -- and it leaves CO and LI alone,
 * so the layout wintty.c computed at startup stays valid and no re-layout
 * (which only SIGWINCH could ask for anyway) is ever needed.
 */
static void
sdl_rescale()
{
    int w, h, pt;

    if (!sdl_ren) return;
    SDL_GetRendererOutputSize(sdl_ren, &w, &h);
    w -= 2 * SDL_MARGIN;
    h -= 2 * SDL_MARGIN;
    if (w < 1 || h < 1) return;         /* minimised */

    pt = sdl_fit_ptsize(w, h);
    if (getenv("NH_SDL_DEBUG"))
        fprintf(stderr, "sdl_rescale %dx%d px -> %dpt (was %dpt)\n",
                w, h, pt, font_ptsize);
    if (pt != font_ptsize) (void) sdl_load_font(pt);
    sdl_center();
    grid_dirty = TRUE;
}

static void
sdl_pump()
{
    SDL_Event ev;

    if (!sdl_up) return;
    while (SDL_PollEvent(&ev)) {
        switch (ev.type) {
        case SDL_QUIT:
            want_quit = TRUE;
            break;
        case SDL_TEXTINPUT:
            sdl_queue_text(ev.text.text);
            break;
        case SDL_KEYDOWN:
            sdl_queue_key(&ev.key.keysym);
            break;
        case SDL_WINDOWEVENT:
            if (ev.window.event == SDL_WINDOWEVENT_SIZE_CHANGED)
                sdl_rescale();
            else if (ev.window.event == SDL_WINDOWEVENT_EXPOSED)
                grid_dirty = TRUE;
            break;
        }
    }
    /* An Alt press with no text behind it: send what the keysym said.
       Done after the drain, so that the SDL_TEXTINPUT belonging to the
       same key press -- which SDL queues immediately after the
       SDL_KEYDOWN -- has already had its chance to claim the prefix. */
    if (meta_pending) {
        meta_pending = FALSE;
        if (meta_fallback) sdl_queue_byte(0x80 | meta_fallback);
    }
    if (want_quit) {
        want_quit = FALSE;
        /* Same contract as losing the terminal: save and get out. */
#ifdef SIGHUP
        (void) raise(SIGHUP);
#else
        /* Windows has no SIGHUP.  hangup() is the handler that raise()
           would have reached anyway -- src/save.c defines it for us -- so
           call it directly rather than inventing a second save path. */
        hangup(0);
#endif
    }
}

int
sdl_getch()
{
    int c;

    sdl_dump_if_asked();
    if (grid_dirty) sdl_repaint();

    if (script) {
        if (script_pos < script_len)
            return (unsigned char) script[script_pos++];
        /* Scripted run finished.  The harness wants the screen as it
           stands, so record it and leave without disturbing anything. */
        sdl_repaint();
        sdl_dump_grid();
        _exit(0);
    }

    sdl_place_ime();
    for (;;) {
        sdl_pump();
        if (kq_head != kq_tail) {
            c = kq[kq_head];
            kq_head = (kq_head + 1) % KQ_SIZE;
            return c;
        }
        if (grid_dirty) sdl_repaint();
        SDL_WaitEventTimeout((SDL_Event *) 0, 20);
    }
}

/* ---------------------------------------------------------------- */
/* the tty backend interface                                            */
/* ---------------------------------------------------------------- */

void
tty_startup(wid, hgt)
int *wid, *hgt;
{
    const char *env;
    int cols = SDL_DEF_COLS, rows = SDL_DEF_ROWS;

    /*
     * Both of these have to be set before SDL_CreateWindow(), which is
     * where SDL acts on them.
     *
     * SDL's defaults are tuned for a full-screen action game, and both are
     * wrong for something standing in for a terminal:
     *
     *   - By default SDL puts _NET_WM_BYPASS_COMPOSITOR=1 on its window,
     *     and a compositing window manager honours that by suspending
     *     compositing for as long as the window is up.  On KDE the whole
     *     desktop visibly loses its compositing the moment the game
     *     starts.  Trading the user's desktop for frame latency is not a
     *     bargain a turn-based game should be making.
     *
     *   - By default SDL also inhibits the screen saver, through
     *     XScreenSaverSuspend and the org.freedesktop.ScreenSaver D-Bus
     *     interface.  NetHack spends nearly all of its time waiting for a
     *     keypress, so a player who walks away would come back to an
     *     unlocked screen.
     *
     * Plain SDL_SetHint() leaves both overridable from the environment
     * (SDL_GetHint() prefers the environment unless the priority is
     * SDL_HINT_OVERRIDE), so anyone who wants SDL's defaults back can set
     * SDL_VIDEO_X11_NET_WM_BYPASS_COMPOSITOR=1 or
     * SDL_VIDEO_ALLOW_SCREENSAVER=0.
     */
    (void) SDL_SetHint(SDL_HINT_VIDEO_X11_NET_WM_BYPASS_COMPOSITOR, "0");
    (void) SDL_SetHint(SDL_HINT_VIDEO_ALLOW_SCREENSAVER, "1");

#ifdef SDL_MAIN_HANDLED
    SDL_SetMainReady();         /* we kept our own main(); see the top of this file */
#endif
    if (SDL_Init(SDL_INIT_VIDEO) != 0) sdl_die(SDL_GetError());
    sdl_up = TRUE;
    if (TTF_Init() != 0) sdl_die(TTF_GetError());

    sdl_open_font();

    if ((env = getenv("NETHACK_SDL_COLS")) != 0 && atoi(env) > 0)
        cols = atoi(env);
    if ((env = getenv("NETHACK_SDL_ROWS")) != 0 && atoi(env) > 0)
        rows = atoi(env);
    if (cols < SDL_MIN_COLS) cols = SDL_MIN_COLS;
    if (rows < SDL_MIN_ROWS) rows = SDL_MIN_ROWS;

    sdl_win = SDL_CreateWindow("JNetHack (SDL)",
                               SDL_WINDOWPOS_UNDEFINED, SDL_WINDOWPOS_UNDEFINED,
                               cols * cell_w + 2 * SDL_MARGIN,
                               rows * cell_h + 2 * SDL_MARGIN,
                               SDL_WINDOW_RESIZABLE);
    if (!sdl_win) sdl_die(SDL_GetError());

    /* The grid has to stay whole, so the smallest legible font is also the
       smallest window: below this there would be nowhere to put the last
       rows.  SDL enforces it, so the drag simply stops there. */
    {
        int cw, ch;

        if (sdl_measure(SDL_MIN_PTSIZE, &cw, &ch))
            SDL_SetWindowMinimumSize(sdl_win, cw * cols + 2 * SDL_MARGIN,
                                     ch * rows + 2 * SDL_MARGIN);
    }

    sdl_ren = SDL_CreateRenderer(sdl_win, -1, SDL_RENDERER_ACCELERATED);
    if (!sdl_ren) sdl_ren = SDL_CreateRenderer(sdl_win, -1, 0);
    if (!sdl_ren) sdl_die(SDL_GetError());
    SDL_SetRenderDrawBlendMode(sdl_ren, SDL_BLENDMODE_BLEND);

    sdl_alloc_grid(cols, rows);
    sdl_center();

    /* Load the scripted key stream, if the harness asked for one. */
    if ((env = getenv("NH_SDL_KEYS")) != 0) {
        script_len = (int) strlen(env);
        script = (char *) alloc((unsigned) (script_len + 1));
        Strcpy(script, env);
        script_pos = 0;
    }

    CO = grid_cols;
    LI = grid_rows;
    /* Non-null so tty_curs() uses cmov(); the contents are never read. */
    CM = sdl_capname;
    ND = CD = sdl_capname;
    HI = HE = US = UE = sdl_capname;
    ul_hack = FALSE;
    AS = AE = (char *) 0;               /* no alternate character set: H3 */

#ifdef TEXTCOLOR
    {
        int c;
        for (c = 0; c < CLR_MAX; c++) hilites[c] = sdl_capname;
    }
#endif

    SDL_StartTextInput();

    *wid = grid_cols;
    *hgt = grid_rows;

    /* The width test runs instead of the game: it needs the grid and the font, but
       nothing else NetHack sets up afterwards. */
    if ((env = getenv("NH_SDL_WIDTHTEST")) != 0 && *env) {
        sdl_width_test();
        if (atoi(env) > 1) SDL_Delay(atoi(env));
        tty_shutdown();
        _exit(0);
    }
}

void
tty_shutdown()
{
    sdl_dump_if_asked();
    sdl_free_glyphs();
    if (font_bold) { TTF_CloseFont(font_bold); font_bold = 0; }
    if (font_norm) { TTF_CloseFont(font_norm); font_norm = 0; }
    if (sdl_ren) { SDL_DestroyRenderer(sdl_ren); sdl_ren = 0; }
    if (sdl_win) { SDL_DestroyWindow(sdl_win); sdl_win = 0; }
    if (sdl_up) {
        TTF_Quit();
        SDL_Quit();
        sdl_up = FALSE;
    }
}

#ifdef WIN32
/* ---- the tty-state entry points -------------------------------- */
/*
 * On Unix these come from sys/share/unixtty.c, whose SDL_GRAPHICS branches
 * are the same four lines as below; that file cannot be built here because
 * its <termios.h> and <unistd.h> includes sit outside every #ifdef.  The
 * other Windows candidates are out too: sys/winnt/nttty.c drives the Win32
 * console, and sys/share/pctty.c calls disable_ctrlP(), which no WIN32
 * source defines (sys/share/pcsys.c:493 skips it for the same reason).  So
 * the backend that owns the screen supplies them itself.
 */
char erase_char, kill_char;

void
gettty()
{
    /* No terminal to interrogate.  getline.c and topl.c want the editing
       characters, so supply the conventional ones. */
    erase_char = '\b';
    kill_char = '\025';         /* ^U */
    iflags.cbreak = TRUE;
}

/* reset terminal to original state */
void
settty(s)
const char *s;
{
    end_screen();
    if (s) raw_print(s);
    iflags.echo = OFF;
    iflags.cbreak = ON;
}

/* called by init_nhwindows() and resume_nhwindows() */
void
setftty()
{
    start_screen();
}

/*
 * Fatal startup errors.  sys/share/pctty.c's version writes through
 * putchar(), which include/sdlterm.h redirects into the grid; that is the
 * wrong place for these, since the grid is often the thing that failed and
 * the process is about to exit.  This file keeps the real stdio
 * (SDLTERM_KEEP_STDIO at the top), so send them to the console instead.
 */
void
error VA_DECL(const char *, s)
    char buf[BUFSZ];

    VA_START(s);
    VA_INIT(s, const char *);
    if (iflags.window_inited) end_screen();
    Vsprintf(buf, s, VA_ARGS);
    (void) fprintf(stderr, "\n%s\n", buf);
    (void) fflush(stderr);
    /*
     * Also in a box.  A player who started the game from Explorer has
     * nowhere to read stderr, and the most likely message here is
     * "no usable monospace font found" -- which would otherwise look
     * like the game simply not starting.
     */
    (void) SDL_ShowSimpleMessageBox(SDL_MESSAGEBOX_ERROR, "JNetHack",
                                    buf, (SDL_Window *) 0);
    VA_END();
    exit(EXIT_FAILURE);
}
#endif /* WIN32 */

/*ARGSUSED*/
void
tty_number_pad(state)
int state;
{
    /* The keypad is decoded in sdl_dirkey() straight from iflags.num_pad,
       so there is no terminal mode to switch. */
    return;
}

void
tty_start_screen()
{
    /* No TI/VS to emit: the window is ours from tty_startup() onward. */
    return;
}

void
tty_end_screen()
{
    clear_screen();
    sdl_repaint();
}

/* ---- cursor ---------------------------------------------------- */

void
cmov(x, y)
register int x, y;
{
    if (x < 0) x = 0;
    if (y < 0) y = 0;
    if (x >= grid_cols) x = grid_cols - 1;
    if (y >= grid_rows) y = grid_rows - 1;
    cur_x = x;
    cur_y = y;
    wrap_pending = FALSE;
    /* ttyDisplay is not allocated until after tty_startup() returns, and
       the width-test hook runs inside it. */
    if (ttyDisplay) {
        ttyDisplay->curx = x;
        ttyDisplay->cury = y;
    }
    grid_dirty = TRUE;
}

void
nocmov(x, y)
int x, y;
{
    /* With direct addressing there is nothing "no-cm" about it. */
    cmov(x, y);
}

void
home()
{
    cmov(0, 0);
}

void
backsp()
{
    wrap_pending = FALSE;
    if (cur_x > 0) cur_x--;
    grid_dirty = TRUE;
}

/* ---- output ---------------------------------------------------- */

/*
 * Place one code point at the cursor, occupying w cells.  Control
 * characters are given the meaning a terminal in ONLCR mode would give
 * them, because that is what wintty.c was written against.
 */
static void
sdl_put_wide(cp, w)
long cp;
int w;
{
    int x;

    if (!grid) return;

    switch (cp) {
    case '\n':
        wrap_pending = FALSE;
        cur_x = 0;
        if (++cur_y >= grid_rows) {
            sdl_scroll_up(cur_y - grid_rows + 1);
            cur_y = grid_rows - 1;
        }
        grid_dirty = TRUE;
        return;
    case '\r':
        wrap_pending = FALSE;
        cur_x = 0;
        grid_dirty = TRUE;
        return;
    case '\b':
        backsp();
        return;
    case '\t':
        wrap_pending = FALSE;
        cur_x = (cur_x + 8) & ~7;
        if (cur_x >= grid_cols) cur_x = grid_cols - 1;
        grid_dirty = TRUE;
        return;
    case '\007':
        tty_nhbell();
        return;
    case '\0':
        return;
    }

    if (wrap_pending) sdl_wrap_now();

    if (w < 1) w = 1;
    if (w > grid_cols) w = grid_cols;
    if (cur_x + w > grid_cols) {
        /* A double-width glyph never straddles the right margin: it moves
           to the next line whole.  The cell it would have half-filled is
           left blank rather than becoming a stray half glyph. */
        sdl_wrap_now();
    }

    x = cur_x;
    sdl_clear_cell(x, cur_y);           /* evict whatever was here */
    if (w == 2) sdl_clear_cell(x + 1, cur_y);

    CELL(x, cur_y).ch = cp;
    CELL(x, cur_y).attr = (unsigned char) cur_attr;
    CELL(x, cur_y).color = (unsigned char) cur_color;
    CELL(x, cur_y).wide = (w == 2) ? SW_LEFT : SW_NARROW;
    if (w == 2) {
        CELL(x + 1, cur_y).ch = cp;
        CELL(x + 1, cur_y).attr = (unsigned char) cur_attr;
        CELL(x + 1, cur_y).color = (unsigned char) cur_color;
        CELL(x + 1, cur_y).wide = SW_RIGHT;
    }

    cur_x += w;
    if (cur_x >= grid_cols) {
        cur_x = grid_cols - 1;
        wrap_pending = TRUE;
    }
    grid_dirty = TRUE;
}

/* Place one code point, taking its width from the code point itself. */
void
sdl_putcp(cp)
int cp;
{
    sdl_put_wide((long) cp, sdl_cp_width((long) cp));
}

/*
 * Place one EUC-JP two-byte character.
 *
 * The width is 2 because the character *arrived as two bytes*, and not
 * because of anything about the code point it maps to.  This is the whole
 * of the port's answer to the East Asian Ambiguous width problem that
 * UTF8-PLAN.md section 4.1 wrestled with.
 *
 * It matters concretely.  JIS row 1 contains characters whose Unicode
 * code points are Ambiguous in UAX #11 -- U+00B1 (plus-minus), U+00D7
 * (multiplication), U+2212 (minus), U+00A7 (section) and a good many
 * more.  On a terminal, whether each of those takes one column or two is
 * decided by the user's locale, and JNetHack's layout code has already
 * decided it takes two: is_kanji1()/is_kanji2() and split_japanese() in
 * japanese/jlib.c count every byte of a pair, and topl.c folds message
 * lines on that basis.  Consulting sdl_cp_width() here would make the
 * backend disagree with the port about where the next column is, which is
 * exactly the drift the cell grid exists to prevent.
 *
 * So the rule is: the encoding decides the width, not the glyph.  The
 * happy consequence is that there is nothing for the user to configure
 * and nothing that can differ between machines.
 *
 * The one exception is the half-width katakana of JIS X 0201, which EUC-JP
 * writes as SS2 (0x8E) plus one byte.  Those arrive as two bytes but are
 * one column wide -- that is the whole point of them, and every terminal
 * draws them so.  Taking the pair at face value gave the spaced-out
 * "A I U E O" of a name typed on an IME's half-width kana setting.  The
 * lead byte says which of the two rules applies, so no glyph property is
 * consulted here either.
 *
 * jlib.c's is_kanji() calls SS2 a lead byte, so the port still counts such
 * a character as two columns when it folds lines.  That errs towards short
 * lines rather than towards overrun, and it is what a real terminal does
 * with JNetHack too.
 */
void
sdl_puteuc(b1, b2)
int b1, b2;
{
    int w = ((b1 & 0xFF) == 0x8E) ? 1 : 2;

    sdl_put_wide(sdl_euc_to_ucs(b1, b2), w);
}

/*
 * Take one byte off the port, work out which character it stands for
 * under the graphics set now in effect, and place it.
 */
void
sdl_putbyte(b)
int b;
{
    long cp;

    b &= 0xFF;

    if (alt_charset && b >= 0x5F && b <= 0x7E) {
        cp = dec_special[b - 0x5F];
#ifdef ASCIIGRAPH
    } else if (iflags.IBMgraphics && b >= 0x80) {
        cp = cp437_high[b - 0x80];
    } else if (iflags.IBMgraphics && b < 0x20) {
        cp = cp437_low[b];              /* leaves BEL/BS/HT/LF/CR alone */
#endif
    } else {
        cp = b;
    }

    sdl_put_wide(cp, sdl_cp_width(cp));
}

/*
 * xputc() and xputs() go through jlib.c's accumulator, exactly as
 * win/tty/termcap.c's versions do, so that a string containing EUC-JP
 * reaches sdl_puteuc() as characters rather than as loose bytes.  There
 * are no capability strings to worry about: those died with termcap.c.
 */
void
xputc(c)
char c;
{
    cputchar((int) (unsigned char) c);
}

void
xputs(s)
const char *s;
{
    while (*s) cputchar((int) (unsigned char) *s++);
}

/* What the putchar() macro in sdlterm.h resolves to.  See the comment
   there for why this is a function rather than a macro. */
int
sdl_putchar(c)
int c;
{
    cputchar(c & 0xFF);
    return c & 0xFF;
}

void
sdl_puts(s)
const char *s;
{
    xputs(s);
    sdl_putcp('\n');
}

/*ARGSUSED*/
void
sdl_fputs(s, fp)
const char *s;
FILE *fp;
{
    if (fp == stdout || fp == stderr) xputs(s);
    else (void) fputs(s, fp);
}

int
sdl_fputc(c, fp)
int c;
FILE *fp;
{
    if (fp == stdout || fp == stderr) {
        cputchar(c & 0xFF);
        return c & 0xFF;
    }
    return fputc(c, fp);
}

int
sdl_fflush(fp)
FILE *fp;
{
    if (fp == stdout || fp == (FILE *) 0) {
        sdl_pump();
        sdl_dump_if_asked();
        if (grid_dirty) sdl_repaint();
        return 0;
    }
    return fflush(fp);
}

/* ---- erasing --------------------------------------------------- */

void
cl_end()
{
    int x;

    for (x = cur_x; x < grid_cols; x++) sdl_clear_cell(x, cur_y);
    /* Clearing the right half of a double-width glyph that starts to the
       left of the cursor would leave its left half orphaned; sdl_clear_cell
       removes both, which is the behaviour width-test case 2 checks for. */
    wrap_pending = FALSE;
    grid_dirty = TRUE;
}

void
cl_eos()
{
    int x, y;

    for (x = cur_x; x < grid_cols; x++) sdl_clear_cell(x, cur_y);
    for (y = cur_y + 1; y < grid_rows; y++)
        for (x = 0; x < grid_cols; x++) sdl_clear_cell(x, y);
    wrap_pending = FALSE;
    grid_dirty = TRUE;
}

void
clear_screen()
{
    sdl_blank_grid();
    cur_x = cur_y = 0;
    wrap_pending = FALSE;
    if (ttyDisplay) {
        ttyDisplay->curx = 0;
        ttyDisplay->cury = 0;
    }
}

/* ---- attributes ------------------------------------------------ */

void
standoutbeg()
{
    cur_attr |= SA_INVERSE;
}

void
standoutend()
{
    cur_attr &= ~SA_INVERSE;
}

void
term_start_attr(attr)
int attr;
{
    switch (attr) {
    case ATR_ULINE:     cur_attr |= SA_ULINE; break;
    case ATR_BOLD:      cur_attr |= SA_BOLD; break;
    case ATR_BLINK:     cur_attr |= SA_BLINK; break;
    case ATR_INVERSE:   cur_attr |= SA_INVERSE; break;
    case ATR_DIM:       break;
    }
}

void
term_end_attr(attr)
int attr;
{
    switch (attr) {
    case ATR_ULINE:     cur_attr &= ~SA_ULINE; break;
    case ATR_BOLD:      cur_attr &= ~SA_BOLD; break;
    case ATR_BLINK:     cur_attr &= ~SA_BLINK; break;
    case ATR_INVERSE:   cur_attr &= ~SA_INVERSE; break;
    case ATR_DIM:       break;
    }
}

void
term_start_raw_bold()
{
    cur_attr |= SA_BOLD;
}

void
term_end_raw_bold()
{
    cur_attr &= ~SA_BOLD;
}

#ifdef TEXTCOLOR
void
term_start_color(color)
int color;
{
    cur_color = (color >= 0 && color < CLR_MAX) ? color : NO_COLOR;
}

void
term_end_color()
{
    cur_color = NO_COLOR;
}

/*ARGSUSED*/
int
has_color(color)
int color;
{
    return 1;           /* every colour is available: it is our palette */
}
#endif /* TEXTCOLOR */

/* ---- alternate character set ----------------------------------- */

#ifdef ASCIIGRAPH
/*
 * On a terminal these swap the font in and out of an alternate character
 * set, and the terminal then has its own opinion about how wide the
 * result is -- box-drawing characters are East Asian Ambiguous, so that
 * opinion depends on the user's locale.  That is the H3 problem in
 * miniature.
 *
 * Here they only record which table sdl_putbyte() should read the next
 * bytes through.  No font is switched, and the width is a property of
 * the resulting code point, so the ambiguity never arises.
 */
void
graph_on()
{
    alt_charset = TRUE;
}

void
graph_off()
{
    alt_charset = FALSE;
}
#endif

/* ---- misc ------------------------------------------------------ */

void
tty_nhbell()
{
    if (flags.silent) return;
    /* No audio device is opened for the experiment; flash instead. */
    if (sdl_ren) {
        SDL_SetRenderDrawColor(sdl_ren, 90, 90, 90, 255);
        SDL_RenderClear(sdl_ren);
        SDL_RenderPresent(sdl_ren);
        SDL_Delay(20);
        grid_dirty = TRUE;
        sdl_repaint();
    }
}

/*
 * A yes/no question asked directly on the grid.
 *
 * This exists for the one prompt that happens before there is a window
 * system to ask through: getlock() in sys/unix/unixunix.c runs after
 * init_nhwindows() -- so the SDL window is up and the grid is allocated --
 * but before WIN_MESSAGE is created, so yn() would panic.  The stock code
 * falls back to reading fd 0 there, which this build must not do.
 *
 * Returns 'y' or 'n'; ESC counts as 'n', since every caller is asking
 * about destroying something.
 */
int
sdl_yn(prompt)
const char *prompt;
{
    int c;

    clear_screen();
    cmov(0, 0);
    xputs(prompt);
    sdl_repaint();

    for (;;) {
        c = sdl_getch();
        if (c == 'y' || c == 'Y') { c = 'y'; break; }
        if (c == 'n' || c == 'N' || c == '\033') { c = 'n'; break; }
        tty_nhbell();
    }

    clear_screen();
    sdl_repaint();
    return c;
}

void
tty_delay_output()
{
    if (grid_dirty) sdl_repaint();
    sdl_pump();
    if (!script) SDL_Delay(50);
}

/*
 * sys/share/ioctl.c leaves getwindowsz() empty under SYSV, so this is
 * the only place the port can learn the screen size.  It is also called
 * by wintty.c's SIGWINCH handler, which sdl_resize() raises.
 */
void
get_scr_size()
{
    CO = grid_cols;
    LI = grid_rows;
    if (getenv("NH_SDL_DEBUG"))
        fprintf(stderr, "get_scr_size -> CO=%d LI=%d\n", CO, LI);
}

/* ---------------------------------------------------------------- */
/* test hooks (see SDL-PORT.md)                                 */
/* ---------------------------------------------------------------- */

/*
 * Dump the cell grid as text so it can be diffed against the tty
 * version's screen.  Written on every flush, so the file always holds
 * what is on screen right now.
 */
void
sdl_dump_grid()
{
    const char *path = getenv("NH_SDL_DUMP");
    FILE *fp;
    int x, y, last;

    if (!path || !*path || !grid) return;
    /* Binary: the harness diffs this against a dump taken on the tty side,
       so the line endings have to stay LF even on Windows. */
    if (!(fp = fopen(path, "wb"))) return;

    for (y = 0; y < grid_rows; y++) {
        for (last = grid_cols - 1; last >= 0 && CELL(last, y).ch == ' '; last--)
            continue;
        for (x = 0; x <= last; x++) {
            long ch = CELL(x, y).ch;
            if (CELL(x, y).wide == SW_RIGHT) continue;  /* already emitted */
            if (ch < 0x80L) {
                (void) fputc((int) ch, fp);
            } else if (ch < 0x800L) {
                (void) fputc((int) (0xC0 | (ch >> 6)), fp);
                (void) fputc((int) (0x80 | (ch & 0x3F)), fp);
            } else {
                (void) fputc((int) (0xE0 | (ch >> 12)), fp);
                (void) fputc((int) (0x80 | ((ch >> 6) & 0x3F)), fp);
                (void) fputc((int) (0x80 | (ch & 0x3F)), fp);
            }
        }
        (void) fputc('\n', fp);
    }
    (void) fclose(fp);
}

/* Save what is on screen as a BMP.  Works under the dummy video driver
   too, so a headless run can still be inspected. */
static void
sdl_shot_if_asked()
{
    const char *path = getenv("NH_SDL_SHOT");
    SDL_Surface *shot;
    int w, h;

    if (!path || !*path || !sdl_ren) return;
    SDL_GetRendererOutputSize(sdl_ren, &w, &h);
    shot = SDL_CreateRGBSurfaceWithFormat(0, w, h, 32, SDL_PIXELFORMAT_ARGB8888);
    if (!shot) return;
    if (SDL_RenderReadPixels(sdl_ren, (SDL_Rect *) 0, SDL_PIXELFORMAT_ARGB8888,
                             shot->pixels, shot->pitch) == 0)
        (void) SDL_SaveBMP(shot, path);
    SDL_FreeSurface(shot);
}

static void
sdl_dump_if_asked()
{
    static int asked = -1;

    if (asked < 0) {
        const char *p = getenv("NH_SDL_DUMP");
        asked = (p && *p) ? 1 : 0;
    }
    if (asked) sdl_dump_grid();
}

/*
 * The double-width drawing test.  Runs instead of the game when
 * NH_SDL_WIDTHTEST is set, and writes its verdict to stdout.
 *
 * Text is fed in as EUC-JP bytes through jputchar(), so what is being
 * tested is the whole path the game uses -- jlib.c's byte accumulator,
 * sdl_puteuc(), the table, and the grid -- and not just the last step.
 *
 *   1. a two-byte character at column x occupies x and x+1, and x+2
 *      onward is undisturbed;
 *   2. writing a one-cell character over the right half of a two-cell
 *      character does not leave a stray left half behind, whether it is
 *      overwritten directly or erased with cl_end();
 *   3. the result does not depend on LANG;
 *   4. East Asian Ambiguous characters still take two cells, because they
 *      arrived as two bytes.  This is the case a wcwidth()-based backend
 *      gets wrong, and it is why sdl_puteuc() does not consult
 *      sdl_cp_width();
 *   5. every character in the table survives EUC -> Unicode -> EUC, so
 *      what is typed in an IME is what the game stores;
 *   6. half-width katakana takes one cell, even though it too arrives as
 *      two bytes -- the exception rule 4 is stated against;
 *   7. UTF-8 from the keyboard reaches the queue as EUC-JP pairs.
 */
void
sdl_width_test()
{
    /* U+65E5 U+672C U+8A9E U+6F22 U+5B57 ("nihongo kanji") in EUC-JP */
    static const char kanji_euc[] =
        "\306\374\313\334\270\354\264\301\273\372";
    static const long kanji_ucs[] =
        { 0x65E5L, 0x672CL, 0x8A9EL, 0x6F22L, 0x5B57L };
    /*
     * JIS row 1 characters whose Unicode code points are East Asian
     * Ambiguous: U+00B1 U+00D7 U+00F7 U+00A7 U+00B0.  A terminal's column count for these depends
     * on the user's locale; JNetHack's own layout code always counts two.
     */
    static const char ambig_euc[] = "\241\336\241\337\241\340\241\370\241\353";
    static const long ambig_ucs[] =
        { 0x00B1L, 0x00D7L, 0x00F7L, 0x00A7L, 0x00B0L };
    /*
     * Half-width katakana A I U E O, U+FF71..U+FF75.  SS2 pairs: they
     * arrive as two bytes each but take one cell each.
     */
    static const char kana_euc[] =
        "\216\261\216\262\216\263\216\264\216\265";
    static const long kana_ucs[] =
        { 0xFF71L, 0xFF72L, 0xFF73L, 0xFF74L, 0xFF75L };
    int i, x, fail = 0;
    const char *lang = getenv("LANG");

    (void) printf("width test: EUC-JP double-width cells (LANG=%s)\n",
                  lang ? lang : "<unset>");
    (void) printf("    cell = %dx%d px, grid = %dx%d\n",
                  cell_w, cell_h, grid_cols, grid_rows);

    /* --- case 1: occupancy and no drift ------------------------- */
    clear_screen();
    cmov(0, 0);
    xputs("[");
    xputs(kanji_euc);
    xputs("]END");

    /* '[' at 0, five 2-cell characters at 1..10, ']' at 11, "END" at 12..14 */
    for (i = 0; i < 5; i++) {
        x = 1 + i * 2;
        if (CELL(x, 0).ch != kanji_ucs[i] || CELL(x, 0).wide != SW_LEFT) {
            (void) printf("    FAIL: kanji %d not at column %d as left half"
                          " (got U+%04lX wide=%d)\n",
                          i, x, CELL(x, 0).ch, (int) CELL(x, 0).wide);
            fail++;
        }
        if (CELL(x + 1, 0).wide != SW_RIGHT) {
            (void) printf("    FAIL: column %d is not the right half\n", x + 1);
            fail++;
        }
    }
    if (CELL(11, 0).ch != ']') {
        (void) printf("    FAIL: ']' landed at some column other than 11\n");
        fail++;
    }
    if (CELL(12, 0).ch != 'E' || CELL(13, 0).ch != 'N' ||
        CELL(14, 0).ch != 'D') {
        (void) printf("    FAIL: text after the wide run drifted\n");
        fail++;
    }
    if (!fail) (void) printf("    case 1 (occupancy, no drift): PASS\n");

    /* --- case 2: overwriting a half ----------------------------- */
    {
        int before = fail;

        cmov(4, 0);             /* right half of the second kanji */
        xputc('X');
        if (CELL(4, 0).ch != 'X' || CELL(4, 0).wide != SW_NARROW) {
            (void) printf("    FAIL: 'X' did not take column 4 cleanly\n");
            fail++;
        }
        if (CELL(3, 0).ch != ' ' || CELL(3, 0).wide != SW_NARROW) {
            (void) printf("    FAIL: orphaned left half left at column 3\n");
            fail++;
        }

        /* and the same via cl_end(), which is how wintty.c erases */
        cmov(6, 0);
        xputc('Y');             /* splits the third kanji at 5..6 */
        cmov(6, 0);
        cl_end();
        for (x = 6; x < grid_cols; x++)
            if (CELL(x, 0).ch != ' ') {
                (void) printf("    FAIL: cl_end left column %d dirty\n", x);
                fail++;
                break;
            }
        if (CELL(5, 0).wide == SW_LEFT) {
            (void) printf("    FAIL: cl_end left a dangling left half at 5\n");
            fail++;
        }
        if (fail == before)
            (void) printf("    case 2 (half overwrite, cl_end): PASS\n");
    }

    /* --- case 3: locale independence ---------------------------- */
    {
        int before = fail;
        static const char *const langs[] = {
#ifdef WIN32
            /* the POSIX names below are all rejected by the Windows CRT,
               which would make this case pass without proving anything */
            "C", "English_United States.1252", "Japanese_Japan.932",
            "Japanese_Japan.utf8", "Chinese_China.936", 0
#else
            "C", "en_US.UTF-8", "ja_JP.UTF-8", "ja_JP.eucJP", "zh_CN.UTF-8", 0
#endif
        };
        int saved[12], n, k;

        clear_screen();
        cmov(0, 0);
        xputs(kanji_euc);
        for (n = 0; n < 12; n++) saved[n] = (int) CELL(n, 0).wide;

        for (k = 0; langs[k]; k++) {
            (void) setlocale(LC_ALL, langs[k]);
            clear_screen();
            cmov(0, 0);
            xputs(kanji_euc);
            for (n = 0; n < 12; n++)
                if ((int) CELL(n, 0).wide != saved[n]) {
                    (void) printf("    FAIL: layout changed under LANG=%s\n",
                                  langs[k]);
                    fail++;
                    break;
                }
        }
        (void) setlocale(LC_ALL, "C");
        if (fail == before)
            (void) printf("    case 3 (locale independence): PASS\n");
    }

    /* --- case 4: Ambiguous characters are still two cells ------- */
    {
        int before = fail;

        clear_screen();
        cmov(0, 0);
        xputs(ambig_euc);
        for (i = 0; i < 5; i++) {
            x = i * 2;
            if (CELL(x, 0).ch != ambig_ucs[i] || CELL(x, 0).wide != SW_LEFT ||
                CELL(x + 1, 0).wide != SW_RIGHT) {
                (void) printf("    FAIL: U+%04lX is not two cells at column %d"
                              " (got U+%04lX wide=%d)\n",
                              ambig_ucs[i], x, CELL(x, 0).ch,
                              (int) CELL(x, 0).wide);
                fail++;
            }
            /* the point of the case: the code point alone says "narrow" */
            if (sdl_cp_width(ambig_ucs[i]) != 1) {
                (void) printf("    NOTE: U+%04lX is not Ambiguous after all;"
                              " pick another character for this case\n",
                              ambig_ucs[i]);
            }
        }
        if (fail == before)
            (void) printf("    case 4 (Ambiguous width from encoding): PASS\n");
    }

    /* --- case 5: EUC -> Unicode -> EUC round trip --------------- */
    {
        int before = fail, b1, b2, row, cell, checked = 0;

        for (row = 1; row <= JIS0208_ROWS; row++)
            for (cell = 1; cell <= JIS0208_CELLS; cell++) {
                long cp;

                if (!jis0208_to_ucs[(row - 1) * JIS0208_CELLS + (cell - 1)])
                    continue;
                cp = sdl_euc_to_ucs(0xA0 + row, 0xA0 + cell);
                checked++;
                if (!sdl_ucs_to_euc(cp, &b1, &b2) ||
                    b1 != 0xA0 + row || b2 != 0xA0 + cell) {
                    if (fail == before) /* report the first one only */
                        (void) printf("    FAIL: row %d cell %d (U+%04lX)"
                                      " does not round trip\n", row, cell, cp);
                    fail++;
                }
            }
        if (fail == before)
            (void) printf("    case 5 (round trip, %d characters): PASS\n",
                          checked);
        else
            (void) printf("    case 5: %d of %d characters failed\n",
                          fail - before, checked);
    }

    /* --- case 6: half-width katakana is one cell ---------------- */
    {
        int before = fail;

        clear_screen();
        cmov(0, 0);
        xputs("[");
        xputs(kana_euc);
        xputs("]END");

        /* '[' at 0, five 1-cell characters at 1..5, ']' at 6 */
        for (i = 0; i < 5; i++) {
            x = 1 + i;
            if (CELL(x, 0).ch != kana_ucs[i] ||
                CELL(x, 0).wide != SW_NARROW) {
                (void) printf("    FAIL: U+%04lX is not one cell at column %d"
                              " (got U+%04lX wide=%d)\n",
                              kana_ucs[i], x, CELL(x, 0).ch,
                              (int) CELL(x, 0).wide);
                fail++;
            }
        }
        if (CELL(6, 0).ch != ']' || CELL(7, 0).ch != 'E') {
            (void) printf("    FAIL: text after the half-width run drifted\n");
            fail++;
        }
        /* and it survives the way back, so an IME's half-width kana is
           stored as the SS2 pair the game reads */
        {
            int b1, b2;

            for (i = 0; i < 5; i++)
                if (!sdl_ucs_to_euc(kana_ucs[i], &b1, &b2) ||
                    b1 != 0x8E ||
                    b2 != (unsigned char) kana_euc[i * 2 + 1]) {
                    (void) printf("    FAIL: U+%04lX does not round trip to"
                                  " SS2\n", kana_ucs[i]);
                    fail++;
                }
        }
        if (fail == before)
            (void) printf("    case 6 (half-width katakana, 1 cell): PASS\n");
    }

    /* --- case 7: the input path, UTF-8 in -> EUC-JP in the queue -- */
    {
        int before = fail, n = 0;
        /* U+65E5 U+672C U+8A9E as UTF-8, which is what SDL_TEXTINPUT
           delivers when an IME commits "nihongo". */
        static const char utf8[] =
            "\346\227\245\346\234\254\350\252\236";
        static const unsigned char want[] =
            { 0306, 0374, 0313, 0334, 0270, 0354 };

        kq_head = kq_tail = 0;
        sdl_queue_text(utf8);
        while (kq_head != kq_tail && n < (int) sizeof want) {
            if (kq[kq_head] != want[n]) {
                (void) printf("    FAIL: input byte %d is 0x%02X, wanted"
                              " 0x%02X\n", n, kq[kq_head], want[n]);
                fail++;
            }
            kq_head = (kq_head + 1) % KQ_SIZE;
            n++;
        }
        if (n != (int) sizeof want) {
            (void) printf("    FAIL: input queue holds %d bytes, wanted %d\n",
                          n, (int) sizeof want);
            fail++;
        }
        if (kq_head != kq_tail) {
            (void) printf("    FAIL: input queue has bytes left over\n");
            fail++;
        }
        kq_head = kq_tail = 0;
        if (fail == before)
            (void) printf("    case 7 (UTF-8 input -> EUC-JP pairs): PASS\n");
    }

    /* leave something on screen for the screenshot */
    clear_screen();
    cmov(0, 0);
    xputs("|");
    xputs(kanji_euc);
    xputs("|<- columns 1..10");
    cmov(0, 1);
    xputs("....+....1....+....2");
    cmov(0, 2);
    xputs("|");
    xputs(ambig_euc);
    xputs("|<- Ambiguous, still 2 cells each");
    cmov(0, 3);
    xputs("|");
    xputs(kana_euc);
    xputs("|<- half-width katakana, 1 cell each");
    sdl_dump_grid();
    sdl_repaint();

    (void) printf("width test: %s (%d failure%s)\n", fail ? "FAIL" : "PASS",
                  fail, fail == 1 ? "" : "s");
    (void) fflush(stdout);
}

#endif /* TTY_GRAPHICS && SDL_GRAPHICS */

/*sdlterm.c*/
