#!/bin/sh
# Screen-equivalence test: termcap backend vs SDL backend.
#
# Runs the same key sequence against the termcap build (captured through a
# pty and a VT100 emulator) and against the SDL build (captured from its own
# cell grid), then diffs the two 80x24 text screens.
#
#   ./test/compare.sh [workdir]
#
# NO FIXED SEED.  setrandom() takes its seed from the clock and this tree
# offers no way to override it, so two runs are two different dungeons.
# Only screens that do not show the map, the status line, the character
# roll or the starting inventory can be diffed at all; the case list below
# has NOT been re-measured since the seed was removed, so treat a failure
# here as "this case depends on the dungeon" until someone checks.
#
# The pty side is decoded as EUC-JP and rendered by pyte, which places
# East Asian Wide characters in two cells -- so a Japanese status line has
# to come out at the same columns on both sides for a case to pass.  Note
# that pyte gets its widths from wcwidth(), which calls East Asian
# Ambiguous characters narrow; the SDL backend calls anything that arrived
# as an EUC-JP pair wide.  A screen containing, say, U+00B1 will therefore
# differ, and the SDL side is the one that agrees with what JNetHack's own
# layout code counted.  See SDL-PORT.md.
set -e

cd "$(dirname "$0")/.."
work=${1:-/tmp/jnh-compare}

# This measures whether the two backends lay the screen out identically.
#
# The map is where they are allowed to differ, and now always do: the SDL
# build draws the eleven wall symbols as lines with rounded corners (see
# sdl_put_wall() in win/tty/sdlterm.c), which is the port's look and not
# something the termcap side can be asked for.  Cases that show the map
# therefore differ by design as well as by dungeon.  What is being compared
# on those screens is the layout around the walls, not the walls.
#
# !DECgraphics is left here for the termcap side, which does offer it; the
# SDL build has no character sets at all any more.  test/walls.sh is what
# checks the walls themselves.
opts=color,!DECgraphics

mkdir -p "$work"
: >"$work/report.txt"

for bin in src/jnethack.tty src/jnethack.sdl; do
    [ -x "$bin" ] || { echo "missing $bin -- run test/build.sh" >&2; exit 1; }
done

# Each case is "name|keys".  Keys use the ptydrive.py token syntax; the SDL
# side gets the same sequence with the tokens expanded to bytes.
cases='
startup|n v h l
rolemenu|n
racemenu|n v
alignmenu|n v h
map|n v h l SPACE SPACE
inventory|n v h l SPACE SPACE i
redraw|n v h l SPACE SPACE ^R
menu_page|n v h l SPACE SPACE i SPACE
kickprompt|n v h l SPACE SPACE \x04
spellmsg|n v h l SPACE SPACE \x14
topline_more|n v h l SPACE
helpmenu|n v h l SPACE SPACE ?
guidebook|n v h l SPACE SPACE ? a
overview_page2|n v h l SPACE SPACE ? a SPACE SPACE
extcmd_echo|n v h l SPACE SPACE # w i z a r d
options|n v h l SPACE SPACE O
escape_menu|n v h l SPACE SPACE i ESC
walkabout|n v h l SPACE SPACE l l l j j h k
quit|n v h l SPACE SPACE l l j j i ESC # q u i t RET y
endgame|n v h l SPACE SPACE l l j j i ESC # q u i t RET y SPACE SPACE
'

# ptydrive tokens -> a literal byte string for NH_SDL_KEYS
expand() {
    printf '%s' "$1" | awk '{
        for (i = 1; i <= NF; i++) {
            t = $i
            if (t == "SPACE") printf " "
            else if (t == "ESC") printf "\033"
            # RET reaches the pty as "\r" and the line discipline turns
            # it into "\n" (ICRNL) before NetHack sees it; tty_getlin()
            # accepts only "\n".  Nothing does that in front of the grid,
            # so sdl_queue_key() maps the real Return key itself and an
            # injected byte has to arrive already mapped.
            else if (t == "RET") printf "\n"
            else if (t == "NL") printf "\n"
            else if (t == "TAB") printf "\t"
            else if (t ~ /^\^./) printf "%c", (index("ABCDEFGHIJKLMNOPQRSTUVWXYZ", toupper(substr(t,2,1))))
            else if (t ~ /^\\x/) printf "%c", strtonum("0x" substr(t,3))
            else printf "%s", t
        }
    }'
}

echo "$cases" | while IFS='|' read -r name keys; do
    [ -n "$name" ] || continue

    ./test/mkplaydir.sh "$work/tty" >/dev/null
    ./test/mkplaydir.sh "$work/sdl" >/dev/null

    HACKDIR="$work/tty" NETHACKOPTIONS=$opts \
        python3 test/ptydrive.py --charset euc-jp --keys "$keys" \
            --dump "$work/$name.tty" \
            -- src/jnethack.tty -u poc >/dev/null 2>&1 || true

    HACKDIR="$work/sdl" NETHACKOPTIONS=$opts \
        NH_SDL_KEYS="$(expand "$keys")" NH_SDL_DUMP="$work/$name.sdl" \
        SDL_VIDEODRIVER=${SDL_VIDEODRIVER:-dummy} \
            timeout 60 src/jnethack.sdl -u poc >/dev/null 2>&1 || true

    # Both sides are compared with trailing blanks stripped: a terminal and
    # a cell grid disagree about whether unwritten cells past the end of a
    # line exist, and nothing on screen depends on the answer.
    sed -e 's/[ \t]*$//' "$work/$name.tty" >"$work/$name.tty.n" 2>/dev/null || : >"$work/$name.tty.n"
    sed -e 's/[ \t]*$//' "$work/$name.sdl" >"$work/$name.sdl.n" 2>/dev/null || : >"$work/$name.sdl.n"

    if diff -u "$work/$name.tty.n" "$work/$name.sdl.n" >"$work/$name.diff"; then
        echo "compare $name: PASS"
    else
        # NR>2 skips diff's own ---/+++ header, which would otherwise be
        # counted as differing lines (and '---More--' as one too).
        echo "compare $name: FAIL ($(awk 'NR>2 && /^[-+]/' "$work/$name.diff" | wc -l) differing lines)"
        sed -n '1,40p' "$work/$name.diff"
    fi
done | tee "$work/report.txt"

grep -q FAIL "$work/report.txt" && exit 1
exit 0
