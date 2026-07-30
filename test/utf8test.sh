#!/bin/sh
# Build and run the UTF-8 primitive self-test.  See test/utf8test.c.
#
# Standalone: it needs nothing from the game but the headers, so it can be
# run before the tree has been configured or built.

set -e

here=`dirname "$0"`
top=`cd "$here/.." && pwd`
out=${TMPDIR:-/tmp}/jnh-utf8test.$$

CC=${CC:-gcc}
CFLAGS=${CFLAGS:--O2 -Wall -std=gnu89 -fcommon}

# rn2() is pulled in by hack.h's declarations only, not called, but the
# linker still wants the few symbols the test's translation units name.
$CC $CFLAGS -I"$top/include" \
    "$top/test/utf8test.c" "$top/japanese/utf8.c" \
    -o "$out"

"$out"
rc=$?
rm -f "$out"
exit $rc
