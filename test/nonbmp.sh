#!/bin/sh
# Acceptance test for option B of UTF8-PLAN.md: the game accepts the whole of
# Unicode, not the Basic Multilingual Plane.
#
#   ./test/nonbmp.sh [workdir]
#
# The plan named this as the condition that decides whether option B was
# actually delivered, so it is a test rather than a paragraph.
#
# Under EUC-JP none of this could happen: sdl_ucs_to_euc() rejected anything
# outside JIS X 0208 at the input boundary, so a four-byte character could
# not be entered, let alone stored.  What is checked here is that it now
# survives the whole path -- config file, plname, the status line's
# column-counted truncation, the cell grid, and the dump encoder -- and comes
# back out of a save file unchanged.
#
# The characters are chosen for what they exercise:
#
#   U+20B9F  CJK ext B, four bytes of UTF-8, two columns.  A kanji that
#            JIS X 0208 does not have and EUC-JP therefore cannot spell.
#   U+1F344  an emoji, four bytes, two columns.  Nothing in the game's own
#            text looks like this.
#   U+00E9   two bytes, one column -- the narrow case, so that a failure
#            here means "four bytes" and not merely "not ASCII".
set -e

cd "$(dirname "$0")/.."
work=${1:-/tmp/jnh-nonbmp}
seed=${NETHACK_SEED:-20260729}

rm -rf "$work"
mkdir -p "$work"

fail=0

# name | octal escapes | how many columns the name occupies
cases='extb|\360\240\256\237\346\274\242|4
emoji|\360\237\215\204\360\237\215\204|4
latin1|\303\251\303\251\303\251|3'

echo "$cases" | while IFS='|' read -r label oct cols; do
    [ -n "$label" ] || continue

    nm=$(printf "$oct")
    d=$work/$label
    ./test/mkplaydir.sh "$d" >/dev/null

    { printf 'OPTIONS=color,!DECgraphics\n'
      printf 'OPTIONS=name:'; printf '%s' "$nm"; printf '\n'
    } >"$d/NetHack.cnf"

    # Two runs, because the dump is written at exit: one that stops with the
    # status line on screen, and one that goes on to save.
    #
    # nVy: decline the random role, take Valkyrie, confirm.  Then spaces to
    # clear --More--, and for the second run S y to save and quit.
    run() {
        NETHACK_SEED=$seed HACKDIR="$d" NETHACKOPTIONS=@"$d/NetHack.cnf" \
            NH_SDL_KEYS="$1" NH_SDL_DUMP="$2" \
            SDL_VIDEODRIVER=${SDL_VIDEODRIVER:-dummy} \
                timeout 120 ./src/jnethack.sdl >/dev/null 2>&1 || true
    }

    run "nVy  " "$d/dump.txt"

    # The first run leaves without saving, so it leaves its lock behind and
    # the second would sit waiting on it.  Clear the lock and the level
    # files, but not NetHack.cnf -- the name is the whole point.
    rm -f "$d"/*_lock "$d"/[0-9]* "$d"/save/* 2>/dev/null || true

    run "nVy  Sy" "$d/exit.txt"

    if [ ! -s "$d/dump.txt" ]; then
        echo "nonbmp $label: FAIL (no screen dump; the game did not start)"
        fail=1
        continue
    fi

    # 1. the name reached the screen intact.  The status line truncates to a
    #    column count, so a short name has to come through whole.
    if grep -Uq "$(printf '%s' "$nm")" "$d/dump.txt"; then
        echo "nonbmp $label: PASS (name on screen)"
    else
        echo "nonbmp $label: FAIL (name not on screen)"
        echo "    wanted: $(printf '%s' "$nm" | od -An -tx1 | tr -d ' \n')"
        echo "    line  : $(head -c 200 "$d/dump.txt" | tr -d '\000')"
        fail=1
    fi

    # 2. the dump is valid UTF-8.  This is the encoder in sdl_dump_grid(),
    #    whose four-byte branch did not exist before Phase 0 -- a code point
    #    at U+10000 or above used to have its top bits shifted off.
    if python3 -c "import sys; open(sys.argv[1],'rb').read().decode('utf-8')" \
            "$d/dump.txt" 2>/dev/null; then
        echo "nonbmp $label: PASS (dump is valid UTF-8)"
    else
        echo "nonbmp $label: FAIL (dump is not valid UTF-8)"
        fail=1
    fi

    # 3. and the save file holds the name, not a mangled version of it.
    #    plname goes into the save through the same buffers the status line
    #    truncates, so this is the part that would catch a byte-wise cut.
    #    Its *name* is built from plname too, which is a second reading of
    #    the same question through a different path.
    sf=$(ls "$d"/save/* 2>/dev/null | head -1)
    if [ -z "$sf" ]; then
        echo "nonbmp $label: FAIL (no save file)"
        fail=1
        continue
    fi
    #    The name may be cut short: regularize() holds file names to eleven
    #    bytes for a System V limit, and four of those are the uid.  What it
    #    may not do is cut through a character, so the test is that the name
    #    on disk is a whole number of characters from the front of plname.
    if python3 - "$(basename "$sf")" "$nm" <<'PY'
import sys
got, want = sys.argv[1], sys.argv[2]
# set_savefile_name() builds "save/<uid><plname>"; drop the uid.
j = 0
while j < len(got) and got[j].isdigit():
    j += 1
tail = got[j:]
sys.exit(0 if tail and want.startswith(tail) else 1)
PY
    then
        echo "nonbmp $label: PASS (save file name is whole characters of it)"
    else
        echo "nonbmp $label: FAIL (save file is '$(basename "$sf")')"
        fail=1
    fi
    if python3 - "$sf" "$nm" <<'PY'
import sys
blob = open(sys.argv[1], 'rb').read()
want = sys.argv[2].encode('utf-8')
sys.exit(0 if want in blob else 1)
PY
    then
        echo "nonbmp $label: PASS (save file holds the name)"
    else
        echo "nonbmp $label: FAIL (name not found in the save file)"
        fail=1
    fi
done

exit $fail
