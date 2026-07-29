# 素の NetHack 3.2.3 SDL ポーティング 実証実験 結果報告

- 計画書: [`SDL-POC-PLAN.md`](SDL-POC-PLAN.md)
- 対象: **NetHack 3.2.3**（base commit `1aabe8c`）
- 実施日: 2026-07-30
- 環境: Linux 6.8 / GCC 14 / SDL2 2.30.0 / SDL2_ttf 2.22.0

---

## 0. 結論

**4 つの仮説はすべて成立した。**

| # | 仮説 | 結果 | 実測値 |
|---|---|---|---|
| **H1** | `wintty.c` / `topl.c` / `getline.c` は無改造で済む | **成立** | diff **0 行**（3 ファイルとも base とバイト単位で一致） |
| **H2** | 1,500〜2,500 行に収まる | **成立** | `sdlterm.c` **1,703 行** + `sdlterm.h` 63 行 = **1,766 行** |
| **H3** | セルグリッドで East Asian Ambiguous 幅問題が消える | **成立** | A4 の 3 ケースすべて合格。`LANG` を 5 種類変えても描画不変 |
| **H4** | 入力フックは `#define tgetch` 一箇所 | **成立** | `include/unixconf.h` の 1 行のみ |

**JNetHack への含意**: H3 が確認できたので、`UTF8-PLAN.md` §4.1 の Ambiguous 幅対策
**A（端末設定を利用者に強いる）と C（wcwidth フォールバック）は SDL 経路では不要**。
同 §フェーズ4（Unicode マップ描画）も SDL 側でコードポイントを直接描けるため不要になる。

---

## 1. 成果物

| ファイル | 行数 | 内容 |
|---|---|---|
| `win/tty/sdlterm.c` | 1,703 | **差し替えた backend**。`termcap.c` (1,110 行) と同じ接続面を実装 |
| `include/sdlterm.h` | 63 | stdio リダイレクトマクロと backend API の宣言 |
| `test/build.sh` | — | tty 版と SDL 版を 1 コマンドでビルド |
| `test/mkplaydir.sh` | — | 使い捨て HACKDIR の生成 |
| `test/ptydrive.py` | — | pty + VT100 エミュレータで tty 版の画面を取得（A3 の基準側） |
| `test/a3_compare.sh` | — | A3 画面一致テスト（17 ケース） |
| `test/walls_compare.sh` | — | 罫線の壁の検証（§13） |

ビルド:

```sh
./test/build.sh            # src/nethack.tty と src/nethack.sdl を生成
./test/mkplaydir.sh ~/nhdir
HACKDIR=~/nhdir ./src/nethack.sdl -u あなたの名前
```

---

## 2. A1: 無改造性の証明（H1）

```sh
$ git diff --stat 1aabe8c -- win/tty/wintty.c win/tty/topl.c win/tty/getline.c
$                       # 出力なし = 0 行
```

**合格（0 行）。**

### どうやって putchar 直呼びを回避したか

計画書 §3 が指摘したとおり、この 3 ファイルには backend を経由しない stdio 直呼びがある。
実測すると 35 箇所あった。

| 呼び出し | wintty.c | topl.c | getline.c | 計 |
|---|---|---|---|---|
| `putchar()` | 21 | 1 | 0 | **22** |
| `fflush()` | 9 | 0 | 1 | **10** |
| `puts()` | 2 | 0 | 0 | **2** |
| `fputs()` | 1 | 0 | 0 | **1** |
| | | | | **35** |

これを `include/sdlterm.h` の **4 個のマクロ**でまとめて backend に向けた。
`wintty.h` が `SDL_GRAPHICS` のときだけこのヘッダを読み込む。
3 ファイルはいずれも `hack.h`（＝`<stdio.h>`）の**後**に `wintty.h` を読むので、
マクロが確実に勝つ。

```c
#define putchar(c)  (sdl_putbyte((int)(unsigned char)(c)), (int)(c))
#define puts(s)     (sdl_puts(s), 0)
#define fputs(s,f)  (sdl_fputs(s,f), 0)
#define fflush(f)   sdl_fflush(f)
```

**この手法は 3.2.3 自身が既に使っている。** `include/wintty.h:236-242` に
MSDOS / WIN32CON 向けの `#define putchar(x) xputc(x)` が元からある。
新しい発明ではなく、既存の慣習に倣っただけ。

> **JNetHack への還元（計画書 §8-2 の修正）**
> 計画書は「24 → 90 箇所のリダイレクトが必要」と見積もっていたが、
> **箇所数は作業量に比例しない**。マクロ 4 個で 35 箇所を一度に処理できたので、
> JNetHack の 90 箇所（`jlib.c` の 30 箇所を含む）も同じ 4 マクロで足りるはず。
> 増えるのは「どの関数を経由するか」ではなく「バイト列を文字に再構成する層」（§8-3）だけ。

---

## 3. A2: 規模の計測（H2）

| | 行数 |
|---|---|
| `win/tty/sdlterm.c` | **1,703** |
| `include/sdlterm.h` | 63 |
| **合計** | **1,766** |
| （参考）`win/tty/termcap.c` | 1,110 |
| （参考）`win/X11/*` 新規ウィンドウポート | 8,520 |

