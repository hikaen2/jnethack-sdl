# JNetHack UTF-8 化 / Unicode マップ描画 計画書

- 対象: JNetHack 1.1.5 (NetHack 3.2.3 ベース) / ブランチ `jnethack-1.1.5`
- 作成日: 2026-07-28
- このファイルの文字コード: **UTF-8**（本文中で参照するソースは断りのない限り EUC-JP）

---

## 1. 目的とスコープ

### 目的

1. **UTF-8 端末で JNetHack が正しく動くようにする**（最優先）
2. マップを Unicode 文字（罫線素片など）で描画できるようにする
3. （任意）SDL によるセルグリッド描画バックエンドで、端末依存を完全に排除する

### スコープ内

- tty ウィンドウポート (`win/tty/`)
- 漢字コード変換層 (`japanese/jlib.c`)
- マップ記号テーブル (`src/drawing.c`, `include/rm.h`)

### スコープ外（今回やらないこと）

| 項目 | 理由 |
|---|---|
| **内部コードの完全 UTF-8 化** | 後述「方針B」。コストが一桁大きく、得られるものは開発体験の改善のみ。§3.2 参照 |
| X11 ポートの UTF-8 対応 | ロケール方針の判断が別途必要。tty 完成後に再検討 |
| GTK ポートの UTF-8 対応 | GDK1 依存 (`win/gtk/gtkmap.c:585` の `gdk_font_load`)。GTK2 以降への移植とセットになる |
| セーブデータ / bones / record の形式変更 | 方針A では不要（§3.1）。互換性を維持する |

---

## 2. 前提条件の検証結果（2026-07-28 実施済み）

**「現代の環境でビルドできるか」は検証済みで、答えは YES。** これは本計画全体の前提であり、既にクリアされている。

### 検証環境

| 項目 | 値 |
|---|---|
| OS | Debian 13 (Linux 6.12.41) |
| コンパイラ | GCC 14.2.0 |
| make / bison / flex | GNU Make 4.4.1 / bison 3.8.2 / flex 2.6.4 |

### 結果

- **ソース無改変でビルド成功。エラー 0 件、警告 147 件（`-Wall`）**
- `makedefs` / `lev_comp` / `dgn_comp` およびダンジョン定義のコンパイルもすべて通過
- 実際にゲームが起動し、マップ・日本語ステータス行・メニュー・物品名の活用まで正常動作を確認

```
                                武器
                                a -  +1 長剣(手にしている)
                                b -  +0 短剣
                                鎧
                                c - 呪われていない +3 小さな盾(身を守っている)
                                食料
                                d - 2つの呪われていない食料
                                (end)
                                             |............@.|
                                             +..........)f..+
                                             ------------+---
Tc4 見習い             強:18/03 早:11 耐:18 知:7 賢:10 魅:8  中立
地下:1  $:0  体:16(16) 魔:2(2) 鎧:6  経験:1
```

### ビルドに必要だったビルド設定の変更（ソース変更はゼロ）

| 変更 | 理由 |
|---|---|
| `CFLAGS` に `-std=gnu89` | GCC 14 の既定は C23。暗黙の関数宣言が**エラー**になる（`util/makedefs.c:2256` の `isspace` で最初に停止） |
| `CFLAGS` に `-fcommon` | GCC 10 以降 `-fno-common` が既定。tentative definition の重複でリンク失敗 |
| `WINTTYLIB = -ltermlib` → `-lncurses` | `libtermlib` は現存しない |
| tty のみに限定 | `include/config.h:214-215` の `X11_GRAPHICS` / `GTK_GRAPHICS` を無効化し、`src/Makefile` の `WINSRC`/`WINOBJ`/`WINLIB` を tty のみに。GTK1 は `gtk-config` ごと現存しない |

### 警告の内訳（`-Wall`、全 147 件）

```
35  builtin-declaration-mismatch   ← string.h 未 include で memcpy 等が暗黙宣言
26  implicit-function-declaration  ← 同上。ctype.h / string.h の追加で解消
20  misleading-indentation         ← 見た目のみ
19  pointer-sign                   ← char* / unsigned char* の混在（日本語処理由来）
 8  unused-but-set-variable
 6  int-to-pointer-cast / 4 pointer-to-int-cast
 4  stringop-truncation            ← strncpy の切り詰め
 2  int-in-bool-context / 1 parentheses / 1 maybe-uninitialized / 1 format-extra-args
```

