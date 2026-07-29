#!/bin/sh
# A3: screen-equivalence test (SDL-POC-PLAN.md §7).
#
# Runs the same key sequence against the termcap build (captured through a
# pty and a VT100 emulator) and against the SDL build (captured from its
# own cell grid), then diffs the two 80x24 text screens.
#
#   ./test/a3_compare.sh [workdir]
#
# Both runs share one fixed RNG seed, so they see the same dungeon.
set -e

cd "$(dirname "$0")/.."
work=${1:-/tmp/nh-a3}
seed=${NETHACK_SEED:-20260729}

mkdir -p "$work"
: >"$work/report.txt"

for bin in src/nethack.tty src/nethack.sdl; do
    [ -x "$bin" ] || { echo "missing $bin -- run test/build.sh" >&2; exit 1; }
done

# Each case is "name|keys".  Keys use the ptydrive.py token syntax; the
# SDL side gets the same sequence with the tokens expanded to bytes.
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

fails=0
total=0

echo "$cases" | while IFS='|' read -r name keys; do
    [ -n "$name" ] || continue
    total=$((total + 1))

    ./test/mkplaydir.sh "$work/tty" >/dev/null
    ./test/mkplaydir.sh "$work/sdl" >/dev/null

    NETHACK_SEED=$seed HACKDIR="$work/tty" NETHACKOPTIONS=color \
	python3 test/ptydrive.py --keys "$keys" --dump "$work/$name.tty" \
	    -- src/nethack.tty -u poc >/dev/null 2>&1 || true

    NETHACK_SEED=$seed HACKDIR="$work/sdl" NETHACKOPTIONS=color \
	NH_SDL_KEYS="$(expand "$keys")" NH_SDL_DUMP="$work/$name.sdl" \
	SDL_VIDEODRIVER=${SDL_VIDEODRIVER:-dummy} \
	    timeout 60 src/nethack.sdl -u poc >/dev/null 2>&1 || true

    # Both sides are compared with trailing blanks stripped: a terminal
    # and a cell grid disagree about whether unwritten cells past the end
    # of a line exist, and nothing on screen depends on the answer.
    sed -e 's/[ \t]*$//' "$work/$name.tty" >"$work/$name.tty.n" 2>/dev/null || : >"$work/$name.tty.n"
    sed -e 's/[ \t]*$//' "$work/$name.sdl" >"$work/$name.sdl.n" 2>/dev/null || : >"$work/$name.sdl.n"

    if diff -u "$work/$name.tty.n" "$work/$name.sdl.n" >"$work/$name.diff"; then
	echo "A3 $name: PASS"
    else
	echo "A3 $name: FAIL ($(grep -c '^[-+][^-+]' "$work/$name.diff") differing lines)"
	sed -n '1,40p' "$work/$name.diff"
	fails=$((fails + 1))
    fi
done | tee "$work/report.txt"

grep -q FAIL "$work/report.txt" && exit 1
exit 0
