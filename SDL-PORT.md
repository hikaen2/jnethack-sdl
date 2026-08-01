# JNetHack 1.1.5 SDL ポート

- 実証実験の報告: `experimental/sdl-nethack` ブランチの `SDL-POC-RESULT.md`
- 対象: **JNetHack 1.1.5**（NetHack 3.2.3 ベース）
- 環境: Linux 6.8 / GCC 13 / SDL2 2.30.0 / SDL2_ttf 2.22.0

---

## 0. 何をしたか

`win/tty/termcap.c` を **`win/tty/sdlterm.c`** に差し替えた。
ウィンドウポートは増やしていない。**tty ポートの下にある端末を、
自前のセルグリッドに置き換えた**だけである。

`wintty.c` / `topl.c` / `getline.c` は無改造。

```sh
./test/build.sh                    # src/jnethack.tty と src/jnethack.sdl
./test/mkplaydir.sh ~/jnhdir
HACKDIR=~/jnhdir ./src/jnethack.sdl -u あなたの名前
```

検証結果:

| テスト | 結果 |
|---|---|
| `test/compare.sh`（termcap 版と SDL 版の画面一致、17 ケース） | **17/17 PASS** |
| `NH_SDL_WIDTHTEST=1`（2 セル幅・幅の由来・往復変換、7 ケース） | **7/7 PASS** |
| `test/walls.sh`（罫線の壁） | **PASS** |
| `test/stalelock.sh`（古いロックの扱い、stdin なし） | **3/3 PASS** |
| `test/xdrive.py`（本物の X キーイベントでの操作・セーブ・リストア） | **動作確認済み** |

---

## 1. この移植の核心 —— 幅はエンコーディングが決める

実験の H3 は「セルグリッドなら East Asian Ambiguous 幅問題が消える」だった。
JNetHack に持ち込むと、この主張は**より強い形**になる。

`sdl_puteuc()` は幅を**届いたバイト列だけから決める**。コードポイントを見ない。

```c
void
sdl_puteuc(b1, b2)
int b1, b2;
{
    int w = ((b1 & 0xFF) == 0x8E) ? 1 : 2;

    sdl_put_wide(sdl_euc_to_ucs(b1, b2), w);
}
```

理由は「JIS X 0208 の 2 バイトで届いたから」である。
`sdl_cp_width()` を呼んではいけない。

JIS X 0208 の第 1 区には、Unicode のコードポイントが UAX #11 で
**Ambiguous** に分類される文字が並んでいる —— `±`(U+00B1)、`×`(U+00D7)、
`÷`(U+00F7)、`§`(U+00A7)、`°`(U+00B0) など。
端末では、これらが 1 桁か 2 桁かは利用者のロケールが決める。
一方 JNetHack のレイアウト側は**すでに 2 桁と決めている**:

- `japanese/jlib.c` の `is_kanji1()` / `is_kanji2()` / `split_japanese()` は
  2 バイトを 2 桁として数える
- `win/tty/topl.c` の `folding_japanese()` はそれを前提にメッセージを折り返す
- `win/tty/wintty.c` の `tty_putstr()` はステータス行を
  **バイト添字をそのまま桁番号として** `tty_putsym()` に渡す

ここで `sdl_cp_width()` を信用すると、backend とポートが
「次の桁はどこか」で食い違う。セルグリッドが防ごうとしていた桁ずれそのものである。

**エンコーディングが幅を決め、グリフは決めない。** 帰結として、
利用者に設定させるものが何もなく、マシンによって変わるものも何もない。

width test の case 4 がこの 1 点だけを検証している。
`sdl_cp_width()` がこれらを 1 と答えることも同時に確認しており、
「コードポイントだけを見る実装なら間違える」ことを固定している。

### 1-1. 例外は SS2 の半角カナだけ

