# JNetHack 3.4.3 SDL バックエンドの Windows 移植

- 対象: ブランチ `experimental/jnethack-3.4.3-sdl`（[`SDL-PORT.md`](SDL-PORT.md) の続き）
- 移植元: `experimental/jnethack-1.1.5-sdl`（2026-07-30 作成）
- 状態: **動作する。** MinGW-w64 クロスビルド + wine で検証済み

3.4.3 の Windows 側で 1.1.5 と変わった点は
[`SDL-PORT.md`](SDL-PORT.md) §11-3 にまとめてある。要約すると 3 つ:
`g_putch()` が `#ifndef WIN32` になったので `sdlterm.c` が用意する、
`USER_SOUNDS` を切る、`sys/winnt/winnt.c` の `error()` を切る。
それ以外はこの文書のとおりである。

```
$ ./test/build.sh
=== building utilities ===
=== building tty backend ===
=== building SDL backend ===
=== building data files ===
=== building Windows (MinGW-w64) SDL backend ===
=== building Windows data files ===
src/JNetHack.exe   src/jnethack.sdl   src/jnethack.tty
```

---

## 0. 結果

| 検証 | 結果 |
|---|---|
| `test/wincompare.sh` — Linux tty 版との画面 diff 20 ケース | **5 PASS / 15 EXPECTED-DIFF**（§7-4。うち 13 は乱数を揃えるのをやめた代償） |
| `test/walls.sh WORK win` — DEC 罫線の Unicode 変換 | **FAIL**（差分 5〜12 行、実行ごとに変動）。§1-3 で種を外したので画面全体の diff は原理的に成立しない。罫線が出ていること自体（`┌│─` の存在）は通る |
| `NH_SDL_WIDTHTEST=1` — 全角セル 7 ケース | **PASS**（6879 文字の往復含む） |
| `test/winjname.sh` — 日本語名のセーブファイル分離 | **PASS**（3 名 → 3 ファイル） |
| `test/closesave.sh WORK win` — ウィンドウを閉じたら保存 | **PASS** |
| Linux 側の退行（`stalelock.sh` / `closesave.sh`） | **PASS**（3/3 と 1/1） |
| Linux 側の退行（`compare.sh` / `walls.sh`） | **6/20 PASS** と **FAIL**。どちらも種を外した代償で、`wincompare.sh` と違い EXPECTED-DIFF の判定を入れていないため FAIL と出る |

実ウィンドウ（wine の windows ドライバ）でも日本語・色・罫線が正しく出ることを
1.1.5 の移植時にスクリーンショットで確認しており、**実機 Windows 11 でも
起動とフォント解決（MS ゴシック）を確認済み**（§8）。3.4.3 への移植では
実機の再確認はしていない。

以下 §1〜§6 は 1.1.5 の移植で判明したことで、3.4.3 でもそのまま成り立つ。
3.4.3 で変わった点は §4 の表と §5-1、そして §7-3 にある。

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

が `g_putch()` の中で **高位ビット付きのバイトの第 8 ビットを落とす
唯一の場所**になっている。

移植当時、SDL 版は DECgraphics で壁を描いていた。`src/drawing.c` の
`dec_graphics[]` は `0xf8`（縦線）`0xf1`（横線）のように**高位ビット付き**で
罫線を表すので、この strip が行われないとそのバイトが
`japanese/jlib.c` の `jbuffer()` に届き、`is_kanji()` が真になって
**EUC-JP の 2 バイト対として組まれ、地図の壁が漢字になった。**

```
NO_TERMS ありのとき                NO_TERMS なしのとき（正しい）
   跣髑髑驪                          ╭──────╮
   ...%.                             │...%..│
```

現在の SDL 版は文字集合を使わず、壁を glyph から描く（`SDL-PORT.md` §2-1）ので
高位ビット付きのバイトは発生せず、この経路は通らない。それでも `NO_TERMS` を
定義しないままにするのは、`tty_exit_nhwindows()` の `tty_shutdown()` 呼び出しが
同じフラグで消えてしまうからである（`include/ntconf.h:71` の注記）。

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
**Windows でも完全に成立した。** `wintty.c` に後から入った例外は
壁を線で描く 6 行だけで（`SDL-PORT.md` §2-1）、それも両プラットフォーム共通である。

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

### 1-3. 乱数生成器は揃えていない

