#!/bin/sh
# Check the map walls of the SDL backend.
#
#   ./test/walls.sh [workdir] [target]
#
#   target: sdl (default) = src/jnethack.sdl
#           win           = src/JNetHack.exe under wine, playdir from datwin/dat
#
# The walls are drawn as lines with rounded corners, and nothing else is
# touched: sdl_put_wall() in win/tty/sdlterm.c names eleven map symbols and
# refuses every other glyph, so the rest of the map keeps the characters
# src/drawing.c chose for it.  Both halves of that are checked here.
#
# No reference run and no fixed seed are needed.  Every assertion below is
# about the alphabet of the screen rather than about its contents, so it
# holds for any dungeon -- which is what makes this one pass while the
# screen-diffing tests cannot (see test/compare.sh).
#
# Rooms on dlvl 1 are always lit, so the four corners of the starting room
# are on screen before a single key is pressed.
set -e

cd "$(dirname "$0")/.."
work=${1:-/tmp/jnh-walls}
target=${2:-sdl}
sdlkeys=$(printf 'y ')          # accept a random character, clear --More--

case $target in
sdl|win) ;;
*) echo "usage: walls.sh [workdir] [sdl|win]" >&2; exit 2 ;;
esac

mkdir -p "$work"

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

# The eleven, and only the eleven: sdl_put_wall() in win/tty/sdlterm.c.
WALLS = set('│─╭╮╰╯┼┴┬┤├')
CORNERS = set('╭╮╰╯')

with open(os.path.join(work, 'sdl.txt'), encoding='utf-8') as f:
    screen = f.read()

fails = []

if not (CORNERS & set(screen)):
    fails.append('no rounded corners on screen')
if not ({'│', '─'} & set(screen)):
    fails.append('no wall lines on screen')

# Anything else drawn from the box-drawing block would have to have come
# from a symbol the port has no business redrawing.
stray = sorted(set(c for c in screen
                   if 0x2500 <= ord(c) <= 0x257F and c not in WALLS))
if stray:
    fails.append('box-drawing characters outside the eleven: %s'
                 % ' '.join('U+%04X %s' % (ord(c), c) for c in stray))

# "Walls are lines, everything else is the default character."  The map and
# the status lines are ASCII plus Japanese; a symbol substituted anywhere
# else -- a DEC diamond for water, a shaded block for a door -- lands in
# between, and this is what would catch it.
odd = sorted(set(c for c in screen
                 if ord(c) > 0x7F and ord(c) < 0x3000 and c not in WALLS))
if odd:
    fails.append('non-default characters outside the walls: %s'
                 % ' '.join('U+%04X %s' % (ord(c), c) for c in odd))

if fails:
    print('%s: FAIL' % label)
    for f in fails:
        print('  ' + f)
    sys.exit(1)

corners = sum(screen.count(c) for c in CORNERS)
print('%s: PASS (%d rounded corners, nothing else redrawn)' % (label, corners))
PY
