# JNetHack 1.1.5 SDL バックエンドの Windows 移植

- 対象: ブランチ `sdl/jnethack-1.1.5`（[`SDL-PORT.md`](SDL-PORT.md) の続き）
- 作成日: 2026-07-30
- 状態: **動作する。** MinGW-w64 クロスビルド + wine で検証済み

```
$ ./test/build.sh
=== building utilities ===
=== building tty backend ===
=== building SDL backend ===
=== building data files ===
=== building Windows (MinGW-w64) SDL backend ===
=== building Windows data files ===
src/jnethack.exe   src/jnethack.sdl   src/jnethack.tty
```

---

## 0. 結果

| 検証 | 結果 |
|---|---|
| `test/wincompare.sh` — Linux tty 版との画面 diff 17 ケース | **16 PASS / 1 EXPECTED-DIFF**（§7-2） |
| `test/walls.sh WORK win` — DEC 罫線の Unicode 変換 | **PASS**（角丸 4 個、pyte と一致） |
| `NH_SDL_WIDTHTEST=1` — 全角セル 6 ケース | **PASS**（6879 文字の往復含む） |
| `test/winjname.sh` — 日本語名のセーブファイル分離 | **PASS**（3 名 → 3 ファイル） |
| `test/closesave.sh WORK win` — ウィンドウを閉じたら保存 | **PASS** |
| Linux 側の退行（`compare.sh` / `walls.sh` / `stalelock.sh` / `closesave.sh`） | **すべて PASS** |

実ウィンドウ（wine の windows ドライバ）でも日本語・色・罫線が正しく出ることを
スクリーンショットで確認した。**実機 Windows 11 でも起動とフォント解決
（MS ゴシック）を確認済み**（§8）。

前身の調査文書（`experimental/sdl-nethack` の `SDL-WINDOWS-PLAN.md`、素の
NetHack 3.2.3 対象）の見立ては概ね当たっていたが、**外していた点が 3 つあった**。
それが本移植でいちばん時間を食った部分なので先に書く（§1）。

---

## 1. 事前調査が外していた 3 点

### 1-1. 【最大の誤り】`NO_TERMS` を定義してはいけない

先行調査は「`MICRO` + `NO_TERMS` + `SDL_GRAPHICS`」を推奨し、`NO_TERMS` は
`CM` / `ul_hack` の参照が消えるので**追い風**だと書いていた。逆だった。

`NO_TERMS` を定義すると `win/tty/wintty.c:34` が

```c
#ifndef NO_TERMS
#include "termcap.h"
#endif
```

で `include/termcap.h` を読まなくなる。ところが **`ASCIIGRAPH` の出どころは
`termcap.h:14-19`** であり、`wintty.c:2166` の

```c
# if defined(ASCIIGRAPH) && !defined(NO_TERMS)
    ...
    else if (ch & 0x80) {
	if (!GFlag) { graph_on(); GFlag = TRUE; }
	(void) cputchar((ch ^ 0x80));	/* Strip 8th bit */
    }
```

が `g_putch()` の中で **DEC 罫線バイトの第 8 ビットを落として
`graph_on()` を呼ぶ唯一の場所**になっている。

`src/drawing.c` の `dec_graphics[]` は `0xf8`（縦線）`0xf1`（横線）のように
**高位ビット付き**で罫線を表す。この strip が行われないと、そのバイトが
`japanese/jlib.c` の `jbuffer()` に届き、`is_kanji()` が真になって
**EUC-JP の 2 バイト対として組まれ、地図の壁が漢字になる。**

```
NO_TERMS ありのとき                NO_TERMS なしのとき（正しい）
   跣髑髑驪                          ╭──────╮
   ...%.                             │...%..│
```

そこで `include/ntconf.h` は

```c
#ifndef SDL_GRAPHICS
#define NO_TERMS
#endif
```