深刻なものはない。ただし `-Wpointer-sign` 19 件は `jlib.c` 系の `char*`/`uchar*` 混在に由来し、**まさに本計画で触る箇所と重なる**。

### 再現手順

```sh
cp -a <repo> /path/to/build && cd /path/to/build
cp sys/unix/Makefile.top ./Makefile
cp sys/unix/Makefile.dat dat/Makefile
cp sys/unix/Makefile.doc doc/Makefile
cp sys/unix/Makefile.src src/Makefile
cp sys/unix/Makefile.utl util/Makefile
# 上記4点の設定変更を適用してから
make -C util && make -C src && make -C dat
# プレイディレクトリを用意し HACKDIR で起動
HACKDIR=/path/to/playdir ./jnethack -u testchar
```

> **注意**: `sys/unix/setup.sh` は cwd を見て動作するため、リポジトリ直下から実行すると
> **実リポジトリに Makefile 5 個を生成する**。上記のように直接 `cp` すること。

---

## 3. 現状アーキテクチャ（調査結果）

### 3.1 日本語テキスト処理

**内部コード (IC) は EUC-JP 固定。** コンパイル時に文字列リテラルのバイト値で判別している。

```c
/* japanese/jlib.c:21 */
#define IC ((unsigned char)("表"[0])==0x8a)   /* 0:EUC 1:SJIS */
```

**出力は 1 箇所に集約されている（これが本計画の鍵）。**

```
各所 → jputchar() / cputchar()          (japanese/jlib.c:428,434)
     → jbuffer() / cbuffer()            (japanese/jlib.c:325,395)  ← 2バイト溜めて変換
     → tty_jputc2() / tty_cputc2()      (japanese/jlib.c:304,283)
     → putchar()
```

出力コードは `OPTIONS=kcode:E/S/J/I` で切替可能（`src/options.c:1205`、説明文は `src/options.c:298`）。
既定値の設定は `sys/unix/unixmain.c:108` の `setkcode('I')`。
入力側は `str2ic()`（`japanese/jlib.c:124`）が設定ファイル読み込み（`src/files.c:1293`）で使われる。

**全体を貫く不変条件: 「1 バイト = 1 桁」「漢字 = 2 バイト = 2 桁」**

- `is_kanji(c)` は EUC では実質 `c & 0x80`（`japanese/jlib.c:47`）
- `is_kanji1/2(s,pos)` は先頭から 2 バイトずつ走査してバイト位置の第1/第2バイトを判定（`japanese/jlib.c:452` 付近）
- ステータス行の差分更新はバイトペア比較（`win/tty/wintty.c:1596-1657`、`ismbchar` は `:1629`）
- MAP/BASE 描画は **1 バイトごとに `curx++`**（`win/tty/wintty.c:1665-1690`）
- メッセージ行の折返し判定は `strlen()` を桁数として使用（`win/tty/topl.c:103,182,214`）

**その他の EUC 依存箇所**

| 箇所 | 内容 |
|---|---|
| `japanese/jconj.c:39` | `hira_tab[][2] = {{0xa4,0xa2},...}` — ひらがなを EUC バイト値で直接テーブル化した活用エンジン |
| `japanese/jtrns.c` + `include/jdata.h` | `dat/jtrnsmon.dat`/`jtrnsobj.dat` から makedefs が生成するバイト列ハッシュの英和辞書 |
| `win/tty/getline.c:110,118` | 入力を生バイトで受け（`' ' <= uc && uc < '\377'`）、`is_kanji2` で BS を 2 バイト戻す |
| `src/topten.c:288`, `src/botl.c:495` | `is_kanji1` によるバイト境界調整 |
| `src/do_name.c:202,432`, `src/engrave.c:108,114,1062,1064,1177` | 同上 |
| `win/X11/winmesg.c:281` | `split_japanese()` |

**確認済みの事実**

