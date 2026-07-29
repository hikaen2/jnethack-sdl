# 素の NetHack 3.2.3 SDL ポーティング 実証実験 計画書

- 対象: **NetHack 3.2.3**（commit `1aabe8c022a7a64a0ed9d7c462dce6f9893b8130` = `nethack-3.2.3.tgz`）
- 位置づけ: [`UTF8-PLAN.md`](UTF8-PLAN.md) フェーズ5（SDL バックエンド）の**実証実験**
- 作成日: 2026-07-29

---

## 1. 目的

JNetHack で SDL バックエンドに着手する前に、**日本語処理という変数を取り除いた素の NetHack 3.2.3 で先に同じことをやり、設計の妥当性を実測で確かめる。**

### 実証したい 4 つの仮説

| # | 仮説 | 検証方法 |
|---|---|---|
| **H1** | `win/tty/termcap.c` の差し替えだけで SDL 化でき、`wintty.c` / `topl.c` / `getline.c` は**無改造**で済む | 完成時にこの 3 ファイルの diff が 0 行であること |
| **H2** | 規模は **1,500〜2,500 行**に収まる（新規ウィンドウポートなら約 8,500 行） | `sdlterm.c` の行数を実測 |
| **H3** | セルグリッドを自前で持てば **East Asian Ambiguous 幅問題が消える**。2 セル幅の文字を桁ずれなく置ける | §7 の A4（ダミー漢字描画テスト） |
| **H4** | 入力フックは `#define tgetch` **一箇所**で足りる | 差分の実測 |

H3 が JNetHack にとって最重要。これが確認できれば、`UTF8-PLAN.md` §4.1 で挙げた Ambiguous 幅への対策 A（端末設定を利用者に強いる）と C（wcwidth フォールバック）が**両方とも不要**になる。

### この実験でやらないこと

- 日本語表示・入力（素の 3.2.3 に日本語は存在しない。H3 はダミー文字での幅検証のみ）
- タイル描画（セルにビットマップを置く拡張は SDL 化後に自明になるため、実証不要）
- Windows / macOS 対応（Linux + SDL2 のみ）

---

## 2. 検証済みの前提（2026-07-29 実施）

**素の 3.2.3 は現代環境でビルド・起動・プレイ可能であることを実測済み。**

### 実行結果

```
                                  Weapons
                                  a - a blessed +1 long sword (weapon in hand)
                                  b - a +0 dagger
                                  Armor
                                  c - an uncursed +3 small shield (being worn)
                                  Comestibles
                                  d - an uncursed food ration
                                  (end)

       ---+-----
       |.......+
       |.......|
       |.......|
       |...@....
       |..d....|
       ---------

Poc the Stripling           St:18/03 Dx:13 Co:16 In:9 Wi:8 Ch:8  Neutral
Dlvl:1  $:0  HP:16(16) Pw:2(2) AC:6  Exp:1
```

### ビルドに必要だった設定（ソース変更ゼロ）

| # | 変更 | 理由 |
|---|---|---|
| 1 | `include/unixconf.h`: `BSD`/`SUNOS4` を無効化し `SYSV`+`LINUX` を有効化 | **素の 3.2.3 の既定は BSD + SunOS 4.x**。そのままだと `srandom`/`qsort`/`strlen` の自前プロトタイプが glibc と衝突する。JNetHack はここに `__linux__` 自動判別を追加していたため問題が出なかった |
| 2 | `CFLAGS` に `-std=gnu89` | GCC 14 の既定は C23。暗黙の関数宣言がエラー |
| 3 | `CFLAGS` に `-fcommon` | GCC 10 以降 `-fno-common` が既定 |
| 4 | `WINTTYLIB = -ltermlib` → `-lncurses` | `libtermlib` は現存しない |
| 5 | `-Wno-error=implicit-int` ほか | GCC 14 が既定でエラー化した項目。**該当箇所は 5 つだけ**（後述） |

> JNetHack と違い、素の 3.2.3 は `include/config.h` が既定で tty のみ有効なので、
> ウィンドウシステムの絞り込み作業は不要だった。

### 警告の実測（`-Wall`、エラー 0 件）

