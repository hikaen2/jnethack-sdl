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
# having MS Gothic.  M PLUS 1 Code is a monospace face whose kanji advance
# is exactly twice its ASCII one, which is what the cell grid wants; it
# passes NH_SDL_WIDTHTEST.  Redistributing it under the SIL OFL means the
# licence has to travel with it, hence MPLUS1Code-OFL.txt.
font=MPLUS1Code-Regular.ttf
for f in "$font" MPLUS1Code-OFL.txt; do
    [ -f "$f" ] || { echo "missing $f in the tree root" >&2; exit 1; }
done

# The version the game itself reports, so the zip cannot disagree with it.
ver=$(LC_ALL=C sed -n 's/.*JNetHack Version \([0-9.]*\).*/\1/p' include/date.h | head -1)
[ -n "$ver" ] || { echo "cannot read the version from include/date.h" >&2; exit 1; }

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

cp -f "$font" MPLUS1Code-OFL.txt "$stage/"

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

# The NetHack.cnf the original JNH115.LZH shipped (it is sys/winnt/winnt.cnf
# in this tree, unchanged since 1995), with one line corrected and a short
# section added.  ASCII only, like the original: a Japanese comment here
# would have to be EUC-JP, which no Windows editor will save back.
{
    cat <<'CNF'
# JNetHack (SDL2) additions to the stock Windows NT configuration file.
#
# The font.  MPLUS1Code-Regular.ttf is in this folder; the game runs with
# this folder as its current directory, so the bare name finds it.
SDLFONT=MPLUS1Code-Regular.ttf
#
# Point it at any other TrueType/OpenType face by full path if you prefer:
#SDLFONT=C:\Windows\Fonts\msgothic.ttc
#
# Point size.  Omitted means 18.
#SDLFONTSIZE=24
#
# Both are overridden, for one run, by the environment:
#
#   set NETHACK_SDL_FONT=C:\Windows\Fonts\msgothic.ttc
#   set NETHACK_SDL_FONTSIZE=24
#
# A Japanese name cannot be set with "OPTIONS=name:" below.  Type it at the
# prompt the game gives you instead; see README.txt.
#
# IBMgraphics, which the stock file below turns on, is commented out.  This
# build draws the map with box-drawing lines already (the DECgraphics
# option, on by default), and code page 437 cannot be used at the same time
# as EUC-JP: a map byte over 0x80 would be taken for the first half of a
# two-byte character.

CNF
    sed -e 's/^OPTIONS=IBMgraphics$/#OPTIONS=IBMgraphics\t# see above/' \
        sys/winnt/winnt.cnf
} | sed 's/$/\r/' >"$stage/NetHack.cnf"

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

このフォルダの MPLUS1Code-Regular.ttf を使います。指定しているのは同じ
フォルダの NetHack.cnf の次の行です。

    SDLFONT=MPLUS1Code-Regular.ttf

別のフォントを使いたいときはこの行を書き換えてください。フルパスでも
指定できます。

    SDLFONT=C:\Windows\Fonts\msgothic.ttc

大きさは SDLFONTSIZE で変えられます (省略時は 18)。

    SDLFONTSIZE=24

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
    MPLUS1Code-Regular.ttf  表示に使うフォント (M PLUS 1 Code)
    MPLUS1Code-OFL.txt      そのフォントのライセンス
    NetHack.cnf             設定ファイル
    README.txt              このファイル
    NetHack.txt             NetHack の概要 (英語)
    jGuidebook.txt          日本語版ガイドブック (遊びかたの詳しい説明)
    license                 ライセンス
    save/                   セーブデータの置き場所


■ 設定

同じフォルダの NetHack.cnf を編集してください。書きかたはファイルの中に
書いてあります。ゲーム中に O を押すと現在の設定を一覧・変更できます。

NetHack.cnf は ASCII のみで書いてください。日本語を書くと文字コードの
問題で正しく読めません (下の「日本語の名前について」を参照)。


■ 日本語の名前について

キャラクター名に日本語を使う場合は、ゲーム開始時に出る名前の入力欄で
入力してください。そこからの入力は正しく扱われます。

NetHack.cnf の name: とコマンドラインの -u では日本語を使えません。
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

同梱のフォント M PLUS 1 Code は SIL Open Font License 1.1 で、その本文は
同じフォルダの MPLUS1Code-OFL.txt にあります。

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
