#!/bin/sh
# Check that a Japanese player name survives into the Windows save file name.
#
#   ./test/winjname.sh [workdir]
#
# 3.4 builds the WIN32 save file name as
# "<user>-<plname>.NetHack-saved-game", passing it through fname_encode()
# (src/files.c set_savefile_name).  In 3.2 it went through regularize() --
# nt_regularize() on this platform -- whose loop turns every byte over 127
# into '_', so every Japanese name collapsed to one file and two players
# overwrote each other's game.  This checks that 3.4's route really does
# keep them apart, rather than assuming it from reading the code.
#
# The name is set from a defaults.nh in the playdir, not from NETHACKOPTIONS
# or -u.  Both of those cross wine's environment/argv translation, which
# replaces bytes that are not valid UTF-8 with '?' before the program sees
# them -- a wine limitation, not a JNetHack one.  A config file is read as
# bytes, so it reaches str2ic() intact.
set -e

cd "$(dirname "$0")/.."
work=${1:-/tmp/jnh-winjname}

[ -x src/JNetHack.exe ] || {
    echo "missing src/JNetHack.exe -- run test/build.sh win" >&2; exit 1; }
[ -f datwin/dat/dungeon ] || {
    echo "missing datwin/dat -- run test/build.sh win" >&2; exit 1; }

sdlroot=${SDLROOT:-$HOME/opt/mingw-sdl2}
rm -rf "$work"
mkdir -p "$work"

fail=0

# EUC-JP: あか, きく, あき.  The third shares its first character with the
# first, which is what a byte-wise prefix bug would hide.
names='\244\242\244\253 \244\255\244\257 \244\242\244\255'

for oct in $names; do
    nm=$(printf "$oct")
    hex=$(printf '%s' "$nm" | od -An -tx1 | tr -d ' \n')
    d=$work/$hex

    ./test/mkplaydir.sh "$d" datwin/dat >/dev/null
    cp -f src/JNetHack.exe "$sdlroot/bin/SDL2.dll" "$sdlroot/bin/SDL2_ttf.dll" \
          "$d/"
    { printf 'OPTIONS=color\n'
      printf 'OPTIONS=name:'; printf '%s' "$nm"; printf '\n'
    } >"$d/defaults.nh"

    # nvhl: decline the random character, then pick Valkyrie / human /
    # lawful from the three menus.  Then two spaces to clear --More--,
    # then S y to save and exit.
    ( cd "$d" &&
      NH_SDL_KEYS="nvhl  Sy" NH_SDL_DUMP=dump.txt \
        SDL_VIDEODRIVER=${SDL_VIDEODRIVER:-dummy} WINEDEBUG=${WINEDEBUG:--all} \
            timeout 120 wine ./JNetHack.exe >/dev/null 2>&1 || true )

    # The WIN32 name is "<user>-<plname>.NetHack-saved-game", in HACKDIR.
    set -- "$d"/*.NetHack-saved-game
    if [ ! -e "$1" ]; then
        echo "winjname $hex: FAIL (no save file created)"
        fail=1
        continue
    fi
    if [ $# -ne 1 ]; then
        echo "winjname $hex: FAIL ($# save files, expected 1)"
        fail=1
        continue
    fi

    # wine stores the name the program passed to CreateFile after its own
    # ANSI->UTF-16->UTF-8 round trip, so compare against that rather than
    # against the raw EUC-JP bytes.
    # The user name is prepended, so compare only the part after the '-'.
    got=$(basename "$1" .NetHack-saved-game)
    got=${got#*-}
    want=$(printf '%s' "$nm" | iconv -f iso-8859-1 -t utf-8)
    if [ "$got" != "$want" ]; then
        echo "winjname $hex: FAIL (save file is '$got', wanted '$want')"
        echo "    got : $(printf '%s' "$got" | od -An -tx1 | tr -d ' \n')"
        echo "    want: $(printf '%s' "$want" | od -An -tx1 | tr -d ' \n')"
        fail=1
        continue
    fi
    echo "winjname $hex: PASS (save file kept the name, no '_' substitution)"
done

# Distinctness is the point of the whole exercise.
n_total=$(ls -d "$work"/*/ | wc -l)
n_uniq=$(for f in "$work"/*/*.NetHack-saved-game; do basename "$f"; done | sort -u | wc -l)
if [ "$n_total" -ne "$n_uniq" ]; then
    echo "winjname distinct: FAIL ($n_total names produced $n_uniq file names)"
    fail=1
else
    echo "winjname distinct: PASS ($n_uniq distinct save files for $n_total names)"
fi

exit $fail
