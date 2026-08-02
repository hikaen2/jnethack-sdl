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
set -e

dir=${1:?usage: mkplaydir.sh DIR [datdir]}
srcdat=${2:-dat}
here=$(cd "$(dirname "$0")/.." && pwd)

mkdir -p "$dir/save"
# Unix names the level files <uid><plname>.<n> and the lock perm/*_lock;
# the Windows build (MICRO, include/ntconf.h) uses <user>-<plname>.<n>,
# <plname>.sav and NHPERM.  Clearing both keeps a run that was killed
# mid-game from making the next one ask "Recover?" -- PC_LOCKING and
# SELF_RECOVER are on for that build.
rm -f "$dir"/*_lock "$dir"/save/* "$dir"/[0-9]*
rm -f "$dir"/*.[0-9] "$dir"/*.[0-9][0-9] "$dir"/*.sav "$dir"/*.bak "$dir"/NHPERM

cd "$here/$srcdat"
# include/config.h defines DLB, so everything but the licence lives in
# nhdat.  Copying the loose files as well would not help if it were
# missing: dlb_fopen() returns nothing at all once dlb_init() has failed.
cp -f nhdat license "$dir/"

: >"$dir/perm"
[ -f "$dir/record" ] || : >"$dir/record"
[ -f "$dir/logfile" ] || : >"$dir/logfile"

echo "HACKDIR ready: $dir"