とした。`NO_TERMS` は「termcap 相当の画面層を持たない移植である」という意味で、
`win/tty/sdlterm.c` は **まさにその層**（`termcap.c` を関数単位で差し替え、
`NO_TERMS` が切ろうとするコードのために `tc_lcl_data` で `CM` と `ul_hack` を
供給している）なのだから、定義しないのが正しい。

副産物として `SDL-PORT.md` の H1（`wintty.c` / `topl.c` / `getline.c` 無改造）が
**Windows でも完全に成立した。** `wintty.c` は 1 行も触っていない。

### 1-2. データファイルは共有できない（LLP64）

`dat/*.lev` と `dat/dungeon` は構造体のバイナリダンプで、`src/version.c:77` が
`sizeof(struct flag/obj/monst/you)` を照合する。Linux は LP64、Windows は LLP64:

| | Linux | Windows |
|---|---|---|
| `sizeof(long)` | 8 | 4 |
| `struct you` | 1008 | 784 |
| `struct monst` | 136 | 112 |
| `struct obj` | 104 | 80 |

そのため `dat/` をそのまま渡すと起動直後に
`Configuration incompatability for file "dungeon".` で落ちる。

**`util/` も MinGW でクロスビルドし、wine で走らせてデータを作り直す**しかない。
`sys/unix/Makefile.utl` に `MINGW=1` を足し、`$(X)`（`.exe`）と
`$(RUN)`（`wine`）で切り替えるようにした。出力は `datwin/dat/`。

さらに `util/makedefs.c:113-125` は出力先を `"../dat/%s"` / `"../include/%s"` と
**決め打ち**しているので、`datwin/` を

```
datwin/dat/         ← 実際のステージング先（ここで make する）
datwin/util    -> ../util
datwin/include -> ../include
datwin/src     -> ../src
```

という形に組む。こうすると `../dat` が cwd 自身、`../util` が本物の `util/` を
指す。`test/build.sh` の `stage_datwin()` がこれを作る。

### 1-3. 乱数生成器が違うと検証できない

`include/ntconf.h` は `RANDOM` を定義していて `Rand()` が
`sys/share/random.c` の `random()` になる。一方 Unix 側は
`include/unixconf.h:304` の `Rand() lrand48()`。**同じ `NETHACK_SEED` で
別のダンジョンが生成される**ので、画面 diff がまったく成立しない。

`sys/share/rand48.c`（新規）を追加して MinGW に POSIX の
`srand48()`/`lrand48()` を供給し、`RANDOM` を定義しないようにした。
glibc の実装と 5 種のシードで各 10 万値まで一致することを確認済み。

`src/rnd.c:12` の

```c
# if defined(UNIX) || defined(RANDOM) || defined(WIN32)
#define RND(x)	(int)(Rand() % (long)(x))
# else
#define RND(x)	(int)((Rand()>>3) % (x))	/* 下位ビットが循環する生成器向け */
```

にも `WIN32` を足す必要があった。ビットの取り方が違えば生成器を揃えた意味がない。

---

## 2. 採った構成

```
WIN32 + MICRO + SDL_GRAPHICS          （NO_TERMS も WIN32CON も定義しない）
main = sys/share/pcmain.c
tty  = win/tty/sdlterm.c              （nttty.c でも termcap.c でもない）
Rand = lrand48()                      （sys/share/rand48.c）
```

- **`MICRO` は必須**。`src/files.c:166,302,482` のセーブ・bones・レベルファイルが
  `O_BINARY` で開かれるのはこれ頼みで、無いとセーブが壊れる。
- **`WIN32CON` は定義しない**。`include/wintty.h` の `WIN32CON` ブロックと
  `SDL_GRAPHICS` ブロックは排他になっておらず、両方立つと `putchar` が二重定義に
  なる。外すと `wintty.c:2476` の `nttty_open()` 呼び出しも消えるのでスタブも不要。
