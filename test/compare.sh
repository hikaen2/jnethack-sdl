#!/bin/sh
# Screen-equivalence test: termcap backend vs SDL backend.
#
# Runs the same key sequence against the termcap build (captured through a
# pty and a VT100 emulator) and against the SDL build (captured from its own
# cell grid), then diffs the two 80x24 text screens.
#
#   ./test/compare.sh [workdir]
#
# Both runs share one fixed RNG seed, so they see the same dungeon.
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
seed=${NETHACK_SEED:-20260729}

# This measures whether the two backends lay the screen out identically, so
# both sides run with the same symbol set.  The SDL build turns DECgraphics
# on by default (its walls are Unicode box-drawing); turning it off here
# keeps the comparison about layout rather than about which glyph a wall is.
opts=color,!DECgraphics

mkdir -p "$work"
: >"$work/report.txt"

for bin in src/jnethack.tty src/jnethack.sdl; do
    [ -x "$bin" ] || { echo "missing $bin -- run test/build.sh" >&2; exit 1; }
done

# Each case is "name|keys".  Keys use the ptydrive.py token syntax; the SDL
# side gets the same sequence with the tokens expanded to bytes.
cases='
startup|n V y
map|n V y SPACE SPACE
inventory|n V y SPACE SPACE i
redraw|n V y SPACE SPACE ^R
menu_page|n V y SPACE SPACE i SPACE
kickprompt|n V y SPACE SPACE \x04
spellmsg|n V y SPACE SPACE \x14
topline_more|n V y SPACE
helpmenu|n V y SPACE SPACE ?
guidebook|n V y SPACE SPACE ? a
overview_page2|n V y SPACE SPACE ? a SPACE SPACE
extcmd_echo|n V y SPACE SPACE # w i z a r d
options|n V y SPACE SPACE O
escape_menu|n V y SPACE SPACE i ESC
walkabout|n V y SPACE SPACE l l l j j h k
quit|n V y SPACE SPACE l l j j i ESC Q y n
endgame|n V y SPACE SPACE l l j j i ESC Q y n SPACE SPACE
'

# ptydrive tokens -> a literal byte string for NH_SDL_KEYS
expand() {
    printf '%s' "$1" | awk '{
        for (i = 1; i <= NF; i++) {
            t = $i
            if (t == "SPACE") printf " "
            else if (t == "ESC") printf "\033"
            else if (t == "RET") printf "\r"
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

    NETHACK_SEED=$seed HACKDIR="$work/tty" NETHACKOPTIONS=$opts \
        python3 test/ptydrive.py --charset euc-jp --keys "$keys" \
            --dump "$work/$name.tty" \
            -- src/jnethack.tty -u poc >/dev/null 2>&1 || true

    NETHACK_SEED=$seed HACKDIR="$work/sdl" NETHACKOPTIONS=$opts \
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
