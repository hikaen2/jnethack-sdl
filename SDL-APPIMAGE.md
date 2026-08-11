# JNetHack 3.4.3 SDL の Linux 配布物（AppImage）

- 対象: ブランチ `experimental/jnethack-3.4.3-sdl`（[`SDL-PORT.md`](SDL-PORT.md) の続き）
- 状態: **計画。まだ実装していない**（2026-08-09 時点）
- 対になるもの: Windows 版の [`SDL-WINDOWS.md`](SDL-WINDOWS.md) と `sys/winnt/mkdist.sh`

実装したらこの文書は、`SDL-WINDOWS.md` が「§0 結果」から始まっているのと同じ形の、
実際にやったことの記録に置き換わる。

---

## Context

このツリーは Windows 向けには `sys/winnt/mkdist.sh` で「展開してダブルクリックすれば
遊べる zip」を出している（`dist/jnethack-3.4.3-0.11-sdl-win64.zip`）。
Linux 側には同等のものが無く、遊ぶには `test/build.sh` でビルドし
`test/mkplaydir.sh` で HACKDIR を手で作る必要がある。

Linux 用の自己完結な配布物として **AppImage** を作る。Windows zip と同じ位置づけ、
同じ命名規則、同じ作りの 1 スクリプトで済ませる。

満たすべき制約は 3 つ:

1. **AppImage は読み取り専用でマウントされる。** ところがこのビルドは
   `VAR_PLAYGROUND` も `LOCKDIR` も定義しておらず（`include/unixconf.h:122,128`）、
   `record` / `logfile` / `perm` / `save/` / ロックファイルがすべて HACKDIR に置かれる
   （`src/files.c:841` が `save/%d%s`）。したがって **HACKDIR は AppImage の外の
   書き込み可能な場所**でなければならない。
   `sys/unix/unixmain.c:132-133` が `NETHACKDIR` → `HACKDIR` の順に環境変数を見るので、
   AppRun がこれを使って誘導できる（`SECURE` は未定義なので制約なし）。
2. **フォント。** `win/tty/sdlterm.c:75-91` の候補は Debian/Ubuntu 固有の絶対パスで、
   Fedora や Arch では全滅して `sdl_die("no usable monospace font found")` になる。
   Windows zip と同じく **M PLUS 9800 を同梱**する必要がある。
3. **アイコン。** `sys/winnt/nhico.uu` を展開した `NetHack.ico`（32×32 / 16×16、16 色）を使う。
   `sys/winnt/console.rc:6` と `win/win32/winhack.rc:37` が Windows 版で使っている
   のと同じもので、ツリーに既にあり NetHack ライセンスで再配布できる。

## 前提（この計画で置いた判断）

- **`~/.jnethack/` に設定もデータもまとめる。**
  ゲーム本体は設定を `~/.jnethackrc`（`src/files.c:1606`）に探すが、
  `src/options.c:796-800` の `NETHACKOPTIONS=@<path>` でその場所を移せる。
  AppRun がこれを使い、ホーム直下にファイル（`.jnethackrc`）とディレクトリ（`.jnethack`）が
  並ぶのを避ける。プレイヤーの JNetHack はこの 1 ディレクトリで全部、という形にする。
  `$JNETHACK_HOME` で場所を上書きでき、`NETHACKDIR` / `NETHACKOPTIONS` を
  自分で設定している利用者はそちらが勝つ（`unixmain.c:132`、`options.c:796`）。
  AppImage をどこに置いても同じセーブと設定が見える。
- **appimagetool はパッケージ時にダウンロードする。** apt に無い。
  `$APPIMAGETOOL` が指定されていればそれを使い、無ければ `~/opt/appimagetool-x86_64.AppImage`
  を探し、それも無ければ GitHub から取得してキャッシュする。
  `sys/winnt/mkdist.sh` が `$SDLROOT` を要求するのと同じ流儀。
  取得できなければ明示的なエラーで止める（勝手に劣化しない）。

---