EUC-JP は JIS X 0201 の半角カナを SS2（`0x8E`）＋ 1 バイトで書く。
これも 2 バイトで届くが、**1 桁**である —— それが半角カナの存在理由であり、
どの端末もそう描く。素朴に 2 桁として扱うと、IME を半角カナにして
入力した名前が `ｱ ｲ ｳ ｴ ｵ` と間延びして出た。

区別はすべて先頭バイトが持っている（`0x8E` か否か）ので、
ここでもグリフの性質は参照しない。**幅を決めるのはやはりエンコーディングである。**

なお `jlib.c` の `is_kanji()` は `0x8E` も「上位ビットが立っている」として
先頭バイト扱いにするため、ポート側の折り返しは半角カナを 2 桁と数え続ける。
ずれる向きは「行が短くなる」側で、はみ出しは起きない。
これは JNetHack を本物の端末で動かしたときも同じである。

width test の case 6 がこれを検証している。

> **`UTF8-PLAN.md` §4.1 への回答**: Ambiguous 幅対策 A（端末設定を利用者に強いる）
> と C（wcwidth フォールバック）は、SDL 経路では不要どころか**有害**である。
> C は上記のとおり間違った答えを出す。

---

## 2. 出力経路 —— jlib.c が既に正しい場所にあった

実験の報告 §12-3 は「バイト列 → 文字の再構成が唯一の新規作業」と見積もっていた。
実際には **JNetHack 1.1.5 はそれを既に持っていた。**

`japanese/jlib.c` の `jbuffer()` / `cbuffer()` は EUC-JP の 2 バイトを溜めてから
**差し替え可能なコールバック** `f1`（1 バイト用）/ `f2`（2 バイト用）を呼ぶ。
既定は `tty_jputc` / `tty_jputc2` / `tty_cputc` / `tty_cputc2` の 4 つで、
いずれも `putchar()` を叩いていた。

移植はこの 4 つを向け直しただけである:

| jlib.c の関数 | SDL では |
|---|---|
| `tty_cputc(c)` / `tty_jputc(c)` | `sdl_putbyte(c)` |
| `tty_cputc2(c1,c2)` / `tty_jputc2(c1,c2)` | `sdl_puteuc(c1,c2)` |

3 層構造は実験と同じで、真ん中に 1 段増えただけ:

| 層 | 役割 |
|---|---|
| `jbuffer()` / `cbuffer()` | バイト列 → 文字（EUC-JP の対を組む）。**既存** |
| `sdl_putbyte(b)` | 1 バイト → コードポイント。文字集合（DEC / CP437）の解決 |
| `sdl_puteuc(b1,b2)` | EUC-JP の対 → コードポイント。幅 2 で確定 |
| `sdl_putcp(cp)` | コードポイントをセルに置く |

### `setkcode()` を IC に固定した

`jbuffer()` は `f2` を呼ぶ**前に**内部コード → `output_kcode` の変換をする。
SDL の窓に「出力エンコーディング」は存在しないので、
`setkcode()` は SDL ビルドでは常に `output_kcode = IC` にする。
これで `tty_jputc2()` に届くバイト対は内部コードのまま、
`sdl_puteuc()` が期待する形になる。

---

## 3. EUC-JP → Unicode の変換表

`include/jis0208.h`（生成物、1,246 行 / 8,836 エントリ / 17 KB の .rodata）。
生成は `japanese/mkjis0208.py`。

```sh
./japanese/mkjis0208.py > include/jis0208.h
```

**iconv を使わなかった理由**は、それが H3 の対偶になるからである。
`iconv(3)` は変換を C ライブラリとロケールの有無に依存させる。
セルグリッドがロケール依存を追い出すために存在しているのに、
その中心にロケール依存を置くのは筋が通らない。
`SDL_iconv_string()` も、SDL 自身がシステム iconv 付きでビルドされていないと
EUC-JP を持たないので同じ問題がある。

生成表なら実行時の依存が 0 で、**どのマシンでも同じ答え**になる。
生成元は Python の `euc_jp` コーデックなので、再生成にデータファイルも要らない。