- **main は `pcmain.c`**。`sys/unix/unixmain.c` は `<pwd.h>` / `getuid()` /
  `setuid()` / `signal(SIGHUP)` / `SIGXCPU` を使い、`unixunix.c` の `getlock()` は
  `link()` に依存する。いずれも MinGW に無い。`pcmain.c` / `pcsys.c` / `pcunix.c` は
  3 本とも JNetHack 化済みで `init_jtrns()` も呼ぶ。

代償（承知の上）: `pcmain.c` にまともな `getlock()` が無いので多重起動の検出が
効かない。コミット `501d5ac`（`getlock()` の 100% CPU スピン修正）は
`unixunix.c` への修正なので Windows には効かないが、同時にその問題も起きない。

---

## 3. `win/tty/sdlterm.c` の変更

| 箇所 | 内容 |
|---|---|
| 先頭 | `#ifdef WIN32 #define SDL_MAIN_HANDLED`。`tty_startup()` で `SDL_SetMainReady()`。`main()` は `pcmain.c` のものを使い続ける |
| `sdl_pump()` | `#ifdef SIGHUP` があれば `raise(SIGHUP)`、無ければ **`hangup(0)` を直接呼ぶ**（下記） |
| `SDL_CreateWindow` | `SIGWINCH && CLIPPING` のときだけ `SDL_WINDOW_RESIZABLE`。Windows では固定ウィンドウ（§8） |
| `font_candidates[]` | `C:\Windows\Fonts\msgothic.ttc:0` ほかを `#ifdef WIN32` で先頭に。`sdl_try_font()` の `strrchr(':')` はドライブレターと衝突しない（コロンの次が数字かを見る）ことを実測で確認 |
| `sdl_dump_grid()` | `fopen(path, "w")` → `"wb"`。テキストモードでは `\n` が CRLF になり Linux 側ダンプとの diff が全行不一致になる |
| `sdl_width_test()` | `setlocale()` に渡すロケール名を Windows CRT のものに（`Japanese_Japan.932` 等）。POSIX 名は全部拒否されるので、放置すると case 3 が何も証明せずに PASS する |
| 末尾 `#ifdef WIN32` | `gettty` / `settty` / `setftty` / `error` / `erase_char` / `kill_char` を追加（下記） |

### `hangup(0)` を直接呼ぶ

`hangup()` の実体は `src/save.c:86` にあり、`#if defined(UNIX) || defined(VMS)` で
囲まれていた。`WIN32` では `include/config.h:118` が `UNIX` を `#undef` するので
コンパイルされない。そこでガードに `SDL_GRAPHICS` を足し、
`include/decl.h:138` の `program_state.done_hup` と `src/save.c` の `HUP` マクロも
同じように広げた。これで Linux と Windows が**同じ「保存して落ちる」経路**を通る。

先行調査が「保存処理を直接呼ぶ形にする（~10 行）」としていた部分は、
既存関数の再利用で済んだ。

### tty 状態の 4 関数を持つ理由

`gettty` / `settty` / `setftty` / `error` の供給元が Windows には無い。

- `sys/share/unixtty.c` — SDL 分岐はあるが `<termios.h>` / `<unistd.h>` の
  include が `#ifdef` の外にあり MinGW で通らない
- `sys/winnt/nttty.c` — Win32 コンソールを叩く実装。SDL では使わない
- `sys/share/pctty.c` — `disable_ctrlP()` を呼ぶが、これを定義する WIN32
  ソースが無い（`sys/share/pcsys.c:499` も `#ifndef WIN32` で避けている）

中身は `unixtty.c` の SDL 分岐と同じ 4 行。`error()` だけは `pctty.c` 版と違って
`putchar()`（グリッドへ飛ばされる）ではなく **stderr** に出す。グリッドが
死んでいるときに出したいメッセージだからで、`sdlterm.c` は
`SDLTERM_KEEP_STDIO` で本物の stdio を保持しているのでそれができる。

---

## 4. `include/` と `sys/` の変更