**合格（2,500 行以内）。** 当初は 1,501 行で仮説の下限にほぼ一致し、
その後 §13 の罫線描画を足して 1,766 行になった。

内訳（関数本体の実測。合計 1,158 行。残りはコメント・宣言・文字集合テーブル・空行）:

| 区分 | 行数 | 内訳 |
|---|---|---|
| 計画書 §5 の接続面（25 関数） | **292** | `tty_startup` 70 が最大。他は平均 9 行 |
| 出力の中核（`sdl_putbyte` / `sdl_putcp` ほか） | 115 | `sdl_putcp` 70 が制御文字・折返し・2 セル幅を全部見る |
| セルグリッド管理・スクロール・リサイズ | 124 | `sdl_resize` 50 を含む |
| フォント読込・グリフキャッシュ | 108 | SDL_ttf のテクスチャキャッシュ |
| セル描画 | 81 | `sdl_draw_cell` / `sdl_repaint` |
| **罫線の自前描画**（§13） | 94 | `sdl_draw_box` / `sdl_arc` / `sdl_pen` |
| 入力（キーマップ・FIFO・イベント） | 115 | |
| 幅判定 `sdl_cp_width()` | **24** | UAX #11 の W/F 範囲。**これだけで H3 が成立する** |
| その他（`sdl_die`） | 15 | |
| 実験用フック（A3 ダンプ / A4 テスト / スクリーンショット） | **179** | **本番では不要**。`sdl_width_test` 124 が大半 |

このほか文字集合テーブル（CP437 の上下半分と VT100 特殊図形）が約 60 行ある。

**実験用フックを除けば約 1,530 行。**

---

## 4. A3: 画面一致テスト ★中核

同一シード（`NETHACK_SEED=20260729`）・同一キー列で、
tty 版（pty + VT100 エミュレータ）と SDL 版（`NH_SDL_DUMP` のセル配列ダンプ）を diff。

```sh
$ ./test/a3_compare.sh
A3 startup: PASS          A3 helpmenu: PASS
A3 map: PASS              A3 guidebook: PASS
A3 inventory: PASS        A3 overview_page2: PASS
A3 redraw: PASS           A3 extcmd_echo: PASS
A3 menu_page: PASS        A3 options: PASS
A3 kickprompt: PASS       A3 escape_menu: PASS
A3 spellmsg: PASS         A3 walkabout: PASS
A3 topline_more: PASS     A3 quit: PASS
                          A3 endgame: PASS
```

**17 ケースすべて diff 0。合格。**

| ケース | キー列 | 何を見ているか |
|---|---|---|
| `startup` | `n V y` | 起動画面、`--More--` の位置 |
| `map` | `n V y ␣ ␣` | 部屋の罫線、`@`、ステータス行 |
| `inventory` | `… i` | 持ち物ウィンドウの重ね描き（反転属性つき見出し） |
| `redraw` | `… ^R` | `cl_eos` / `clear_screen` の正しさ |
| `menu_page` | `… i ␣` | メニューのページ送り |
| `topline_more` | `n V y ␣` | メッセージ行の `--More--` |
| `guidebook` | `… ? a` | 全画面テキストウィンドウ + `--More--` |
| `overview_page2` | `… ? a ␣ ␣` | テキストウィンドウの 2 ページ目 |
| `extcmd_echo` | `… # w i z a r d` | `getline.c` のエコー・補完（`# wipeizard` と表示される挙動まで一致） |
| `options` | `… O` | オプションメニュー全体 |
| `escape_menu` | `… i ESC` | メニュー取消後の復元描画 |
| `walkabout` | `… l l l j j h k` | 移動に伴う差分描画 |
| `quit` | `… Q y n` | 終了確認と「You quit…」画面 |
| `endgame` | `… Q y n ␣ ␣` | 最終スコアボードまで |

> 計画書 §7 A3 は `#quit` を挙げているが、**3.2.3 に `#quit` は存在しない**
> （`extcmdlist[]` に無い。追加は 3.3 以降）。終了は `Q` に割り当てられている。
> なお 3.2.3 の拡張コマンドは前方一致補完をするので、`#` の後に `quit` と
> 打つと `q` が `quit` に補完されたうえで `uit` が続き `quituit` になる。
> `extcmd_echo` ケース（`# wizard` → `# wipeizard`）はこの挙動が
> 両 backend で一致することを確認している。

一致例（`map`、両者バイト単位で同一）:

```
Unknown command ' '.

                                                     -+--------------
                                                     |?.............|
                                                     |..............|
                                                     |..........d...|
                                                     ..k.......@....|
                                                     ----------------

Poc the Stripling           St:16 Dx:13 Co:18 In:10 Wi:7 Ch:11  Neutral
Dlvl:1  $:0  HP:16(16) Pw:2(2) AC:6  Exp:1
```

### 比較の条件

- 行末の空白は両者とも除去して比較する。端末とセルグリッドは「書かれていないセルが
  存在するか」で認識が食い違うが、画面上の見た目はどちらでも同じ。