```
5738  -Wcomment              ← config ヘッダの入れ子コメント /* #define X /* ... */
                                （JNetHack では修正済み。見た目のみ、無害）
  32  -Wdangling-else
  23  -Wchar-subscripts
  19  -Wmisleading-indentation
  18  -Wformat-overflow=
   6  -Wint-to-pointer-cast / 4 -Wpointer-to-int-cast
   4  -Wstringop-truncation
   4  -Wimplicit-int         ← -Wno-error= が必要だった実体（例: src/dogmove.c:711 register ndist;）
   2  -Wint-in-bool-context
   1  -Wparentheses / 1 -Wimplicit-function-declaration
```

`-Wcomment` を除けば実質 **114 件**。`-Wno-error=` 群は **5 箇所のソース修正**（`-Wimplicit-int` 4 + `-Wimplicit-function-declaration` 1）で不要にできる。

### 再現手順

```sh
git archive 1aabe8c | tar -x -C /path/to/nh323
cd /path/to/nh323
cp sys/unix/Makefile.top ./Makefile
cp sys/unix/Makefile.dat dat/Makefile
cp sys/unix/Makefile.doc doc/Makefile
cp sys/unix/Makefile.src src/Makefile
cp sys/unix/Makefile.utl util/Makefile
# 上記 1〜5 の設定を適用してから
make -C util && make -C src && make -C dat
HACKDIR=/path/to/playdir ./src/nethack -u poc
```

> `sys/unix/setup.sh` は cwd を見て動くため、リポジトリ直下から実行すると
> 意図しない場所に Makefile を生成する。上記のように直接 `cp` すること。

---

## 3. なぜ素の 3.2.3 で先にやるのか（JNetHack との差分の実測）

| 指標 | 素の 3.2.3 | JNetHack 1.1.5 | 差 |
|---|---|---|---|
| 8bit バイトを含むソース | **0 ファイル** | 99 ファイル | 文字コードの変数がゼロ |
| `win/tty/` 合計行数 | 4,030 行 | 4,334 行 | ほぼ同じ |
| `putchar`/`fputc` 直呼び | **24 箇所**<br>(wintty 21 / termcap 2 / topl 1) | **90 箇所**<br>(wintty 54 / topl 6 / jlib 30) | **1/4** |
| 出力バッファリング層 | なし | `jbuffer()`/`cbuffer()` が介在 | 素の方は直結で追いやすい |
| 漢字コード変換 | なし | `jlib.c` 749 行 | — |

**素の 3.2.3 は「バイト = 文字 = 桁」が完全に成立している最も単純な状態。** ここで SDL バックエンドの骨格を固めてから、JNetHack 側の「2 バイト = 1 文字 = 2 桁」を足す方が、問題の切り分けが確実にできる。

---

## 4. アーキテクチャ: 何を差し替えるか

### 方針

**新規ウィンドウポートは書かない。`win/tty/termcap.c` を `win/tty/sdlterm.c` に差し替える。**

```
      [変更しない]                          [差し替える]
  src/*.c  ──→  win/tty/wintty.c (2,335)  ──→  win/tty/termcap.c (1,110)  ──→ 端末
                win/tty/topl.c   (371)                  ↓
                win/tty/getline.c (214)         win/tty/sdlterm.c  ──→ SDL2 セルグリッド
```

比較のため: 新規ウィンドウポート（`struct window_procs` の約 40 関数）を書く場合、
X11 ポートの実測が **8,520 行**。

### 設計上の要点

1. **escape sequence を一切出さない。** SDL 側は 80×24 のセル配列を持ち、各プリミティブはその配列を更新するだけ。`tputs`/`tgoto`/`tgetent` の使用 17 箇所はすべて `termcap.c` 内なので、差し替えと同時に消える。
2. **`xputs` は平文しか受け取らない。** 能力文字列（`CE`/`CL`/`HO`/`SO` 等）を渡す呼び出しはすべて `termcap.c` 内。外部からの `xputs` は `wintty.c:498-503,776` の 5 箇所だけで、いずれも平文。
3. **入力は `#define` 一箇所。** `include/unixconf.h:249` に `#define tgetch getchar` とある。これを `sdl_getch()` に差し替えるだけでよい（H4）。
4. **画面サイズは `sys/share/ioctl.c` の `get_scr_size()`。** SDL 版では固定値かウィンドウサイズから算出する。

