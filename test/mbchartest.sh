#!/bin/sh
# Build and run the multibyte-layer self-test in both encodings.
#
# mbchar.c's whole reason to exist is that Phase 1 of UTF8-PLAN.md rewrites
# callers in terms of characters while the literals are still EUC-JP, and
# Phase 3 then flips the encoding without touching any of them.  Testing only
# the half that is currently compiled would leave the other half to be
# discovered broken at the worst possible moment, so both are built here.

set -e

here=`dirname "$0"`
top=`cd "$here/.." && pwd`
out=${TMPDIR:-/tmp}/jnh-mbchartest.$$

CC=${CC:-gcc}
CFLAGS=${CFLAGS:--O2 -Wall -std=gnu89 -fcommon}

rc=0

for mode in euc utf8; do
    case $mode in
        euc)  def= ;;
        utf8) def=-DJP_INTERNAL_UTF8 ;;
    esac

    $CC $CFLAGS $def -I"$top/include" \
        "$top/test/mbchartest.c" "$top/japanese/mbchar.c" \
        "$top/japanese/utf8.c" \
        -o "$out.$mode"

    "$out.$mode" || rc=1
    rm -f "$out.$mode"
done

exit $rc