| ファイル | 内容 |
|---|---|
| `include/config.h:112-115` | MinGW-w64 は `_WIN32` しか定義しないので `WIN32` を導出 |
| `include/ntconf.h` | `NO_TERMS` を `#ifndef SDL_GRAPHICS` で囲む（§1-1）／`RANDOM` を無効化（§1-3）／`SDL_GRAPHICS` 用の `tgetch sdl_getch`（`unixconf.h:265` と同じ）／`<process.h>` を `__MINGW32__` でも／`strncmpi` を `_strnicmp` へ／**`PATHLEN` 64 → 260** |
| `include/system.h` | `NHSYS_CRT_HEADERS` を導入。MinGW の CRT と食い違う手書き宣言（`size_t` の `unsigned int` typedef、`chdir`、`time`/`localtime`）を系統的に避ける。`<time.h>` が `time()` を `static __inline` で定義するので、先に `extern` を出すとエラーになる |
| `include/wintty.h` | 変更なし（`NO_TERMS` を定義しないので不要になった） |
| `sys/winnt/winnt.c` | `nt_regularize()` に `is_kanji()` スキップ（§5-1）。`win32api.h` を `hack.h` より前へ（§5-2） |
| `sys/winnt/win32api.h` | `WIN32_LEAN_AND_MEAN` を定義。`<rpcndr.h>` の無ガードな `typedef unsigned char boolean` を避ける |
| `sys/share/pcmain.c` | `<sys\stat.h>` を MinGW では `<sys/stat.h>` に。`win32api.h` を `hack.h` より前へ |
| `src/files.c:28-34` | 同じ `<sys\stat.h>` の件 |
| `extension/{nhbuf,nhinet,extension}.c` | `<winsock.h>` を `hack.h` より前へ（§5-2） |
| `sys/share/{dgn_lex,lev_lex}.c`, `util/{dgn_comp,lev_comp}.l` | `VOIDYYPUT` の判定に `__MINGW32__` を追加。`POSIX_TYPES` が立たないので `yyoutput`/`yyunput` の宣言と定義が食い違っていた |

`PATHLEN` を 260 にしたのは実害があったため。64 だと `pcmain.c:158` の
`getcwd(orgdir, sizeof orgdir)` が普通のインストールパスで失敗し、
`NetHack: current directory path too long` でゲームが始まらない。

---

## 5. JNetHack 固有の論点

### 5-1. `nt_regularize()` が日本語名を潰す（修正済み）

`include/ntconf.h` の `#define regularize nt_regularize` により、
`sys/winnt/winnt.c` の実装がセーブファイル名（`src/files.c:419-438`）と
ロックファイル名（`pcmain.c:288`）に効く。素の実装は

```c
	     *lp == '*' || *lp == '|' || *lp == ':' || (*lp > 127))
			*lp = '_';
```

と **0x80 以上を全部 `_` にする**ので、日本語プレイヤー名はすべて `____.sav` に
潰れ、**別々の名前のプレイヤーが同じセーブファイルを共有する**。

`sys/share/pcunix.c:129` の JNetHack 版と同じ `is_kanji()` スキップを入れた。
`test/winjname.sh` がこれを検証する（修正を外すと 3 名すべて `____` になることも確認）。

### 5-2. `<windows.h>` は `hack.h` より前に読む

`include/youprop.h:79` は

```c
#define Protection		u.uprops[PROTECTION].p_flgs
```

と定義していて、`/usr/share/mingw-w64/include/memoryapi.h:56` の
`VirtualAllocFromApp(..., ULONG Protection)` がこれに展開され構文エラーになる。
`Warning` / `Confusion` も同じ危険がある。`WIN32_LEAN_AND_MEAN` では
`memoryapi.h` は除外されないので、**Windows ヘッダを先に読ませる**しかない。
読んだ後に `TRUE` / `FALSE` を `#undef` する（`include/global.h:71-75` が
無ガードで再定義するため）。