逆方向（IME 入力用）は表の線形走査である。8,836 回の比較だが、
IME が確定するたびにしか走らないので実測で問題にならない。
索引を作れば速いが、観測できる差のないコードが増えるだけである。

割り当ては 6,879 文字で、JIS X 0208 の文字数と一致する。
重複するコードポイントは 0 なので、逆変換は一意に決まる。
width test の case 5 が 6,879 文字すべての往復を検証している。

| EUC-JP | 扱い |
|---|---|
| `0xA1..0xFE` × 2 | JIS X 0208。表を引く。幅 2 |
| `0x8E` + `0xA1..0xDF` | 半角カタカナ → U+FF61..U+FF9F。**幅 2**（後述） |
| `0x8F` + 2 バイト | JIS X 0212。`jbuffer()` が 3 バイトを渡せないので U+FFFD |

半角カタカナを幅 2 にしているのは、`is_kanji1()`/`is_kanji2()` が
2 バイト = 2 桁として数えているからである（§1 と同じ理由）。
グリフは半角なので見た目は間になるが、**桁は合う**。
なお JNetHack のソースとデータには 1 箇所も現れないので実際には起きない。

---

## 4. 入力 —— UTF-8 から EUC-JP へ

`SDL_StartTextInput()` は呼んである。`SDL_TEXTINPUT` は UTF-8 で来るので、
`sdl_queue_text()` が逆変換して**両方のバイトを**キューに入れる。

両方でなければならない理由は `win/tty/getline.c` にある。
BS の処理が、集めたバッファに対して `is_kanji2()` を呼び、
対だと分かれば 2 バイト消す。キューに先頭バイトだけが残ると同期が壊れる。

`SDL_SetTextInputRect()` でカーソル位置を IME に伝えているので、
変換候補ウィンドウが入力行を覆わない。

width test の case 6 が `sdl_queue_text("日本語")` の UTF-8 を入れて、
キューに `C6 FC CB DC B8 EC` が並ぶことを直接確認している。

**実 IME での確認は未了。** この環境に IME が入っていないため、
変換そのもの（case 6）と、SDL のイベント経路が本物のキーイベントで動くこと
（`test/xdrive.py`）は別々に検証したが、両者を通した確認はしていない。

### 4-1. Alt はメタ —— 上位ビットを立てるのは移植側の仕事

`src/cmd.c` のメタコマンドは `M(c) == 0x80|c` である
（`M-n` = `ddocall`、`M-p` = `dopray`）。端末なら 8 ビット目を立てて送るのは
端末の役目だが、ここには端末がない。`rhack()` に届くまでに ESC 接頭辞を
解釈する場所も**ない**ので、上位ビット付きの 1 バイトだけが唯一の経路である。

どの文字に Alt が付いたかは**キーボードレイアウトの問題**で、こちらでは
決められない。US 配列の `M('?')` は Alt+Shift+/ だが、他の配列では別の
キーである。SDL は同じキー押下から `SDL_KEYDOWN` と `SDL_TEXTINPUT` の
両方を積むので、`sdl_queue_key()` は「メタが来た」ことだけを覚え、
文字は次の `SDL_TEXTINPUT` から取る。両者は `sdl_pump()` の同じ
ドレインの中で見えるため、待ち合わせは要らない。

`SDL_TEXTINPUT` が続かない場合（Alt の組み合わせを食べてしまう配列や
ウィンドウマネージャ、`WM_CHAR` を出さない Windows）はキーシムを使う。
このときだけ Shift の解釈が US 配列の仮定になる。

`./test/xdrive.py --keys 'n V y SPACE SPACE M-n y b poi RET i'` で、
名前を付けた短剣が持ち物一覧に出るところまで確認済み。

---

## 5. 見つかった不具合 4 件

5-1 と 5-2 は `test/compare.sh` が、5-3 は実際に 100% CPU で張り付いた
プロセスを調べて、5-4 は利用者からの報告で見つけた。

### 5-1. `putchar` マクロの二重評価