- 日本語リテラルを含むファイル: `src/` 75、`win/` 5、`sys/` 3、`util/` 1、`dat/` 15、`japanese/` 7
- **半角カナ（EUC の SS2 = 0x8E）はゲームデータ中に存在しない**（`japanese/Install.winnt` という文書ファイルにのみ出現）。したがって桁幅は「ASCII=1、それ以外=2」で完全に割り切れる
- `japanese/jlib.h` は `jlib.c` のほぼ複製だが**どこからも include されていない死にファイル**

### 3.2 方針A と方針B

| | **方針A: 内部 EUC 維持・境界で変換**（採用） | 方針B: 内部を完全 UTF-8 化（不採用） |
|---|---|---|
| 変更箇所 | `jlib.c`, `getline.c`, ビルド設定（数百行） | 全 106 ファイル、`jconj.c` 全面書換え、全 dat 変換 |
| 桁計算 | **無変更で正しいまま** | `strlen` ベースの桁計算を全て桁幅計算に置換 |
| バッファ | 無変更 | `BUFSZ 256` / `QBUFSZ 128`（`include/global.h:316-317`）、`NAMSZ` が 1.5 倍必要 |
| 既存セーブ/記録 | **互換維持** | 非互換（移行処理が必要） |
| リスク | 低 | 高 |

**方針A を採用する理由**: 変換が `putchar` 直前の最終段でのみ起きるため、呼び出し側は EUC の 2 バイト = 2 桁で桁を数え、端末は UTF-8 3 バイトを 2 桁で描画する。**桁の帳尻が自動的に合う。** 方針B はこの不変条件を壊すため、桁計算・バッファ・活用エンジンを同時に直す必要がある。

方針B の唯一の利点は「ソースを UTF-8 で直接編集できる」という開発体験であり、ゲームの UTF-8 対応そのものは方針A で達成される。

### 3.3 マップ描画パイプライン

```
src/display.c: print_glyph()
  → win/tty/wintty.c:2261  tty_print_glyph()     glyph → 1バイト記号に解決
       showsyms[offset]              地形        include/rm.h:217    uchar[MAXPCHARS]
       monsyms[mons[].mlet]          モンスター  include/decl.h:212  uchar[]
       oc_syms[objects[].oc_class]   物品        include/decl.h:210  uchar[]
  → win/tty/wintty.c:2162  g_putch(ch)
  → cputchar() → cbuffer() → putchar()
  → wintty.c:2360-2361     curx++                1グリフ = 1桁
```

**重要な発見が 2 つある。**

**(1) tty のマップウィンドウはセルバッファを持たない。**
`tty_putsym`（`win/tty/wintty.c:1497`）と `tty_print_glyph` は `cw->data[][]` に一切書き込まず、端末へ直接出力する（`cw->data` を使うのは NHW_STATUS / NHW_MENU / NHW_TEXT のみ）。
**つまり「1セル=1バイト」の配列を widen する必要がない。**

**(2) 拡張点が既に用意されている。**
`switch_graphics()`（`src/drawing.c:631`）と `assign_graphics()`（`src/drawing.c:619`）が `dec_graphics[]`（`src/drawing.c:398`）/ `ibm_graphics[]`（`src/drawing.c:301`）を `showsyms[]` に流し込む設計。`g_putch` も既に `ch & 0x80` を見て代替文字セット（SO/SI）へ切り替える分岐を持つ（`win/tty/wintty.c:2185`）。
**Unicode は第3のグラフィックスセットとして自然に嵌る。**

### 3.4 tty バックエンドの接続面（SDL 化の検討用）

`termcap.c` が提供し `wintty.c`/`topl.c` が使うプリミティブは **約 20 個**で、すべて `include/wintty.h:107-148` に列挙されている。

```
xputc / xputc2 / xputs / cl_end / cl_eos / clear_screen / home /
cmov / nocmov / backsp / standoutbeg / standoutend / graph_on / graph_off /
term_start_attr / term_end_attr / term_start_raw_bold / term_end_raw_bold /
term_start_color / term_end_color / has_color / tty_startup / tty_shutdown
```

一方、新規ウィンドウポートを書く場合は `struct window_procs`（`include/winprocs.h:9-59`）の約 40 関数を実装する必要がある。