## 1. `win/tty/sdlterm.c` — 同梱フォントを候補の先頭に置く（1 行）

`font_candidates[]`（`win/tty/sdlterm.c:75`）の先頭に相対名を足す:

```c
static const char *const font_candidates[] = {
    /* Shipped alongside the data files by the AppImage and the Windows zip.
       A relative name resolves against the playground -- main() has chdir()ed
       there by the time sdl_open_font() runs -- so this entry finds it and
       falls through harmlessly in a tree that has no copy. */
    "MPLUS9800-Regular.ttf",
#ifdef WIN32
    ...
```

`sdl_try_font()` の頭のコメント（`win/tty/sdlterm.c:625-636`）が
「playground に置いたフォントが効く」と既に説明している設計そのもの。

これで優先順位は **`NETHACK_SDL_FONT` 環境変数 → 設定ファイルの `SDLFONT` →
同梱フォント → ホストのフォント** になる。AppRun が環境変数を強制設定すると
利用者の `SDLFONT` が無視されてしまうので、環境変数ではなくこの経路を使う。
§4 で配る `jnethackrc` の `SDLFONT` 行はコメントアウトしたままにする
——同梱フォントを見つける仕組みはこの候補 1 本に絞り、設定ファイルは
「変えたい人が変える場所」に留める。

Windows の `sys/winnt/defaults.nh:15` は引き続き `SDLFONT` で明示するが、
設定ファイルは候補より優先されるので何も壊れない。

## 2. `sys/unix/mkappimage.sh`（新規）

`sys/winnt/mkdist.sh` と対になるスクリプト。同じ構成・同じ検証・同じ命名。

```
./sys/unix/mkappimage.sh          # dist/jnethack-<ver>-sdl-x86_64.AppImage
./sys/unix/mkappimage.sh --build  # test/build.sh sdl を先に走らせる
```

やること:

1. **入力の検証** — `src/jnethack.sdl`、`dat/nhdat`、`dat/license`、`include/date.h`、
   `MPLUS9800-Regular.ttf`、`MPLUS9800-OFL.txt` が揃っているか。
   無ければ `run ./test/build.sh sdl` と言って終了。
2. **バージョン** — `mkdist.sh:52-61` の `include/date.h` から読む処理をそのまま流用し、
   `3.4.3-0.11` を得る。名前は `jnethack-3.4.3-0.11-sdl-x86_64.AppImage`。
3. **AppDir を組む**（`dist/JNetHack.AppDir`）:

   ```
   AppRun                                       # §3 のシェルスクリプト
   jnethack.desktop                             # §5
   jnethack.png            -> usr/share/icons/hicolor/256x256/apps/jnethack.png
   .DirIcon                -> 同上
   usr/bin/jnethack                             # src/jnethack.sdl（strip 済みコピー）
   usr/bin/recover                              # util/recover（INSURANCE が有効なので同梱）
   usr/lib/*.so.*                               # §6 のライブラリ
   usr/share/jnethack/nhdat                     # dat/nhdat
   usr/share/jnethack/license
   usr/share/jnethack/MPLUS9800-Regular.ttf
   usr/share/jnethack/MPLUS9800-OFL.txt
   usr/share/jnethack/jnethackrc                # §4。初回だけ playground に複写
   usr/share/doc/jnethack/nethack.txt
   usr/share/doc/jnethack/jGuidebook.txt        # EUC-JP -> UTF-8、LF のまま、BOM なし
   usr/share/applications/jnethack.desktop
   ```

4. **アイコン** — `sys/winnt/nhico.uu` を uudecode し、32×32 の面を
   ImageMagick の最近傍補間で 256×256 PNG にする:

   ```sh
   convert "$tmp/nethack.ico[0]" -alpha on -filter point -resize 256x256 "$stage/jnethack.png"
   ```

   ドット絵なので `-filter point` でぼやけない。`uudecode` が無い環境向けに
   Python の `binascii.a2b_uu` で代替する（どちらも無ければエラーで停止）。