---

## 5. 実装すべき関数の完全リスト

`include/wintty.h:95-146` と `termcap.c` の非 static 関数から抽出した**全 25 個**。これがバックエンドの接続面のすべて。

### 画面・カーソル（11）

| 関数 | SDL での実装 |
|---|---|
| `tty_startup(wid,hgt)` | SDL 初期化、ウィンドウ生成、フォント読込、`LI`/`CO` 設定 |
| `tty_shutdown()` | SDL 終了 |
| `tty_start_screen()` / `tty_end_screen()` | 画面クリア（`TI`/`VS`/`VE`/`TE` は不要） |
| `cmov(x,y)` / `nocmov(x,y)` | カーソル変数の更新のみ |
| `home()` | `cmov(0,0)` |
| `cl_end()` | カーソル行の右側を空白で埋める |
| `cl_eos()` | 以降の全行を空白で埋める |
| `clear_screen()` | 全セルを空白に |
| `backsp()` | カーソルを 1 桁戻す |

### 出力（2）

| 関数 | SDL での実装 |
|---|---|
| `xputc(c)` | 現在位置のセルに文字と現在の属性を書き、カーソルを進める |
| `xputs(s)` | `xputc` のループ（平文のみ） |

### 属性・色（8）

| 関数 | SDL での実装 |
|---|---|
| `standoutbeg()` / `standoutend()` | 反転属性フラグの ON/OFF |
| `term_start_attr(attr)` / `term_end_attr(attr)` | 属性フラグ |
| `term_start_raw_bold()` / `term_end_raw_bold()` | 太字フラグ |
| `term_start_color(c)` / `term_end_color()` | 前景色の設定・解除 |
| `has_color(c)` | 常に TRUE |

### その他（4）

| 関数 | SDL での実装 |
|---|---|
| `graph_on()` / `graph_off()` | **セルグリッドでは不要**。代替文字セットの概念がないので no-op（ここが H3 の要） |
| `tty_nhbell()` | 効果音またはウィンドウフラッシュ |
| `tty_delay_output()` | `SDL_Delay` |
| `tty_number_pad(state)` | キーマップの切替 |

### 外部（2）

| 関数 | 場所 |
|---|---|
| `tgetch()` | `include/unixconf.h:249` の `#define` を差し替え |
| `get_scr_size()` | `sys/share/ioctl.c`。SDL 版を用意 |

> `tty_decgraphics_termcap_fixup()` / `tty_ascgraphics_hilite_fixup()` /
> `init_hilite()` / `kill_hilite()` は termcap 固有の細工なので SDL 版では不要（空実装）。

---

## 6. フェーズ構成

### P0: ビルド基盤の固定

- §2 の設定 1〜5 を Makefile / ヘッダに反映し、誰でも再現できる状態にする
- `-Wno-error=` 群を 5 箇所のソース修正に置き換える
- **受け入れ基準**: `make` 一発でビルドが通り、§2 の画面が再現する

### P1: 骨組み — 文字が出るまで

- `win/tty/sdlterm.c` を新規作成。SDL2 初期化、80×24 のセル配列、等幅フォント描画
- 実装: `tty_startup`, `tty_shutdown`, `xputc`, `xputs`, `cmov`, `nocmov`, `clear_screen`, `home`
- 残りは一旦スタブ（no-op）
- ビルドを `termcap.c` → `sdlterm.c` に切替（`src/Makefile` の `WINTTYSRC`）
- **受け入れ基準**: SDL ウィンドウが開き、タイトル画面の文字が読める

### P2: 画面制御一式 — マップが正しく描けるまで

- 実装: `cl_end`, `cl_eos`, `backsp`, `standoutbeg/end`, `term_start_attr`, `term_end_attr`, `term_start_raw_bold`, `term_end_raw_bold`, `tty_start_screen`, `tty_end_screen`
- `graph_on`/`graph_off` は no-op で確定
- **受け入れ基準**: マップ・ステータス行・メニューが tty 版と同じレイアウトで描画される（この時点ではまだ操作不可）

