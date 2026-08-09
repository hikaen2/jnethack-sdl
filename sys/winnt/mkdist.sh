#!/bin/sh
# Package the Windows build into a zip that runs from wherever it is unpacked.
#
#   ./sys/winnt/mkdist.sh          # dist/jnethack-<ver>-sdl-win64.zip
#   ./sys/winnt/mkdist.sh --build  # run test/build.sh first
#
# Build src/JNetHack.exe and datwin/dat with test/build.sh before running
# this; the data files here have to be the datwin/dat ones, since the ones
# in dat/ are laid out for LP64 and the .exe rejects them (see the note at
# the top of sys/unix/Makefile.dat).
#
# The layout is flat: sys/share/pcmain.c takes HACKDIR from NETHACKDIR, then
# HACKDIR, then exepath(argv[0]), so the .exe and its data living in one
# directory is all the configuration needed.
set -e

cd "$(dirname "$0")/../.."

if [ "$1" = --build ]; then
    ./test/build.sh
fi

[ -x src/JNetHack.exe ] || {
    echo "missing src/JNetHack.exe -- run ./test/build.sh win" >&2; exit 1; }
[ -f datwin/dat/dungeon ] || {
    echo "missing datwin/dat -- run ./test/build.sh win" >&2; exit 1; }

sdlroot=${SDLROOT:-$HOME/opt/mingw-sdl2}
for dll in SDL2.dll SDL2_ttf.dll; do
    [ -f "$sdlroot/bin/$dll" ] || {
        echo "missing $sdlroot/bin/$dll -- set SDLROOT" >&2; exit 1; }
done

# The zip carries its own font rather than relying on the player's Windows
# having MS Gothic.  M PLUS 9800 is a monospace face built from M+ whose
# advances are 500/1000 units per em for ASCII and 1000/1000 for kanji --
# exactly the 1:2 the cell grid wants -- and it passes NH_SDL_WIDTHTEST at
# the size defaults.nh asks for (24, giving 12x24 px cells).  Redistributing
# it under the SIL OFL means the licence has to travel with it, hence
# MPLUS9800-OFL.txt.
font=MPLUS9800-Regular.ttf
for f in "$font" MPLUS9800-OFL.txt; do
    [ -f "$f" ] || { echo "missing $f in the tree root" >&2; exit 1; }
done

# The version the game itself reports, so the zip cannot disagree with it.
# JNetHack 3.4 numbers itself twice: the NetHack version it is based on
# (VERSION_STRING) and its own patch level, which #version prints as
# "JNetHack ... Version <j>".  Both go in the name, as "3.4.3-0.11".
nhver=$(LC_ALL=C sed -n 's/.*#define VERSION_STRING "\([0-9.]*\)".*/\1/p' \
            include/date.h | head -1)
jver=$(LC_ALL=C sed -n 's/.*JNetHack[^"]* Version \([0-9.]*\).*/\1/p' \
            include/date.h | head -1)
[ -n "$nhver" ] || { echo "cannot read the version from include/date.h" >&2; exit 1; }
# The third component of the JNetHack number is its edit level, which the
# releases do not carry in their name (jnethack-3.4.3-0.11.diff.gz).
ver=$nhver${jver:+-$(printf '%s' "$jver" | cut -d. -f1,2)}

name=jnethack-$ver-sdl-win64
stage=dist/$name

rm -rf "$stage" "dist/$name.zip"
mkdir -p "$stage/save"

cp -f src/JNetHack.exe "$stage/"
cp -f "$sdlroot/bin/SDL2.dll" "$sdlroot/bin/SDL2_ttf.dll" "$stage/"

# libsdl.org ships SDL2_ttf.dll unstripped, with FreeType and HarfBuzz
# linked in: 68MB of which all but a few is debug symbols.  Strip the
# copies here, never the originals under $sdlroot.
strip=${MINGW_PREFIX:-x86_64-w64-mingw32-}strip
if command -v "$strip" >/dev/null; then
    "$strip" --strip-unneeded "$stage/JNetHack.exe" \
                              "$stage/SDL2.dll" "$stage/SDL2_ttf.dll"
else
    echo "warning: $strip not found; the zip will be much larger" >&2
fi

# include/config.h defines DLB, so the 112 data files are one nhdat, as in
# every released JNetHack.  The licence stays outside it (DATNODLB in
# sys/unix/Makefile.top) so that it can be read without the game.
cp -f datwin/dat/nhdat datwin/dat/license "$stage/"

cp -f "$font" MPLUS9800-OFL.txt "$stage/"

: >"$stage/record"
: >"$stage/logfile"

# The two documents the original JNH115.LZH shipped.  doc/jGuidebook.txt is
# EUC-JP in the tree, as the sources are; converted here to UTF-8 with CRLF.
# No BOM: current Windows Notepad reads UTF-8 without one.  (The original
# used Shift_JIS, which was the 2000 answer to the same problem.)
sed 's/$/\r/' doc/nethack.txt >"$stage/NetHack.txt"
iconv -f EUC-JP -t UTF-8 doc/jGuidebook.txt | sed 's/$/\r/' >"$stage/jGuidebook.txt"

# The configuration file, verbatim: it is ASCII with CRLF in the tree
# already (see .gitattributes), which is what Windows wants.  Unlike the
# documentation above it is not converted to UTF-8, so it has to stay ASCII.
#
# Named defaults.nh, not NetHack.cnf: 3.4 renamed it (src/files.c:1482) and
# the old name is only tried under MSDOS, never on WIN32.
cp sys/winnt/defaults.nh "$stage/defaults.nh"

# No wrapping directory inside the archive: unpacking with Explorer already
# creates one named after the zip, and a second would nest.
( cd "$stage" && zip -q -r "../$name.zip" . )

echo "dist/$name.zip"
unzip -l "dist/$name.zip" | tail -3