5. **appimagetool を呼ぶ** — `ARCH=x86_64 appimagetool --appimage-extract-and-run
   dist/JNetHack.AppDir dist/<name>.AppImage`。
   `--appimage-extract-and-run` はビルド機に FUSE が無くても通すため。
6. 最後に `ls -l` と、Windows 版と同じく中身の要約を出す。

出力先の `dist/` は `.gitignore:71` で既に無視されている。

## 3. AppRun

```sh
#!/bin/sh
# The playground has to be writable and the AppImage is not, so everything
# the player owns lives in one directory of their own: the configuration,
# the data files, and everything the game writes -- record, logfile, perm,
# the lock files and save/, all of which land in HACKDIR because this build
# defines neither VAR_PLAYGROUND nor LOCKDIR.
set -e
here=$(dirname "$(readlink -f "$0")")
dir=${JNETHACK_HOME:-$HOME/.jnethack}
share=$here/usr/share/jnethack

mkdir -p "$dir/save"
# nhdat, the licence and the font are read-only data: refresh them when they
# differ, so an upgraded AppImage does not leave the old level files behind.
# dlb_fopen() gives up entirely when nhdat is missing (see include/config.h),
# so it has to be here and not merely inside the AppImage.
for f in nhdat license MPLUS9800-Regular.ttf MPLUS9800-OFL.txt; do
    cmp -s "$share/$f" "$dir/$f" || cp -f "$share/$f" "$dir/$f"
done
# The configuration is the player's, so it is seeded once and never
# overwritten.  Keeping it here rather than in ~/.jnethackrc is what lets
# this one directory be the whole of their JNetHack.
[ -f "$dir/jnethackrc" ] || cp "$share/jnethackrc" "$dir/jnethackrc"
[ -f "$dir/record" ]  || : >"$dir/record"
[ -f "$dir/logfile" ] || : >"$dir/logfile"
[ -f "$dir/perm" ]    || : >"$dir/perm"

# NETHACKDIR beats HACKDIR in sys/unix/unixmain.c:132, and NETHACKOPTIONS
# is read straight from the environment in src/options.c:796, so a player
# who sets either keeps their own.  The '@' prefix names a config file
# (src/options.c:799-800); without it the game would look in ~/.jnethackrc.
export HACKDIR="$dir"
[ -n "$NETHACKOPTIONS" ] || export NETHACKOPTIONS="@$dir/jnethackrc"
export LD_LIBRARY_PATH="$here/usr/lib${LD_LIBRARY_PATH:+:$LD_LIBRARY_PATH}"

# INSURANCE is on (include/config.h), so a crashed game leaves a checkpoint
# that only util/recover can turn back into a save file.
if [ "$1" = --recover ]; then
    shift
    cd "$dir" && exec "$here/usr/bin/recover" "$@"
fi
exec "$here/usr/bin/jnethack" "$@"
```

`LD_LIBRARY_PATH` は fork される子プロセスにも伝わるが、
`include/config.h:190` の `COMPRESS "/usr/bin/gzip"` が必要とするのは libc だけで、
§6 のとおり libc も libz も同梱しないので影響しない。

## 4. `sys/unix/jnethackrc`（新規）

`sys/winnt/defaults.nh` の Linux 版。AppRun が初回だけ `~/.jnethack/jnethackrc`
に複写する。中身は defaults.nh に倣い、ほぼ全行コメント:

```
# JNetHack の設定ファイル。AppImage が初回起動時にここへ置いた。
# 以後は上書きされないので、自由に書き換えてよい。
#
# AppImage は NETHACKOPTIONS=@~/.jnethack/jnethackrc を設定して起動するので、
# 読まれるのはこのファイルであって ~/.jnethackrc ではない。前から
# ~/.jnethackrc を使っていたなら、中身をここへ移すか、
# NETHACKOPTIONS=@$HOME/.jnethackrc を自分で設定して起動する。

# *** SDL2 ***
#
# フォント。既定では同梱の MPLUS9800-Regular.ttf がこのディレクトリから
# 見つかる（win/tty/sdlterm.c の候補の先頭）ので、指定は要らない。
#SDLFONT=MPLUS9800-Regular.ttf
#
# 文字の大きさ。省略すると 18。
#SDLFONTSIZE=18

# *** OPTIONS ***
#OPTIONS=name:Janet,role:Valkyrie,race:Human,gender:female,align:lawful
#OPTIONS=dogname:Fido,catname:Morris,fruit:guava
OPTIONS=!autopickup
```