### P3: 入力 — 遊べるまで

- `SDL_KEYDOWN` / `SDL_TEXTINPUT` をバイト FIFO に積み、`sdl_getch()` が取り出す
- `include/unixconf.h:249` の `#define tgetch` を差し替え
- `get_scr_size()` の SDL 版
- 特殊キー（矢印、`ESC`、`Ctrl` 系）のマッピング
- **受け入れ基準**: 起動からキャラクタ選択、移動、`i`、`#quit` まで一通り遊べる

### P4: 色と見た目の同等化

- 実装: `term_start_color`, `term_end_color`, `has_color`, `tty_nhbell`, `tty_delay_output`, `tty_number_pad`
- 16 色パレットの定義
- **受け入れ基準**: `§7 A3` の画面一致テストに合格

### P5: 仕上げと計測

- ウィンドウリサイズ、フォント選択の設定化
- `sdlterm.c` の行数計測（H2 の検証）
- §7 の実証項目をすべて実施し記録
- **受け入れ基準**: §7 の A1〜A4 すべて合格

---

## 7. 実証項目と成功基準

**この実験の成果物は「動く SDL 版」ではなく「4 つの仮説の検証結果」である。** 以下を必ず計測・記録する。

### A1: 無改造性の証明（H1）

```sh
git diff --stat <base> -- win/tty/wintty.c win/tty/topl.c win/tty/getline.c
```

- **合格**: 0 行
- **部分的合格**: 差分があれば、その内容と理由を記録する。JNetHack への適用時に同じ差分が必要になるため

### A2: 規模の計測（H2）

- `wc -l win/tty/sdlterm.c`
- **合格**: 2,500 行以内。超過した場合は内訳（フォント処理 / 入力 / 描画）を記録

### A3: 画面一致テスト（総合品質）★中核

**同一シード・同一キー列で tty 版と SDL 版の画面が一致することを機械的に検証する。**

- tty 版: 既に用意した pty ドライバ（VT100 エミュレータで 80×24 のテキストグリッドに再現）で画面を取得
- SDL 版: `sdlterm.c` にデバッグフック（環境変数 `NH_SDL_DUMP` でセル配列をテキスト出力）を仕込む
- 両者を `diff` する

検証するキー列の例:

| ケース | キー列 | 見るもの |
|---|---|---|
| 起動〜キャラ選択 | `n V y` | タイトル、選択プロンプト |
| マップ初期表示 | `<space> <space>` | 部屋の罫線、`@`、ステータス行 |
| メニュー | `i` | 持ち物ウィンドウの重ね描き |
| 再描画 | `^R` | `cl_eos` / `clear_screen` の正しさ |
| メッセージ折返し | 長いメッセージを発生させる | `--More--` の位置 |

- **合格**: 全ケースで diff が 0

### A4: 2 セル幅描画テスト（H3）★JNetHack にとって最重要

素の NetHack に日本語は無いので、**専用のデバッグ経路で検証する。**

- `sdlterm.c` に、指定座標へ「2 セル幅を占有するグリフ」を描くテスト関数を用意する
- 検証内容:
  1. 2 セル幅グリフを桁 `x` に置いたとき、桁 `x` と `x+1` を占有し、`x+2` 以降がずれないこと
  2. 2 セル幅グリフの右半分にカーソルを移動して 1 セル文字を書いたとき、破綻せずに再描画されること（`cl_end` との相互作用）
  3. 端末のロケール設定（`LANG`）を変えても描画が一切変わらないこと
- **合格**: 1〜3 すべて。これが通れば `UTF8-PLAN.md` §4.1 の対策 A / C が不要になる

### A5: 移植性の記録（JNetHack への還元用）

- `putchar` 直呼び 24 箇所のうち、実際に SDL 側へ回さねばならなかった数
- JNetHack では同じ箇所が 90 箇所ある。**その比率が想定どおりか**を記録する

---

## 8. JNetHack への還元計画

実証実験が成功した場合、JNetHack への適用は以下になる。**この計画は実験結果を見てから確定する。**