- `NETHACKOPTIONS=color,!DECgraphics` を両者に与える。SDL 版は `color` と
  `DECgraphics` の既定値を変えている（§7 / §13）ため、揃えないと
  オプション画面と壁の見た目が食い違う。**A3 は「レイアウトが一致するか」を
  見るテストなので、記号集合は両者で揃える**のが正しい。
  罫線そのものの検証は `test/walls_compare.sh`（§13）が受け持つ。

### 実機での対話確認（A3 は自動、こちらは実ウィンドウ）

A3 はスクリプト投入（`NH_SDL_KEYS`）でキューを直接埋めるので、SDL のイベント経路
そのものは通らない。そこで X11 の XTEST で**本物のキーイベント**を送って確認した。

| 項目 | 結果 |
|---|---|
| 通常キー（`SDL_TEXTINPUT` 経由） | 起動 → 職業選択 → 移動 → `i` → `ESC` まで動作 |
| 矢印キー（`SDL_KEYDOWN` 経由） | `←↑→↓` が `hkjl` に変換され移動できる |
| Ctrl 系 | `^R` で再描画 |
| 色 | ドア `+` が茶、コボルト `k` が橙、`@` が白で描画される |
| ウィンドウリサイズ | 720×648 → 1100×700 でグリッドが 80×24 → 122×25 に追従し、内容を保ったまま再レイアウトされる |

`SDL_KEYDOWN` では `SDL_TEXTINPUT` を伴わないキー（ESC / Return / BS / Tab /
矢印 / テンキー / Ctrl 系）だけを処理している。両方を拾うと通常の文字が
二重に入力されるため。

---

## 5. A4: 2 セル幅描画テスト（H3）★JNetHack にとって最重要

`NH_SDL_WIDTHTEST=1` で起動すると、ゲームの代わりに専用テストが走る。

```
A4: double-width cell test (LANG=ja_JP.UTF-8)
    cell = 9x27 px, grid = 80x24
    case 1 (occupancy, no drift): PASS
    case 2 (half overwrite, cl_end): PASS
    case 3 (locale independence): PASS
A4: PASS (0 failures)
```

**3 ケースすべて合格。**

### case 1: 占有と桁ずれ

`[日本語漢字]END` を桁 0 から書き、セル配列を直接検証:

- 桁 0 = `[`
- 桁 1,3,5,7,9 = 各漢字の**左半分**、桁 2,4,6,8,10 = **右半分**
- 桁 11 = `]`、桁 12-14 = `END` — **2 セル幅グリフ 5 個の後で桁がずれない**

目視確認（`|` の間に漢字 5 個、下段は桁定規）:

```
A4: |日本語漢字|<- columns 1..10
....+....1....+....2
ASCII row for comparison
```

スクリーンショットで、閉じ `|` が定規の `+`（桁 14）の直後、
つまり桁 15 に正確に落ちることを確認済み。

### case 2: 半分の上書き

- 2 セル幅グリフの**右半分**に 1 セル文字 `X` を書く
  → `X` が桁を占有し、**孤立した左半分が残らない**（左半分は空白化される）
- 同じことを `cl_end()` 経由でも検証
  → `cl_end` は行末までを消し、境界にまたがるグリフの相方も一緒に消す

これは `sdl_clear_cell()` が「片割れも一緒に消す」よう実装してあることの検証。
セルグリッドを持たない端末では、この操作が桁ずれの主要な発生源になる。

### case 3: ロケール非依存

`LANG` を `C` / `en_US.UTF-8` / `ja_JP.UTF-8` / `ja_JP.eucJP` / `zh_CN.UTF-8`
に切り替えても、セル配列の幅割り当てが 1 ビットも変わらないことを確認。
さらにプロセス全体の `LANG` を変えて 3 回起動しても結果は同一。

**理由は設計そのものにある。** 幅は `sdl_cp_width()` という
**24 行の純関数**が決める。UAX #11 の Wide / Fullwidth 範囲だけを 2 とし、
Ambiguous は 1 として扱う。`wcwidth()` も `setlocale()` も端末も一切参照しない。

```c
static int
sdl_cp_width(cp)
long cp;
{
    if (cp < 0x1100L) return 1;
    if ((cp >= 0x3041L && cp <= 0x33FFL) ||   /* かな・ハングル・CJK互換 */
        (cp >= 0x4E00L && cp <= 0x9FFFL) ||   /* CJK統合漢字 */
        ...) return 2;
    return 1;
}
```

### `graph_on()` / `graph_off()` について

計画書 §5 は **no-op** を予想していた。実験の最初の版はそのとおりだったが、
§13 で罫線の壁を実装した際に **「次のバイトをどの表で読むか」を記録するだけの
2 行**になった。フォントの切り替えは依然として発生しない。

端末では、罫線を出すために代替文字セットへ切り替え、
**その結果の幅について端末と合意できていることを祈る**必要がある。
罫線文字（U+2500〜257F）は UAX #11 で **Ambiguous** に分類されており、
まさに幅がロケール依存になる領域である。
セルグリッドでは幅は結果のコードポイントの性質でしかなく、
`sdl_cp_width()` が 1 と決めるので、この曖昧さは最初から発生しない。

つまり計画書の予想は「やることが無い」という点では外れたが、
**「幅の曖昧さが消える」という H3 の主張はむしろ補強された**。

