#!/bin/sh
# Golden-output regression test for japanese/jlib.c.  See test/jlibtest.c.
#
#   ./test/jlibtest.sh              # compare against test/jlib.golden
#   ./test/jlibtest.sh --record     # (re)create the baseline
#
# Same contract as test/jconjtest.sh: record once on unmodified code, then
# the file is what the Phase 1 rewrite has to reproduce.  The baseline is
# kept in UTF-8 whatever jlib.c is written in, and the encoding is detected
# rather than configured, so Phase 3's mass iconv does not invalidate it.

set -e

here=`dirname "$0"`
top=`cd "$here/.." && pwd`
work=${TMPDIR:-/tmp}/jnh-jlibtest.$$
golden="$top/test/jlib.golden"

CC=${CC:-gcc}
CFLAGS=${CFLAGS:--O2 -std=gnu89 -fcommon}

mkdir -p "$work"
trap 'rm -rf "$work"' EXIT

if iconv -f UTF-8 -t UTF-8 < "$top/japanese/jlib.c" >/dev/null 2>&1; then
    srcenc=UTF-8
else
    srcenc=EUC-JP
fi

# --- test data -------------------------------------------------------------
# Chosen for what breaks: pure ASCII, pure Japanese, the mixed case where
# byte offsets and character offsets diverge, halfwidth katakana (two bytes
# but one column in EUC-JP), the kinsoku characters split_japanese() moves
# the break around, and the brackets it pulls back.
cat > "$work/strings.utf8" <<'EOF'
hello world, this is a long ascii line for wrapping
これは日本語のテキストです。
abc漢字def漢字ghi
あなたは剣を拾った。それは呪われている！
ｱｲｳｴｵ半角カナ
[括弧]と（全角）と｛波括弧｝の話
短い
a
これは、とても、長い、日本語の、文章、です。
abcdefghijklmnopqrstuvwxyz0123456789nospaceatallinthisverylongline
一二三四五六七八九十一二三四五六七八九十一二三四五六七八九十
あ、い。う！え？お，か．き
xxxxxxxxxxxxxxxxxxxxxxxx漢字xxxxxxxxxxxxxxxxxxxx
[a](b){c}［あ］（い）｛う｝
　全角空白ではじまる行　のあいだ
ab　cd漢字ef　gh
EOF

# Individual characters for jrndm_replace: one per JIS row the function
# special-cases, so a change to any branch shows up.
cat > "$work/chars.utf8" <<'EOF'
漢
あ
ア
Ａ
３
Α
Б
蓮
堯
、
ａ
±
EOF

# Words for jnumeral()/jcounter(): each numeral, each counter word, the
# combinations readobjnam() actually parses, and things that must not match.
cat > "$work/words.utf8" <<'EOF'
一
二
三
四
五
六
七
八
九
十
一冊の
三本の
五着の
七個の
九枚の
十つの
二の
冊の
本の
着の
個の
枚の
つの
の
百
壱
a
あ
漢
一a
EOF

gen() {
    var=$1; file=$2
    iconv -f UTF-8 -t "$srcenc" < "$file" | perl -ne '
        chomp;
        next if $_ eq "";
        print "static const char *const '"$var"'[] = {\n" unless $started++;
        my $e = join "", map { sprintf "\\%03o", ord } split //;
        print "    \"$e\",\n";
        END { print "    0\n};\n" }
    '
}

{
    gen jlib_strings "$work/strings.utf8"
    gen jlib_chars "$work/chars.utf8"
    gen jlib_words "$work/words.utf8"
} > "$work/jlib_strings.h"

# --- build and run ---------------------------------------------------------
$CC $CFLAGS -I"$top/include" -I"$work" \
    "$top/test/jlibtest.c" "$top/japanese/jlib.c" \
    "$top/japanese/mbchar.c" "$top/japanese/utf8.c" "$top/japanese/jiscode.c" \
    -o "$work/jlibtest"

"$work/jlibtest" | iconv -f "$srcenc" -t UTF-8 > "$work/out.utf8"

lines=`wc -l < "$work/out.utf8"`

# split_japanese must never lose or invent bytes.  This is a property, not a
# baseline, so it fails even on a fresh --record.
if grep -Uq '^LOSSY' "$work/out.utf8"; then
    echo "jlibtest: FAIL -- split_japanese did not preserve its input"
    grep -U '^LOSSY' "$work/out.utf8" | head -20
    exit 1
fi

# jrndm_replace() must never leave something that is not a character.
if grep -Uq '^NOTACHAR' "$work/out.utf8"; then
    echo "jlibtest: FAIL -- jrndm_replace produced a non-character"
    grep -U '^NOTACHAR' "$work/out.utf8" | head -10
    exit 1
fi

# jnumeral()/jcounter() must report real character lengths, not a fixed 2.
if grep -Uq '^BADLEN\|^BADCOUNTER' "$work/out.utf8"; then
    echo "jlibtest: FAIL -- a numeral or counter length is not a character length"
    grep -U '^BADLEN\|^BADCOUNTER' "$work/out.utf8" | head -20
    exit 1
fi

if [ "$1" = "--record" ]; then
    cp "$work/out.utf8" "$golden"
    echo "jlibtest: recorded $lines lines (source $srcenc)"
    exit 0
fi

if [ ! -f "$golden" ]; then
    echo "jlibtest: no baseline at $golden; run '$0 --record' first" >&2
    exit 2
fi

if diff -u "$golden" "$work/out.utf8" > "$work/diff"; then
    echo "jlibtest: PASS ($lines lines, source $srcenc)"
    exit 0
fi

echo "jlibtest: FAIL -- jlib.c output changed"
head -60 "$work/diff"
n=`grep -Uc '^[-+]' "$work/diff" || true`
echo "($n changed lines in total)"
exit 1
