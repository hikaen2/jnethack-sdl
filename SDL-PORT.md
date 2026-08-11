# JNetHack 3.4.3 SDL ポート

- 対象: **JNetHack 3.4.3-0.11**（NetHack 3.4.3 ベース）
- 環境: Linux 6.8 / GCC 13 / SDL2 2.30.0 / SDL2_ttf 2.22.0
- 移植元: `experimental/jnethack-1.1.5-sdl`（JNetHack 1.1.5 / NetHack 3.2.3 ベース）。
  差分は §12 にまとめてある

---

## 0. 何をしたか

`win/tty/termcap.c` を **`win/tty/sdlterm.c`** に差し替えた。
ウィンドウポートは増やしていない。**tty ポートの下にある端末を、
自前のセルグリッドに置き換えた**だけである。

`topl.c` / `getline.c` は無改造。`wintty.c` も 1 箇所の例外を除いて無改造で、
その 1 箇所は壁を線で描くための 6 行である（§2-1）。

```sh
./test/build.sh                    # src/jnethack.tty と src/jnethack.sdl
./test/mkplaydir.sh ~/jnhdir
HACKDIR=~/jnhdir ./src/jnethack.sdl -u あなたの名前
```

検証結果:

| テスト | 結果 |
|---|---|
| `test/compare.sh`（termcap 版と SDL 版の画面一致、20 ケース） | **5/20 PASS**（下の「重要な限界」。残りは種を固定できないことと、地図の壁が意図的に違うことによる） |
| `NH_SDL_WIDTHTEST=1`（2 セル幅・幅の由来・往復変換、7 ケース） | **7/7 PASS** |
| `test/walls.sh`（角丸の壁と、壁以外がデフォルト文字であること） | **PASS**（画面の内容ではなく文字種を見るので種に依存しない） |
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

`japanese/jlib.c` の `jbuffer()` / `cbuffer()` は EUC-JP の 2 バイトを溜めてから
**差し替え可能なコールバック** `f1`（1 バイト用）/ `f2`（2 バイト用）を呼ぶ。
既定は `tty_jputc` / `tty_jputc2` / `tty_cputc` / `tty_cputc2` の 4 つで、
いずれも `putchar()` を叩いていた（3.4.3 では `NO_TERMS && (MSDOS || WIN32CON)`
のときだけ `xputc()` を経由するが、この移植はどちらも通らない）。

この構造は 1.1.5 と 3.4.3 で同じである。**この移植のいちばん大きな前提が、
バージョンをまたいで変わっていない。**

移植はこの 4 つを向け直しただけである:

| jlib.c の関数 | SDL では |
|---|---|
| `tty_cputc(c)` / `tty_jputc(c)` | `sdl_putbyte(c)` |
| `tty_cputc2(c1,c2)` / `tty_jputc2(c1,c2)` | `sdl_puteuc(c1,c2)` |

3 層構造は実験と同じで、真ん中に 1 段増えただけ:

| 層 | 役割 |
|---|---|
| `jbuffer()` / `cbuffer()` | バイト列 → 文字（EUC-JP の対を組む）。**既存** |
| `sdl_putbyte(b)` | 1 バイト → コードポイント。バイトは見たままの文字（文字集合は無い） |
| `sdl_puteuc(b1,b2)` | EUC-JP の対 → コードポイント。幅 2 で確定 |
| `sdl_putcp(cp)` | コードポイントをセルに置く |

### 2-1. 壁だけは glyph から描く

バイトだけを見ていては壁を線にできない。地図の `-` は横壁でもあり、角でもあり、
開いた扉でもあり、爆発の上辺でもある。`|` は縦壁でもあり、墓でもあり、光線でもある。
`src/drawing.c` が選んだ文字がバイトになった時点で、その区別は失われている。

区別が残っている最後の場所は `win/tty/wintty.c` の `tty_print_glyph()` で、
ここには `glyph` がそのまま渡る。そこに 6 行だけ足した:

```c
#ifdef SDL_GRAPHICS
    if (!sdl_put_wall(glyph))
#endif
	g_putch(ch);		/* print the character */
```

`sdl_put_wall()`（`win/tty/sdlterm.c`）は 11 個の壁シンボルだけを表に持ち、
それ以外の glyph には FALSE を返す。FALSE が返れば従来どおり文字が印字される。
つまり**壁以外はすべてデフォルトの文字**で、打ち消しリストは存在しない。
角丸 4 種を含む罫線は `sdl_draw_box()` がベクタで描くので、
フォントに U+256D..U+2570 が無くても結果は変わらない。

