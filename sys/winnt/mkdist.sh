#!/bin/sh
# Package the Windows build into a zip that runs from wherever it is unpacked.
#
#   ./sys/winnt/mkdist.sh          # dist/jnethack-<ver>-sdl-win64.zip
#   ./sys/winnt/mkdist.sh --build  # run test/build.sh first
#
# Build src/jnethack.exe and datwin/dat with test/build.sh before running
# this; the data files here have to be the datwin/dat ones, since the ones
# in dat/ are laid out for LP64 and the .exe rejects them (see the note at
# the top of sys/unix/Makefile.dat).
#
# The layout is flat: sys/share/pcmain.c takes HACKDIR from NETHACKDIR, then
# HACKDIR, then exepath(argv[0]), so the .exe and its data living in one
# directory is all the configuration needed.
set -e

cd "$(dirname "$0")/../.."

if [ "$1" = --build ]; then
    ./test/build.sh
fi

[ -x src/jnethack.exe ] || {
    echo "missing src/jnethack.exe -- run ./test/build.sh win" >&2; exit 1; }
[ -f datwin/dat/dungeon ] || {
    echo "missing datwin/dat -- run ./test/build.sh win" >&2; exit 1; }

sdlroot=${SDLROOT:-$HOME/opt/mingw-sdl2}
for dll in SDL2.dll SDL2_ttf.dll; do
    [ -f "$sdlroot/bin/$dll" ] || {
        echo "missing $sdlroot/bin/$dll -- set SDLROOT" >&2; exit 1; }
done

# The zip carries its own font rather than relying on the player's Windows
# having MS Gothic.  M PLUS 9800 is a monospace face built from M+ whose
# advances are 500/1000 units per em for ASCII and 1000/1000 for kanji --
# exactly the 1:2 the cell grid wants -- and it passes NH_SDL_WIDTHTEST at
# the size defaults.nh asks for (24, giving 12x24 px cells).  Redistributing
# it under the SIL OFL means the licence has to travel with it, hence
# MPLUS9800-OFL.txt.
font=MPLUS9800-Regular.ttf
for f in "$font" MPLUS9800-OFL.txt; do
    [ -f "$f" ] || { echo "missing $f in the tree root" >&2; exit 1; }
done

# The version the game itself reports, so the zip cannot disagree with it.
# JNetHack 3.4 numbers itself twice: the NetHack version it is based on
# (VERSION_STRING) and its own patch level, which #version prints as
# "JNetHack ... Version <j>".  Both go in the name, as "3.4.3-0.11".
nhver=$(LC_ALL=C sed -n 's/.*#define VERSION_STRING "\([0-9.]*\)".*/\1/p' \
            include/date.h | head -1)
jver=$(LC_ALL=C sed -n 's/.*JNetHack[^"]* Version \([0-9.]*\).*/\1/p' \
            include/date.h | head -1)
[ -n "$nhver" ] || { echo "cannot read the version from include/date.h" >&2; exit 1; }
# The third component of the JNetHack number is its edit level, which the
# releases do not carry in their name (jnethack-3.4.3-0.11.diff.gz).
ver=$nhver${jver:+-$(printf '%s' "$jver" | cut -d. -f1,2)}

name=jnethack-$ver-sdl-win64
stage=dist/$name

rm -rf "$stage" "dist/$name.zip"
mkdir -p "$stage/save"

cp -f src/jnethack.exe "$stage/"
cp -f "$sdlroot/bin/SDL2.dll" "$sdlroot/bin/SDL2_ttf.dll" "$stage/"

# libsdl.org ships SDL2_ttf.dll unstripped, with FreeType and HarfBuzz
# linked in: 68MB of which all but a few is debug symbols.  Strip the
# copies here, never the originals under $sdlroot.
strip=${MINGW_PREFIX:-x86_64-w64-mingw32-}strip
if command -v "$strip" >/dev/null; then
    "$strip" --strip-unneeded "$stage/jnethack.exe" \
                              "$stage/SDL2.dll" "$stage/SDL2_ttf.dll"
else
    echo "warning: $strip not found; the zip will be much larger" >&2
fi

# include/config.h defines DLB, so the 112 data files are one nhdat, as in
# every released JNetHack.  The licence stays outside it (DATNODLB in
# sys/unix/Makefile.top) so that it can be read without the game.
cp -f datwin/dat/nhdat datwin/dat/license "$stage/"

cp -f "$font" MPLUS9800-OFL.txt "$stage/"

: >"$stage/record"
: >"$stage/logfile"

# The two documents the original JNH115.LZH shipped.  doc/jGuidebook.txt is
# EUC-JP in the tree, as the sources are; converted here to UTF-8 with a BOM
# and CRLF so that any Windows text editor opens it correctly.  (The
# original used Shift_JIS, which was the 2000 answer to the same problem.)
sed 's/$/\r/' doc/nethack.txt >"$stage/NetHack.txt"
{ printf '\357\273\277'
  iconv -f EUC-JP -t UTF-8 doc/jGuidebook.txt | sed 's/$/\r/'
} >"$stage/jGuidebook.txt"

# The configuration file, verbatim: it is ASCII with CRLF in the tree
# already (see .gitattributes), which is what Windows wants.  Unlike the
# documentation above it is not converted to UTF-8, so it has to stay ASCII.
#
# Named defaults.nh, not NetHack.cnf: 3.4 renamed it (src/files.c:1482) and
# the old name is only tried under MSDOS, never on WIN32.
cp sys/winnt/defaults.nh "$stage/defaults.nh"