素の 3.4.3 では、Windows は `include/ntconf.h` の `RANDOM` により
`sys/share/random.c` の `random()`、Unix は `include/unixconf.h` の条件
（`BSD || LINUX || ULTRIX || CYGWIN32 || RANDOM || __APPLE__`）がどれも
成立せず `lrand48()` になる —— **`LINUX` は `#define` がコメントのままで、
`__linux__` からの自動定義も無い**（プリプロセッサで確認済み）。

**この移植では `LINUX` を定義した。** Linux 側も `random()` になるが、これは
**glibc の** `random()` である（`RANDOM` は立てないので
`sys/share/random.c` はコンパイルされない）。Windows 側は素のまま
同梱実装を使う。

| | `Rand()` | 実体 |
|---|---|---|
| Linux | `random()` | glibc |
| Windows | `random()` | `sys/share/random.c` |

`lrand48()` をやめた理由は乱数の質である。`rn2(x)` は `Rand() % x` なので
`rn2(2)` / `rn2(4)` は返り値の下位ビットそのもので、そこは LCG が
いちばん弱い。実測:

| | 最下位ビットの周期 |
|---|---|
| `lrand48()` | **262,144（2¹⁸）** |
| `random()` | 2²⁵ まで周期なし |

`lrand48` の 2¹⁸ は理論どおり（法 2⁴⁸ の LCG の上位 31 ビットを返すので、
`rn2()` に届く最下位ビットは状態のビット 17）。長い一局で普通に踏む長さである。
加算的生成器のほうは、下位ビットが弱いことを承知していて出力時に最下位
ビットを捨てている。

#### 代償: 2 つのビルドは同じダンジョンを作らない

glibc 版と同梱版は**同じ生成器だが種の展開が違う**。glibc は MINSTD
（`16807·x mod 2³¹−1`）で 31 語の初期状態を埋め、同梱版は 1988 年 BSD の
LCG（`1103515245·x + 12345`）で埋める。生成の中核（`x[n] = x[n-3] +
x[n-31] mod 2³²`、次数 31 / 間隔 3、310 個空回し、最下位ビット破棄）は
一致しているが、種が違えば系列は別物になる:

```
glibc random()       (seed=1): 1804289383 846930886 ...
sys/share/random.c   (seed=1): 2078917053 143302914 ...
```

**その結果、両者は別のダンジョンを作る。**（そもそも種を固定する手段も無い
—— ゲーム側に手を入れないので `setrandom()` は素のまま時刻を使う。）
`test/wincompare.sh` は 2 つのビルドの画面を diff するテストなので、
地図・ステータス行・持ち物が写る画面はすべて食い違う。残るのは
ダンジョンに依存しない画面だけである（§7-4 に実測値）。

> 揃えたければ `include/unixconf.h` で `LINUX` の代わりに `RANDOM` を
> 定義すればよい。両方が `sys/share/random.c` を使うので系列が一致する。
> ただし `initstate()` / `setstate()` の署名（この木は `int n`、glibc は
> `size_t n`）が衝突するので、その 2 つを外す必要がある —— `RANDOM` は
> 元来「この OS に random(3) が無い」という宣言なので、glibc がある環境で
> 立てるのは想定外の組み合わせである。

---

## 2. 採った構成