### 5-3. ソース文字コードは EUC-JP + LF のまま

`japanese/jlib.c:30` の

```c
#define IC ((unsigned char)("漢"[0])==0x8a)
```

は実質「コンパイル時にソース自身の文字コードを検出する」マクロ。**MinGW-w64 の
GCC は EUC-JP のリテラルをバイト透過で通す**（実測: `-finput-charset` 無指定で
`0xB4 0xC1` がそのまま残る）ので `IC == EUC` になり、`dat/` と整合する。
`japanese/Install.winnt` が記録する歴史的手順（全ファイルを Shift-JIS + CRLF に
変換して VC++ の nmake）は**採らない**。特別なコンパイルフラグも要らなかった。

**ただしソースを編集する道具には注意が要る。** EUC-JP のリテラルを UTF-8 前提の
エディタやツールに通すと、高位バイトが黙って U+FFFD に置き換わる。壊れても
コンパイルは通り、tty 版と SDL 版が同じソースを共有するので `compare.sh` も
通ってしまう。実際に本作業中に `src/allmain.c` と `src/save.c` を壊した。
検出は次のワンライナーが確実（非 ASCII バイト列のハッシュを HEAD と比べる）:

```sh
for f in $(git diff --name-only); do
  a=$(git show HEAD:"$f" | LC_ALL=C tr -dc '\200-\377' | md5sum)
  b=$(LC_ALL=C tr -dc '\200-\377' <"$f" | md5sum)
  [ "$a" != "$b" ] && echo "NON-ASCII CHANGED: $f"
done
```

`SDL-PORT.md` §8 の「EUC-JP ソースに対する GNU grep の罠」と同じ種類の話である。

### 5-4. wine は env と argv の非 UTF-8 バイトを潰す

```
host  : getenv: a4 a2 a4 ab   argv[1]: a4 a2 a4 ab
wine  : getenv: 3f 3f 3f 3f   argv[1]: 3f 3f 3f 3f
```

`NETHACKOPTIONS=name:あか` や `-u あか` は wine を通ると `????` になる。
**wine の制約であって JNetHack の問題ではない**が、日本語名のテストは
設定ファイル（`NetHack.cnf`、`src/files.c:900-907`）経由で行う必要がある。
ファイルはバイトのまま読まれ、`str2ic()`（`jlib.c:161`、`input_kcode == IC` なので
恒等）を素通りする。

なお**実 Windows では argv が CP932 で来る**という別の問題が残っている（§8）。

### 5-5. `nttty.c` に `xputc2()` が無い

`include/wintty.h:111` は 2 バイト文字用 `xputc2()` を宣言しているが、
`sys/winnt/nttty.c:383-399` の `xputc()`/`xputs()` は `WriteConsole()` を
1 バイトずつ叩くだけで `xputc2()` の実装が無い（実装があるのは
`sys/msdos/video.c:589` など MS-DOS 側だけ）。つまり **既存の WIN32CON
コンソール版はそもそも日本語が出ない。** Windows で JNetHack をまともに
動かす手段は現状 SDL バックエンドしかない、というのが本移植の動機である。

---

## 6. DECgraphics を Windows で使えるようにした

`iflags.DECgraphics` と `dec_graphics[]` は `TERMLIB` で囲まれていた。
`TERMLIB` は `include/termcap.h:11` の `#ifndef MICRO` で決まるので、
**`MICRO` を定義する Windows では立たない。** そのままだと

- `src/options.c:79` の `boolopt[]` で `DECgraphics` が読み取り専用の
  プレースホルダになり、オプションとして設定できない
- `src/options.c:460` の SDL 用デフォルト ON が効かない
- `src/drawing.c:401` の `dec_graphics[]` 自体が存在しない

となり、地図の壁が `-` と `|` のままになる。SDL では DEC は端末機能ではなく
「`sdlterm.c` が Unicode 罫線に読み替えるバイト表」なので、6 箇所のガードを
`defined(TERMLIB) || defined(SDL_GRAPHICS)` に広げた（`drawing.c` 2、`options.c` 4）。

