#!/bin/sh
# Build a throwaway HACKDIR from the compiled dat/ directory.
#
#   ./test/mkplaydir.sh /path/to/playdir            # from dat/
#   ./test/mkplaydir.sh /path/to/playdir datwin/dat # for src/jnethack.exe
#
# The Windows binary needs the second form: its level files are laid out
# for LLP64 and dat/ holds LP64 ones.  See sys/unix/Makefile.dat.
#
# Safe to re-run: it wipes saves and stale lock files, so a run killed
# mid-game does not leave the next one waiting on perm_lock.
#
# Re-run it after rebuilding dat/, too.  The playdir gets a *copy* of
# nhdat, and a stale one against a current binary does not fail loudly:
# quest.dat is xcrypt()ed per line, so if the two disagree about where the
# lines are the text decrypts out of phase and comes out as plausible
# Japanese made of the wrong characters.
#
set -e

dir=${1:?usage: mkplaydir.sh DIR [datdir]}
srcdat=${2:-dat}
here=$(cd "$(dirname "$0")/.." && pwd)

mkdir -p "$dir/save"
rm -f "$dir"/*_lock "$dir"/save/* "$dir"/[0-9]*

cd "$here/$srcdat"
# include/config.h defines DLB, so everything but the licence lives in
# nhdat.  Copying the loose files as well would not help if it were
# missing: dlb_fopen() returns nothing at all once dlb_init() has failed.
cp -f nhdat license "$dir/"

: >"$dir/perm"
[ -f "$dir/record" ] || : >"$dir/record"
[ -f "$dir/logfile" ] || : >"$dir/logfile"

echo "HACKDIR ready: $dir"