```
WIN32 + MICRO + SDL_GRAPHICS          （NO_TERMS も WIN32CON も定義しない）
main = sys/share/pcmain.c
tty  = win/tty/sdlterm.c              （nttty.c でも termcap.c でもない）
Rand = random()                       （sys/share/random.c。Unix 側は glibc の同名関数）
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
| `SDL_CreateWindow` | 常に `SDL_WINDOW_RESIZABLE`。リサイズは桁数ではなくフォントサイズを変えるので、`SIGWINCH` の無い Windows でも再レイアウトが要らない（`SDL-PORT.md` §10） |
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
| `include/config1.h` | MinGW-w64 は `_WIN32` しか定義しないので `WIN32` を導出。3.4.3 は WIN32 の自動検出をここに集めている |
| `include/ntconf.h` | `NO_TERMS` を `#ifndef SDL_GRAPHICS` で囲む（§1-1）／`random()`・`srandom()` の宣言（このプラットフォームは `system.h` を読まない）／`SDL_GRAPHICS` 用の `tgetch sdl_getch`（`unixconf.h` と同じ）／`USER_SOUNDS` を SDL では定義しない（`play_usersound()` は `nttty.c`）／`<process.h>` を `__MINGW32__` でも／`strncmpi` を `_strnicmp` へ |
| `include/unixconf.h` | `RANDOM` を定義（§1-3）。これで両プラットフォームが `sys/share/random.c` を使う |
| `sys/share/random.c` | `initstate()` / `setstate()` を `#if 0` で外す（§1-3）。glibc の `<stdlib.h>` と署名が食い違い、しかも未使用 |
| `include/system.h` | **変更なし。** 3.4.3 の WIN32 経路はこのヘッダを読まない（`unixconf.h` だけが読む）ので、1.1.5 で必要だった `NHSYS_CRT_HEADERS` が要らない |
| `include/ntconf.h` の `PATHLEN` | **変更なし。** 3.4.3 は既に `BUFSZ` / `_MAX_PATH`（1.1.5 の 64 ではない） |
| `include/wintty.h` | 変更なし（`NO_TERMS` を定義しないので不要になった） |
| `sys/winnt/winnt.c` | `nt_regularize()` に `is_kanji()` スキップ（§5-1）。`win32api.h` を `hack.h` より前へ（§5-2） |
| `sys/winnt/win32api.h` | **変更なし。** 3.4.3 は既に `WIN32_LEAN_AND_MEAN` を定義している |
| `sys/share/pcmain.c` | `<sys\stat.h>` を MinGW では `<sys/stat.h>` に。`win32api.h` を `hack.h` より前へ |
| `src/files.c:28-34` | 同じ `<sys\stat.h>` の件 |
| `sys/winnt/winnt.c` の `error()` | `#if !defined(WIN32CON) && !defined(SDL_GRAPHICS)`。`msmsg()`（`nttty.c`）を呼ぶうえ、GUI サブシステムのバイナリには書き出すコンソールが無い |
| `win/tty/sdlterm.c` の `g_putch()` | 3.4.3 の `wintty.c` は `#ifndef WIN32` で囲むので、Windows では移植側が出す（[`SDL-PORT.md`](SDL-PORT.md) §11-3） |
| `extension/` | **3.4.3 には無い**（JNetHack 1.x のもの） |
| `sys/share/{dgn_lex,lev_lex}.c`, `util/{dgn_comp,lev_comp}.l` | **変更なし。** 3.4.3 のパーサはそのままクロスビルドできる |

1.1.5 で必要だった `PATHLEN` 64 → 260 は 3.4.3 では不要である。
`ntconf.h` が既に `BUFSZ`（`_MAX_PATH` が小さければそちら）にしていて、
`pcmain.c` の `getcwd(orgdir, sizeof orgdir)` は普通のパスで通る。

---

## 5. JNetHack 固有の論点

### 5-1. 日本語名とセーブファイル名 —— 3.4 では直っていた

1.1.5 では `include/ntconf.h` の `#define regularize nt_regularize` により
`sys/winnt/winnt.c` の実装がセーブファイル名に効いていた。素の実装は

```c
	     *lp == '*' || *lp == '|' || *lp == ':' || (*lp > 127))
			*lp = '_';
```

と **0x80 以上を全部 `_` にする**ので、日本語プレイヤー名はすべて `____.sav` に
潰れ、**別々の名前のプレイヤーが同じセーブファイルを共有していた**。
1.1.5 の移植では `sys/share/pcunix.c` と同じ `is_kanji()` スキップを入れて直した。

**3.4.3 ではその経路自体が無い。** `set_savefile_name()`（`src/files.c:833-839`）は
WIN32 でセーブ名を

```c
	Sprintf(fnamebuf, "%s-%s", get_username(0), plname);
	(void)fname_encode("ABCDEFGHIJ...xyz_-.", '%', fnamebuf, encodedfnamebuf, BUFSZ);
	Sprintf(SAVEF, "%s.NetHack-saved-game", encodedfnamebuf);
```

と組み立てる。`regularize()` を通らず、`fname_encode()` は不正な文字を
`_` に潰さず `%XX` に符号化するので、**別々の名前は別々のファイルになる。**
`nt_regularize()` は `src/files.c` の 3 箇所の `regularize()` から呼ばれるが、
どれも VMS / MICRO / 非 WIN32 の枝で、このビルドでは到達しない。
したがって 1.1.5 の修正は移していない。

読んで判断するだけで済ませず、`test/winjname.sh` を 3.4 の名前規則
（`<user>-<plname>.NetHack-saved-game`）に直して実際に確かめてある ——
3 つの日本語名が 3 つの別ファイルになる。

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
設定ファイル（`defaults.nh`。3.4 で `NetHack.cnf` から改名された ——
`src/files.c:1482`）経由で行う必要がある。
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