| | 新規ウィンドウポート | **バックエンド差し替え** |
|---|---|---|
| 実装対象 | `window_procs` 約 40 関数 + メニュー/テキスト整形/ページャ | `termcap.c` のプリミティブ約 20 個 |
| 規模の実測 | X11 ポートが **約 9,500 行** | `termcap.c` が **1,115 行** |
| `wintty.c`(2,507行) / `topl.c`(467) / `getline.c`(245) | 全部書き直し | **無改造で流用** |

なお `putchar`/`fputc` の直呼びが **90 箇所**ある（`wintty.c` 54、`topl.c` 6、`jlib.c` 30）。抽象化を迂回しているため、SDL 化ではマクロで一括リダイレクトが必要。

---

## 4. フェーズ構成

```
フェーズ0  土台整備 ────────────────┐
                                      │
フェーズ1  出力の UTF-8 化 (kcode:U)  │  ここまでで
フェーズ2  入力の UTF-8 化            ├─ 「UTF-8 端末で遊べる」
フェーズ3  ロケール自動判別 ──────────┘

フェーズ4  Unicode マップ描画（フェーズ1-3 に依存）

フェーズ5  SDL バックエンド（任意・フェーズ4 の代替にもなる）
```

---

## フェーズ 0: 土台整備

**目的**: 型を広げる改修に入る前に、コンパイラの型チェックを効かせられる状態にする。

### 作業項目

| # | 内容 | 対象 |
|---|---|---|
| 0-1 | `<ctype.h>` / `<string.h>` の追加 | `util/makedefs.c`, `japanese/jconj.c`, `japanese/jlib.c` ほか警告の出るファイル |
| 0-2 | `-std=gnu89` に頼らずビルドできるようにする | 上記で暗黙宣言 61 件が解消するはず |
| 0-3 | ビルド設定の現代化を Makefile に反映 | `sys/unix/Makefile.src`（`-fcommon`、`-lncurses`） |
| 0-4 | 死にファイル `japanese/jlib.h` の削除判断 | どこからも include されていない |

### 受け入れ基準

- `-std=gnu17 -Wall` でエラー 0 件
- 警告が 147 件 → 80 件程度に減少
- フェーズ2で検証したゲーム画面が変化しないこと（回帰なし）

---

## フェーズ 1: 出力の UTF-8 化

**目的**: `OPTIONS=kcode:U` で UTF-8 端末に正しく出力する。

### 作業項目

| # | 内容 | 対象 |
|---|---|---|
| 1-1 | JIS X 0208 ⇄ Unicode 変換表を生成・追加 | 新規 `japanese/jis2uni.h`（約 17KB + 逆引き約 20KB）、生成スクリプト `japanese/mkjis2uni.py` を同梱 |
| 1-2 | `#define UTF8 3` を追加 | `japanese/jlib.c:14-16` の `EUC/SJIS/JIS` に続けて |
| 1-3 | `setkcode()` に `'U'/'u'` を追加 | `japanese/jlib.c:60` |
| 1-4 | 2バイト出力関数を 3 バイト出力可能にする | `japanese/jlib.c:304` `tty_jputc2()`、`:283` `tty_cputc2()` |
| 1-5 | `jbuffer()` の `IC == EUC` 分岐に `case UTF8:` を追加 | `japanese/jlib.c:349` 付近 |
| 1-6 | `cbuffer()`（無変換パス）の監査と対応 | `japanese/jlib.c:395`。現状 SJIS/JIS でも無変換で、**既存の潜在バグでもある** |
| 1-7 | `str2ic()` に UTF-8 → EUC 分岐を追加 | `japanese/jlib.c:124` |

> **変換表を iconv でなく静的表にする理由**: JNetHack は MSDOS/WinNT/Amiga 等もサポートしており libc 依存を増やしたくない。ただし `e2u()`/`u2e()` の内部実装として隠蔽し、Unix 限定なら iconv 版に差し替え可能な構造にする。

### 受け入れ基準

- `kcode:E` での表示が従来と完全一致（回帰なし）
- `kcode:U` かつ UTF-8 端末で、タイトル画面・メッセージ・ステータス行・メニュー・持ち物一覧が正しく表示される
- ステータス行の差分更新（`win/tty/wintty.c:1596-1657`）で漢字が半分だけ書き換わらないこと