---

## 6. A5: 移植性の記録

§2 の表のとおり、リダイレクトが必要だったのは 3 ファイル中 **35 箇所**
（`putchar` 22 / `fflush` 10 / `puts` 2 / `fputs` 1）で、
これを **マクロ 4 個**で処理した。`termcap.c` にあった `putchar` 2 箇所は
ファイルごと消えたので数に入らない。

JNetHack 側は `putchar` 相当が 90 箇所あるが、**同じ 4 マクロで覆えるはず**である。
比率（24 : 90）が作業量の比率になる、という計画書の想定は**成立しない**
——箇所数ではなくヘッダ 1 枚で決まる。

ただし JNetHack で**本当に新規になる**のは計画書 §8-3 の
「バイト列 → 文字の再構成」で、こちらは今回の実験には対応物が無い。
素の 3.2.3 は「バイト = 文字 = 桁」なので `xputc()` が 1 バイト受け取れば
そのままコードポイントになるが、EUC-JP では 2 バイトを溜めてから
1 グリフにする層が要る。**`sdlterm.c` はこれを受け入れる形になっている**:
セルは `long ch`（コードポイント）を保持し、`sdl_putcp(int cp)` が
コードポイント単位の入口として既に分離してある。`xputc()` は
`sdl_putcp()` の 1 バイト用ラッパにすぎない。

---

## 7. 計画書からの変更点と、実装中に判明したこと

### R-2 の解決: `wintty.c` は termcap 内部変数を参照していた

計画書のリスク R-2「`wintty.c` が `AS`/`AE`/`CO`/`LI` を直接参照していないか」は
**参照していた**が、影響は軽微だった。

| 変数 | 参照箇所 | 対応 |
|---|---|---|
| `CO` / `LI` | wintty.c 16 箇所、topl.c 6 箇所 | `decl.c` の `tc_gbl_data` にあり、backend が値を入れるだけ |
| `CM` | wintty.c `tty_curs()` 2 箇所 | **非 NULL にする必要がある**。NULL だと「カーソル番地指定不可の端末」経路に落ちる。中身は読まれないのでダミー文字列を入れた |
| `ul_hack` | wintty.c `tty_print_glyph()` | `FALSE` を入れる |
| `AS` / `AE` | wintty.c からの参照は**なし** | NULL のままでよい |

`tc_lcl_data` は `sdlterm.c` 側で定義した（`termcap.c` の代わり）。

### R-3 の解決: 「Don't use xputs()」コメントは無害だった

`wintty.c:964` のコメントは、`tputs()` が `*` をパディング指定と誤解する
という termcap 固有の事情を述べたもので、**性能上の理由で `putchar` を直接使う**
という話にすぎなかった。`putchar` → `xputc` のリダイレクトで意味は完全に保存される。

### 追加で必要になった変更（計画書に無かったもの）

いずれも保護対象 3 ファイルの外側。

| ファイル | 変更 | 理由 |
|---|---|---|
| `include/wintty.h` | `SDL_GRAPHICS` のとき `sdlterm.h` を include（6 行） | stdio リダイレクトと `<signal.h>`。**既存の MSDOS ブロックの真下**に置いた |
| `include/unixconf.h` | `#define tgetch sdl_getch`（H4） | 入力フック |
| `sys/share/unixtty.c` | `gettty`/`settty`/`setftty`/`intron`/`introff` を SDL では no-op に | **SDL 版は起動元の端末を display として使わない。** 端末モードを触るとパイプ経由の起動で `Inappropriate ioctl for device` になり、また利用者の端末設定を壊す。`erase_char`/`kill_char` だけは `getline.c` が使うので慣例値を入れる |
| `sys/share/ioctl.c` | `getwindowsz()` から `get_scr_size()` を呼ぶ | SYSV では `getwindowsz()` が空実装。ウィンドウリサイズの伝達路 |
| `sys/unix/unixunix.c` | `isatty(0)` チェックを SDL では省略 | このチェックは「fd 0 が端末か」を見るが、SDL 版の入力は fd 0 から来ない |
| `src/options.c` | `color` の既定を SDL では TRUE に / `getenv("TERM")` の NULL 参照を回避 | 「端末がモノクロかもしれない」という但し書きが SDL には当てはまらない。また `TERM` 未設定で起動できるべき |
| `src/hacklib.c` | `NETHACK_SEED` 環境変数で RNG を固定 | **A3 のためのテスト用フック。** 同一ダンジョンでないと画面比較ができない |

### コンパイル警告

`sdlterm.c` は `-Wall` で警告 0 件（`config.h` 由来の `-Wcomment` を除く）。

### `<signal.h>` の落とし穴（SIGWINCH）

`wintty.c` は `<signal.h>` を **BSD 系でしか** include しない:

```c
#ifdef CLIPPING		/* might want SIGWINCH */
# if defined(BSD) || defined(ULTRIX) || defined(AIX_31) || defined(_BULL_SOURCE)
#include <signal.h>
# endif
#endif
```

今回は `SYSV` 構成なので `SIGWINCH` が未定義になり、
`winch()` ハンドラごとコンパイルから消えていた。
つまり**ウィンドウをリサイズしても何も起きない**。

