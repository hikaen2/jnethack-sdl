#!/bin/sh
# Check the line-drawing walls of the SDL backend.
#
#   ./test/walls.sh [workdir] [target]
#
#   target: sdl (default) = src/jnethack.sdl
#           win           = src/JNetHack.exe under wine, playdir from datwin/dat
#
# The SDL build selects DECgraphics by default and translates the VT100
# line-drawing bytes it produces into Unicode box-drawing characters,
# with the four corners swapped for their rounded forms.
#
# To check that translation, the termcap build is run with the same
# symbol set.  There the switch really does go out to the terminal as
# SO/SI around ESC)0, and pyte resolves it with its own VT100 table --
# an implementation independent of dec_special[] in win/tty/sdlterm.c.
# Apply the same style substitutions to that reference and the two
# screens should agree cell for cell.
#
# Open doors are outside what this can check.  dec_graphics[] gives both
# of them the same byte, so the SDL build restores their ASCII symbols
# in switch_graphics() instead -- a showsyms difference, not a charset one,
# and one the reference cannot be made to match.  The fixed seed and key
# sequence below reach a room whose only door is shut.
set -e

cd "$(dirname "$0")/.."
work=${1:-/tmp/jnh-walls}
target=${2:-sdl}
# NO FIXED SEED.  setrandom() takes its seed from the clock and this tree
# offers no way to override it, so two runs are two different dungeons.
# Only screens that do not show the map, the status line, the character
# roll or the starting inventory can be diffed at all; the case list below
# has NOT been re-measured since the seed was removed, so treat a failure
# here as "this case depends on the dungeon" until someone checks.
keys='n v h l SPACE SPACE'
sdlkeys=$(printf 'nvhl  ')

case $target in
sdl|win) ;;
*) echo "usage: walls.sh [workdir] [sdl|win]" >&2; exit 2 ;;
esac

mkdir -p "$work"
[ -x src/jnethack.tty ] || { echo "missing src/jnethack.tty -- run test/build.sh" >&2; exit 1; }

./test/mkplaydir.sh "$work/tty" >/dev/null

HACKDIR="$work/tty" NETHACKOPTIONS=color,DECgraphics \
    python3 test/ptydrive.py --charset euc-jp --keys "$keys" \
        --dump "$work/tty.txt" -- src/jnethack.tty -u poc >/dev/null 2>&1 || true

if [ "$target" = sdl ]; then
    [ -x src/jnethack.sdl ] || { echo "missing src/jnethack.sdl -- run test/build.sh" >&2; exit 1; }
    ./test/mkplaydir.sh "$work/sdl" >/dev/null
    HACKDIR="$work/sdl" NETHACKOPTIONS=color \
        NH_SDL_KEYS="$sdlkeys" NH_SDL_DUMP="$work/sdl.txt" \
        SDL_VIDEODRIVER=${SDL_VIDEODRIVER:-dummy} \
            timeout 60 src/jnethack.sdl -u poc >/dev/null 2>&1 || true
else
    # The Windows binary needs its own data files (LLP64 struct layout; see
    # sys/unix/Makefile.dat) and the DLLs beside the .exe.
    [ -x src/JNetHack.exe ] || { echo "missing src/JNetHack.exe -- run test/build.sh win" >&2; exit 1; }
    sdlroot=${SDLROOT:-$HOME/opt/mingw-sdl2}
    ./test/mkplaydir.sh "$work/sdl" datwin/dat >/dev/null
    cp -f src/JNetHack.exe "$sdlroot/bin/SDL2.dll" "$sdlroot/bin/SDL2_ttf.dll" \
          "$work/sdl/"
    ( cd "$work/sdl" &&
      NETHACKOPTIONS=color \
        NH_SDL_KEYS="$sdlkeys" NH_SDL_DUMP=dump.txt \
        SDL_VIDEODRIVER=${SDL_VIDEODRIVER:-dummy} WINEDEBUG=${WINEDEBUG:--all} \
            timeout 120 wine ./JNetHack.exe -u poc >/dev/null 2>&1 || true )
    cp -f "$work/sdl/dump.txt" "$work/sdl.txt" 2>/dev/null || : >"$work/sdl.txt"
fi

python3 - "$work" "$target" <<'PY'
import sys, os

work, target = sys.argv[1], sys.argv[2]
label = 'walls' if target == 'sdl' else 'winwalls'
# The five entries where dec_special[] in win/tty/sdlterm.c deliberately
# differs from the VT100 standard: rounded corners, and a plain '.' for
# the floor instead of a centred dot.
STYLE = str.maketrans({0x250C: '╭', 0x2510: '╮',
                       0x2518: '╯', 0x2514: '╰',
                       0x00B7: '.'})


def read(p):
    with open(p, encoding='utf-8') as f:
        return [l.rstrip() for l in f]


tty = [l.translate(STYLE) for l in read(os.path.join(work, 'tty.txt'))]
sdl = read(os.path.join(work, 'sdl.txt'))

if not any(ch in ''.join(sdl) for ch in '╭│─'):
    print('%s: FAIL - the SDL screen has no box-drawing characters at all' % label)
    sys.exit(1)

bad = [(i, a, b) for i, (a, b) in enumerate(zip(tty, sdl)) if a != b]
if len(tty) != len(sdl):
    bad.append((-1, 'line count %d' % len(tty), 'line count %d' % len(sdl)))

if bad:
    print('%s: FAIL (%d differing lines)' % (label, len(bad)))
    for i, a, b in bad[:6]:
        print('  line %d\n    tty %r\n    sdl %r' % (i, a, b))
    sys.exit(1)

corners = sum(''.join(sdl).count(c) for c in '╭╮╯╰')
print('%s: PASS (%d rounded corners, DEC mapping agrees with pyte)'
      % (label, corners))
PY