`include/sdlterm.h` は実験と同じく `putchar` をリダイレクトする。
実験の版は

```c
#define putchar(c)	(sdl_putbyte((int)(unsigned char)(c)), (int)(c))
```

で、引数を**2 回**書いている。素の 3.2.3 はこの経路で `putchar(*p++)` を
書いていないので問題にならなかったが、JNetHack の `wintty.c` の `dmore()` は

```c
      p = prompt;
      while(*p)
	putchar(*(p++));
```

と書く。`p` が 1 周につき 2 回進み、`--More--` が `-Mr-` になる。
`(end)` は `(n)`、`(1 of 3)` は `( f3` になった。

関数にして解決した:

```c
# define putchar(c)	sdl_putchar((int)(unsigned char)(c))
```

`sdl_putchar()` は `cputchar()` の薄い包みである。`sdl_putbyte()` ではなく
`cputchar()` を経由するのは、EUC-JP の対を `putchar()` で 1 バイトずつ
書く場所があるため（`tty_askname()` の日本語プレイヤー名のエコー）。

### 5-2. `src/end.c` が起動元の端末にエスケープシーケンスを書いていた

終了時、DECgraphics なら `ESC $ ) B` を出して端末の G1 集合を戻す:

```c
	if (iflags.DECgraphics){
	  putchar(033); putchar('$'); putchar(')'); putchar('B');
	}
```

`end.c` は `wintty.h` を読まないので、この `putchar` は**本物**である。
SDL 版は DECgraphics を既定にしたので毎回発火し、
シェルに `$)B` が残っていた。`#ifndef SDL_GRAPHICS` で囲んだ。

`wintty.h` を読むファイルは `win/tty/` の 4 つ以外に
**`src/windows.c` があった**（`#ifdef TTY_GRAPHICS` の下）。
そこの `def_raw_print()` は「ウィンドウポートが選ばれる前の `raw_print`」なので、
端末に出なければならない。`SDLTERM_KEEP_STDIO` を定義して本物の stdio を残した。
放置すると、起動時のエラーメッセージが**まだ存在しないグリッド**に
書かれて消える。

### 5-3. `isatty(0)` チェックを外したことで `getlock()` が無限ループになった

**これが一番たちが悪い。** stdin が `/dev/null` のとき、
古いロックファイルがあると **100% CPU で永久に張り付く。**

`sys/unix/unixunix.c` の `getlock()` は、ロックファイルを見つけると
「破棄しますか？」と尋ねる。ところがこれは `init_nhwindows()` の**後**・
`WIN_MESSAGE` 生成の**前**に走るので、`iflags.window_inited` はまだ偽であり、
`yn()` は使えない。素の実装はそこで fd 0 を読む:

```c
	    c = getchar();
	    (void) putchar(c);
	    while (getchar() != '\n') ;	/* eat rest of line and newline */
```

stdin が `/dev/null` だと `getchar()` は **EOF を返し続ける**（FILE の EOF
フラグは粘着する）ので、最後の行が終わらない。しかも EOF 以降は
**システムコールを 1 つも出さない**ため、症状はこうなる:

```
$ awk '{print "utime="$14" stime="$15}' /proc/<pid>/stat
utime=172968 stime=5          # user 1729 秒、system 0.05 秒
$ strace -p <pid>
（何も出ない）
```

ウィンドウには何も出ず、メッセージも出ず、strace も無言。
**utime が巨大で stime がほぼ 0** という形が目印である。

素の JNetHack ではこの経路に到達しない。`getlock()` の手前に
`if (!isatty(0)) error("You must play from a terminal.")` があって弾かれるからだ。
SDL 版はこのチェックを外している（fd 0 から入力を取らないので当然）ため、
**外したことで既存の無限ループが露出した。**

テスト環境だけの問題ではない。デスクトップのランチャから起動すれば
stdin は `/dev/null` になるので、普通の利用者が普通に踏む。

対処は 2 つ:

1. **SDL では窓で訊く。** `getlock()` の時点で SDL のウィンドウは既にあり
   （`tty_startup()` が `init_nhwindows()` から走っている）、グリッドも確保済みで、
   足りないのは `WIN_MESSAGE` だけである。そこで `sdl_yn()` を足した ——
   グリッドに直接プロンプトを書き、`sdl_getch()` で 1 キー読む 20 行の関数で、
   ESC は 'n'（破棄しない）扱いにしてある。
2. **素の経路も EOF で抜けるようにした。** tty ビルドでは `isatty(0)` が
   守っているので到達しないが、地雷を残す理由がない。

`test/stalelock.sh` が回帰テストである。修正前のバイナリで走らせると
3 ケースすべてが `FAIL (killed after 15s -- it is spinning)` になることを
確認済み。

> **反省**: この 100% CPU プロセスは、移植中に私が
> 「スクリーンショットの実行が 2 分でタイムアウトした」と報告したその実行である。
> 別のディレクトリで再実行したら通ったのでそのまま先に進んでしまい、
> **28 分間 CPU を焼き続けるプロセスを残した。**
> タイムアウトそのものが調べるべき兆候だった。

---

### 5-4. SDL の既定がデスクトップ全体に手を出していた

起動すると **KDE のコンポジタが切れる。** ウィンドウのプロパティを見れば一目である:

```sh
$ xprop -name 'JNetHack (SDL)' | grep -i bypass
_NET_WM_BYPASS_COMPOSITOR(CARDINAL) = 1
```

SDL2 は X11 でこれを**既定で 1 にする**（`SDL_VIDEO_X11_NET_WM_BYPASS_COMPOSITOR`）。
コンポジット型のウィンドウマネージャはこれを尊重して、
そのウィンドウが出ている間コンポジットを止める。

同じ理由で SDL は**スクリーンセーバも既定で抑止する**
（`XScreenSaverSuspend` と `org.freedesktop.ScreenSaver` の D-Bus 経由。
`SDL_VIDEO_ALLOW_SCREENSAVER` で制御される）。

```sh
$ strings /lib/x86_64-linux-gnu/libSDL2-2.0.so.0 | grep -iE 'bypass_compositor|allow_screensaver|ScreenSaverSuspend|org.freedesktop.ScreenSaver'
_NET_WM_BYPASS_COMPOSITOR
/org/freedesktop/ScreenSaver
org.freedesktop.ScreenSaver
SDL_VIDEO_ALLOW_SCREENSAVER
SDL_VIDEO_X11_NET_WM_BYPASS_COMPOSITOR
XScreenSaverSuspend
```

**どちらも全画面のアクションゲーム向けの既定**で、端末の代わりを務めるものには
まったく合わない。フレーム遅延のために利用者のデスクトップを差し出す取引を
ターン制のゲームがする理由はないし、NetHack はほぼ常時キー入力を待っているので、
席を離れた利用者が戻ってきたら画面がロックされていない、というのも困る。

`tty_startup()` で `SDL_CreateWindow()` の前に両方とも切った:

```c
    (void) SDL_SetHint(SDL_HINT_VIDEO_X11_NET_WM_BYPASS_COMPOSITOR, "0");
    (void) SDL_SetHint(SDL_HINT_VIDEO_ALLOW_SCREENSAVER, "1");
```

`SDL_SetHint()`（`SDL_HINT_OVERRIDE` ではない）なので**環境変数が優先される**。
SDL の既定に戻したい人は `SDL_VIDEO_X11_NET_WM_BYPASS_COMPOSITOR=1` /
`SDL_VIDEO_ALLOW_SCREENSAVER=0` を設定すればよい。確認済み:

```sh
$ xprop -name 'JNetHack (SDL)' | grep -i bypass          # 既定
（プロパティなし）
$ SDL_VIDEO_X11_NET_WM_BYPASS_COMPOSITOR=1 ...           # 上書き
_NET_WM_BYPASS_COMPOSITOR(CARDINAL) = 1
```

