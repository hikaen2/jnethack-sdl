#!/bin/sh
# Build a throwaway HACKDIR from the compiled dat/ directory.
#
#   ./test/mkplaydir.sh /path/to/playdir
#
# Safe to re-run: it wipes saves and stale lock files, so a run killed
# mid-game does not leave the next one waiting on perm_lock.
set -e

dir=${1:?usage: mkplaydir.sh DIR}
here=$(cd "$(dirname "$0")/.." && pwd)

mkdir -p "$dir/save"
rm -f "$dir"/*_lock "$dir"/save/* "$dir"/[0-9]*

cd "$here/dat"
cp -f *.lev data oracles options quest.dat rumors license \
      help hh cmdhelp history opthelp wizhelp dungeon "$dir/"

: >"$dir/perm"
[ -f "$dir/record" ] || : >"$dir/record"
[ -f "$dir/logfile" ] || : >"$dir/logfile"

echo "HACKDIR ready: $dir"