## 6. DECgraphics と Windows

`iflags.DECgraphics` と `dec_graphics[]` は `TERMLIB` で囲まれている。
`TERMLIB` は `include/tcap.h:11` の `#ifndef MICRO` で決まり、
`include/ntconf.h:46` が `MICRO` を `#undef` するので、**Windows でも立つ**。
プリプロセッサで確認済み（`x86_64-w64-mingw32-gcc -DSDL_GRAPHICS -E`）。

したがってガードを広げる必要はなく、DECgraphics は両プラットフォームで
上流どおりの普通のオプションとして使える。かつてここには
`defined(TERMLIB) || defined(SDL_GRAPHICS)` に広げた 6 箇所のガードと
SDL 用のデフォルト ON があったが、いずれも撤去した。ポートの見た目を
文字集合オプションに載せるのをやめたためで、経緯は SDL-PORT.md を参照。

Windows 側で必要だったのは `NO_TERMS` の方である（§1-3 と `include/ntconf.h:71`）。
`g_putch()` が高位ビットを落として `graph_on()` を呼ぶ経路は
`ASCIIGRAPH && !NO_TERMS` で決まり、これが無いと DEC のバイトが
そのまま `jlib.c` に届いて EUC-JP の対として組み立てられてしまう。

---

## 7. ビルドと検証

### 7-1. ビルド

`sys/winnt/Makefile.nt` は MS NMAKE + Visual C++ 4.x 専用で使えない。
**新規に書くのではなく `sys/unix/Makefile.{src,utl,dat}` に `MINGW=1` を足した。**
`SDLGRAPH=1` の仕組みが既にそこにあり、差分は SYSSRC の入れ替えと
ツールチェーン接頭辞だけで済む（700 行の Makefile を複製せずに済む）。

```
./test/build.sh          # 3 つとも: jnethack.tty / jnethack.sdl / JNetHack.exe
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
- `include/date.h` は `makedefs -v` が固定テンプレートで書くため、
  Windows フェーズ後にホスト版へ戻す。
- コンパイル時オプション一覧のファイル名は**プラットフォームで違う**。
  `include/global.h` では `options`、`include/ntconf.h` では `ttyoptions`
  （`OPTIONS_USED`。GUI 版と tty 版が同じディレクトリに同居できるように）。
  `Makefile.dat` の `OPTFILE` が `MINGW=1` で切り替える。これを忘れると
  `dlb cf nhdat ... options` が `Can't open options` で落ちる。

### 7-2. 検証

`test/ptydrive.py` は pty 依存で Windows 側には使えないので、**基準は Linux の
tty 版が出したダンプをそのまま使う。** ただし §1-3 のとおり両者は同じ
ダンジョンを作らないので、突き合わせられるのはダンジョンに依存しない画面
だけである。

```
./test/wincompare.sh WORK      # 20 ケースの画面 diff（tty 版 vs JNetHack.exe）
./test/walls.sh WORK win       # DEC 罫線の Unicode 変換（pyte が独立実装の基準）
./test/winjname.sh WORK        # 日本語名 → セーブファイル名
./test/closesave.sh WORK win   # WM_DELETE_WINDOW → 保存して終了
```

`test/mkplaydir.sh` は Windows 側の残骸も消す。Unix はレベルファイルを
`<uid><plname>.<n>`、ロックを `perm` / `*_lock` と名づけるが、`MICRO` の
Windows 版は `<user>-<plname>.<n>` / `<plname>.sav` / `NHPERM` を使う。
`PC_LOCKING` と `SELF_RECOVER` が有効なので、消し残すと次の実行が
`There are files from a game in progress under your name. Recover?` を出し、
そこで止まる。

`test/walls.sh` と `test/xdrive.py` は Windows 用に引数を足して共用にした。
`xdrive.py` には `--close`（WM 経由でウィンドウを閉じ、終了ステータスを見る）と
`--cwd` を追加し、ウィンドウ探索を `_NET_WM_PID` でも照合するようにした
（タイトルだけで探すと**ビルドを起動した端末のウィンドウ**や前回の残骸に当たる）。

### 7-3. 2 つのビルドは違うダンジョンを作る