`wintty.c` は無改造で通したいので、`wintty.h`（＝`sdlterm.h`）から
`<signal.h>` を include して解決した。`wintty.c` は
この `#if` を評価する**前**に `wintty.h` を読むので、これで足りる。

あわせて `sdl_resize()` では `CO`/`LI` を**先に書き換えない**ようにした。
`winch()` は「保存 → `getwindowsz()` → 変化したか」で判定するので、
先に書き換えると「何も変わっていない」と判断されて再レイアウトが走らない。
新しいサイズは `get_scr_size()` から公開する。
（`get_scr_size()` が計画書 §5 の一覧に入っていた理由がここで判明した。）

リサイズ時はセル配列の内容を可能な範囲で引き継ぐ。引き継がないと、
次の再描画までウィンドウが真っ黒のままになる。

---

## 8. ビルド基盤（P0）で実際に必要だったこと

計画書 §2 の 5 点に加えて、2 点が必要だった。

| # | 変更 | 状態 |
|---|---|---|
| 1 | `unixconf.h`: `BSD`/`SUNOS4` → `SYSV`+`LINUX` | 計画どおり |
| 2 | `CFLAGS` に `-std=gnu89` | 計画どおり。**5 箇所を直しても外せない**（3.2.3 は全体が K&R 形式の関数定義で、C23 では文法エラーになる） |
| 3 | `CFLAGS` に `-fcommon` | 計画どおり |
| 4 | `WINTTYLIB` → `-lncurses` | 計画どおり |
| 5 | `-Wno-error=` 群 → **5 箇所のソース修正** | 計画どおり。予測どおり正確に 5 箇所だった |
| 6 | `sys/share/*_lex.c` の 4 行修正 | **新規**。yacc/lex が無い環境向けの生成済みパーサが glibc で通らない |
| 7 | `dat/` の再生成 | **新規**。`TEXTCOLOR` を有効化すると `VERSION_FEATURES` が変わり、既存の `.lev` が無効になる |
| 8 | `COMPRESS` を `/usr/ucb/compress` → `/usr/bin/gzip` | **新規**。存在しないプログラムを指しているのでセーブが `Exec to compress ... failed` で失敗する |

### 5 箇所（計画書の予測と完全一致）

```
src/dogmove.c:711   register ndist;          -> register int ndist;
src/hacklib.c:297   register dx = ..., dy    -> register int dx = ..., dy
src/sp_lev.c:1349   register i;              -> register int i;
src/topten.c:280    yyyymmdd() の暗黙宣言    -> include/extern.h に E long FDECL(yyyymmdd,(time_t));
```

修正後、`-Wall` ビルドで `-Wimplicit-int` / `-Wimplicit-function-declaration` は **0 件**。

### 6: 生成済み lex パーサ

`yacc` / `lex` / `bison` / `flex` がインストールされていない環境向けに、
3.2.3 は `sys/share/` に生成済みの `lev_lex.c` / `lev_yacc.c` / `dgn_*.c` を同梱している。
これが glibc で 2 種類の理由により通らなかった:

```c
FILE *yyin = {stdin}, *yyout = {stdout};   /* glibc の stdin は定数式ではない */
yyunput(c) int c; { ... }                  /* 戻り値型が宣言と矛盾 */
```

`init_yyin()` / `init_yyout()` が最初の使用前に必ず呼ばれるので、
`yyin`/`yyout` を 0 で初期化し、`yyinput`/`yyoutput`/`yyunput` に戻り値型を付けた（計 4 行 × 2 ファイル）。

### 7: `VERSION_FEATURES`

`TEXTCOLOR` を有効にすると `include/date.h` の `VERSION_FEATURES` が
`0x005c04c6` → `0x005e04c6` に変わる。`lev_comp` / `dgn_comp` はこの値を
生成データに焼き込むので、**config.h の機能を変えたらデータも作り直す**必要がある。
生成済みパーサを使っているとこの依存関係が Makefile から見えないため、
`test/build.sh` はユーティリティとデータを毎回作り直す。

`SDL_GRAPHICS` は `makedefs.c` の機能ビットに含まれないので、
**tty 版と SDL 版は同じ `dat/` を共有できる**（A3 の前提）。

### 8: セーブの圧縮プログラム

`include/config.h` の既定は `COMPRESS "/usr/ucb/compress"` で、これは
現代の Linux には存在しない。この状態でセーブすると

```
Exec to compress save/1000poc failed.
```

となり、セーブファイルが使えなくなる。`/usr/bin/gzip` に変更した。
変更後、`S` によるセーブと再起動時のリストアが両方とも動作することを確認済み
（`save/1000poc.gz` が生成され、次回起動でマップ・持ち物とも復元される）。

これは backend に依存しない問題で、tty 版も同じ影響を受ける。

---

## 9. 未決事項への回答