> スクリーンセーバ側は**この環境では動作を観測できていない。**
> X の screensaver timeout が元から 0 で、KDE のロックは
> 自前の idle サービスが握っているため、変化が見えない。
> 上記のとおり SDL 側の既定が抑止であることは確認したので、
> 明示的に切るのが正しいと判断した。

---

---

## 6. ビルド基盤

JNetHack 1.1.5 は素の 3.2.3 よりかなり現代化されていて、
実験で必要だった 8 点のうち多くは既に入っていた。

| 実験で必要だったこと | JNetHack 1.1.5 では |
|---|---|
| `SYSV`+`LINUX` の選択 | **済**（`__linux__` で自動） |
| `TEXTCOLOR` | **済** |
| `COMPRESS` を gzip に | **済** |
| implicit-int 3 箇所 | **済** |
| `-std=gnu89` / `-fcommon` | 必要（`Makefile.src` と **`Makefile.utl`**） |
| `-lncurses` | 必要 |
| 生成済み lex パーサ | `dgn_lex.c` のみ必要（`lev_lex.c` は flex 版で既に対応済み） |
| `dat/` の再生成 | 必要（`test/build.sh` が毎回やる） |

JNetHack 固有に必要だったもの:

| 変更 | 理由 |
|---|---|
| `include/config.h`: `X11_GRAPHICS` / `GTK_GRAPHICS` を無効化 | X11R6/Xaw と GTK+ 1.x はもう入らない。SDL は tty の下を差し替えるので両方不要 |
| `include/system.h`: `LINUX` で `<string.h>` / `<strings.h>` を include | `memset` / `memcpy` / `strstr` / `strncasecmp` が暗黙宣言になっていた（mem* の手書きプロトタイプは JNetHack が既にコメントアウト済みで、代わりが無かった） |
| `include/extern.h`: `yyyymmdd()` / `get_scr_size()` の宣言 | 前者は `topten.c` が未宣言で使う。後者は `ioctl.c` から呼ぶ |
| `sys/unix/Makefile.utl`: `-fcommon` | 生成済みパーサ 3 つが `newbie` の仮定義を共有していてリンクできない |
| `test/mkplaydir.sh`: `j` 付きのデータファイル名 | `global.h` の `HELP`/`RUMORFILE`/`ORACLEFILE` は実行時も `jhelp`/`jrumors`/`joracles` |

`-Wall` ビルドで、`sdlterm.c` と `jis0208.h` の警告は **0 件**。
tty 版と SDL 版の警告の差分は 0 件である（確認方法は §8 参照）。

---

## 7. テスト

### `test/compare.sh` —— 画面一致（17 ケース）

同一シード・同一キー列で、termcap 版（pty + pyte）と SDL 版
（`NH_SDL_DUMP` のセルダンプ）の 80×24 画面を diff する。

**日本語が入った画面で桁が合うことまで見ている。** pyte は East Asian Wide を
2 セルに置くので、日本語ステータス行が両者で同じ桁に来なければ落ちる。

```
compare startup: PASS          compare options: PASS
compare map: PASS              compare escape_menu: PASS
compare inventory: PASS        compare walkabout: PASS
...                            compare endgame: PASS
```

一致例（両者バイト単位で同一）:

```
' 'コマンド？
                                                     -+--------------
                                                     |?.............|
                                                     ..k.......@....|
                                                     ----------------
Poc 見習い             強:16 早:13 耐:18 知:10 賢:7 魅:11  中立
地下:1  $:0  体:16(16) 魔:2(2) 鎧:6  経験:1
```

> **既知の限界**: pyte は幅を `wcwidth()` から取るので Ambiguous を 1 と数える。
> `±` などが出る画面では両者が食い違い、**正しいのは SDL 側**である
> （§1 のとおり、JNetHack のレイアウト側が 2 と数えている）。
> 17 ケースにはそういう文字が出ないので全部通る。

### `NH_SDL_WIDTHTEST=1` —— 幅と変換（7 ケース）