| # | 作業 | 実験からの差分 |
|---|---|---|
| 1 | `sdlterm.c` の移植 | ほぼそのまま流用 |
| 2 | `putchar` 直呼びのリダイレクト | 24 → **90 箇所**（`jlib.c` の 30 箇所を含む） |
| 3 | バイト列 → 文字の再構成 | **新規**。`japanese/jlib.c:325 jbuffer()` にフックし、EUC 2 バイトを 1 グリフにして 2 セル幅で描く |
| 4 | フォント | ASCII 1 セル / 漢字 2 セルの等幅日本語フォント。1 枚で満たせなければ 1:2 比率の 2 枚構成 |
| 5 | 入力の IME 対応 | `SDL_StartTextInput`。UTF-8 で受けて EUC-JP に変換して `getline.c` へ渡す |

**還元が成立すれば、`UTF8-PLAN.md` のフェーズ 4（Unicode マップ描画）は不要になる**（SDL 側でコードポイントを直接描けるため）。フェーズ 1〜3（`kcode:U`）は、SDL を使わない tty 利用者のために引き続き価値がある。

---

## 9. リスクと未決事項

| # | 項目 | 評価 |
|---|---|---|
| R-1 | 3.2.3 が現代環境でビルドできるか | **解消済み**（§2）。設定 5 点で通る |
| R-2 | `wintty.c` が termcap の内部変数（`AS`/`AE`/`CO`/`LI`）を直接参照していないか | 未確認。`include/termcap.h` の `tc_gbl_data`/`tc_lcl_data` を SDL 版でも定義する必要があるかもしれない。**P1 で最初に判明する** |
| R-3 | `wintty.c:964` のコメント「Don't use xputs() because under unix it calls...」が示す特殊事情 | 未確認。P2 で該当箇所を読むこと |
| R-4 | フォント選定（A4 の前提） | ASCII と 2 セル幅グリフの比が正確に 1:2 になるフォントが必要 |
| R-5 | SDL2 と SDL3 のどちらを使うか | 未決。SDL2 の方が枯れており `SDL_TEXTINPUT` の実績もある。**SDL2 を推奨** |

### 未決事項

1. SDL2 / SDL3 の選択（推奨: SDL2）
2. フォントを SDL_ttf で扱うか、ビットマップフォントを内蔵するか（後者は依存が減るが CJK で破綻する）
3. 実験の成果物を素の 3.2.3 のまま残すか、JNetHack ブランチに取り込むか

---

## 10. 進め方の推奨

```
P0 (ビルド基盤)  →  P1 (文字が出る)  →  A4 を先行実施 ★
                                          ↓
                              ここで H3 が否定されたら計画を見直す
                                          ↓
                    P2 → P3 → P4 → P5 → A1/A2/A3/A5
```

**A4（2 セル幅描画テスト）を P1 直後に前倒しすることを強く推奨する。**
これは JNetHack にとって最も価値のある検証項目であり、かつ P1 の骨組みさえあれば実施できる。
もしここで問題が出れば、P2 以降に投じる労力を節約できる。

---

## 付録: 主要ファイル索引（素の 3.2.3）

| ファイル | 行数 | 役割 |
|---|---|---|
| `win/tty/termcap.c` | 1,110 | **差し替え対象**。非 static 関数 36 個、`tputs`/`tgoto` 使用 17 箇所 |
| `win/tty/wintty.c` | 2,335 | ウィンドウ管理・マップ描画。**無改造の目標**。`putchar` 直呼び 21 箇所 |
| `win/tty/topl.c` | 371 | メッセージ行。`putchar` 直呼び 1 箇所 |
| `win/tty/getline.c` | 214 | 行入力。`tty_nhgetch()` 経由 |
| `include/wintty.h` | — | バックエンド接続面の宣言（95-146 行） |
| `include/winprocs.h` | — | ウィンドウポート抽象（比較用） |
| `include/unixconf.h` | — | 249 行に `#define tgetch getchar`。**入力フック点** |
| `include/termcap.h` | — | `tc_gbl_data` / `tc_lcl_data`（R-2 で要確認） |
| `sys/share/ioctl.c` | — | `get_scr_size()` |
| `win/X11/*.c` | 8,520 | 新規ウィンドウポートを書いた場合の規模の実測値（比較用） |
