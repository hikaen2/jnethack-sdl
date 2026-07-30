#!/bin/sh
# Build a throwaway HACKDIR from the compiled dat/ directory.
#
#   ./test/mkplaydir.sh /path/to/playdir
#
# Safe to re-run: it wipes saves and stale lock files, so a run killed
# mid-game does not leave the next one waiting on perm_lock.
#
# The Japanese data files keep their 'j' names at run time -- see the
# HELP/RUMORFILE/ORACLEFILE defines in include/global.h -- so they are
# copied under those names and not renamed.
set -e

dir=${1:?usage: mkplaydir.sh DIR}
here=$(cd "$(dirname "$0")/.." && pwd)

mkdir -p "$dir/save"
rm -f "$dir"/*_lock "$dir"/save/* "$dir"/[0-9]*

cd "$here/dat"
cp -f *.lev data joracles options quest.dat jrumors license \
      jhelp jhh jcmdhelp jhistory jopthelp jwizhelp jjj dungeon "$dir/"

: >"$dir/perm"
[ -f "$dir/record" ] || : >"$dir/record"
[ -f "$dir/logfile" ] || : >"$dir/logfile"

echo "HACKDIR ready: $dir"