ゲームの代わりに走る。テキストは `jputchar()` に EUC-JP バイトで入れるので、
**jlib.c の蓄積器から表・グリッドまで経路全体**が対象になる。

```
    case 1 (occupancy, no drift): PASS          2 バイト文字が x と x+1 を占め、桁がずれない
    case 2 (half overwrite, cl_end): PASS       片割れが残らない（直接上書き / cl_end 経由）
    case 3 (locale independence): PASS          LANG を 5 種変えても不変
    case 4 (Ambiguous width from encoding): PASS §1。wcwidth 実装なら落ちる
    case 5 (round trip, 6879 characters): PASS  EUC → Unicode → EUC
    case 6 (half-width katakana, 1 cell): PASS  §1 の例外。SS2 は 2 バイトだが 1 桁
    case 7 (UTF-8 input -> EUC-JP pairs): PASS  §4
```

### `test/walls.sh` —— 罫線の壁

実験と同じ。`dec_special[]` の 5 エントリ（角丸 4 種 + 床の `.`）を
pyte 側にも適用して比較する。

```
walls: PASS (4 rounded corners, DEC mapping agrees with pyte)
```

```
                                                     ╭+─────────────╮
                                                     │?.............│
                                                     ..k.......@....│
                                                     ╰──────────────╯
Poc 見習い             強:16 早:13 耐:18 知:10 賢:7 魅:11  中立
```

### `test/stalelock.sh` —— 古いロックの扱い（§5-3 の回帰テスト）

stdin を閉じ、ロックファイルを置いた状態で 3 ケース。

```
stalelock prompt: PASS (asked on the grid, not on fd 0)
stalelock decline: PASS (exited in 0s, old game kept)
stalelock accept: PASS (exited in 0s, game started)
```

`NH_SDL_KEYS` を空にすると `sdl_getch()` が最初の入力要求で画面を
ダンプして抜けるので、`prompt` ケースは「利用者が実際に見る画面」を
そのまま検証できる。

### `test/xdrive.py` —— 本物のウィンドウ

`NH_SDL_KEYS` はキューを直接埋めるので、SDL のイベント経路を通らない。
XTEST で本物のキーイベントを送って確認する。

```sh
HACKDIR=~/jnhdir ./test/xdrive.py --keys 'n V y SPACE SPACE l l j LEFT UP i ESC' \
    --shot /tmp/shot.bmp -- ./src/jnethack.sdl -u poc
```

確認済み: 通常キー（`SDL_TEXTINPUT`）、矢印キー（`SDL_KEYDOWN` → `hjkl`）、
Ctrl 系、メニューの開閉、`S y` でのセーブと再起動時のリストア、
日本語の反転属性つき見出し（武器 / 鎧 / 食料）。

> セーブファイルが gzip されないのは tty 版でも同じで、この移植の前からの挙動。

---

## 8. 注意: 端末の grep が EUC-JP のファイルで黙って失敗する

このリポジトリのソースは EUC-JP なので、UTF-8 ロケールの GNU grep は
これらを不正なマルチバイト列と見て**マッチを黙って捨てる**。
終了コードも 1 になるので、「0 件」と「読めなかった」が区別できない。

```sh
grep -c putchar win/tty/wintty.c        # 何も出ない（実際は 43 箇所）
LC_ALL=C grep -ac putchar win/tty/wintty.c   # 43
```

ビルドログも同じで、警告を数えるときは `LC_ALL=C grep -a` を使う。
移植中に一度これで「警告 0 件」と誤認した。

---

## 9. 環境変数