---

## フェーズ 2: 入力の UTF-8 化

**目的**: UTF-8 端末からの日本語入力（`#name`、彫り込み、プレイヤー名）を受け付ける。

### 作業項目

| # | 内容 | 対象 |
|---|---|---|
| 2-1 | UTF-8 先行バイト (0xE0-0xEF) を検出し 3 バイト読んで `u2e()` で EUC 2 バイトに変換して `buf` に格納 | `win/tty/getline.c:118` |
| 2-2 | 同様の生バイト入力箇所の確認・対応 | `src/do_name.c:202,432`、`src/engrave.c:108,114,1062,1064,1177` |

> エコーは変換前の内部 EUC を `raw_putsyms` に渡せばフェーズ1の出力層が UTF-8 に戻すため、
> BS 処理（`win/tty/getline.c:110` の `is_kanji2` による 2 バイト戻し）は**無変更で動く**。

### 受け入れ基準

- UTF-8 端末で `#name` に日本語を入力し、正しく表示・保存される
- 入力途中の BS が漢字単位で正しく戻る
- 入力した名前が record ファイルに EUC-JP で保存され、既存 record と混在できる

---

## フェーズ 3: ロケール自動判別

### 作業項目

| # | 内容 | 対象 |
|---|---|---|
| 3-1 | `setlocale(LC_CTYPE,"")` + `nl_langinfo(CODESET)` で UTF-8 を検出したら `setkcode('U')` | `sys/unix/unixmain.c:108`（現在 `setkcode('I')` 固定） |
| 3-2 | 環境変数 / `OPTIONS` による明示指定を自動判別より優先 | 同上 |
| 3-3 | `kcode` オプションの説明文に UTF-8 を追記 | `src/options.c:298` |

### 受け入れ基準

- `LANG=ja_JP.UTF-8` で何も設定せずに起動して正しく表示される
- `LANG=ja_JP.eucJP` で従来どおり動く
- `OPTIONS=kcode:E` が自動判別を上書きする

**★ ここまででプロジェクトの主目的は達成。以降は任意。**

---

## フェーズ 4: Unicode マップ描画

**目的**: 罫線素片などでマップを描画する（`UNICODE_GRAPHICS` セットの追加）。

**依存**: フェーズ1〜3 完了必須。マップだけ UTF-8 バイトを出しても、メッセージ行が EUC-JP のままだと同一画面に 2 つのエンコーディングが混在して端末が壊れる。

### 作業項目

| # | 内容 | 対象 |
|---|---|---|
| 4-1 | `typedef uint32 nhsym;` を新設し記号テーブルの型を広げる | `include/rm.h`。対象は `rm.h:208-217`（`struct symdef.sym`, `showsyms[]`）、`include/decl.h:210,212`（`oc_syms[]`, `monsyms[]`）、`src/drawing.c:619`（`assign_graphics()` 引数）、`src/drawing.c` の REINCARNATION 用 `save_showsyms[]` |
| 4-2 | `unicode_graphics[MAXPCHARS]` テーブルを追加 | `src/drawing.c:398` の `dec_graphics[]` の隣 |
| 4-3 | `#define UNICODE_GRAPHICS 4` と `switch_graphics()` の分岐追加 | `include/rm.h:222` 付近、`src/drawing.c:631` |
| 4-4 | `UTF8graphics` boolean オプション追加 | `src/options.c:71` 付近（`DECgraphics` と同形）、切替は `src/options.c:1449` 付近 |
| 4-5 | `tty_print_glyph` の `uchar ch` → `nhsym ch`、`g_putch(int)` → `g_putch(nhsym)` | `win/tty/wintty.c:2261,2162` |
| 4-6 | `g_putch` に UTF-8 出力分岐を追加 | `win/tty/wintty.c:2185` の `else if (ch & 0x80)` の隣 |
| 4-7 | **wcwidth フォールバック**（§4.1 参照） | 起動時判定 |
| 4-8 | 記号を比較している箇所のガード | `src/do_name.c:107`（farlook で入力文字と `showsyms[sidx]` を比較）、`src/files.c:1138-1143`（`get_uchars`）、`src/options.c:762`（`graphics_opts` の `uchar translate[]`） |
| 4-9 | 重複コピーの追随修正 | `win/win32/nhprocs.c:626-645` に `tty_print_glyph` と同じ glyph→記号の switch がコピーされている（コンパイルを通す範囲で） |