---

## 7. ビルドと検証

### 7-1. ビルド

`sys/winnt/Makefile.nt` は MS NMAKE + Visual C++ 4.x 専用で使えない。
**新規に書くのではなく `sys/unix/Makefile.{src,utl,dat}` に `MINGW=1` を足した。**
`SDLGRAPH=1` の仕組みが既にそこにあり、差分は SYSSRC の入れ替えと
ツールチェーン接頭辞だけで済む（700 行の Makefile を複製せずに済む）。

```
./test/build.sh          # 3 つとも: jnethack.tty / jnethack.sdl / jnethack.exe
./test/build.sh win      # Windows のみ
```

- `SDLROOT`（既定 `$HOME/opt/mingw-sdl2`）に libsdl.org の
  `SDL2-devel-*-mingw.tar.gz` / `SDL2_ttf-devel-*-mingw.tar.gz` を展開しておく。
  apt には無い。**SDL2_ttf 2.0.18 以降が必須**（`TTF_GlyphMetrics32` 等を使う）。
  実行時は `SDL2.dll` / `SDL2_ttf.dll` を `.exe` の隣に置く。
- **GUI サブシステムでリンクする**（`-mwindows -Wl,-emainCRTStartup`）。
  そうしないと Windows がゲームのウィンドウとは別にコンソールを開く。
  `-e` でエントリポイントを通常の C のものに留めるので、`main()` は
  `pcmain.c` のままで `SDL2main` も要らない（`SDL_MAIN_HANDLED` の前提）。
  代わりに stderr の行き先が無くなるので、致命的エラーは `error()` の
  メッセージボックスで知らせる。なお wine は GUI サブシステムでも親の
  ハンドルを渡すので、`NH_SDL_WIDTHTEST` の出力はテストからは見える。
- **Windows フェーズは必ず最後**。`util/makedefs` と `util/lev_comp` は
  `src/*.o` をリンクするので、`src/*.o` と `util/*.o` は一度に一方のターゲットの
  ものしか置けない。`test/build.sh` はこれを順序で保証し、先頭で `src/*.o` を消す。
- `monst.o` / `objects.o` の規則にある `rm -f $(MAKEDEFS)` は `MINGW=1` では
  無効化する（`MAKEDEFS_STALE`）。クロスの `monst.o` からホストの makedefs は
  リンクできない。
- `include/date.h` と `dat/options` は `makedefs -v` が固定テンプレートで書くため
  Windows フェーズ後にホスト版へ戻す。

### 7-2. 検証

`test/ptydrive.py` は pty 依存で Windows 側には使えないので、**基準は Linux の
tty 版が出したダンプをそのまま使う。** `NETHACK_SEED` が揃うので同じダンジョンが
出る（§1-3 がその前提）。

```
./test/wincompare.sh WORK      # 17 ケースの画面 diff（tty 版 vs jnethack.exe）
./test/walls.sh WORK win       # DEC 罫線の Unicode 変換（pyte が独立実装の基準）
./test/winjname.sh WORK        # 日本語名 → セーブファイル名
./test/closesave.sh WORK win   # WM_DELETE_WINDOW → 保存して終了
```

`test/walls.sh` と `test/xdrive.py` は Windows 用に引数を足して共用にした。
`xdrive.py` には `--close`（WM 経由でウィンドウを閉じ、終了ステータスを見る）と
`--cwd` を追加し、ウィンドウ探索を `_NET_WM_PID` でも照合するようにした
（タイトルだけで探すと**ビルドを起動した端末のウィンドウ**や前回の残骸に当たる）。