ロゴレベル（Rogue 階）は除外する。`assign_rogue_graphics()` が
その階のために記号を組み替えており、素の見た目が意図だからである。

**文字集合は持たない。** DEC も CP437 も `sdlterm.c` から削除した。
JNetHack は `switch_graphics()` の IBM 分岐を `#if 0 /*JP*/` で落としているので
CP437 のバイトは元から発生せず、DECgraphics は
「代替文字集合を持たない」という理由で SDL ビルドではオプションに出さない
（`src/options.c`、`AS`/`AE` が null であることと同じ主張）。
`graph_on()` / `graph_off()` は `g_putch()` が呼ぶので空の関数として残っている。

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

## 5. 見つかった不具合

5-1〜5-4 は 1.1.5 の移植で見つけたもので、5-1 は 3.4.3 では発火しない。
5-5 以降は 3.4.3 に移して初めて出たものである。

### 5-1. `putchar` マクロの二重評価（3.4.3 では発火しない）

`include/sdlterm.h` は `putchar` をリダイレクトする。素朴に書くと

```c
#define putchar(c)	(sdl_putbyte((int)(unsigned char)(c)), (int)(c))
```

となり、引数を**2 回**書くことになる。1.1.5 の `wintty.c` の `dmore()` は
`putchar(*(p++))` と書いていたので、`p` が 1 周につき 2 回進み
`--More--` が `-Mr-` になった。

**3.4.3 では同じ場所が `jputchar(*(p++))` になっている**（`wintty.c:1156`）。
`jputchar()` はマクロではなく関数なので、この不具合はこのバージョンには無い。
`putchar()` に副作用つきの引数を渡す箇所は、`wintty.c` にも `topl.c` にも
`getline.c` にも残っていない（`putchar(*cp)` は 3 箇所あるが、いずれも
`#if 0 /*JP*/` の側で、JP 側は `jputchar` / `cputchar` を呼ぶ）。

それでもマクロではなく関数を使う:

```c
# define putchar(c)	sdl_putchar((int)(unsigned char)(c))
```

安いうえに、上流が 1 行変えただけで戻ってくる種類の不具合だからである。
`sdl_putchar()` は `cputchar()` の薄い包みである。`sdl_putbyte()` ではなく
`cputchar()` を経由するのは、EUC-JP の対を `putchar()` で 1 バイトずつ
書く場所があるため（`tty_askname()` の日本語プレイヤー名のエコー）。

### 5-2. `src/end.c` が起動元の端末にエスケープシーケンスを書いていた（解消済み）

終了時、DECgraphics なら `ESC $ ) B` を出して端末の G1 集合を戻す（`src/end.c:1235`）:

```c
	if (iflags.DECgraphics){
	  putchar(033); putchar('$'); putchar(')'); putchar('B');
	}
```

`end.c` は `wintty.h` を読まないので、この `putchar` は**本物**である。
SDL 版が DECgraphics を既定にしていた頃は毎回発火し、シェルに `$)B` が残っていた。
当時は `#ifndef SDL_GRAPHICS` で囲って回避していた。

**現在は囲っていない。** SDL ビルドで `iflags.DECgraphics` が真になる経路が
無くなったからである —— オプションは `boolopt[]` のプレースホルダ（`addr` が 0）
なので `parseoptions()` が書き込めず、`$TERM` からの自動検出は別のガードで
落ちており、`switch_graphics(DEC_GRAPHICS)` の呼び出しも無く、
`iflags` はセーブファイルに入らない（`src/save.c` に `&iflags` の `bwrite` が無い）
ので復元もされない。おかげで **`src/end.c` は上流と 1 バイトも違わない。**

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

### 5-5. `has_colors()` —— SDL 版がリンクしないライブラリを呼んでいた

`sys/share/unixtty.c` の `init_linux_cons()` は、Linux の仮想コンソールで
`TEXTCOLOR` のときに `has_colors()` を呼ぶ。これは terminfo の関数で、
`-lncurses` から来る。SDL 版はそのライブラリをリンクしないので、
**リンクエラーになった**（3.4.3 に移して最初に出たエラーがこれである）。