sed 's/$/\r/' >"$stage/README.txt" <<'DOC'
JNetHack VERSION_PLACEHOLDER  SDL2 版 (64-bit Windows)

これは JNetHack を SDL2 で描画するようにした移植版です。コンソールでは
なく専用のウィンドウに描画するので、Windows のコマンドプロンプトの
日本語表示の制約を受けません。


■ 遊びかた

このフォルダの jnethack.exe をダブルクリックしてください。
インストールは不要です。フォルダごとどこに置いても動きます。

セーブデータ・スコア・記録はこのフォルダの中に作られます。
アンインストールはフォルダを削除するだけです。


■ 必要なもの

  * 64-bit の Windows

フォントは同梱しているので別途用意する必要はありません。


■ フォント

このフォルダの MPLUS9800-Regular.ttf を使います。指定しているのは同じ
フォルダの defaults.nh の次の 2 行です。

    SDLFONT=MPLUS9800-Regular.ttf
    SDLFONTSIZE=24

別のフォントを使いたいときは SDLFONT を書き換えてください。フルパスでも
指定できます。

    SDLFONT=C:\Windows\Fonts\msgothic.ttc

大きさは SDLFONTSIZE で変えられます (行ごと消すと 18)。

一時的に変えたいだけなら環境変数のほうが強く、そちらが優先されます。

    set NETHACK_SDL_FONT=C:\Windows\Fonts\msgothic.ttc
    set NETHACK_SDL_FONTSIZE=24
    jnethack.exe

自分でフォントを選ぶ場合は、漢字が ASCII のちょうど 2 倍の幅である等幅
フォントにしてください。プロポーショナルなフォントでも桁はずれは起きま
せんが (レイアウトはフォントではなく内部の文字グリッドが決めます)、
見た目が窮屈になります。

SDLFONT で指定したフォントが開けないときはエラーになります。この行を
消すと、Windows 標準の MS ゴシック、游ゴシック、メイリオ、Consolas を
順に探します。


■ 同梱ファイル

    jnethack.exe            本体
    SDL2.dll                SDL2 ランタイム
    SDL2_ttf.dll            SDL2_ttf ランタイム
    nhdat                   ゲームデータ (地図・ヘルプ・格言などをまとめたもの)
    MPLUS9800-Regular.ttf   表示に使うフォント (M PLUS 9800)
    MPLUS9800-OFL.txt       そのフォントのライセンス
    defaults.nh             設定ファイル
    README.txt              このファイル
    NetHack.txt             NetHack の概要 (英語)
    jGuidebook.txt          日本語版ガイドブック (遊びかたの詳しい説明)
    license                 ライセンス
    save/                   セーブデータの置き場所


■ 設定

同じフォルダの defaults.nh を編集してください。書きかたはファイルの中に
書いてあります。ゲーム中に O を押すと現在の設定を一覧・変更できます。

defaults.nh は ASCII のみで書いてください。日本語を書くと文字コードの
問題で正しく読めません (下の「日本語の名前について」を参照)。


■ 日本語の名前について

キャラクター名に日本語を使う場合は、ゲーム開始時に出る名前の入力欄で
入力してください。そこからの入力は正しく扱われます。

defaults.nh の name: とコマンドラインの -u では日本語を使えません。
JNetHack の内部文字コードは EUC-JP ですが、Windows のテキストエディタは
EUC-JP で保存できず、コマンドラインは Shift_JIS で渡されるためです。


■ 既知の制限

  * ウィンドウの大きさは変えられません (80x24 固定)。フォントサイズは
    NETHACK_SDL_FONTSIZE で変えられます。
  * 同じフォルダで二重に起動しないでください。多重起動の検出が
    働きません。
  * IME での日本語入力は未検証です。
  * セーブデータはこの Windows 版専用です。Linux 版とは互換性が
    ありません (構造体の大きさが違います)。


■ ライセンス

ゲーム本体は同じフォルダの license を参照してください。NetHack General
Public License です。SDL2 および SDL2_ttf は zlib ライセンスです。

同梱のフォント M PLUS 9800 は M+ FONTS から作られたもので、SIL Open Font
License 1.1 です。その本文は同じフォルダの MPLUS9800-OFL.txt にあります。

  NetHack       https://www.nethack.org/
  JNetHack      http://www.jnethack.org/
  SDL           https://www.libsdl.org/
  M PLUS FONTS  https://github.com/coz-m/MPLUS_FONTS


■ 遊びかたを詳しく知りたいときは

同じフォルダの jGuidebook.txt を読んでください。日本語版のガイドブックで、
コマンド一覧から戦術まで書いてあります。ゲーム中は ? キーでヘルプが出ます。


■ この移植について

移植の内容と検証結果はソースツリーの SDL-PORT.md および
SDL-WINDOWS.md に書いてあります。
DOC

# The version in the README comes from the same place as the zip name.
sed -i "s/VERSION_PLACEHOLDER/$ver/" "$stage/README.txt"

# A UTF-8 BOM, so that Notepad on an older Windows does not read the file
# as CP932.  (The game never reads README.txt; only a human does.)
printf '\357\273\277' | cat - "$stage/README.txt" >"$stage/README.tmp"
mv "$stage/README.tmp" "$stage/README.txt"

# No wrapping directory inside the archive: unpacking with Explorer already
# creates one named after the zip, and a second would nest.
( cd "$stage" && zip -q -r "../$name.zip" . )

echo "dist/$name.zip"
unzip -l "dist/$name.zip" | tail -3