**`wincompare.sh` の 17 ケース中 1 つ（`options`）は原理的に一致しない。**
`MICRO` が読み取り専用の `BIOS` / `rawio` を増やし、`mail` は Unix 専用なので、
両者のオプション数が違ってメニューのページ割りが変わる。メニュー文字と
その 3 項目を正規化しても 1 行残る（`perm_invent` が Unix 側では 1 ページ目に
入り Windows 側では入らない）。`expect_diff` に入れて理由を明記し、
diff は表示したうえで FAIL にはしない扱いにした。

---

## 8. やっていないこと

- **ウィンドウのリサイズ** — `wintty.c:190` の `winch()` は
  `#if defined(SIGWINCH) && defined(CLIPPING)` の中にあり、`SIGWINCH` の無い
  Windows では再レイアウトを依頼する経路が無い。ウィンドウ固定で割り切った。
  やるなら `winch()` を `tty_relayout()` として公開して `sdlterm.c` から呼ぶ形だが、
  それは `SDL-PORT.md` H1 を崩す。
- **`-u 日本語名` / 環境変数の日本語** — 実 Windows の `main()` は argv を
  ANSI（CP932）で受けるので、`plname` に SJIS が入って EUC-JP 前提の
  `is_kanji2()`（`src/do_name.c:202`）や `regularize()` が誤動作する。
  `str2ic()` は argv には掛かっていない（`options.c:523` は環境変数のみ）。
  設定ファイル経由と、ゲーム内の名前入力（`getline.c:151` が `str2ic()` を通す）は
  正しく動く。wine では §5-4 のため検証そのものができない。
- **実機 Windows での網羅的な確認** — 利用者が Windows 11 (build 26100) で
  起動し、`C:\Windows\Fonts\msgothic.ttc` が見つかって日本語が正しく出ることを
  スクリーンショットで確認した。それ以外（長時間のプレイ、セーブ／ロード、
  IME、二重起動）は wine でしか見ていない。
- **フォントの同梱** — `C:\Windows\Fonts\msgothic.ttc` に頼っている。
  リポジトリ直下の `MPLUS1Code-Regular.ttf` はまだコードから参照されておらず、
  同梱するならライセンス文（OFL）も要る。
- **`recover.exe`** — クラッシュ後のレベルファイル復旧ユーティリティ。
  `util/Makefile` に `recover` ターゲットがあるのでクロスは容易だが未同梱。
- **実 IME での日本語入力** — `SDL-PORT.md` §4 / §10 のまま未了。Windows で
  配ることを目的にするなら、これは本移植より優先度が高い。
- **多重起動の検出** — `pcmain.c` に `getlock()` 相当が無い（§2）。
- **`NH_EXTENSION_REPORT`** — `extension/nhinet.c` は `-lwsock32` でビルド・
  リンクは通るが、`score.jnethack.org` への通信は試していない。

---

## 9. 配布

`./sys/winnt/mkdist.sh` が `dist/jnethack-<ver>-sdl-win64.zip` を作る。展開して
`jnethack.exe` を起動するだけで動く（`pcmain.c` が `HACKDIR` を
`exepath(argv[0])` から取るのでフラット配置でよい）。中身は 12 ファイル:

```
jnethack.exe  SDL2.dll  SDL2_ttf.dll
nhdat  license  NetHack.cnf
README.txt  NetHack.txt  jGuidebook.txt
record  logfile  save/
```

**この移植で `include/config.h` の `DLB` を有効にした。** 従来この木では
無効で、データファイルが 112 個そのまま並ぶことになる。原版の
`JNH115.LZH` も現行の JNetHack 3.6.7 Windows 版も 1 個の `nhdat` に
まとめており、そちらに合わせた。

注意点が 2 つある。

- `src/dlb.c` の `dlb_fopen()` は `dlb_init()` が失敗すると**何も返さない**。
  ばらのファイルへのフォールバックは無いので、`nhdat` は HACKDIR に必須。
  `test/mkplaydir.sh` もばら置きから `nhdat` + `license` に変えた。