同じブロックは `linux_flag_console` が立っているとき fd 1 に `ESC ( U` を
書いてコンソールのフォントを差し替える。**これは §5-2 と同じ形の誤り**で、
fd 1 は起動元の端末であってゲームの画面ではない。SDL 版でそれをやると
利用者のシェルが壊れ、窓には何の影響も無い。

したがって `check_linux_console()` を SDL では即 return にして
`linux_flag_console` を立てないようにし、`has_colors()` の 2 箇所
（`init_sco_cons()` と `init_linux_cons()`）を `!SDL_GRAPHICS` で囲んだ。
到達しないコードだが、リンカはそれを知らない。

### 5-6. Return キーには行規律が無い

`win/tty/getline.c` の `tty_getlin()` は行末を **`'\n'` でしか受けない**
（`getline.c:154`。`'\r'` も見るのは `#if defined(apollo)` の側だけ）。
端末では行規律の `ICRNL` が Return の CR を LF に直すので届くが、
グリッドの前には行規律が無い。

`sdl_queue_key()` は `SDLK_RETURN` / `SDLK_KP_ENTER` を `'\n'` に
写しているので**本物の Return キーは元から正しく動く**。引っかかったのは
`test/compare.sh` のほうで、`RET` トークンを pty 側と同じ `"\r"` の
バイトで `NH_SDL_KEYS` に流し込んでいた。`#quit` が確定せず、
`# quity` がトップラインに残る形で出た。

トークンの展開を `"\n"` に直した。**注入経路は `sdl_queue_key()` を
通らない**ので、キーイベントが受け持っている変換は注入側が済ませておく
必要がある —— テスト用フックの一般的な性質で、`NH_SDL_KEYS` を使うときは
常について回る。

---

## 6. ビルド基盤 —— 3.4.3 はほぼ何も要らなかった

**素の JNetHack 3.4.3 は、この Linux で無改造のままビルドでき、動く。**
1.1.5 で必要だった現代化のうち、残っていたのは 1 点だけである。

| 1.1.5 で必要だったこと | JNetHack 3.4.3 では |
|---|---|
| `SYSV`+`LINUX` の選択 | **済** |
| `TEXTCOLOR` | **済** |
| `COMPRESS` を gzip に | **済**（`config.h` が既に `/usr/bin/gzip`） |
| `-lncurses` | **済**（`Makefile.src` で選択済み） |
| `-fcommon` | **不要**。`newbie` の仮定義の共有は 3.4 で解消している |
| 生成済み lex パーサ | **不要**。`sys/share/` の 4 本がそのまま通る（`test/build.sh` がコピーする） |
| `X11_GRAPHICS` / `GTK_GRAPHICS` の無効化 | **不要**。3.4.3 の `config.h` は最初から tty だけ |
| `include/system.h` の `<string.h>` | **不要** |
| `include/extern.h` の `yyyymmdd()` | **済**（`extern.h:747`） |
| `-std=gnu89` | 必要（`Makefile.src` と `Makefile.utl`） |
| `dat/` の再生成 | 必要（`test/build.sh` が毎回やる） |

`-std=gnu89` は GCC 13 では無くても通るが、GCC 14 の既定は C23 で、
この木の K&R 形式の関数定義は構文エラーになる。先に入れてある。

SDL のために必要だったもの:

| 変更 | 理由 |
|---|---|
| `include/config.h`: `DLB` を有効化 | Windows の zip がデータを 1 個の `nhdat` で配れる。歴代の JNetHack と同じ。`sys/unix/Makefile.dat` に `nhdat` ターゲットを足した |
| `include/extern.h`: `get_scr_size()` / `sdl_yn()` の宣言 | `ioctl.c` と `unixunix.c` から呼ぶが、どちらも `sdlterm.h` を読まない |
| `include/unixconf.h`: `PATHLEN` | `unixmain.c` の `orgdir[]` 用（PC 移植は 3.2 から持っている） |
| `sys/share/unixtty.c`: `has_colors()` を SDL では呼ばない | termcap ライブラリの関数で、SDL 版はリンクしない。到達もしない（§5-5） |
| `test/mkplaydir.sh`: `nhdat` と `license` | `DLB` を入れた結果。`dlb_fopen()` は `nhdat` が無いと素のファイルにも落ちない |