Windows の defaults.nh は ASCII + CRLF だが、こちらは **UTF-8 + LF、BOM なし**。
`OPTIONS=IBMGraphics` に相当する行は入れない——このバックエンドは罫線を
自前で描くので（`sdlterm.c:808-818`）不要であり、Windows 版でも無効にしてある。

## 5. jnethack.desktop

```ini
[Desktop Entry]
Type=Application
Name=JNetHack
Name[ja]=JNetHack
Comment=The Japanese NetHack 3.4.3, on an SDL2 cell grid
Comment[ja]=日本語版 NetHack 3.4.3（SDL2 セルグリッド版）
Exec=jnethack
Icon=jnethack
Terminal=false
Categories=Game;RolePlaying;
Keywords=roguelike;nethack;
```

`desktop-file-validate` で検証してから appimagetool に渡す。

## 6. 同梱するライブラリ

`ldd src/jnethack.sdl` は 60 個ほど出るが、同梱するのはこの 7 つだけ:

| ライブラリ | 理由 |
|---|---|
| `libSDL2-2.0.so.0` | 本体。ホストに無い可能性がある |
| `libSDL2_ttf-2.0.so.0` | 同上 |
| `libfreetype.so.6` | SDL2_ttf 2.22 は新しい FreeType に対してビルドされている |
| `libharfbuzz.so.0` | FreeType が引く |
| `libgraphite2.so.3` | HarfBuzz が引く |
| `libbrotlidec.so.1` / `libbrotlicommon.so.1` | FreeType が引く |

残りはすべてホストのものを使う。libc / libm / libX11 / libwayland-* / libxkbcommon /
libGL / libasound / libpulse / libdrm / libgbm / libglib / libdbus / libsystemd は
AppImage の慣行どおり同梱してはいけない（SDL2 はこれらを実行時に `dlopen` して
ホストのドライバに繋ぐので、同梱すると逆に壊れる）。

スクリプトはこの 7 つを名前で持ち、`ldd` の出力から実体のパスを解決してコピーし、
`strip --strip-unneeded` をかける（コピーにだけ。`mkdist.sh:78-84` と同じ配慮）。

## 7. 文書

`SDL-PORT.md` に「§13 Linux の配布物（AppImage）」を追加し、この文書へ張る。
書くのは、playground が AppImage の外にある理由、同梱フォントが候補の先頭に入った理由、
同梱ライブラリを 7 つに絞った理由、設定を `~/.jnethackrc` から
`~/.jnethack/jnethackrc` に移した理由、そして **glibc の下限**（§8）。

## 8. 判明している限界

- **glibc。** Ubuntu 24.04（glibc 2.39）でビルドするので、AppImage は
  glibc 2.39 以降のホストでしか動かない。もっと古い環境も対象にするなら
  古いディストロのコンテナでビルドし直すしかない。今回はしない。
- **既存の `~/.jnethackrc` は AppImage では読まれない。** AppRun が
  `NETHACKOPTIONS=@~/.jnethack/jnethackrc` を設定するため。ネイティブビルドから
  移ってきた人は中身を移すか、`NETHACKOPTIONS=@$HOME/.jnethackrc` を自分で設定する。
  §4 の `jnethackrc` の冒頭にこれを書いておく。
- **`/usr/bin/gzip`** が無いホストではセーブファイルの圧縮に失敗する
  （`include/config.h:190` がパスを埋め込んでいる）。ほぼ全ディストロにある。