テーブルの形:

```c
static nhsym unicode_graphics[MAXPCHARS] = {
/* 0*/  g_FILLER(S_stone),
        0x2502, /* S_vwall:   │ */
        0x2500, /* S_hwall:   ─ */
        0x250C, /* S_tlcorn:  ┌ */
        0x2510, /* S_trcorn:  ┐ */
        0x2514, /* S_blcorn:  └ */
        0x2518, /* S_brcorn:  ┘ */
        0x253C, /* S_crwall:  ┼ */
        ...
        0x00B7, /* S_room:    · */
        0x2248, /* S_pool:    ≈ */
};
```

`put_utf8()` は `cputchar()` を通さず生 `putchar` で出す（`cbuffer` は 2 バイト固定のため）。
桁カウントは `win/tty/wintty.c:2360` の `curx++` がそのまま 1 グリフ 1 桁で機能する。

### 4.1 ★最大の落とし穴: East Asian Ambiguous 幅

**これが JNetHack 固有の本命の問題。**

罫線素片 `U+2500〜U+257F`、中黒 `U+00B7`、`≈ U+2248` などは Unicode の East Asian Width で **Ambiguous** に分類される。**日本語ロケール / CJK フォントの端末では 2 桁幅で描画される**ため、80 桁のマップが右へずれて崩壊する。

DECgraphics ではこの問題は起きない（端末の代替文字セットは常に 1 桁）。

| 対策 | 内容 | 採否 |
|---|---|---|
| A. 端末設定に委ねる | `xterm -cjk_width` 無効、mlterm/Alacritty 等で ambiguous=narrow を指定。README に必須要件として明記 | **採用** |
| B. Narrow なコードポイントのみ使う | Ambiguous を避けると罫線が使えず、ASCII とほぼ変わらない | 不採用 |
| C. `wcwidth()` で実行時判定 | 起動時に `wcwidth(L'─')` を見て 2 なら自動的に `ASCII_GRAPHICS` へフォールバック | **採用** |

**A + C の併用**とする。C の自動フォールバックにより、環境依存でマップが崩れる事故を防ぐ。

### 受け入れ基準

- ambiguous=narrow 設定の UTF-8 端末で、80 桁のマップが崩れずに罫線描画される
- ambiguous=wide の端末で自動的に ASCII 描画にフォールバックする
- `kcode:E` / `UTF8graphics` オフで従来の描画に完全に戻る
- `;`（farlook）が引き続き動作する

---

## フェーズ 5（任意）: SDL バックエンド

**目的**: 端末に依存せずセルグリッドを自前で描画し、Ambiguous 幅問題を根本から消す。

### 方針

**新規ウィンドウポートは書かない。`win/tty/termcap.c` を `win/tty/sdlterm.c` に差し替える。**
§3.4 のとおり、実装対象は約 20 個のプリミティブのみで、`wintty.c` / `topl.c` / `getline.c` は無改造で流用できる。規模の目安は 1,500〜2,500 行（新規ポートなら約 9,500 行）。

### 得られるもの

- **East Asian Ambiguous 幅問題が消滅する。** 端末と幅を交渉する必要がなく、`─` を 1 セルで描くと自分で決められる。§4.1 の対策 A/C が不要になる
- 漢字の 2 セル描画を正確に制御できる
- `jlib.c` の kcode 変換層が出力側で不要になる。SO/SI の代替文字セット切替（`win/tty/wintty.c:2185`）も削除できる
- タイル表示を後から足すのが、セルにビットマップを blit するだけになる

### 素直に面倒な点