| 計画書の未決事項 | 結論 |
|---|---|
| SDL2 / SDL3 | **SDL2** を採用。`SDL_StartTextInput` / `SDL_TEXTINPUT` を使用。IME 対応（§8-5）でも SDL2 の実績が効く |
| SDL_ttf かビットマップフォントか | **SDL_ttf**。CJK をビットマップで内蔵するのは非現実的。フォントは `NETHACK_SDL_FONT` で差し替え可能 |
| 成果物を素の 3.2.3 に残すか JNetHack に取り込むか | **素の 3.2.3 のまま `sdl` ブランチに残す**ことを推奨。JNetHack への移植は §8 の作業として別に行い、この実験を回帰の基準として保つ |

### R-4（フォント選定）の結論

`Noto Sans Mono CJK JP`（`NotoSansCJK-Regular.ttc` の face index 5）を既定にした。
ASCII の advance が 9px、CJK が 18px で **正確に 1:2**。
これが満たされない場合に備え、セル幅はあくまで ASCII の advance から決め、
グリフがセル幅を超える場合は次の桁へはみ出させずに縮める。
**レイアウトはフォントではなくグリッドが決める。**

候補は `sdlterm.c` の `font_candidates[]` に順に並べてあり、
`NETHACK_SDL_FONT=/path/to/font.ttf[:index]` で上書きできる。

---

## 10. 環境変数一覧

| 変数 | 用途 |
|---|---|
| `NETHACK_SDL_FONT` | フォント指定（`path` または `path:face_index`） |
| `NETHACK_SDL_FONTSIZE` | ポイントサイズ（既定 18） |
| `NETHACK_SDL_COLS` / `NETHACK_SDL_ROWS` | 起動時のグリッドサイズ（既定 80×24） |
| `NETHACK_SEED` | RNG を固定（A3 用） |
| `NH_SDL_DUMP` | セル配列を UTF-8 テキストで書き出す（A3 用） |
| `NH_SDL_KEYS` | キー列をスクリプト投入（A3 用、ヘッドレス実行可） |
| `NH_SDL_SHOT` | 描画のたびに BMP を保存 |
| `NH_SDL_WIDTHTEST` | ゲームの代わりに A4 テストを実行 |
| `NH_SDL_DEBUG` | リサイズ関連の診断を stderr へ |

---

## 11. やっていないこと（計画書 §1 のとおり）

- **日本語の表示・入力** — 素の 3.2.3 に日本語は無い。A4 はダミー漢字での幅検証のみ
- **タイル描画** — セルにビットマップを置く拡張は SDL 化後には自明
- **Windows / macOS** — Linux + SDL2 のみ
- **音** — `tty_nhbell()` はウィンドウのフラッシュで代用。音声デバイスは開いていない

また、以下は実験として意図的に割り切った:

- `NETHACK_SEED` / `NH_SDL_KEYS` / `NH_SDL_DUMP` はテスト用フックであり、
  本番の JNetHack に持ち込むかは別途判断すべき。とくに `NH_SDL_KEYS` は
  `sys/unix/unixunix.c` の「自動リロール防止」チェックを無効化する前提で動く
- `sdl_getch()` はスクリプト終端で `_exit(0)` する。テスト専用の経路

---

## 12. JNetHack への還元計画（実験結果を踏まえた改訂）

| # | 作業 | 実験を踏まえた評価 |
|---|---|---|
| 1 | `sdlterm.c` の移植 | **ほぼそのまま流用可**。セルが `long ch`（コードポイント）を持つ設計なので、2 バイト文字を入れる余地は既にある |
| 2 | `putchar` 直呼びのリダイレクト | **箇所数は問題にならない**。`sdlterm.h` の 4 マクロを JNetHack の `wintty.h` に置くだけ。`jlib.c` の 30 箇所も同様 |
| 3 | バイト列 → 文字の再構成 | **ここが唯一の新規作業。** `japanese/jlib.c:325 jbuffer()` にフックし、EUC 2 バイトを 1 コードポイントにして `sdl_putcp()` に渡す。`xputc()` は今もその 1 バイト用ラッパになっている |
| 4 | フォント | **解決済み**。`Noto Sans Mono CJK JP` 1 枚で ASCII 1 セル / 漢字 2 セルが成立する。2 枚構成は不要 |
| 5 | 入力の IME 対応 | `SDL_StartTextInput` は既に呼んである。`sdl_queue_text()` が UTF-8 を受け取る箇所で、Latin-1 に落とす代わりに EUC-JP へ変換すればよい |
| 6 | `<signal.h>` / `SIGWINCH` | JNetHack も `SYSV` 構成なら同じ罠を踏む（§7 参照） |

**`UTF8-PLAN.md` への影響:**

- **フェーズ4（Unicode マップ描画）は SDL 経路では不要** — セルにコードポイントを直接置ける
- **§4.1 の Ambiguous 幅対策 A / C は不要** — H3 が実証された
- **フェーズ1〜3（`kcode:U`）は引き続き価値がある** — SDL を使わない tty 利用者向け

---

## 13. 罫線の壁（角丸）

SDL 版は既定で **DECgraphics** を選び、壁を Unicode の罫線文字で描く。
四隅は角丸（U+256D〜U+2570 ╭ ╮ ╯ ╰）にしてある。

```
          ╭───╮
    @f####....│
    #     │..<│
          │...│
          +...│
          ╰───╯
```

壁は角丸罫線、通路は `#`、床は `.`。