- `nhdat` の中身は `.lev` なので **LP64 と LLP64 で別物**（§1-2）。
  `util/dlb` も MinGW でクロスし、`datwin/dat` 用を wine で作る。

同梱する `NetHack.cnf` は原版のもの（この木の `sys/winnt/winnt.cnf` と同一）
だが、`OPTIONS=IBMgraphics` の 1 行だけコメントアウトしてある。この移植は
DECgraphics を既定で使って罫線を自前で描くし、そもそも CP437 と EUC-JP は
両立しない（0x80 以上のマップバイトが 2 バイト文字の 1 バイト目と解釈される）。

`doc/jGuidebook.txt` はこの木では EUC-JP なので、UTF-8 + BOM + CRLF に変換して
入れる（原版は Shift_JIS だった）。`NetHack.cnf` は原版どおり ASCII のみに
してある — 日本語コメントを入れると EUC-JP で保存する必要があり、Windows の
テキストエディタでは書き戻せなくなるため。

libsdl.org の `SDL2_ttf.dll` は 68MB（FreeType と HarfBuzz を静的リンクした
うえでシンボル未除去）なので、ステージング先のコピーだけ strip する。
zip 全体で 2.7MB。

---

## 10. ついでに直したもの

移植の副産物として、Linux 側にも効く修正が 2 件ある。

1. **`program_state.something_worth_saving` が誰も代入していなかった**
   （`src/allmain.c` の `moveloop()` 冒頭で 1 にした）。
   `src/save.c:103` の `hangup()` はこのフラグを見るので、**端末を失っても
   ウィンドウを閉じても、保存も終了も起きていなかった。** SDL のウィンドウを
   閉じるとゲームが応答せず残り続けるという症状で、Linux 版も同じだった。
   `test/closesave.sh` が両方を検証する。
2. **`test/xdrive.py` のウィンドウ探索が緩すぎた** — 既定タイトルが
   `JNetHack` で、`_NET_WM_PID` も見ていなかったため、ビルドを起動した端末の
   ウィンドウにキーやウィンドウ閉じ要求を送りうる状態だった。

---

## 付録: 参照した箇所

| ファイル:行 | 内容 |
|---|---|
| `include/config.h:112-118` | `_WIN32` → `WIN32`、`WIN32` → `#undef UNIX` |
| `include/global.h:191-193` | `WIN32` → `ntconf.h` を include |
| `include/ntconf.h` | `RANDOM` を定義しない理由、`PATHLEN` 260、`NO_TERMS` を外す理由、`tgetch sdl_getch` |
| `include/termcap.h:11-19` | `TERMLIB` と `ASCIIGRAPH` の出どころ |
| `include/system.h:11-22` | `NHSYS_CRT_HEADERS` |
| `include/youprop.h:79` | `Protection` マクロ |
| `win/tty/wintty.c:34` | `#ifndef NO_TERMS` → `termcap.h` |
| `win/tty/wintty.c:2166-2197` | `g_putch()` の第 8 ビット strip と `graph_on()` |
| `win/tty/sdlterm.c` | `alt_charset` を見て `dec_special[]` を引く／`raise(SIGHUP)` と `hangup(0)` |
| `src/save.c:86-90` | `hangup()` のガード |
| `src/allmain.c:36-45` | `something_worth_saving` |
| `src/rnd.c:12-16` | `RND()` のビットの取り方 |
| `src/files.c:166-177, 302-324, 482-501` | `MICRO` の `O_BINARY` |
| `src/version.c:77` | 構造体サイズの照合 |
| `util/makedefs.c:113-125` | 出力先の固定テンプレート |
| `sys/winnt/winnt.c:163-192` | `nt_regularize()` |
| `sys/share/pcsys.c:499` | `disable_ctrlP()` が WIN32 で使えないこと |
| `japanese/jlib.c:30` | `IC` マクロ |
| `japanese/jlib.c:404-408` | `jbuffer()` が `is_kanji()` で対を組む |
