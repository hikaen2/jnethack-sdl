#!/bin/sh
# Compare src/jnethack.exe (run under wine) against src/jnethack.tty.
#
#   ./test/wincompare.sh [workdir]
#
# Same idea as test/compare.sh, and the same 17 cases: drive both binaries
# with one key script and diff the 80x24 screen they end up with.
#
# 16 of the 17 have to match exactly.  The 17th is in expect_diff below,
# with the reason it cannot.
#
# The reference side is unchanged -- test/ptydrive.py needs pty(3) and only
# runs here -- so this compares the Windows build against the *Linux tty*
# build.  That is the point: it is the layout the port has to reproduce.
#
# The two sides read different data files.  jnethack.exe cannot read dat/
# (LLP64 struct layout; see sys/unix/Makefile.dat), so its playdir is
# staged from datwin/dat.  Both were compiled from the same .des sources by
# the same makedefs, so the same NETHACK_SEED gives the same dungeon.
set -e

cd "$(dirname "$0")/.."
work=${1:-/tmp/jnethack-wincompare}
seed=${NETHACK_SEED:-1}

# Matches test/compare.sh: the SDL backend defaults DECgraphics on, and this
# is about layout, not about which glyph a wall is.
opts=color,!DECgraphics

# The seed only lines up because both builds run the same generator.  See
# sys/share/rand48.c: include/ntconf.h leaves RANDOM undefined so that the
# Windows Rand() is lrand48() exactly as it is on Unix.

# Cases whose screens cannot match, with the reason.  These are reported but
# do not fail the run, and their diff is still printed.
#
#   options  The option list is platform-specific: MICRO adds the read-only
#            BIOS and rawio entries and MAIL is Unix-only (src/options.c), so
#            the two sides have a different number of options and the menu
#            paginates differently.  The loop below normalises away the menu
#            letter and those three entries, which leaves one line --
#            perm_invent fits on page 1 of the Unix list but not the Windows
#            one.
expect_diff="options"

for bin in src/jnethack.tty src/jnethack.exe; do
    [ -x "$bin" ] || { echo "missing $bin -- run test/build.sh" >&2; exit 1; }
done
[ -f datwin/dat/dungeon ] || {
    echo "missing datwin/dat -- run test/build.sh win" >&2; exit 1; }

# The DLLs have to sit next to the .exe; wine has no rpath.
sdlroot=${SDLROOT:-$HOME/opt/mingw-sdl2}
for dll in SDL2.dll SDL2_ttf.dll; do
    [ -f "$sdlroot/bin/$dll" ] || {
        echo "missing $sdlroot/bin/$dll -- set SDLROOT" >&2; exit 1; }
done

mkdir -p "$work"
: >"$work/report.txt"

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
    ./test/mkplaydir.sh "$work/win" datwin/dat >/dev/null
    cp -f src/jnethack.exe "$sdlroot/bin/SDL2.dll" "$sdlroot/bin/SDL2_ttf.dll" \
          "$work/win/"

    NETHACK_SEED=$seed HACKDIR="$work/tty" NETHACKOPTIONS=$opts \
        python3 test/ptydrive.py --charset utf-8 --keys "$keys" \
            --dump "$work/$name.tty" \
            -- src/jnethack.tty -u poc >/dev/null 2>&1 || true

    # Run with the playdir as the working directory: pcmain.c takes HACKDIR
    # from EXEPATH (argv[0]'s directory) when the variable is unset, which
    # avoids handing wine a Unix path it would have to translate.
    ( cd "$work/win" &&
      NETHACK_SEED=$seed NETHACKOPTIONS=$opts \
        NH_SDL_KEYS="$(expand "$keys")" NH_SDL_DUMP=dump.txt \
        SDL_VIDEODRIVER=${SDL_VIDEODRIVER:-dummy} WINEDEBUG=${WINEDEBUG:--all} \
            timeout 120 wine ./jnethack.exe -u poc >/dev/null 2>&1 || true )
    cp -f "$work/win/dump.txt" "$work/$name.win" 2>/dev/null || : >"$work/$name.win"

    for side in tty win; do
        # Trailing blanks: a terminal and a cell grid disagree about whether
        # unwritten cells past the end of a line exist, and nothing on
        # screen depends on the answer.
        sed -e 's/[ \t]*$//' "$work/$name.$side" >"$work/$name.$side.n" \
            2>/dev/null || : >"$work/$name.$side.n"
        # Normalise what is known to differ, so the residual diff stays
        # small enough to read: drop the menu letter and the three
        # platform-specific entries.  See expect_diff above.
        case $name in
        options)
            sed -e 's/^ [a-z] - / ? - /' \
                -e '/ BIOS  */d' -e '/ rawio  */d' -e '/ mail  */d' \
                "$work/$name.$side.n" >"$work/$name.$side.t"
            mv "$work/$name.$side.t" "$work/$name.$side.n"
            ;;
        esac
    done

    if diff -u "$work/$name.tty.n" "$work/$name.win.n" >"$work/$name.diff"; then
        echo "wincompare $name: PASS"
    else
        nlines=$(awk 'NR>2 && /^[-+]/' "$work/$name.diff" | wc -l)
        case " $expect_diff " in
        *" $name "*) echo "wincompare $name: EXPECTED-DIFF ($nlines lines)" ;;
        *)           echo "wincompare $name: FAIL ($nlines differing lines)" ;;
        esac
        sed -n '1,40p' "$work/$name.diff"
    fi
done | tee "$work/report.txt"

grep -q FAIL "$work/report.txt" && exit 1
exit 0