- **AppImage ランタイムは FUSE を要求する。** 最近の appimagetool が埋め込む
  type2-runtime は fuse3 でも動く。動かない環境では `--appimage-extract-and-run`。
- **`JNetHack.exe` にはアイコンが付いていない。** 純正 Makefile
  （`sys/winnt/Makefile.gcc:572,691`）は `nhico.uu` を展開して `windres` で
  `console.rc` を埋め込むが、`sys/unix/Makefile.src` の MinGW 経路にその手順が無い。
  **この計画の範囲外**。同じアイコンを使うので、後で 1 行足せば揃う。

---

## 変更するファイル

| ファイル | 内容 |
|---|---|
| `win/tty/sdlterm.c` | `font_candidates[]` の先頭に `"MPLUS9800-Regular.ttf"`（+ コメント） |
| `sys/unix/mkappimage.sh` | 新規。§2 のパッケージスクリプト |
| `sys/unix/jnethackrc` | 新規。§4 の設定ファイル雛形（`sys/winnt/defaults.nh` の Linux 版） |
| `SDL-PORT.md` | §13 を追加 |
| `SDL-APPIMAGE.md` | この文書を、計画から実際にやったことの記録へ書き換える |

`sys/winnt/nhico.uu` / `MPLUS9800-Regular.ttf` / `dat/*` は既にツリーにあり、そのまま使う。

## 検証

```sh
./test/build.sh sdl                  # src/jnethack.sdl と dat/
./sys/unix/mkappimage.sh             # dist/jnethack-3.4.3-0.11-sdl-x86_64.AppImage
```

1. **初回起動** — `rm -rf ~/.jnethack` してから AppImage を実行し、
   `~/.jnethack/` に `jnethackrc` を含む一式が作られ、ホーム直下には何も増えないこと。
   同梱フォントで日本語のステータス行と罫線の壁が出ること。
2. **同梱フォントが実際に使われている確認** —
   `strace -f -e openat ./dist/*.AppImage 2>&1 | grep MPLUS` で
   playground の `MPLUS9800-Regular.ttf` が開かれていることを見る。
3. **セルグリッドの退行** — `NH_SDL_WIDTHTEST=1` を AppImage に渡して 7 ケース PASS。
4. **セーブ／再開** — ゲーム内で `S`、再度起動して再開できること。
   `~/.jnethack/save/` に `<uid><plname>` ができ、`/usr/bin/gzip` 経由で
   `.gz` になっていること（`e7272e5` が直した fork 経路）。
5. **設定が効く** — `~/.jnethack/jnethackrc` に `SDLFONTSIZE=32` を足して起動し、
   文字が大きくなること（`NETHACKOPTIONS=@` 経路が通っている証拠）。
   2 回目の起動でこの編集が上書きされないことも確認する。
   さらに `NETHACKOPTIONS=@$HOME/.jnethackrc` を明示して起動すると
   そちらが読まれること（利用者の上書きが勝つ）。
6. **ウィンドウを閉じたら保存** — `test/closesave.sh` と同じ操作を手で。
7. **移設テスト** — AppImage を `/opt` など別の場所に移して起動し、同じセーブが見えること
   （playground が AppImage の位置に依存しないことの確認）。
8. **ライブラリの絞り込みの確認** — `LD_LIBRARY_PATH` を効かせた状態で
   `ldd` 相当を取り、`libc` / `libX11` / `libwayland` がホスト側から解決されていること。
9. **デスクトップ統合** — `desktop-file-validate` が通ること、
   `--appimage-extract` した中身にアイコンと .desktop が正しい場所にあること。
10. **Linux 側の退行** — `test/compare.sh` と `test/stalelock.sh` を回し、
    §1 の `sdlterm.c` 変更が既存の結果（`SDL-PORT.md` の表）を動かしていないこと。

ホストに CJK フォントが無い環境（Fedora コンテナ等）での確認は、環境を用意できれば行う。
できなければ §1 の候補追加が唯一の保証になるので、検証 2 をもって代える。