| 変数 | 用途 |
|---|---|
| `NETHACK_SDL_FONT` | フォント指定（`path` または `path:face_index`） |
| `NETHACK_SDL_FONTSIZE` | ポイントサイズ（既定 18） |
| `NETHACK_SDL_COLS` / `NETHACK_SDL_ROWS` | 起動時のグリッドサイズ（既定 80×24） |
| `NETHACK_SEED` | RNG を固定（画面比較用） |
| `NH_SDL_DUMP` | セル配列を UTF-8 テキストで書き出す |
| `NH_SDL_KEYS` | キー列をスクリプト投入（ヘッドレス実行可） |
| `NH_SDL_SHOT` | 描画のたびに BMP を保存 |
| `NH_SDL_WIDTHTEST` | ゲームの代わりに幅テストを実行 |
| `NH_SDL_DEBUG` | リサイズ関連の診断を stderr へ |

フォントの既定は `Noto Sans Mono CJK JP`（`NotoSansCJK-Regular.ttc` の
face index 5）。ASCII の advance が 9px、CJK が 18px で正確に 1:2。
ただし**レイアウトはフォントではなくグリッドが決める**ので、
1:2 でないフォントでも桁はずれない（グリフがセルを超える場合は縮める）。

---

## 10. ウィンドウのリサイズ —— 桁ではなくフォントが変わる

角をドラッグするとフォントのポイントサイズが変わる。グリッドは起動時の
80×24 のまま動かない。端末エミュレータ流に桁数を変える案は採らなかった:

- `wintty.c:190` の `winch()` は `#if defined(SIGWINCH) && defined(CLIPPING)`
  の中にあり、Windows には再レイアウトを依頼する経路が無い
  （[`SDL-WINDOWS.md`](SDL-WINDOWS.md) §8）。グリッドが動かなければ
  `CO` / `LI` は起動時のまま有効で、再レイアウト自体が要らない。
- 角をつかむ人が欲しいのはたいてい大きい文字であって、広い盤面ではない。

`win/tty/sdlterm.c` 側:

| 関数 | 役割 |
|---|---|
| `sdl_open_font()` | 開けた spec を `font_spec` に覚える。リサイズでは**同じ face** を別サイズで開き直す。候補リストを歩き直すと別の face に当たりうるが、face が変われば advance が変わり、グリッドが壊れる |
| `sdl_fit_ptsize()` | セル寸法はポイントサイズにほぼ比例するので、推定値を 1 つ出して ±1 を実測で詰める。探索ではなく補正 |
| `sdl_measure()` | 使用中のフォントに触らずに、指定サイズのセル寸法だけを測る |
| `sdl_load_font()` | 差し替え時に `sdl_free_glyphs()` を呼ぶ。キャッシュのテクスチャは旧サイズで焼かれている |
| `sdl_center()` | 使い切れなかった端数を上下左右へ均等に振る。窓とグリッドの縦横比は普通は一致しないので、余白は必ず出る |

最小サイズは `SDL_SetWindowMinimumSize()` で `SDL_MIN_PTSIZE`（6pt）の
グリッドに固定してある。それ以下はグリッドが入らない。
余白は最低でも `SDL_MARGIN`（4px）確保する —— Windows がウィンドウの角を
丸め、クライアント領域の隅を数 px 食うため。

---

## 11. やっていないこと

- **実 IME での日本語入力**（§4）
- **タイル描画** — セルにビットマップを置く拡張は自明だが手を付けていない
- **macOS** — 手を付けていない
- **Windows** — 移植した。[`SDL-WINDOWS.md`](SDL-WINDOWS.md) を参照。
  MinGW-w64 クロスビルド + wine で検証済みで、`wintty.c` は無改造のまま
  （H1 が Windows でも成立した）。`-u 日本語名` は未対応
- **X11 / GTK ポート** — `config.h` で無効化した。ビルドできる状態に戻すのは
  この移植の範囲外
- **音** — `tty_nhbell()` はウィンドウのフラッシュで代用
- `NETHACK_SEED` / `NH_SDL_KEYS` / `NH_SDL_DUMP` / `NH_SDL_WIDTHTEST` は
  テスト用フックである。とくに `NH_SDL_KEYS` はスクリプト終端で `_exit(0)` し、
  `sys/unix/unixunix.c` の自動リロール防止チェックを無効にする前提で動く