### なぜセルグリッドだと素直にできるのか

port は backend に**バイト**を渡す。そのバイトが何の文字を指すかは
選ばれている記号集合しだいで、DECgraphics では
`graph_on()` と `graph_off()` に挟まれた 7 ビット文字が VT100 罫線を意味する。

端末はこれを「端末側が正しいコードページ／フォントに設定されていること」で
解決する。SDL 版は `sdl_putbyte()` で **Unicode に翻訳して**解決する。
翻訳先はこちらが決めるので、四隅に角丸のコードポイントを選ぶだけでよい。
**port はこの読み替えに一切気づかない**（`wintty.c` は無改造のまま）。

| 層 | 役割 |
|---|---|
| `sdl_putbyte(b)` | バイト → コードポイント。文字集合の解決はここだけ |
| `sdl_putcp(cp)` | コードポイントをセルに置く |

見た目は `dec_special[]`（VT100 特殊図形の変換表）の **5 エントリ**で決まる。
標準からわざと外してあるのはそこだけ:

| 入力 | 標準 | 本実装 | 理由 |
|---|---|---|---|
| `l` `k` `j` `m` | `┌ ┐ ┘ └` | `╭ ╮ ╯ ╰` | 角丸 |
| `~` | `·` | `.` | 床は中黒ではなくピリオド |

**ここに入っているものが、端末との差分のすべて**である。
`test/walls_compare.sh` は同じ読み替えを基準側に適用して比較するので、
この表に足し忘れるとテストが落ちる。

コードポイント段階で置き換える関数を挟む案も試したが、
`cp437_high[]`（IBMgraphics 用）にも同じ影響が及んでしまう。
**IBMgraphics は「PC コンソールならこう見える」という選択肢**として
コードページ 437 に忠実なまま（四角い角・中黒）にしたいので、
読み替えは DEC の表に直接入れてある。

この 3 層構造は §12-3（JNetHack の EUC 2 バイト → 1 グリフ再構成）が
入るべき場所をそのまま示している。**入口は `sdl_putbyte()` である。**

### 罫線はフォントではなく自前で描く

最初はフォントのグリフをそのまま使ったが、**縦線が行ごとに途切れた。**

フォントの罫線グリフはそのフォントの em 用に作られていて、
CJK フォントではセル高さのほうがずっと大きい（ここでは 27px 対 18px）。
その差がそのまま行境界の隙間になる。端末エミュレータも同じ問題を抱えていて、
同じ答えを出している——**自前で描く。**

`sdl_draw_box()` が `─ │ ┌ ┐ └ ┘ ├ ┤ ┬ ┴ ┼` と角丸 4 種を矩形と円弧で描く（94 行）。
副作用として**フォント依存が消えた**: U+256D〜U+2570 を持たない
IPAGothic でも角丸になる。

### 幅の話（H3 との関係）

罫線文字 U+2500〜257F は UAX #11 で **East Asian Ambiguous** である。
つまり端末では、ロケールしだいで 1 桁にも 2 桁にもなりうる文字集合そのもの。
セルグリッドでは `sdl_cp_width()` が 1 と決めるだけで、
`LANG` が何であろうと壁が桁をずらすことはない。

**この機能は H3 の実演になっている。**

### 検証: `test/walls_compare.sh`

termcap 版を同じ記号集合（DECgraphics）で走らせると、
文字集合の切り替えが本当に `ESC(0` と SO/SI として端末に出ていく。
これを **pyte が自前の VT100 表で解決した結果**と、
SDL 版のセルダンプを（角丸の読み替えを揃えたうえで）セル単位で比較する。

```
$ ./test/walls_compare.sh
walls: PASS (4 rounded corners, DEC mapping agrees with pyte)
```

`sdlterm.c` の `dec_special[]` とは独立した実装との突き合わせなので、
表の写し間違いが検出できる。

> pyte は既定（`use_utf8`）では `ESC(0` と SO/SI を無視するため、
> `test/ptydrive.py` はこれを落としている。落とさないと DECgraphics の壁が
> `lqkxmj` という生の文字のまま届く。

### 開いた扉だけは記号の段階で戻す

`dec_graphics[]` は **`S_vodoor` と `S_hodoor` の両方を同じバイト `0xe1`**
（VT100 の `a` = 網掛けブロック ▒）に割り当てている。

これは見た目の好みの問題ではなく**情報が落ちている**。
ASCII の `-` と `|` は扉の向きを示しているのに、DEC 表はそれを 1 つに潰す。
しかも backend に届くバイトが同一なので、**backend 側では区別のしようがない**。

そこで `switch_graphics()` の `DEC_GRAPHICS` 分岐で記号だけ戻している:

```c
assign_graphics(dec_graphics, SIZE(dec_graphics), MAXPCHARS, 0);
showsyms[S_vodoor] = defsyms[S_vodoor].sym;	/* '-' */
showsyms[S_hodoor] = defsyms[S_hodoor].sym;	/* '|' */
```

`showsyms[]` を直接書くのは NetHack 自身が
`assign_rogue_graphics()` でやっているのと同じ手口である。

