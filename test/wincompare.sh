#!/bin/sh
# Compare src/JNetHack.exe (run under wine) against src/jnethack.tty.
#
#   ./test/wincompare.sh [workdir]
#
# Same idea as test/compare.sh, and the same 20 cases: drive both binaries
# with one key script and diff the 80x24 screen they end up with.
#
# Which cases can still match is measured, not assumed; see the header note
# above about the two generators.
#
# The reference side is unchanged -- test/ptydrive.py needs pty(3) and only
# runs here -- so this compares the Windows build against the *Linux tty*
# build.  That is the point: it is the layout the port has to reproduce.
#
# The two sides read different data files.  JNetHack.exe cannot read dat/
# (LLP64 struct layout; see sys/unix/Makefile.dat), so its playdir is
# staged from datwin/dat.  Both were compiled from the same .des sources by
# the same makedefs.
#
# THE TWO SIDES CANNOT SHARE A DUNGEON, for two reasons now.  They seed
# from the clock and nothing here can pin that down, and even given one
# seed they would still diverge: Linux draws from glibc's random()
# (include/unixconf.h defines LINUX) and Windows from sys/share/random.c's
# (include/ntconf.h defines RANDOM), the same generator seeded differently.
# Every screen showing the map, the status line or the inventory therefore
# differs by construction; those cases are in expect_diff.  What still
# compares is the part this port is answerable for: the menus and the
# message line.
set -e

cd "$(dirname "$0")/.."
work=${1:-/tmp/jnethack-wincompare}

# Matches test/compare.sh: the SDL backend defaults DECgraphics on, and this
# is about layout, not about which glyph a wall is.
opts=color,!DECgraphics


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
#
#   helpmenu include/ntconf.h defines PORT_HELP, so the ? menu carries one
#            more entry on Windows ("Windows に特有のヘルプおよびコマンド",
#            src/pager.c).  The entry is real -- sys/winnt/porthelp goes
#            into the Windows nhdat, see sys/unix/Makefile.dat -- and its
#            line is longer, so tty_end_menu() centres the whole menu a few
#            columns further left.  Neither side is wrong.
#
#   the rest  Every screen that shows the map, the status line or the
#            starting inventory.  The two builds seed differently (see the
#            header), so they are playing different dungeons and there is
#            nothing to compare.  Measured, not assumed: with one shared
#            generator these all matched.
#
# What is left really testing something is rolemenu / racemenu / alignmenu
# (drawn before the dungeon exists) and guidebook / overview_page2 (full
# screens of text that cover the map) -- five screens of menu and message
# layout, which is the part this port is responsible for.
expect_diff="options helpmenu
	startup map inventory redraw menu_page kickprompt spellmsg
	topline_more extcmd_echo escape_menu walkabout quit endgame"

for bin in src/jnethack.tty src/JNetHack.exe; do
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
            # See the same note in test/compare.sh: the pty side gets the
            # CR-to-LF that ICRNL would do, so an injected byte has to
            # arrive as "\n" already.
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
    ./test/mkplaydir.sh "$work/win" datwin/dat >/dev/null
    cp -f src/JNetHack.exe "$sdlroot/bin/SDL2.dll" "$sdlroot/bin/SDL2_ttf.dll" \
          "$work/win/"

    HACKDIR="$work/tty" NETHACKOPTIONS=$opts \
        python3 test/ptydrive.py --charset euc-jp --keys "$keys" \
            --dump "$work/$name.tty" \
            -- src/jnethack.tty -u poc >/dev/null 2>&1 || true

    # Run with the playdir as the working directory: pcmain.c takes HACKDIR
    # from EXEPATH (argv[0]'s directory) when the variable is unset, which
    # avoids handing wine a Unix path it would have to translate.
    ( cd "$work/win" &&
      NETHACKOPTIONS=$opts \
        NH_SDL_KEYS="$(expand "$keys")" NH_SDL_DUMP=dump.txt \
        SDL_VIDEODRIVER=${SDL_VIDEODRIVER:-dummy} WINEDEBUG=${WINEDEBUG:--all} \
            timeout 120 wine ./JNetHack.exe -u poc >/dev/null 2>&1 || true )
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
        case " $(echo $expect_diff) " in
        *" $name "*)
            echo "wincompare $name: EXPECTED-DIFF ($nlines lines)"
            # The body is only worth reading where the two sides are
            # playing the same game; for the dungeon-dependent cases it is
            # just two unrelated screens.  It stays in $work/$name.diff.
            case $name in
            options|helpmenu) sed -n '1,40p' "$work/$name.diff" ;;
            esac
            ;;
        *)
            echo "wincompare $name: FAIL ($nlines differing lines)"
            sed -n '1,40p' "$work/$name.diff"
            ;;
        esac
    fi
done | tee "$work/report.txt"

grep -q FAIL "$work/report.txt" && exit 1
exit 0