**設計どおりである。** §1-3 のとおり Linux は glibc の `random()`、
Windows は `sys/share/random.c` の `random()` を使う。同じ生成器だが種の
展開が違うので系列が別になる。加えて**種を固定する手段そのものが無い**
（`setrandom()` は素のまま時刻を使う）。

このため `test/wincompare.sh` で比較できるのは、ダンジョンにも
キャラクタの振り値にも初期持ち物にも依存しない画面だけになった（§7-4）。

> **歴史的な注記。** 一時は両方を `sys/share/random.c` に寄せていた。
> そのときは 20 ケース中 18 が一致したが、それでも**シードによっては
> 依然として食い違った**（18 シード中 3: 11 / 12345678 / 2000000000）。
> つまり乱数を揃えても残る不一致が別にあり、原因は特定できていない ——
> LP64 と LLP64 の差がゲーム側のどこかに出ているとしか言えない。
> 移植で持ち込んだものではない（どちらのビルドも同じ `src/` を使う）。
> 乱数を分けた今はその現象が全シードに埋もれて見えなくなっただけで、
> 直ってはいない。

### 7-4. 一致しない 15 ケース

**`wincompare.sh` の 20 ケース中 15 は原理的に一致しない。** うち 13 は
§7-3 の乱数の分離によるもので、地図・ステータス行・持ち物が写る画面が
すべて該当する。残る 2 つは移植前からの platform 差である。

比較として意味が残っているのは 5 ケース ——
`rolemenu` / `racemenu` / `alignmenu`（ダンジョン生成より前に出る画面）と
`guidebook` / `overview_page2`（地図を覆う全画面テキスト）。
**メニューとメッセージ行のレイアウト**、つまりこの移植が責任を負う部分は
ここで押さえられている。

以下は移植前からの 2 つ。
`MICRO` が読み取り専用の `BIOS` / `rawio` を増やし、`mail` は Unix 専用なので、
両者のオプション数が違ってメニューのページ割りが変わる。メニュー文字と
その 3 項目を正規化しても 1 行残る（`perm_invent` が Unix 側では 1 ページ目に
入り Windows 側では入らない）。`expect_diff` に入れて理由を明記し、
diff は表示したうえで FAIL にはしない扱いにした。

もう 1 つは `helpmenu` である。`include/ntconf.h` が `PORT_HELP` を定義するので
`?` メニューの項目が 1 つ多く（「Windows に特有のヘルプおよびコマンド」、
`src/pager.c`）、その行が長いぶん `tty_end_menu()` がメニュー全体を数桁
左に寄せる。**項目のほうは動くようにした** —— `sys/winnt/porthelp` を
Windows の `nhdat` に入れてある（`sys/unix/Makefile.dat`）。入れないと
選んでも何も出ない項目が残る。

---

## 8. やっていないこと

- **桁数の変わるリサイズ** — `wintty.c:190` の `winch()` は
  `#if defined(SIGWINCH) && defined(CLIPPING)` の中にあり、`SIGWINCH` の無い
  Windows では再レイアウトを依頼する経路が無い。`winch()` を
  `tty_relayout()` として公開して `sdlterm.c` から呼ぶ形もあるが、それは
  `SDL-PORT.md` H1 を崩す。ウィンドウのリサイズ自体はグリッドを固定した
  ままフォントサイズを変える形で対応した（`SDL-PORT.md` §10）。
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
- **同梱フォントの実機確認** — zip は `MPLUS9800-Regular.ttf` を同梱し、
  `defaults.nh` の `SDLFONT` / `SDLFONTSIZE` で既定にしている（wine 上の
  `NH_SDL_WIDTHTEST` は 24pt・12x24px セルで 7/7 PASS）。実機 Windows で
  見たのは `msgothic.ttc` のスクリーンショットだけで、同梱フォントのほうは
  未確認。
- **`recover.exe`** — クラッシュ後のレベルファイル復旧ユーティリティ。
  `util/Makefile` に `recover` ターゲットがあるのでクロスは容易だが未同梱。
- **実 IME での日本語入力** — `SDL-PORT.md` §4 / §11 のまま未了。Windows で
  配ることを目的にするなら、これは本移植より優先度が高い。
- **多重起動の検出** — `pcmain.c` に `getlock()` 相当が無い（§2）。
- **`NH_EXTENSION_REPORT`** — `extension/nhinet.c` は `-lwsock32` でビルド・
  リンクは通るが、`score.jnethack.org` への通信は試していない。

---

## 9. 配布

