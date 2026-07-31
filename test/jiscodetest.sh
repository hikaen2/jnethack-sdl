#!/bin/sh
# Build and run the JIS X 0208 <-> Unicode self-test.  See test/jiscodetest.c.
#
# Standalone: jiscode.c needs nothing from the game, so this runs before the
# tree has been configured or built.

set -e

here=`dirname "$0"`
top=`cd "$here/.." && pwd`
out=${TMPDIR:-/tmp}/jnh-jiscodetest.$$

CC=${CC:-gcc}
CFLAGS=${CFLAGS:--O2 -Wall -std=gnu89 -fcommon}

$CC $CFLAGS -I"$top/include" \
    "$top/test/jiscodetest.c" "$top/japanese/jiscode.c" \
    -o "$out"

"$out"
rc=$?
rm -f "$out"
exit $rc