**`switch_graphics()` の中でなければならない。** 最初は `initoptions()` の
呼び出し側に置いていたが、オプション画面で `DECgraphics` を切り替えると
`options.c:1352` から `switch_graphics()` が再入して `assign_graphics()` が
走り直し、上書きが消えて網掛けブロックに戻ってしまう。
記号表を書き換える場所は 1 つに限るべきで、呼び出し側ではない。

確認（実際に扉を開けて撮ったもの）:

```
縦壁の扉 → '-'              横壁の扉 → '|'
 │......@-                   ╭|─────────────╮
 │...<.d.│                   │:.............│
```

`dec_graphics[]` で複数の記号が 1 バイトに潰れている箇所はほかにもあるが、
**ASCII 側でも同じ文字なので情報は落ちていない**:

| バイト | 潰される記号 | ASCII |
|---|---|---|
| `0xfe` | `S_ndoor` / `S_room` / `S_ice` / 下がった跳ね橋 | すべて `.` |
| `0xe0` | `S_pool` / `S_lava` / `S_water` | すべて `}` |
| `0xe1` | `S_vodoor` / `S_hodoor` | **`-` と `|`** ← ここだけ |

なお `test/walls_compare.sh` はこの差を検証できない。
基準側（termcap 版）は ▒ のままで、記号段階の違いは
コードポイントの読み替えでは埋められないためである。
同スクリプトが使う固定シードは、扉が閉じたままの部屋に着くようにしてある。

### IBMgraphics ではなく DECgraphics を選んだ理由

`ibm_graphics[]`（コードページ 437）でも壁は罫線になるが、
**通路が網掛けブロック（░ ▒）になる**。
`dec_graphics[]` は通路と照明付き通路を `g_FILLER` のまま残すので `#` が保たれる。
床は `dec_graphics[]` では中黒だが、`dec_special[]` の `~` を `.` にして戻している。

CP437 の変換表も実装してあるので `NETHACKOPTIONS=IBMgraphics` でそちらも使える。
そちらは**コードページ 437 に忠実**で、角は四角く床は中黒になる。
素の見た目（`-` と `|`）に戻したい場合は `NETHACKOPTIONS=!DECgraphics`。

---

## 付録: 実装した 25 関数の実際の姿

行数は実測値（`{` から対応する `}` まで、シグネチャ込み）。

| 関数 | 実装 | 行数 |
|---|---|---|
| `tty_startup(wid,hgt)` | SDL/TTF 初期化、フォント、ウィンドウ、グリッド確保、`CO`/`LI` 設定 | 70 |
| `tty_shutdown()` | グリフキャッシュ解放、SDL 終了 | 14 |
| `tty_start_screen()` | **no-op**（`TI`/`VS` に相当するものが無い） | 5 |
| `tty_end_screen()` | `clear_screen()` + 再描画 | 5 |
| `cmov(x,y)` | カーソル変数の更新のみ | 18 |
| `nocmov(x,y)` | `cmov()` へ委譲（直接番地指定できるので区別不要） | 6 |
| `home()` | `cmov(0,0)` | 4 |
| `cl_end()` | カーソル行の右側を空白化（片割れグリフも掃除） | 11 |
| `cl_eos()` | 以降の全行を空白化 | 10 |
| `clear_screen()` | 全セル空白 + カーソル原点 | 10 |
| `backsp()` | カーソルを 1 桁戻す | 6 |
| `xputc(c)` | `sdl_putbyte()` の 1 バイト用ラッパ | 5 |
| `xputs(s)` | `sdl_putcp()` のループ | 7 |
| （`sdl_putbyte(b)`） | バイト → コードポイント。文字集合の解決はここだけ | 25 |
| （`sdl_putcp(cp)`） | **出力の本体。** 制御文字・遅延折返し・2 セル幅・スクロール | 70 |
| `standoutbeg()` / `standoutend()` | 反転フラグ | 各 4 |
| `term_start_attr(a)` / `term_end_attr(a)` | 属性フラグ（`ATR_ULINE`/`BOLD`/`BLINK`/`INVERSE`） | 各 11 |
| `term_start_raw_bold()` / `term_end_raw_bold()` | 太字フラグ | 各 4 |
| `term_start_color(c)` / `term_end_color()` | 前景色 | 5 / 4 |
| `has_color(c)` | **常に真**（パレットは自前） | 5 |
| `graph_on()` / `graph_off()` | 代替文字集合フラグの ON/OFF（フォントは切り替えない） | 各 4 |
| `tty_nhbell()` | ウィンドウフラッシュ | 13 |
| `tty_delay_output()` | `SDL_Delay(50)` + イベント処理 | 6 |
| `tty_number_pad(state)` | **no-op**（`iflags.num_pad` を直接読むため） | 7 |
| `tgetch()` → `sdl_getch()` | FIFO から 1 バイト | 28 |
| `get_scr_size()` | グリッド寸法を `CO`/`LI` に公開 | 7 |

`tty_decgraphics_termcap_fixup()` / `tty_ascgraphics_hilite_fixup()` /
`init_hilite()` / `kill_hilite()` は計画書の予想どおり**そもそも必要なかった**
（`TERMLIB` / `PC9800` 依存で、SDL 構成ではコンパイルされない）。