| # | 内容 |
|---|---|
| 5-a | **バイト列 → コードポイントの再構成。** tty ポートは 1 バイトずつ吐き、桁位置は呼び出し側が `ttyDisplay->curx` で数える。SDL 側は 2 バイト溜めて 1 グリフにし、開始桁に 2 セル幅で描く必要がある。ただしこのバッファリングは `japanese/jlib.c:325 jbuffer()` が既にやっているので、そこにフックすれば済む。**ここが一番細かい作業** |
| 5-b | `putchar`/`fputc` の直呼び 90 箇所（`wintty.c` 54、`topl.c` 6、`jlib.c` 30）のマクロ一括リダイレクト。機械的だが漏れると画面が壊れる |
| 5-c | 入力。`getline.c` は `tgetch()` で 1 バイトずつ読む前提。`SDL_TEXTINPUT` イベントをバイト FIFO に積み `tgetch` が引く形にする。**逆に日本語入力は SDL2 の IME サポート (`SDL_StartTextInput`) で生 tty より楽になる** |
| 5-d | フォント。ASCII = 1 セル、漢字 = ちょうど 2 セルになる等幅日本語フォントが必要。1 フォントで満たせなければ ASCII 用と CJK 用を 1:2 の比率で 2 枚使う |
| 5-e | `^Z` サスペンド、`get_scr_size`、ウィンドウリサイズの扱い |

### 受け入れ基準

- tty 版と同一のゲーム進行・同一の画面レイアウトが得られる
- 端末のロケール設定に一切依存しない
- 罫線と漢字が桁ずれなく描画される

---

## 5. リスクと未決事項

| # | 項目 | 状態 |
|---|---|---|
| R-1 | 現代環境でビルドできるか | **解消済み**（§2）。当初これを最大の未知数と見ていたが、障害にならないことを実測で確認 |
| R-2 | `cbuffer()` の無変換パス | **既存の潜在バグ**。SJIS/JIS 指定時もマップ描画が無変換で出ている。フェーズ 1-6 で監査 |
| R-3 | East Asian Ambiguous 幅 | フェーズ4 の最大リスク。対策 A+C で緩和するが、完全解決はフェーズ5 のみ |
| R-4 | X11 / GTK ポート | 今回スコープ外。`showsyms` の型変更（4-1）で**コンパイルが通らなくなる可能性**があり、追随修正が必要 |
| R-5 | 変換表のライセンス | JIS X 0208 ⇄ Unicode 表を機械生成する際の出典と再配布条件を確認すること |

### 未決事項（着手前に決めたいこと）

1. **フェーズ4 とフェーズ5 のどちらを採るか。** 両方やる必要はない。フェーズ5 はフェーズ4 の上位互換だが、フェーズ4 の 10 倍近い作業量
2. 変換表を静的表にするか iconv にするか（Unix 専用でよければ iconv が簡単）
3. X11 ポートを維持するか、tty のみに整理するか

---

## 6. 推奨する着手順

1. **フェーズ 0**（土台整備）— どの計画に進むにせよ無駄にならない
2. **フェーズ 1 → 2 → 3**（UTF-8 化）— ここで主目的達成。数百行
3. ここでいったん止めて、フェーズ4 / フェーズ5 のどちらに進むかを判断する

---

## 付録: 主要ファイル索引

| ファイル | 行数 | 役割 |
|---|---|---|
| `japanese/jlib.c` | 749 | 漢字コード変換・出力バッファリング。**フェーズ1 の主戦場** |
| `japanese/jconj.c` | — | 動詞活用エンジン。EUC バイト値のひらがな表を持つ。方針A では触らない |
| `japanese/jtrns.c` | 260 | 英和辞書引き（`include/jdata.h` は makedefs 生成） |
| `japanese/jlib.h` | 535 | **死にファイル**（include されていない） |
| `win/tty/wintty.c` | 2,507 | tty ウィンドウ管理・マップ描画 |
| `win/tty/termcap.c` | 1,115 | 端末制御プリミティブ。**フェーズ5 の差し替え対象** |
| `win/tty/topl.c` | 467 | メッセージ行 |
| `win/tty/getline.c` | 245 | 行入力。**フェーズ2 の主戦場** |
| `src/drawing.c` | — | 記号テーブル。**フェーズ4 の主戦場** |
| `include/rm.h` | — | `showsyms` / `defsyms` / グラフィックスセット定義 |
| `include/wintty.h` | — | tty バックエンドの接続面（107-148行） |
| `include/winprocs.h` | — | ウィンドウポート抽象（9-59行） |
