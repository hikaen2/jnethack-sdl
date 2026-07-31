#!/bin/sh
# Check that a Japanese player name survives into the Windows save file name.
#
#   ./test/winjname.sh [workdir]
#
# include/ntconf.h maps regularize() to nt_regularize() in sys/winnt/winnt.c,
# whose stock loop turns every byte over 127 into '_'.  plname goes through
# it on the way to the save file name (src/files.c set_savefile_name), so
# without the is_kanji() skip added there, every Japanese name collapses to
# the same file and two players overwrite each other's game.
#
# The name is set from a NetHack.cnf in the playdir, not from NETHACKOPTIONS
# or -u.  Both of those cross wine's environment/argv translation, which
# replaces bytes that are not valid UTF-8 with '?' before the program sees
# them -- a wine limitation, not a JNetHack one.  A config file is read as
# bytes, so it reaches str2ic() intact.
set -e

cd "$(dirname "$0")/.."
work=${1:-/tmp/jnh-winjname}
seed=${NETHACK_SEED:-1}

[ -x src/jnethack.exe ] || {
    echo "missing src/jnethack.exe -- run test/build.sh win" >&2; exit 1; }
[ -f datwin/dat/dungeon ] || {
    echo "missing datwin/dat -- run test/build.sh win" >&2; exit 1; }

sdlroot=${SDLROOT:-$HOME/opt/mingw-sdl2}
rm -rf "$work"
mkdir -p "$work"

fail=0

# UTF-8: あか, きく, あき.  The third shares its first character with the
# first, which is what a byte-wise prefix bug would hide.  Three bytes each
# now rather than two, which is the point of running this at all.
names='\343\201\202\343\201\213 \343\201\215\343\201\217 \343\201\202\343\201\215'

for oct in $names; do
    nm=$(printf "$oct")
    hex=$(printf '%s' "$nm" | od -An -tx1 | tr -d ' \n')
    d=$work/$hex

    ./test/mkplaydir.sh "$d" datwin/dat >/dev/null
    cp -f src/jnethack.exe "$sdlroot/bin/SDL2.dll" "$sdlroot/bin/SDL2_ttf.dll" \
          "$d/"
    { printf 'OPTIONS=color\n'
      printf 'OPTIONS=name:'; printf '%s' "$nm"; printf '\n'
    } >"$d/NetHack.cnf"

    # nVy: decline the random role, pick Valkyrie, confirm.  Then two
    # spaces to clear --More--, then S y to save and exit.
    ( cd "$d" &&
      NETHACK_SEED=$seed NH_SDL_KEYS="nVy  Sy" NH_SDL_DUMP=dump.txt \
        SDL_VIDEODRIVER=${SDL_VIDEODRIVER:-dummy} WINEDEBUG=${WINEDEBUG:--all} \
            timeout 120 wine ./jnethack.exe >/dev/null 2>&1 || true )

    # MICRO puts the save file in HACKDIR as <plname>.sav (src/files.c:419).
    set -- "$d"/*.sav
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
    # against the bytes the game handed it.
    #
    # The ANSI code page is cp1252, not Latin-1, and the two differ over
    # 0x80..0x9F -- 0x82 is U+201A there, not U+0082.  It did not matter
    # while the name was EUC-JP, because every byte of that was 0xA1 or
    # above, where the two agree.  UTF-8 Japanese is full of bytes in the
    # range where they do not.  The five positions cp1252 leaves undefined
    # are mapped to the code point of the same value, which is what
    # Windows does with them.
    got=$(basename "$1" .sav)
    want=$(printf '%s' "$nm" | python3 -c '
import sys
b = sys.stdin.buffer.read()
out = []
for c in b:
    try:
        out.append(bytes([c]).decode("cp1252"))
    except UnicodeDecodeError:
        out.append(chr(c))
sys.stdout.buffer.write("".join(out).encode("utf-8"))
')
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
n_uniq=$(for f in "$work"/*/*.sav; do basename "$f"; done | sort -u | wc -l)
if [ "$n_total" -ne "$n_uniq" ]; then
    echo "winjname distinct: FAIL ($n_total names produced $n_uniq file names)"
    fail=1
else
    echo "winjname distinct: PASS ($n_uniq distinct save files for $n_total names)"
fi

exit $fail
