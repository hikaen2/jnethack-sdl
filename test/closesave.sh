#!/bin/sh
# Closing the window has to save the game.
#
#   ./test/closesave.sh [workdir] [target]
#
#   target: sdl (default) = src/jnethack.sdl
#           win           = src/jnethack.exe under wine
#
# sdl_pump() in win/tty/sdlterm.c answers SDL_QUIT the same way losing the
# terminal is answered: on Unix with raise(SIGHUP), which src/save.c's
# hangup() turns into dosave0().  Windows has no SIGHUP and no signal()
# handler to install it on, so the #ifdef there calls hangup() directly --
# and src/save.c and include/decl.h had to be widened to compile it, since
# both hangup() and program_state.done_hup were #ifdef UNIX.
#
# Needs a real X display: a synthesised SDL event would not prove the WM
# path works.  test/xdrive.py --close does the closing, with genuine XTEST
# keys beforehand so the game has something worth saving.
set -e

cd "$(dirname "$0")/.."
work=${1:-/tmp/jnh-closesave}
target=${2:-sdl}

case $target in
sdl|win) ;;
*) echo "usage: closesave.sh [workdir] [sdl|win]" >&2; exit 2 ;;
esac

if [ -z "$DISPLAY" ]; then
    echo "closesave($target): SKIP (no DISPLAY)"
    exit 0
fi

d=$work/$target
rm -rf "$d"

if [ "$target" = sdl ]; then
    [ -x src/jnethack.sdl ] || { echo "missing src/jnethack.sdl -- run test/build.sh" >&2; exit 1; }
    ./test/mkplaydir.sh "$d" >/dev/null
    HACKDIR="$d" NETHACKOPTIONS=color \
        python3 test/xdrive.py --keys 'n v h l SPACE SPACE' --close \
            -- src/jnethack.sdl -u poc
else
    [ -x src/jnethack.exe ] || { echo "missing src/jnethack.exe -- run test/build.sh win" >&2; exit 1; }
    sdlroot=${SDLROOT:-$HOME/opt/mingw-sdl2}
    ./test/mkplaydir.sh "$d" datwin/dat >/dev/null
    cp -f src/jnethack.exe "$sdlroot/bin/SDL2.dll" "$sdlroot/bin/SDL2_ttf.dll" "$d/"
    # No Japanese font is installed in the wine prefix; point at the host's
    # through wine's Z: mapping of /.
    NETHACKOPTIONS=color WINEDEBUG=${WINEDEBUG:--all} \
        NETHACK_SDL_FONT=${NETHACK_SDL_FONT:-'Z:\usr\share\fonts\opentype\noto\NotoSansCJK-Regular.ttc:5'} \
        python3 test/xdrive.py --keys 'n v h l SPACE SPACE' --close \
            --cwd "$d" -- wine ./jnethack.exe -u poc
fi

# Windows writes <user>-<plname>.NetHack-saved-game into HACKDIR, MICRO
# writes <plname>.sav there, and Unix writes save/<uid><plname>
# (src/files.c set_savefile_name).
saved=$(find "$d" -maxdepth 2 \
             \( -name '*.NetHack-saved-game' -o -name '*.sav' \
                -o -path '*/save/*' \) \
             -type f -size +0 2>/dev/null | head -1)

if [ -z "$saved" ]; then
    echo "closesave($target): FAIL (window closed but nothing was saved)"
    ls -la "$d" "$d/save" 2>/dev/null | head -20
    exit 1
fi

echo "closesave($target): PASS (saved $(basename "$saved"), $(wc -c <"$saved") bytes)"