`./sys/winnt/mkdist.sh` が `dist/jnethack-<ver>-sdl-win64.zip` を作る。展開して
`JNetHack.exe` を起動するだけで動く（`pcmain.c` が `HACKDIR` を
`exepath(argv[0])` から取るのでフラット配置でよい）。中身は 13 ファイル:

```
JNetHack.exe  SDL2.dll  SDL2_ttf.dll
nhdat  license  defaults.nh
MPLUS9800-Regular.ttf  MPLUS9800-OFL.txt
NetHack.txt  jGuidebook.txt
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

同梱する `defaults.nh` は `sys/winnt/defaults.nh` の `cp` そのもの。**この木では
このファイルだけ CRLF で持っている** — 変換を挟まずに済ませるためで、git が
LF に正規化しないよう `.gitattributes` で `-text` にしてある。原版から変えたのは
2 点:

- 先頭に `SDLFONT` / `SDLFONTSIZE` の節を足し、既定を同梱のフォントにした。
- `OPTIONS=IBMgraphics` をコメントアウト。この移植は DECgraphics を既定で
  使って罫線を自前で描くし、そもそも CP437 と EUC-JP は両立しない
  （0x80 以上のマップバイトが 2 バイト文字の 1 バイト目と解釈される）。

`OPTIONS=kcode:` の行は落とした。SDL バックエンドはセルにコードポイントを
置くだけでバイトを出さないので、選ぶべき出力エンコーディングが無い。

`doc/jGuidebook.txt` はこの木では EUC-JP なので、UTF-8 + CRLF に変換して
入れる（原版は Shift_JIS だった）。BOM は付けない。`defaults.nh` はこの変換の
対象外なので、原版どおり ASCII のみにしてある — 日本語コメントを入れると
EUC-JP のまま配布物に出てしまう。

libsdl.org の `SDL2_ttf.dll` は 68MB（FreeType と HarfBuzz を静的リンクした
うえでシンボル未除去）なので、ステージング先のコピーだけ strip する。
zip 全体で 2.7MB。

### 9-1. アイコン

`JNetHack.exe` には **`NetHack.ico` が埋め込んである** —— NetHack 3.1 以降の
Windows 版がずっと使ってきた、鎖帷子模様の盾に剣を交差させたあの紋章である。
`win/X11/nh_icon.xpm` や `win/Qt/qt_win.cpp:216` にあるのと同じ絵柄で、
X11 版が WM に渡す `win/X11/nh72icon` はその白黒 72×72 版にあたる。

木はこれを uuencode した `sys/winnt/nhico.uu` の形で持っている。純正の
Makefile は自前でビルドした uudecode で展開し（`sys/winnt/Makefile.gcc:691`）
`windres` で `sys/winnt/console.rc` を焼き込む（同 `:572`）が、
`sys/unix/Makefile.src` の MinGW 経路にはその手順が無く、**移植当初の
`JNetHack.exe` はアイコン無しだった**。次の 3 つを足して揃えた。

- `sys/winnt/jnethack.rc` — アイコンだけの資源スクリプト。純正の
  `console.rc` をそのまま使わなかったのは、その `VERSIONINFO` が
  "NetHack for Windows - TTY Interface" と名乗り `OriginalFilename` を
  `NetHack.exe` と書いているからで、どちらも `JNetHack.exe` については
  真ではない。版番号も手書きなので `include/date.h` と食い違う場所が
  1 つ増える。資源 ID は純正と同じ `1` —— Windows は番号が最小の
  アイコンをアプリケーションのアイコンとして使う。
- `sys/unix/Makefile.src` の `$(NHICO)` と `nhres.o` の規則。
  uudecode は sharutils が現代の Linux に入っていないので python で行う
  （この木は `japanese/mkjis0208.py` で既に python を使う）。
  Windows 側の makefile と違ってホストのファイルシステムは大文字小文字を
  区別するので、出力名は `.rc` の綴りと厳密に合わせて `NetHack.ico`。
- `HOBJ` の末尾に `$(WINRESOBJ)`。`RANDOBJ` と同じく `MINGW=1` のときだけ
  中身が入り、Linux ビルドでは空のまま。

検証は `.rsrc` を取り出して元の `.ico` と突き合わせる形で行った。32×32 と
16×16 の 2 面がバイト単位で一致し、`mkdist.sh` の `strip --strip-unneeded`
を通した zip 内の `JNetHack.exe` でも一致する（`strip` は `.rsrc` を落とさない）。

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