`-Wall` ビルドで、`sdlterm.c` と `jis0208.h` の警告は **0 件**。
tty 版と SDL 版の警告の差分は **−1 件**（SDL 版のほうが 1 件少ない。
`unixtty.c` の `has_colors` の暗黙宣言が消えるため）。
確認方法は §8 参照。

---

## 7. テスト

### `test/compare.sh` —— 画面一致（20 ケース）

同一キー列で、termcap 版（pty + pyte）と SDL 版
（`NH_SDL_DUMP` のセルダンプ）の 80×24 画面を diff する。

**日本語が入った画面で桁が合うことまで見ている。** pyte は East Asian Wide を
2 セルに置くので、日本語ステータス行が両者で同じ桁に来なければ落ちる。

> **重要な限界。** 種を固定する手段が無い（ゲームに手を入れない方針のため）
> ので、2 つのビルドは**別のダンジョンを遊ぶ**。地図・ステータス行・
> 振り値・初期持ち物が写る画面は原理的に一致しない。
> `test/wincompare.sh` は同じ事情を EXPECTED-DIFF として表示するが、
> `compare.sh` にはその判定を入れていないので FAIL と出る。
>
> **2 つ目の限界（後から増えた）。** SDL 版は壁を線で描くので、
> 地図が写る画面は種を固定できたとしても一致しない。壁そのものの検査は
> `test/walls.sh` の担当で、`compare.sh` が地図の画面で見ているのは
> 壁の**まわり**のレイアウトである。

```
compare startup: FAIL (2)      compare options: FAIL (24)
compare rolemenu: PASS         compare escape_menu: FAIL (16)
compare racemenu: PASS         compare walkabout: FAIL (16)
compare alignmenu: PASS        compare quit: FAIL (16)
compare map: FAIL (14)         compare endgame: FAIL (14)
compare inventory: FAIL (10)   ...
compare guidebook: PASS        overview_page2: PASS
```

通るのはダンジョンにも地図にも依存しない 5 件 ——
職業／種族／属性のメニュー、ガイドブック、概観の 2 ページ目 —— である。
どのケースが通るかと差分行数は実行ごとに変わる（画面の隅に地図が
写り込むかどうかが、そのときのダンジョン次第だからである）。

キー列は 1.1.5 の `n V y` ではなく **`n v h l`**（職業=ワルキューレ /
種族=人間 / 属性=秩序）である。3.4.3 の職業選択はメニューになっていて、
ワルキューレは小文字の `v`、そのあと種族と属性のメニューが続く。
`y`（おまかせ）では**役が乱数で決まる**ため、画面比較には使えない ——
両者の RNG の消費が同じでも、比較しているのは「同じ役の 2 つの画面」で
なければ意味がない。3.4.3 でメニューが 3 枚に増えたぶん、
`rolemenu` / `racemenu` / `alignmenu` の 3 ケースを足した。

種を固定できていた頃の一致例（`map` ケース。両者バイト単位で同一）。
現在は SDL 側の壁が `╭─│╯` になるので、この形での一致は起こらない:

```
                             -------
                             +.....|
                             |?....|
                             |f@...|
                             |.....|
                             --.----
Poc 見習い             強:18 早:14 耐:18 知:7 賢:10 魅:8 秩序
地下:1  $:0  体:16(16) 魔:2(2) 鎧:6  経験:1
```

> **既知の限界**: pyte は幅を `wcwidth()` から取るので Ambiguous を 1 と数える。
> `±` などが出る画面では両者が食い違い、**正しいのは SDL 側**である
> （§1 のとおり、JNetHack のレイアウト側が 2 と数えている）。
> 20 ケースにはそういう文字が出ないので全部通る。

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

### `test/walls.sh` —— 角丸の壁

参照実行も種の固定も要らない。画面の**内容**ではなく**文字種**を見る:

1. 角丸（`╭╮╰╯`）と線（`│─`）が出ていること
2. 罫線ブロック（U+2500..U+257F）に、許した 11 種以外が現れないこと
3. ASCII と日本語以外の文字が、その 11 種を除いて 1 つも無いこと

3 が「壁以外はデフォルト文字」の回帰テストである。水が `◆` になる、
扉が `▒` になる、といった置換はすべてここに引っかかる。

dlvl 1 の部屋は必ず照明付きなので、キーを 1 つも押さないうちに
開始部屋の四隅が画面に出る。どのダンジョンでも成立する検査なので、
種を固定できない現状でも**通る**:

```
walls: PASS (4 rounded corners, nothing else redrawn)
```

```
                            ╭────╮
                            │....│
                            │$.d.│
                            │...@│
                            ╰────╯
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
HACKDIR=~/jnhdir ./test/xdrive.py --keys 'n v h l SPACE SPACE l l j LEFT UP i ESC' \
    --shot /tmp/shot.bmp -- ./src/jnethack.sdl -u poc
```

確認済み: 3 枚の選択メニュー、通常キー（`SDL_TEXTINPUT`）、
矢印キー（`SDL_KEYDOWN` → `hjkl`）、Unicode の罫線、色、
桁の合った日本語ステータス行。ウィンドウを閉じたときのセーブは
`test/closesave.sh` が別に見る（`PASS`）。

> セーブファイルが gzip されないのは tty 版でも同じで、この移植の前からの挙動。

---

## 8. 注意: 端末の grep が EUC-JP のファイルで黙って失敗する

このリポジトリのソースは EUC-JP なので、UTF-8 ロケールの GNU grep は
これらを不正なマルチバイト列と見て**マッチを黙って捨てる**。
終了コードも 1 になるので、「0 件」と「読めなかった」が区別できない。

```sh
grep -c putchar win/tty/wintty.c        # 何も出ない（実際は 48 箇所）
LC_ALL=C grep -ac putchar win/tty/wintty.c   # 48
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
| `NH_SDL_DUMP` | セル配列を UTF-8 テキストで書き出す |
| `NH_SDL_KEYS` | キー列をスクリプト投入（ヘッドレス実行可） |
| `NH_SDL_SHOT` | 描画のたびに BMP を保存 |
| `NH_SDL_WIDTHTEST` | ゲームの代わりに幅テストを実行 |
| `NH_SDL_DEBUG` | リサイズ関連の診断を stderr へ |

Windows の zip では環境変数を設定させるのが現実的でないので、
フォントは設定ファイル `defaults.nh` の `SDLFONT` / `SDLFONTSIZE` でも
指定できる（`src/files.c`）。環境変数のほうが優先される。
3.4 でこのファイルは `NetHack.cnf` から改名された（`src/files.c:1482`）。

フォントの既定は `Noto Sans Mono CJK JP`（`NotoSansCJK-Regular.ttc` の
face index 5）。ASCII の advance が 9px、CJK が 18px で正確に 1:2。
ただし**レイアウトはフォントではなくグリッドが決める**ので、
1:2 でないフォントでも桁はずれない（グリフがセルを超える場合は縮める）。

---

## 10. ウィンドウのリサイズ —— 桁ではなくフォントが変わる

角をドラッグするとフォントのポイントサイズが変わる。グリッドは起動時の
80×24 のまま動かない。端末エミュレータ流に桁数を変える案は採らなかった:

- `wintty.c:208` の `winch()` は `#if defined(SIGWINCH) && defined(CLIPPING)`
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

## 11. 1.1.5 からの移植で変わったこと

`win/tty/sdlterm.c` の**既存のコードは 3 行**しか変わっていない
（`#include` 1 行と、termcap 変数への代入 2 行。§11-1）。
足したのは Windows 用の `g_putch()` だけで、これは 3.4 が
`wintty.c` の同関数を `#ifndef WIN32` で囲んだことへの対応である（§11-3）。

**セルグリッドという考え方が、上流の 2 バージョン分の変更を素通りした。**
これがこの移植のいちばんの結果である。

### 11-1. termcap 変数の改名

3.4.3 は `CM` / `ND` / `CD` / `HI` / `HE` / `US` / `UE` を
`nh_CM` … `nh_UE` に改名した（`include/tcap.h`。curses.h の同名マクロと
ぶつかるため）。ヘッダも `termcap.h` → `tcap.h` になっている。
`sdlterm.c` の変更はこれだけ:

```c
-#include "termcap.h"
+#include "tcap.h"
-    CM = sdl_capname;
-    ND = CD = sdl_capname;
-    HI = HE = US = UE = sdl_capname;
+    nh_CM = sdl_capname;
+    nh_ND = nh_CD = sdl_capname;
+    nh_HI = nh_HE = nh_US = nh_UE = sdl_capname;
```

