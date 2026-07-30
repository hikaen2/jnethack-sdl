#!/bin/sh
# Golden-output regression test for japanese/jconj.c.  See test/jconjtest.c.
#
#   ./test/jconjtest.sh              # compare against test/jconj.golden
#   ./test/jconjtest.sh --record     # (re)create the baseline
#
# Run --record once, on unmodified jconj.c, before starting Phase 1 of
# UTF8-PLAN.md.  After that the file is the contract: the rewrite of
# jconjsub() from byte arithmetic to character arithmetic is correct exactly
# when this reports no differences.
#
# The baseline is stored in UTF-8 whatever jconj.c is currently written in,
# so that Phase 3's mass iconv of the tree does not invalidate it.  The
# encoding of the program's output is detected rather than configured --
# there is no flag day to remember.

set -e

here=`dirname "$0"`
top=`cd "$here/.." && pwd`
work=${TMPDIR:-/tmp}/jnh-jconjtest.$$
golden="$top/test/jconj.golden"

CC=${CC:-gcc}
# -O2 matters: jconj.c calls e2sj()/sj2e(), which are static in jlib.h and so
# are not linkable from here.  Every call sits behind "if (!IC)", and IC is a
# compile-time constant, so the optimiser drops those branches and with them
# the references.  At -O0 this would fail to link -- which is a fair signal
# that the Shift-JIS half of jconj.c has been dead code for a long time.
CFLAGS=${CFLAGS:--O2 -std=gnu89 -fcommon}

mkdir -p "$work"
trap 'rm -rf "$work"' EXIT

# --- extract the verb table from jconj.c -----------------------------------
# Deliberately not a copy: the harness must cover whatever the table holds
# now, including entries added after the baseline was taken.
perl -ne '
    print "static const char *const jconj_verbs[] = {\n" unless $started++;
    print "    \"$1\",\n" if /^\s*\{\s*"((?:[^"\\]|\\.)*)"\s*,\s*J_/;
    END { print "    0\n};\n" }
' "$top/japanese/jconj.c" > "$work/jconj_verbs.h"

nverbs=`grep -Uc '^    "' "$work/jconj_verbs.h" || true`
if [ "${nverbs:-0}" -lt 10 ]; then
    echo "jconjtest: only $nverbs verbs extracted from jconj.c -- the table" >&2
    echo "           format changed and the pattern in this script needs" >&2
    echo "           updating.  Refusing to run against a near-empty list." >&2
    exit 2
fi

# --- suffixes, in jconj.c's current encoding -------------------------------
# jconjsub() dispatches on the first character, so every branch it has is
# represented here.  Written as UTF-8 and converted to match jconj.c, which
# is detected below.
cat > "$work/sfx.utf8" <<'EOF'
とき
ない
なかった
ます
ますか
ません
た
たら
て
ている
ば
れば
れる
れない
える
えない
う
EOF

# Which encoding is jconj.c written in?  Phase 3 flips this from EUC-JP to
# UTF-8; detect rather than configure.
if iconv -f UTF-8 -t UTF-8 < "$top/japanese/jconj.c" >/dev/null 2>&1; then
    srcenc=UTF-8
else
    srcenc=EUC-JP
fi

iconv -f UTF-8 -t "$srcenc" < "$work/sfx.utf8" | perl -ne '
    chomp;
    print "static const char *const jconj_sfx[] = {\n" unless $started++;
    my $e = join "", map { sprintf "\\%03o", ord } split //;
    print "    \"$e\",\n";
    END { print "    0\n};\n" }
' > "$work/jconj_sfx.h"

# --- build and run ---------------------------------------------------------
$CC $CFLAGS -I"$top/include" -I"$work" \
    "$top/test/jconjtest.c" "$top/japanese/jconj.c" \
    -o "$work/jconjtest"

"$work/jconjtest" | iconv -f "$srcenc" -t UTF-8 > "$work/out.utf8"

lines=`wc -l < "$work/out.utf8"`

if [ "$1" = "--record" ]; then
    cp "$work/out.utf8" "$golden"
    echo "jconjtest: recorded $lines lines from $nverbs verbs (source $srcenc)"
    exit 0
fi

if [ ! -f "$golden" ]; then
    echo "jconjtest: no baseline at $golden; run '$0 --record' first" >&2
    exit 2
fi

if diff -u "$golden" "$work/out.utf8" > "$work/diff"; then
    echo "jconjtest: PASS ($lines lines, $nverbs verbs, source $srcenc)"
    exit 0
fi

echo "jconjtest: FAIL -- conjugation output changed"
head -60 "$work/diff"
n=`grep -Uc '^[-+]' "$work/diff" || true`
echo "($n changed lines in total)"
exit 1
