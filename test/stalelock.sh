#!/bin/sh
# Regression test: the stale-lock prompt must be answerable without stdin.
#
#   ./test/stalelock.sh [workdir]
#
# getlock() in sys/unix/unixunix.c asks "destroy the old game?" when it finds
# a lock file.  It runs after init_nhwindows() but before WIN_MESSAGE exists,
# so yn() is not available and the stock code reads fd 0 instead.  The SDL
# build must not do that: it does not take input from fd 0, and it skips the
# isatty(0) check that would otherwise have rejected such a run.
#
# The failure this guards against is nasty and quiet.  With stdin on
# /dev/null, getchar() returns a *sticky* EOF, so the stock
#
#     while (getchar() != '\n') ;
#
# spins at 100% CPU forever while making no syscalls at all -- nothing in
# strace, nothing drawn in the window, no message anywhere.  That is the
# shape to watch for: a hang with a large utime and a near-zero stime in
# /proc/<pid>/stat.
#
# Launching from a desktop launcher is enough to reach it, so this is not
# only a test-harness concern.
set -e

cd "$(dirname "$0")/.."
work=${1:-/tmp/jnh-stalelock}
bin=src/jnethack.sdl
[ -x "$bin" ] || { echo "missing $bin -- run test/build.sh" >&2; exit 1; }

# What getlock() looks for: <uid><plname>.0, per unixmain.c and
# set_levelfile_name().
lockfile="$(id -u)poc.0"
fails=0

# An empty file is enough: veryold() only treats a lock file as stale when
# its size is sizeof(int), so a zero-length one always reaches the prompt.
setup() {
    ./test/mkplaydir.sh "$work" >/dev/null
    : >"$work/$lockfile"
}

# Run with stdin closed and a wall-clock limit.  Sets rc and elapsed; rc 137
# means it had to be killed, which is the bug.
run() {
    start=$(date +%s)
    rc=0
    SDL_VIDEODRIVER=${SDL_VIDEODRIVER:-dummy} HACKDIR="$work" \
        NETHACKOPTIONS=color \
        NH_SDL_KEYS="$1" NH_SDL_DUMP="$2" \
            timeout -s KILL 15 "$bin" -u poc </dev/null >/dev/null 2>&1 || rc=$?
    elapsed=$(( $(date +%s) - start ))
}

report() {
    echo "stalelock $1: $2"
    [ "$2" = "${2#FAIL}" ] || fails=$((fails + 1))
}

# --- the prompt is actually on screen when input is first wanted ----------
# An empty key script makes sdl_getch() dump and exit the first time the
# game asks for a key, so the dump is the screen as the player would see it.
setup
run '' "$work/prompt.txt"
if [ "$rc" = 137 ]; then
    report prompt "FAIL (killed after ${elapsed}s -- it is spinning)"
elif ! LC_ALL=C grep -aqF '[yn]' "$work/prompt.txt" 2>/dev/null; then
    report prompt "FAIL (no prompt reached the grid)"
    sed -n '1,4p' "$work/prompt.txt" 2>/dev/null
else
    report prompt "PASS (asked on the grid, not on fd 0)"
fi

# --- declining leaves the old game alone and exits promptly --------------
setup
run 'n' "$work/decline.txt"
if [ "$rc" = 137 ]; then
    report decline "FAIL (killed after ${elapsed}s -- it is spinning)"
elif [ ! -f "$work/$lockfile" ]; then
    report decline "FAIL (the old game was destroyed anyway)"
else
    report decline "PASS (exited in ${elapsed}s, old game kept)"
fi

# --- accepting clears the lock and the game starts ----------------------
setup
run 'ynvhl  ' "$work/accept.txt"
if [ "$rc" = 137 ]; then
    report accept "FAIL (killed after ${elapsed}s -- it is spinning)"
elif ! LC_ALL=C grep -aqF '$:0' "$work/accept.txt" 2>/dev/null; then
    report accept "FAIL (never reached a status line)"
    sed -n '1,6p' "$work/accept.txt" 2>/dev/null
else
    report accept "PASS (exited in ${elapsed}s, game started)"
fi

[ "$fails" = 0 ] || exit 1
exit 0