`struct tc_lcl_data` の中身は同じ 8 個なので、初期化子はそのまま通る。
`termcap.c` が持つ関数の一覧は 3.2.3 と 3.4.3 で**完全に一致**していて、
差し替えの前提は変わっていない。

### 11-2. 3.2 の不具合が 3.4 で直っていた

1.1.5 の移植で足した 3 つの修正は、3.4.3 には要らなかった:

| 1.1.5 で足したもの | 3.4.3 では |
|---|---|
| `allmain.c`: `program_state.something_worth_saving = 1` | **上流が直している**（`allmain.c:569` と `restore.c:711`）。窓を閉じたときのセーブは素で動く |
| `decl.h` / `save.c`: `done_hup` と `hangup()` を `SDL_GRAPHICS` でも有効に | **不要**。3.4.3 は既に `WIN32` を条件に含めている |
| `wintty.c` の `dmore()` の二重評価（§5-1） | **発火しない**。`jputchar()` になっている |

### 11-3. Windows 側で増えたもの

3.4.3 の `wintty.c` は `g_putch()` を `#ifndef WIN32` で囲む ——
Windows では移植側が用意する前提で、コンソール版は `sys/winnt/nttty.c` が
出している。この移植は `nttty.c` をコンパイルしないので、
`sdlterm.c` に `WIN32` 用の `g_putch()` を置いた。
中身は `wintty.c` 版の `ASCIIGRAPH && !NO_TERMS` の枝から PC9800 を
抜いたもので、**2 つのビルドが同じバイトを同じ場所へ送る**ようにしてある。
それが `test/wincompare.sh` が確かめている等価性そのものだからである。

同じ理由で 2 つ:

- `include/ntconf.h` の `USER_SOUNDS` を SDL では定義しない。
  `play_usersound()` は `nttty.c` にあり、Unix 側の SDL ビルドにも音は無い
- `sys/winnt/winnt.c` の `error()` を SDL では定義しない。
  `msmsg()`（`nttty.c`）を呼ぶうえ、GUI サブシステムのバイナリには
  書き出す先のコンソールが無い。`sdlterm.c` 版がメッセージボックスを出す

### 11-4. `_WIN32` の自動検出の位置

1.1.5 では `include/config.h` に置いた `_WIN32` → `WIN32` の橋渡しは、
3.4.3 では `include/config1.h` の「Windows NT Autodetection」の節に移した。
3.4.3 がその判定をそこに集めているためで、`config.h` は `config1.h` を
読むだけになっている。

---

## 12. やっていないこと

- **実 IME での日本語入力**（§4）
- **タイル描画** — セルにビットマップを置く拡張は自明だが手を付けていない
- **macOS** — 手を付けていない
- **Windows** — 移植した。[`SDL-WINDOWS.md`](SDL-WINDOWS.md) を参照。
  MinGW-w64 クロスビルド + wine で検証済み。
  `-u 日本語名` は未対応（設定ファイルの `OPTIONS=name:` を使う）。
  **実機 Windows での再確認はしていない**（1.1.5 では確認済み）
- **Windows と Linux は違うダンジョンを作る** — 乱数を分けたので設計どおり
  （Linux は glibc の `random()`、Windows は `sys/share/random.c`）。
  ただし揃えていたときも一部のシードで食い違っていて、そちらの原因は
  未特定のままである。[`SDL-WINDOWS.md`](SDL-WINDOWS.md) §7-3
- **X11 / Qt / Gnome ポート** — 3.4.3 の `config.h` は最初から tty だけを
  有効にしているので、この移植は何もしていない（1.1.5 では X11 と GTK を
  無効化する必要があった）
- **音** — `tty_nhbell()` はウィンドウのフラッシュで代用
- `NH_SDL_KEYS` / `NH_SDL_DUMP` / `NH_SDL_WIDTHTEST` はテスト用フックである。
  とくに `NH_SDL_KEYS` はスクリプト終端で `_exit(0)` し、
  `sys/unix/unixunix.c` の自動リロール防止チェックを無効にする前提で動く
- **乱数の種は固定できない。** ゲーム側に手を入れないため、`setrandom()` は
  素のまま時刻から種を取る。したがって 2 回の実行は別のダンジョンになり、
  `test/compare.sh` で比較できるのは地図・ステータス行・振り値・初期持ち物が
  写らない画面だけである（§7）
